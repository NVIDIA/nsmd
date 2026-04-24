/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Tests for NsmDebugTokenUnifiedObject::installMultiRecordToken (the
 * multi-record install path; dispatched from installToken()), added by the
 * targeted multi-record install support change. These tests synthesize a
 * DebugTokenHeader + N TLV records in a memfd and walk the per-record install
 * loop end-to-end:
 *
 *   - All records install cleanly                    -> Success
 *   - Some records fail (postPatchIO error per chunk) -> partial-success
 *     accounting (failCount > 0, ok < total)
 *   - Oversized header.fileSize                       -> early reject
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "debug-token.h"
#include "debug-token/tlv.h"
#include "debug-token/types.h"

#define private public
#define protected public

#include "debugTokenUtils.hpp"
#include "nsmDebugTokenUnified.hpp"

#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <vector>

using namespace nsm;
using namespace nsm::token_utils;

namespace
{

// Build a single TLV record's bytes (StructureHeader + payloadSize bytes).
std::vector<uint8_t> makeRecord(uint32_t payloadSize, uint8_t fillByte)
{
    debug_token::StructureHeader sh{};
    std::memset(&sh, 0, sizeof(sh));
    sh.identifier[0] = debug_token::TLV_IDENTIFIER[0];
    sh.identifier[1] = debug_token::TLV_IDENTIFIER[1];
    sh.identifier[2] = debug_token::TLV_IDENTIFIER[2];
    sh.identifier[3] = debug_token::TLV_IDENTIFIER[3];
    sh.versionMajor = 1;
    sh.versionMinor = 0;
    sh.size = payloadSize;

    std::vector<uint8_t> bytes(sizeof(sh) + payloadSize);
    std::memcpy(bytes.data(), &sh, sizeof(sh));
    std::fill(bytes.begin() + sizeof(sh), bytes.end(), fillByte);
    return bytes;
}

// Compose a multi-record token file: DebugTokenHeader followed by `records`.
// If overrideFileSize is set, that value is used in header.fileSize verbatim
// (so callers can simulate oversized / mismatched headers).
std::vector<uint8_t>
    makeMultiRecordFile(const std::vector<std::vector<uint8_t>>& records,
                        uint32_t overrideFileSize = 0)
{
    DebugTokenHeader hdr{};
    std::memset(&hdr, 0, sizeof(hdr));
    hdr.version = 1;
    hdr.type = FileTypeDebugToken;
    hdr.numberOfRecords = static_cast<uint16_t>(records.size());
    hdr.offsetToListOfStructs = sizeof(DebugTokenHeader);

    size_t total = sizeof(DebugTokenHeader);
    for (const auto& r : records)
    {
        total += r.size();
    }
    hdr.fileSize = (overrideFileSize != 0) ? overrideFileSize
                                           : static_cast<uint32_t>(total);

    std::vector<uint8_t> out(total);
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    size_t off = sizeof(DebugTokenHeader);
    for (const auto& r : records)
    {
        std::memcpy(out.data() + off, r.data(), r.size());
        off += r.size();
    }
    return out;
}

int writeMemFd(const std::vector<uint8_t>& data)
{
    int fd = memfd_create("multirecord_test", 0);
    if (fd < 0)
    {
        return -1;
    }
    if (!data.empty())
    {
        ssize_t n = write(fd, data.data(), data.size());
        if (n != static_cast<ssize_t>(data.size()))
        {
            close(fd);
            return -1;
        }
    }
    lseek(fd, 0, SEEK_SET);
    return fd;
}

} // namespace

// ============================================================================
// Fixture
// ============================================================================

struct NsmDebugTokenUnifiedMultiRecordTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "Unified_MultiRecord";
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string debugTokenDeviceType = "GPU";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;
    std::shared_ptr<NsmDebugTokenUnifiedObject> debugToken;

    NsmDebugTokenUnifiedMultiRecordTest() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(mockDevice, nullptr);
    }

    ~NsmDebugTokenUnifiedMultiRecordTest()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
            bus, name, uuid, debugTokenDeviceType);
        EXPECT_NE(debugToken, nullptr);
        // Each record is small enough to fit in a single chunk so the per-
        // record install issues exactly one postPatchIO. That keeps the
        // expected mock-call count predictable in the multi-record loop.
        debugToken->installationChunkSize = 4096;
    }

    Response createInstallTokenResponse(uint8_t cc = NSM_SUCCESS,
                                        uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_install_token_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        [[maybe_unused]] auto rc =
            encode_nsm_install_token_resp(0, cc, reasonCode, msg);
        return response;
    }

    // Drive the multi-record install path against fd containing a multi-record
    // file of size totalSize. The single-vs-multi dispatch now lives in
    // installToken(); this helper parses the header the same way and calls
    // installMultiRecordToken() directly (the isMultiRecordContainer()
    // discrimination is covered separately in debugTokenUtils_test).
    auto callMultiRecordInstall(int fd, size_t totalSize)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        auto info =
            std::make_shared<NsmDebugTokenUnifiedObject::TokenInstallationInfo>(
                fd, totalSize);
        DebugTokenHeader header{};
        [[maybe_unused]] auto n = read(fd, &header, sizeof(header));
        lseek(fd, 0, SEEK_SET);
        debugToken
            ->installMultiRecordToken(info, header, statusInterface,
                                      valueInterface)
            .data();
        return std::make_pair(statusInterface, valueInterface);
    }
};

