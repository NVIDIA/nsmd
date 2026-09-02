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
 * Tests for NsmSetErrorInjectionCapabilities: a multi-type batch lands in one
 * device write, untouched types keep their value, validation rejects before
 * any write, and serialized patches reseed from the published mask.
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

// Pulled in ahead of the private/protected redefinition below so that no
// standard header is parsed with those keywords rewritten.
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

using namespace ::testing;

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"

#include "nsmErrorInjection/nsmErrorInjection.hpp"
#include "nsmSetErrorInjection.hpp"

using namespace nsm;

namespace
{

using CapabilityVariant =
    std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
using CapabilityEntries =
    std::vector<std::tuple<std::string, CapabilityVariant>>;

/** The four capability types createNsmErrorInjectionSensors registers. Each
 *  maps to a distinct EI mask bit. */
constexpr std::array<ErrorInjectionCapabilityIntf::Type, 4> standardTypes = {
    ErrorInjectionCapabilityIntf::Type::MemoryErrors,
    ErrorInjectionCapabilityIntf::Type::PCIeErrors,
    ErrorInjectionCapabilityIntf::Type::NVLinkErrors,
    ErrorInjectionCapabilityIntf::Type::ThermalErrors};

std::string leafName(ErrorInjectionCapabilityIntf::Type type)
{
    auto name = ErrorInjectionCapabilityIntf::convertTypeToString(type);
    return name.substr(name.find_last_of('.') + 1);
}

/** Local copy of the fixture helper: SensorManagerTest::allocMessage sits
 *  above the class's first access specifier, so it is implicitly private and
 *  cannot be reached from a derived fixture. */
void allocResponse(const Response& response,
                   std::shared_ptr<const nsm_msg>& responseMsg,
                   size_t& responseLen)
{
    responseLen = response.size();
    if (responseLen > 0)
    {
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
            [](const nsm_msg* ptr) { free((void*)ptr); });
        memcpy((uint8_t*)responseMsg.get(), response.data(), responseLen);
    }
}

bool maskBitSet(const nsm_error_injection_types_mask& mask,
                ErrorInjectionCapabilityIntf::Type type)
{
    auto bit = getErrorInjectionBitPosition(type);
    return (mask.mask[bit / 8] & (1 << (bit % 8))) != 0;
}

/**
 * @brief Owns the device table for the fixture. SensorManagerTest binds a
 *        reference in its constructor, and bases initialize before members, so
 *        declaring it in a preceding base guarantees it is constructed first.
 */
struct DeviceTableOwner
{
    NsmDeviceTable devices;
};

} // namespace

// ============================================================================
// Fixture
// ============================================================================

