// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

/*
 * Repeated install/erase cycles for
 * nsmd/nsmDebugToken/nsmDebugTokenUnified.cpp.
 *
 * The status refresh that runs before Success is published must happen on
 * EVERY iteration, not just the first, on all three completion paths:
 *
 *   - eraseToken            (targeted erase)
 *   - installTokenAsync     (targeted install)
 *   - installTokenDirect    (aggregate install)
 *
 * Each test drives the same operation repeatedly and asserts the published
 * InstallationStatus after every iteration. Because the refresh is what
 * publishes that status, a status matching the operation just performed can
 * only be observed if the refresh completed before Success was reported.
 * Before the fix the refresh was detached, so the published value lagged the
 * operation and these assertions would fail from the first cycle onwards.
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "debug-token.h"
#include "debug-token/tlv.h"
#include "debug-token/types.h"

#define private public
#define protected public

#include "nsmDebugTokenUnified.hpp"

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmDebugTokenUnifiedMultiCycleTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "Unified_MultiCycle";
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string debugTokenDeviceType = "GPU";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;
    std::shared_ptr<NsmDebugTokenUnifiedObject> debugToken;

    static constexpr int cycles = 3;

    NsmDebugTokenUnifiedMultiCycleTest() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(mockDevice, nullptr);
    }

    ~NsmDebugTokenUnifiedMultiCycleTest()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
            bus, name, uuid, debugTokenDeviceType);
        EXPECT_NE(debugToken, nullptr);
    }

    // -- Response helpers --

    Response createEraseTokenResponse(uint8_t completionCode = NSM_SUCCESS,
                                      uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_erase_token_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        [[maybe_unused]] auto rc =
            encode_nsm_erase_token_resp(0, completionCode, reasonCode, msg);
        return response;
    }

    Response createInstallTokenResponse(uint8_t completionCode = NSM_SUCCESS,
                                        uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_install_token_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        [[maybe_unused]] auto rc =
            encode_nsm_install_token_resp(0, completionCode, reasonCode, msg);
        return response;
    }

    Response createQueryTokenResponse(const std::vector<uint8_t>& tlvPayload,
                                      uint8_t completionCode = NSM_SUCCESS,
                                      uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_resp) -
                              1 + tlvPayload.size(),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        [[maybe_unused]] auto rc = encode_nsm_query_token_resp(
            0, completionCode, reasonCode, tlvPayload.data(), tlvPayload.size(),
            msg);
        return response;
    }

    // TLV payload reporting a token as installed and processed.
    std::vector<uint8_t> installedTokenPayload()
    {
        debug_token::tlv_encoder::Structure enc;
        enc.add(debug_token::types::InstallationStatus, uint8_t(1));
        enc.add(debug_token::types::ProcessingStatus, uint8_t(1));
        return enc.encode();
    }

    // TLV payload reporting no token installed.
    std::vector<uint8_t> erasedTokenPayload()
    {
        debug_token::tlv_encoder::Structure enc;
        enc.add(debug_token::types::InstallationStatus, uint8_t(0));
        enc.add(debug_token::types::ProcessingStatus, uint8_t(0));
        return enc.encode();
    }

    // Single-chunk token file; caller owns the returned descriptor.
    int makeTokenFile(std::vector<uint8_t>& data)
    {
        char tempPath[] = "/tmp/multicycle_token_XXXXXX";
        int fd = mkstemp(tempPath);
        if (fd == -1)
        {
            return -1;
        }
        if (write(fd, data.data(), data.size()) < 0)
        {}
        lseek(fd, 0, SEEK_SET);
        unlink(tempPath);
        return fd;
    }

    std::shared_ptr<NsmDebugTokenUnifiedObject::TokenInstallationInfo>
        createTokenInstallationInfo(const std::vector<uint8_t>& tokenData)
    {
        char tempPath[] = "/tmp/multicycle_info_XXXXXX";
        int fd = mkstemp(tempPath);
        EXPECT_NE(fd, -1);
        if (fd != -1)
        {
            ssize_t written = write(fd, tokenData.data(), tokenData.size());
            EXPECT_EQ(written, static_cast<ssize_t>(tokenData.size()));
            lseek(fd, 0, SEEK_SET);
            unlink(tempPath);
        }
        return std::make_shared<
            NsmDebugTokenUnifiedObject::TokenInstallationInfo>(
            fd, tokenData.size());
    }

    auto callInstallTokenAsync(
        std::shared_ptr<NsmDebugTokenUnifiedObject::TokenInstallationInfo> info)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        debugToken->installTokenAsyncHandler(info, statusInterface,
                                             valueInterface);
        return std::make_pair(statusInterface, valueInterface);
    }
};

// ============================================================================
// installTokenAsync over repeated cycles: the targeted install path must also
// refresh status on every iteration.
// ============================================================================

TEST_F(NsmDebugTokenUnifiedMultiCycleTest,
       installTokenAsync_RepeatedCycles_RefreshesEveryCycle)
{
    debugToken->installationChunkSize = 4084;
    auto payload = installedTokenPayload();

    EXPECT_CALL(*mockDevice, postPatchIO)
        .Times(cycles)
        .WillRepeatedly(
            mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)));

    EXPECT_CALL(*mockDevice, sensorIO)
        .Times(cycles)
        .WillRepeatedly(
            mockSensorIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));

    for (int i = 0; i < cycles; ++i)
    {
        std::vector<uint8_t> tokenData(64, 0xC3);
        auto info = createTokenInstallationInfo(tokenData);

        auto [statusIntf, valueIntf] = callInstallTokenAsync(info);
        EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success)
            << "cycle " << i;
        EXPECT_TRUE(debugToken->installationStatus()) << "cycle " << i;
    }
}

// ============================================================================
// installTokenDirect over repeated cycles: the aggregate install path must
// refresh status on every iteration, so N installs produce N refreshes.
// ============================================================================

TEST_F(NsmDebugTokenUnifiedMultiCycleTest,
       installTokenDirect_RepeatedCycles_RefreshesEveryCycle)
{
    debugToken->installationChunkSize = 4084;
    auto payload = installedTokenPayload();

    EXPECT_CALL(*mockDevice, postPatchIO)
        .Times(cycles)
        .WillRepeatedly(
            mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)));

    // One refresh per install, not one refresh overall.
    EXPECT_CALL(*mockDevice, sensorIO)
        .Times(cycles)
        .WillRepeatedly(
            mockSensorIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));

    for (int i = 0; i < cycles; ++i)
    {
        std::vector<uint8_t> data(64, 0xA5);
        int fd = makeTokenFile(data);
        ASSERT_NE(fd, -1);

        uint16_t errorCode = 0xFFFF;
        std::string errorMessage;
        debugToken->installTokenDirect(fd, data.size(), errorCode,
                                       errorMessage);

        EXPECT_EQ(errorCode, 0) << "cycle " << i;
        EXPECT_EQ(errorMessage, "Success") << "cycle " << i;
        // Status published by the awaited refresh, not left stale.
        EXPECT_TRUE(debugToken->installationStatus()) << "cycle " << i;
    }
}

// ============================================================================
// eraseToken over repeated cycles, for each EraseType variant.
// ============================================================================

TEST_F(NsmDebugTokenUnifiedMultiCycleTest,
       eraseToken_RepeatedCycles_RefreshesEveryCycle)
{
    using EraseTypeEnum =
        sdbusplus::server::com::nvidia::debug_token::Action::EraseType;
    using TokenTypeEnum =
        sdbusplus::common::com::nvidia::debug_token::Common::Types;

    auto payload = erasedTokenPayload();

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillRepeatedly(
            mockPostPatchIO(createEraseTokenResponse(NSM_SUCCESS, 0)));

    EXPECT_CALL(*mockDevice, sensorIO)
        .Times(cycles)
        .WillRepeatedly(
            mockSensorIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));

    for (int i = 0; i < cycles; ++i)
    {
        auto objPath = debugToken->eraseToken(EraseTypeEnum::EraseAll,
                                              TokenTypeEnum::None);
        EXPECT_FALSE(objPath.str.empty()) << "cycle " << i;
        EXPECT_FALSE(debugToken->installationStatus()) << "cycle " << i;
    }
}

// ============================================================================
// Alternating install and erase, the sequence a tester actually runs.
// Each half-cycle refreshes, so 2N operations produce 2N refreshes and the
// published status tracks the operation rather than lagging it.
// ============================================================================

TEST_F(NsmDebugTokenUnifiedMultiCycleTest,
       installThenErase_AlternatingCycles_StatusTracksEachOperation)
{
    using EraseTypeEnum =
        sdbusplus::server::com::nvidia::debug_token::Action::EraseType;
    using TokenTypeEnum =
        sdbusplus::common::com::nvidia::debug_token::Common::Types;

    debugToken->installationChunkSize = 4084;
    auto installed = installedTokenPayload();
    auto erased = erasedTokenPayload();

    // cycles == 3: install, erase, install, erase, install, erase.
    // Consecutive WillOnce actions on one expectation are consumed in order,
    // so each operation gets the response type it actually decodes.
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillOnce(mockPostPatchIO(createEraseTokenResponse(NSM_SUCCESS, 0)))
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillOnce(mockPostPatchIO(createEraseTokenResponse(NSM_SUCCESS, 0)))
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillOnce(mockPostPatchIO(createEraseTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    // One refresh after each operation, reporting the state that operation
    // produced.
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(installed, NSM_SUCCESS, 0)))
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(erased, NSM_SUCCESS, 0)))
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(installed, NSM_SUCCESS, 0)))
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(erased, NSM_SUCCESS, 0)))
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(installed, NSM_SUCCESS, 0)))
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(erased, NSM_SUCCESS, 0)))
        .WillRepeatedly(mockSensorIO(NSM_ERROR));

    for (int i = 0; i < cycles; ++i)
    {
        std::vector<uint8_t> data(64, 0x5A);
        int fd = makeTokenFile(data);
        ASSERT_NE(fd, -1);

        uint16_t errorCode = 0xFFFF;
        std::string errorMessage;
        debugToken->installTokenDirect(fd, data.size(), errorCode,
                                       errorMessage);
        EXPECT_EQ(errorCode, 0) << "install cycle " << i;
        EXPECT_TRUE(debugToken->installationStatus()) << "install cycle " << i;

        auto objPath = debugToken->eraseToken(EraseTypeEnum::EraseAll,
                                              TokenTypeEnum::None);
        EXPECT_FALSE(objPath.str.empty()) << "erase cycle " << i;
        EXPECT_FALSE(debugToken->installationStatus()) << "erase cycle " << i;
    }
}

// ============================================================================
// A failing refresh is logged but must not change the operation outcome: the
// device already reported the install itself as successful.
// ============================================================================

TEST_F(NsmDebugTokenUnifiedMultiCycleTest,
       installTokenDirect_RefreshFails_OperationStillSucceeds)
{
    debugToken->installationChunkSize = 4084;

    EXPECT_CALL(*mockDevice, postPatchIO)
        .Times(cycles)
        .WillRepeatedly(
            mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)));

    // Every refresh fails.
    EXPECT_CALL(*mockDevice, sensorIO).WillRepeatedly(mockSensorIO(NSM_ERROR));

    for (int i = 0; i < cycles; ++i)
    {
        std::vector<uint8_t> data(64, 0x33);
        int fd = makeTokenFile(data);
        ASSERT_NE(fd, -1);

        uint16_t errorCode = 0xFFFF;
        std::string errorMessage;
        debugToken->installTokenDirect(fd, data.size(), errorCode,
                                       errorMessage);

        EXPECT_EQ(errorCode, 0) << "cycle " << i;
        EXPECT_EQ(errorMessage, "Success") << "cycle " << i;
    }
}

// ============================================================================
// Multi-chunk install repeated: the refresh happens once per install, after
// the final chunk, not once per chunk.
// ============================================================================

TEST_F(NsmDebugTokenUnifiedMultiCycleTest,
       installTokenDirect_MultiChunkRepeated_OneRefreshPerInstall)
{
    debugToken->installationChunkSize = 50;
    auto payload = installedTokenPayload();

    constexpr int chunksPerInstall = 3; // 150 bytes / 50
    EXPECT_CALL(*mockDevice, postPatchIO)
        .Times(cycles * chunksPerInstall)
        .WillRepeatedly(
            mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)));

    EXPECT_CALL(*mockDevice, sensorIO)
        .Times(cycles)
        .WillRepeatedly(
            mockSensorIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));

    for (int i = 0; i < cycles; ++i)
    {
        std::vector<uint8_t> data(150, 0x77);
        int fd = makeTokenFile(data);
        ASSERT_NE(fd, -1);

        uint16_t errorCode = 0xFFFF;
        std::string errorMessage;
        debugToken->installTokenDirect(fd, data.size(), errorCode,
                                       errorMessage);

        EXPECT_EQ(errorCode, 0) << "cycle " << i;
        EXPECT_TRUE(debugToken->installationStatus()) << "cycle " << i;
    }
}