// ============================================================================
// All records install successfully -> Success status, no failures reported
// ============================================================================

TEST_F(NsmDebugTokenUnifiedMultiRecordTest, AllRecordsInstallSuccess)
{
    std::vector<std::vector<uint8_t>> recs = {
        makeRecord(/*payloadSize=*/16, 0xAA),
        makeRecord(/*payloadSize=*/16, 0xBB),
        makeRecord(/*payloadSize=*/16, 0xCC),
    };
    auto file = makeMultiRecordFile(recs);
    int fd = writeMemFd(file);
    ASSERT_GE(fd, 0);

    // Each record fits in one chunk, so its installTokenDirect issues exactly
    // one install postPatchIO. installTokenDirect then fires a per-record
    // queryTokenHandler via .detach() (fire-and-forget) for the on-device
    // status refresh; that detached coroutine is never driven in the unit test
    // (no io_context run), so it issues no postPatchIO here. Net: one install
    // call per record. Return a valid install-success response to every call so
    // all three records install cleanly (a valid install reply also lets any
    // stray call decode harmlessly).
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillRepeatedly(
            mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)));

    auto [statusIntf, valueIntf] = callMultiRecordInstall(fd, file.size());

    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
    using ValueTuple = std::tuple<uint16_t, std::string>;
    ASSERT_TRUE(std::holds_alternative<ValueTuple>(valueIntf->value()));
    const auto result = std::get<ValueTuple>(valueIntf->value());
    EXPECT_EQ(std::get<0>(result), 0u);
    EXPECT_EQ(std::get<1>(result), "Success");

    close(fd);
}

// ============================================================================
// Middle record fails on the wire -> partial-success accounting:
//   okCount in (0, total) and failCount > 0
// ============================================================================

TEST_F(NsmDebugTokenUnifiedMultiRecordTest, PartialFailureOnMiddleRecord)
{
    std::vector<std::vector<uint8_t>> recs = {
        makeRecord(16, 0xAA), makeRecord(16, 0xBB), makeRecord(16, 0xCC)};
    auto file = makeMultiRecordFile(recs);
    int fd = writeMemFd(file);
    ASSERT_GE(fd, 0);

    // One install postPatchIO per record (the success-path queryTokenHandler is
    // detached and never driven in the unit test, so it issues no call). The
    // middle record's install returns NSM_ERROR on the wire, so
    // installTokenDirect bails before its query: rec1 install(ok), rec2
    // install(FAIL), rec3 install(ok) -> 2 of 3 installed, 1 failed.
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillOnce(mockPostPatchIO(NSM_ERROR))
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(
            mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)));

    auto [statusIntf, valueIntf] = callMultiRecordInstall(fd, file.size());

    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
    using ValueTuple = std::tuple<uint16_t, std::string>;
    ASSERT_TRUE(std::holds_alternative<ValueTuple>(valueIntf->value()));
    const auto result = std::get<ValueTuple>(valueIntf->value());
    EXPECT_EQ(std::get<0>(result), 1u);
    // Partial-success message format: "Installed X of Y record(s); Z failed"
    EXPECT_NE(std::get<1>(result).find("Installed 2 of 3"), std::string::npos);
    EXPECT_NE(std::get<1>(result).find("1 failed"), std::string::npos);

    close(fd);
}

// ============================================================================
// header.fileSize > info->totalSize  -> rejected before any postPatchIO
// ============================================================================

TEST_F(NsmDebugTokenUnifiedMultiRecordTest, RejectsOversizedHeaderFileSize)
{
    std::vector<std::vector<uint8_t>> recs = {makeRecord(16, 0xAA)};
    // Claim a fileSize 1 GiB larger than what is actually on disk.
    auto file = makeMultiRecordFile(recs, /*overrideFileSize=*/(1U << 30));
    int fd = writeMemFd(file);
    ASSERT_GE(fd, 0);

    // Multi-record path must reject before issuing any installation IO.
    EXPECT_CALL(*mockDevice, postPatchIO).Times(0);

    auto [statusIntf, valueIntf] = callMultiRecordInstall(fd, file.size());
    (void)valueIntf;
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);

    close(fd);
}