struct NsmSetErrorInjectionBatchTest :
    public testing::Test,
    public utils::DBusTest,
    protected DeviceTableOwner,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::shared_ptr<MockNsmDevice> gpu;

    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    std::map<ErrorInjectionCapabilityIntf::Type,
             std::shared_ptr<ErrorInjectionCapabilityIntf>>
        byType;
    std::shared_ptr<NsmErrorInjectionEnabled> enabledSensor;
    std::shared_ptr<NsmSetErrorInjectionCapabilities> capabilities;

    NsmSetErrorInjectionBatchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);

        auto& bus = utils::DBusHandler::getBus();
        for (auto type : standardTypes)
        {
            std::filesystem::path path =
                std::filesystem::path("/test/ei/batch") / leafName(type);
            auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(
                bus, path.string().c_str());
            intf->type(type);
            intf->enabled(false);
            interfaces[path] = intf;
            byType[type] = intf;
        }

        enabledSensor = std::make_shared<NsmErrorInjectionEnabled>(
            NsmInterfaceProvider<ErrorInjectionCapabilityIntf>(
                "ErrorInjectionCapability", "NSM_ErrorInjectionCapability",
                interfaces));
        capabilities = std::make_shared<NsmSetErrorInjectionCapabilities>(
            "ErrorInjectionCapabilities", mockManager, interfaces,
            enabledSensor);
    }

    ~NsmSetErrorInjectionBatchTest()
    {
        cleanupDeviceSensors(devices);
    }

    /** A successful SetCurrentErrorInjectionTypesV1 response. */
    static Response successResponse()
    {
        Response buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_set_current_error_injection_types_v1_resp(0, NSM_SUCCESS,
                                                         ERR_NULL, msg);
        return buf;
    }

    /** A GetCurrentErrorInjectionTypes response carrying @p mask. */
    static Response typesResponse(const nsm_error_injection_types_mask& mask)
    {
        Response buf(sizeof(nsm_msg_hdr) +
                         sizeof(nsm_get_error_injection_types_mask_resp),
                     0);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_get_current_error_injection_types_v1_resp(0, NSM_SUCCESS,
                                                         ERR_NULL, &mask, msg);
        return buf;
    }

    /**
     * Expects the write followed by the in-lock read-back. @p captured takes
     * the mask the write carried; the read-back echoes it, so the PDIs end up
     * with exactly what was written.
     */
    void expectOneWriteCapturing(nsm_error_injection_types_mask& captured)
    {
        expectWriteThenReadBack(captured, nullptr);
    }

    /**
     * As above, but the device answers the read-back with @p readBack instead
     * of echoing, so a test can prove the PDIs follow device truth rather than
     * the mask that was requested.
     */
    void expectWriteThenReadBack(nsm_error_injection_types_mask& captured,
                                 const nsm_error_injection_types_mask* readBack)
    {
        auto response = successResponse();
        EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
            .Times(1)
            .WillOnce([&captured,
                       response](eid_t, Request& request,
                                 std::shared_ptr<const nsm_msg>& responseMsg,
                                 size_t& responseLen) -> requester::Coroutine {
            auto* msg = reinterpret_cast<const nsm_msg*>(request.data());
            decode_set_current_error_injection_types_v1_req(msg, request.size(),
                                                            &captured);
            allocResponse(response, responseMsg, responseLen);
            // coverity[missing_return]
            co_return NSM_SUCCESS;
        });
        // The read-back reuses the polling sensor's update(), so it lands on
        // sensorIO. It answers with readBack when supplied, else echoes the
        // mask the write carried.
        EXPECT_CALL(*gpu, sensorIO)
            .Times(1)
            .WillOnce([&captured, readBack](
                          eid_t, Request&,
                          std::shared_ptr<const nsm_msg>& responseMsg,
                          size_t& responseLen, bool) -> requester::Coroutine {
            allocResponse(typesResponse(readBack ? *readBack : captured),
                          responseMsg, responseLen);
            // coverity[missing_return]
            co_return NSM_SUCCESS;
        });
    }

    /** Current PDI value for a type. */
    bool enabledOf(ErrorInjectionCapabilityIntf::Type type)
    {
        return byType[type]->enabled();
    }

    static CapabilityEntries
        entries(std::initializer_list<
                std::pair<ErrorInjectionCapabilityIntf::Type, bool>>
                    items)
    {
        CapabilityEntries result;
        for (const auto& [type, value] : items)
        {
            result.emplace_back(leafName(type), CapabilityVariant{value});
        }
        return result;
    }

    /** Every request shape that must be rejected before any device write. */
    static std::vector<AsyncSetOperationValueType> badRequests()
    {
        return {
            // Wrong variant alternative entirely.
            AsyncSetOperationValueType{true},
            // Empty list.
            AsyncSetOperationValueType{CapabilityEntries{}},
            // Name that is not a capability type at all.
            AsyncSetOperationValueType{
                CapabilityEntries{{"NotACapability", CapabilityVariant{true}}}},
            // Valid type name, but not registered on this device.
            AsyncSetOperationValueType{CapabilityEntries{
                {leafName(ErrorInjectionCapabilityIntf::Type::FatalErrors),
                 CapabilityVariant{true}}}},
            // Non-boolean value for a valid type.
            AsyncSetOperationValueType{CapabilityEntries{
                {leafName(ErrorInjectionCapabilityIntf::Type::ThermalErrors),
                 CapabilityVariant{uint32_t{1}}}}},
            // Same mask bit asked to be both set and cleared.
            AsyncSetOperationValueType{entries(
                {{ErrorInjectionCapabilityIntf::Type::ThermalErrors, true},
                 {ErrorInjectionCapabilityIntf::Type::ThermalErrors, false}})}};
    }
};

// ============================================================================
// The bug: several types in one batch must all survive
// ============================================================================

TEST_F(NsmSetErrorInjectionBatchTest, BatchEnable_AllRequestedBitsSet)
{
    nsm_error_injection_types_mask sent{};
    expectOneWriteCapturing(sent);

    auto value = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::ThermalErrors, true},
                 {ErrorInjectionCapabilityIntf::Type::NVLinkErrors, true},
                 {ErrorInjectionCapabilityIntf::Type::PCIeErrors, true}})};
    auto status = AsyncOperationStatusType::Success;
    capabilities->capabilitiesEnabled(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);

    // All three requested bits ride the same device command.
    EXPECT_TRUE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::ThermalErrors));
    EXPECT_TRUE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::NVLinkErrors));
    EXPECT_TRUE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::PCIeErrors));
    // The untouched type keeps its value.
    EXPECT_FALSE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::MemoryErrors));

    // ... and the confirmed mask is published to every PDI.
    EXPECT_TRUE(enabledOf(ErrorInjectionCapabilityIntf::Type::ThermalErrors));
    EXPECT_TRUE(enabledOf(ErrorInjectionCapabilityIntf::Type::NVLinkErrors));
    EXPECT_TRUE(enabledOf(ErrorInjectionCapabilityIntf::Type::PCIeErrors));
    EXPECT_FALSE(enabledOf(ErrorInjectionCapabilityIntf::Type::MemoryErrors));
}

TEST_F(NsmSetErrorInjectionBatchTest, BatchEnable_PreservesUntouchedSiblings)
{
    byType[ErrorInjectionCapabilityIntf::Type::MemoryErrors]->enabled(true);

    nsm_error_injection_types_mask sent{};
    expectOneWriteCapturing(sent);

    auto value = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::ThermalErrors, true}})};
    auto status = AsyncOperationStatusType::Success;
    capabilities->capabilitiesEnabled(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_TRUE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::MemoryErrors));
    EXPECT_TRUE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::ThermalErrors));
}

TEST_F(NsmSetErrorInjectionBatchTest, BatchDisable_ClearsOnlyRequestedBits)
{
    for (auto type : standardTypes)
    {
        byType[type]->enabled(true);
    }

    nsm_error_injection_types_mask sent{};
    expectOneWriteCapturing(sent);

    auto value = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::ThermalErrors, false},
                 {ErrorInjectionCapabilityIntf::Type::PCIeErrors, false}})};
    auto status = AsyncOperationStatusType::Success;
    capabilities->capabilitiesEnabled(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::ThermalErrors));
    EXPECT_FALSE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::PCIeErrors));
    EXPECT_TRUE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::NVLinkErrors));
    EXPECT_TRUE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::MemoryErrors));
}

// ============================================================================
// Back-to-back batches: the second must observe the first's result
// ============================================================================

TEST_F(NsmSetErrorInjectionBatchTest, BackToBackBatches_NoStaleClobber)
{
    nsm_error_injection_types_mask first{};
    expectOneWriteCapturing(first);

    auto firstValue = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::ThermalErrors, true},
                 {ErrorInjectionCapabilityIntf::Type::NVLinkErrors, true}})};
    auto status = AsyncOperationStatusType::Success;
    capabilities->capabilitiesEnabled(firstValue, &status, gpu);
    ASSERT_EQ(status, AsyncOperationStatusType::Success);

    // Second batch touches a different type; the first batch's bits must
    // still be present in the mask it sends.
    nsm_error_injection_types_mask second{};
    expectOneWriteCapturing(second);

    auto secondValue = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::PCIeErrors, true}})};
    status = AsyncOperationStatusType::Success;
    capabilities->capabilitiesEnabled(secondValue, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_TRUE(
        maskBitSet(second, ErrorInjectionCapabilityIntf::Type::ThermalErrors));
    EXPECT_TRUE(
        maskBitSet(second, ErrorInjectionCapabilityIntf::Type::NVLinkErrors));
    EXPECT_TRUE(
        maskBitSet(second, ErrorInjectionCapabilityIntf::Type::PCIeErrors));
}

// ============================================================================
// Validation: every rejection happens before any device write
// ============================================================================

TEST_F(NsmSetErrorInjectionBatchTest, Validation_RejectsWithoutDeviceWrite)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).Times(0);

    for (const auto& bad : badRequests())
    {
        auto status = AsyncOperationStatusType::Success;
        capabilities->capabilitiesEnabled(bad, &status, gpu);
        // Reported, not thrown: a throw from a handler coroutine is stored in
        // its promise and never reaches setImpl, which would leave the client
        // seeing Success while nothing was written.
        EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
    }

    // No request reached the device and no PDI was touched.
    for (auto type : standardTypes)
    {
        EXPECT_FALSE(enabledOf(type));
    }
}

TEST_F(NsmSetErrorInjectionBatchTest, Validation_RepeatedSameValueAccepted)
{
    nsm_error_injection_types_mask sent{};
    expectOneWriteCapturing(sent);

    // A client resending a full snapshot must not be rejected.
    auto value = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::ThermalErrors, true},
                 {ErrorInjectionCapabilityIntf::Type::ThermalErrors, true}})};
    auto status = AsyncOperationStatusType::Success;
    EXPECT_NO_THROW(capabilities->capabilitiesEnabled(value, &status, gpu));

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_TRUE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::ThermalErrors));
}

// ============================================================================
// Device failure: status reported, PDI left alone
// ============================================================================

TEST_F(NsmSetErrorInjectionBatchTest, DeviceWriteFails_NoPdiPublish)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto value = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::ThermalErrors, true},
                 {ErrorInjectionCapabilityIntf::Type::NVLinkErrors, true}})};
    auto status = AsyncOperationStatusType::Success;
    capabilities->capabilitiesEnabled(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    // Nothing was accepted by the device, so nothing is published.
    for (auto type : standardTypes)
    {
        EXPECT_FALSE(enabledOf(type));
    }
}

// ============================================================================
// Per-type patch routes through the same owner
// ============================================================================

TEST_F(NsmSetErrorInjectionBatchTest, PerTypePatch_PreservesSiblings)
{
    byType[ErrorInjectionCapabilityIntf::Type::PCIeErrors]->enabled(true);

    NsmSetErrorInjectionEnabled perType(
        "ThermalErrors", ErrorInjectionCapabilityIntf::Type::ThermalErrors,
        interfaces, capabilities);

    nsm_error_injection_types_mask sent{};
    expectOneWriteCapturing(sent);

    auto value = AsyncSetOperationValueType{true};
    auto status = AsyncOperationStatusType::Success;
    perType.enabled(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_TRUE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::ThermalErrors));
    // The sibling that was already on stays on -- this is the single-type
    // half of the same read-modify-write.
    EXPECT_TRUE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::PCIeErrors));
    EXPECT_FALSE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::NVLinkErrors));
}

TEST_F(NsmSetErrorInjectionBatchTest, PerTypePatch_RejectsNonBool)
{
    auto perType = std::make_shared<NsmSetErrorInjectionEnabled>(
        "ThermalErrors", ErrorInjectionCapabilityIntf::Type::ThermalErrors,
        interfaces, capabilities);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).Times(0);

    // Driven through the dispatcher because that is where a rejected value has
    // to surface. Registered without a sensor so setImpl's post-handler refresh
    // stays out of the reject path.
    const std::string objPath = "/test/ei/batch/pertype_nonbool";
    auto& dispatcher =
        *AsyncOperationManager::getInstance()->getDispatcher(objPath);
    dispatcher.addAsyncSetOperation(
        "com.nvidia.ErrorInjection.ErrorInjectionCapability", "Enabled",
        AsyncSetOperationInfo{
            std::bind_front(&NsmSetErrorInjectionEnabled::enabled,
                            perType.get()),
            nullptr, gpu});

    auto result = AsyncOperationManager::getInstance()->getNewStatusInterface();
    dispatcher.setImpl("com.nvidia.ErrorInjection.ErrorInjectionCapability",
                       "Enabled", AsyncSetOperationValueType{uint32_t{1}},
                       result.second);

    EXPECT_EQ(result.second->status(),
              AsyncOperationStatusType::InvalidArgument);

    EXPECT_FALSE(enabledOf(ErrorInjectionCapabilityIntf::Type::ThermalErrors));
}

// ============================================================================
// Patches serialize: each one reseeds from the state the previous published
// ============================================================================

TEST_F(NsmSetErrorInjectionBatchTest, SecondPatchComposesFromFirstResult)
{
    nsm_error_injection_types_mask first{};
    expectOneWriteCapturing(first);

    auto firstValue = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::ThermalErrors, true}})};
    auto firstStatus = AsyncOperationStatusType::Success;
    capabilities->capabilitiesEnabled(firstValue, &firstStatus, gpu);
    ASSERT_EQ(firstStatus, AsyncOperationStatusType::Success);
    Mock::VerifyAndClearExpectations(gpu.get());

    // Completing at all proves the semaphore was released; carrying the first
    // patch's bit proves the mask was reseeded from the published state.
    nsm_error_injection_types_mask second{};
    expectOneWriteCapturing(second);

    auto secondValue = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::PCIeErrors, true}})};
    auto secondStatus = AsyncOperationStatusType::Success;
    capabilities->capabilitiesEnabled(secondValue, &secondStatus, gpu);

    EXPECT_EQ(secondStatus, AsyncOperationStatusType::Success);
    EXPECT_TRUE(
        maskBitSet(second, ErrorInjectionCapabilityIntf::Type::ThermalErrors));
    EXPECT_TRUE(
        maskBitSet(second, ErrorInjectionCapabilityIntf::Type::PCIeErrors));
}

// ============================================================================
// The PDIs follow the mask read back from the device, not the mask requested
// ============================================================================

TEST_F(NsmSetErrorInjectionBatchTest, PdiFollowsReadBackNotRequestedMask)
{
    // Device accepts the write but reports a different mask: it also has
    // NVLinkErrors set, which this request never asked for.
    nsm_error_injection_types_mask deviceTruth{};
    auto thermalBit = getErrorInjectionBitPosition(
        ErrorInjectionCapabilityIntf::Type::ThermalErrors);
    auto nvlinkBit = getErrorInjectionBitPosition(
        ErrorInjectionCapabilityIntf::Type::NVLinkErrors);
    deviceTruth.mask[thermalBit / 8] |= 1 << (thermalBit % 8);
    deviceTruth.mask[nvlinkBit / 8] |= 1 << (nvlinkBit % 8);

    nsm_error_injection_types_mask sent{};
    expectWriteThenReadBack(sent, &deviceTruth);

    auto value = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::ThermalErrors, true}})};
    auto status = AsyncOperationStatusType::Success;
    capabilities->capabilitiesEnabled(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    // The request only carried ThermalErrors ...
    EXPECT_TRUE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::ThermalErrors));
    EXPECT_FALSE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::NVLinkErrors));
    // ... but the PDIs must reflect what the device reported back.
    EXPECT_TRUE(enabledOf(ErrorInjectionCapabilityIntf::Type::ThermalErrors));
    EXPECT_TRUE(enabledOf(ErrorInjectionCapabilityIntf::Type::NVLinkErrors));
}

TEST_F(NsmSetErrorInjectionBatchTest, ReadBackFailureFallsBackToAckedMask)
{
    auto response = successResponse();
    nsm_error_injection_types_mask sent{};
    // write succeeds, read-back fails
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .Times(1)
        .WillOnce(
            [&sent, response](eid_t, Request& request,
                              std::shared_ptr<const nsm_msg>& responseMsg,
                              size_t& responseLen) -> requester::Coroutine {
        auto* msg = reinterpret_cast<const nsm_msg*>(request.data());
        decode_set_current_error_injection_types_v1_req(msg, request.size(),
                                                        &sent);
        allocResponse(response, responseMsg, responseLen);
        // coverity[missing_return]
        co_return NSM_SUCCESS;
    });
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));

    auto value = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::ThermalErrors, true}})};
    auto status = AsyncOperationStatusType::Success;
    capabilities->capabilitiesEnabled(value, &status, gpu);

    // The write landed, so the patch is a success and the acknowledged mask is
    // published even though the read-back could not confirm it.
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_TRUE(enabledOf(ErrorInjectionCapabilityIntf::Type::ThermalErrors));
}

TEST_F(NsmSetErrorInjectionBatchTest, SemaphoreReleasedAfterWriteFailure)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto failValue = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::ThermalErrors, true}})};
    auto failStatus = AsyncOperationStatusType::Success;
    capabilities->capabilitiesEnabled(failValue, &failStatus, gpu);
    ASSERT_EQ(failStatus, AsyncOperationStatusType::WriteFailure);
    Mock::VerifyAndClearExpectations(gpu.get());

    // A leaked semaphore on the error path would wedge every later patch.
    nsm_error_injection_types_mask sent{};
    expectOneWriteCapturing(sent);

    auto value = AsyncSetOperationValueType{
        entries({{ErrorInjectionCapabilityIntf::Type::PCIeErrors, true}})};
    auto status = AsyncOperationStatusType::Success;
    capabilities->capabilitiesEnabled(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_TRUE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::PCIeErrors));
}

// ============================================================================
// Aliased types: an override governs the mask bit, not just its own type
// ============================================================================

TEST_F(NsmSetErrorInjectionBatchTest, AliasedTypes_DisableIsNotUndoneBySibling)
{
    // FatalErrors and PortRecoveryErrors both map to EI_DEVICE_ERRORS, and the
    // MCU path registers them in one shared container. The mask build has to be
    // keyed by bit: otherwise disabling one while the other's PDI still reads
    // true would OR the bit straight back on.
    auto& bus = utils::DBusHandler::getBus();
    Interfaces<ErrorInjectionCapabilityIntf> aliased;
    for (auto type : {ErrorInjectionCapabilityIntf::Type::FatalErrors,
                      ErrorInjectionCapabilityIntf::Type::PortRecoveryErrors})
    {
        std::filesystem::path path = std::filesystem::path("/test/ei/alias") /
                                     leafName(type);
        auto intf = std::make_shared<ErrorInjectionCapabilityIntf>(
            bus, path.string().c_str());
        intf->type(type);
        intf->enabled(true);
        aliased[path] = intf;
    }

    auto aliasedSensor = std::make_shared<NsmErrorInjectionEnabled>(
        NsmInterfaceProvider<ErrorInjectionCapabilityIntf>(
            "ErrorInjectionCapability", "NSM_ErrorInjectionCapability",
            aliased));
    auto aliasedCaps = std::make_shared<NsmSetErrorInjectionCapabilities>(
        "AliasedCapabilities", mockManager, aliased, aliasedSensor);

    nsm_error_injection_types_mask sent{};
    expectOneWriteCapturing(sent);

    auto value = AsyncSetOperationValueType{CapabilityEntries{
        {leafName(ErrorInjectionCapabilityIntf::Type::FatalErrors),
         CapabilityVariant{false}}}};
    auto status = AsyncOperationStatusType::Success;
    aliasedCaps->capabilitiesEnabled(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(
        maskBitSet(sent, ErrorInjectionCapabilityIntf::Type::FatalErrors));
}

// ============================================================================
// Construction guards
// ============================================================================

TEST_F(NsmSetErrorInjectionBatchTest, Constructor_NullCapabilities_Throws)
{
    EXPECT_THROW(NsmSetErrorInjectionEnabled(
                     "ThermalErrors",
                     ErrorInjectionCapabilityIntf::Type::ThermalErrors,
                     interfaces, nullptr),
                 std::invalid_argument);
}

TEST_F(NsmSetErrorInjectionBatchTest, ResolveTypeName_OnlyRegisteredTypes)
{
    EXPECT_TRUE(capabilities
                    ->resolveTypeName(leafName(
                        ErrorInjectionCapabilityIntf::Type::ThermalErrors))
                    .has_value());
    // Registered on the MCU path only, never on this container.
    EXPECT_FALSE(capabilities
                     ->resolveTypeName(leafName(
                         ErrorInjectionCapabilityIntf::Type::FatalErrors))
                     .has_value());
    EXPECT_FALSE(capabilities->resolveTypeName("NotACapability").has_value());
}
