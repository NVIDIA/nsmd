/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

// =============================================================================
// Unit tests for nsmSoCPowerSmoothing.cpp after the
// MaxACPowerRampPercentPerSecond (double) -> MaxACPowerRampRateWattsPerSecond
// (uint32) rename. Covers the GPU/MCU property maps, the registrar/updater
// helpers for MaxACPowerRampRate, and the setSoCSetting async write path.
// =============================================================================

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cstring>
#include <limits>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"
#include "platform-environmental.h"

#include "nsmSoCPowerSmoothing.hpp"

using namespace nsm;

// Shared boost/asio infrastructure for tests that need a real
// sdbusplus::asio::object_server to host the SoC features interface.
static boost::asio::io_context sIo;
static auto sBus = std::make_shared<sdbusplus::asio::connection>(sIo);
static auto sObjServer = std::make_shared<sdbusplus::asio::object_server>(sBus);

// =============================================================================
// Fixture
// =============================================================================
struct NsmSoCPowerSmoothingTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_0";
    static constexpr const char* kPropName = "MaxACPowerRampRateWattsPerSecond";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmSoCPowerSmoothingTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmSoCPowerSmoothingTest() override
    {
        cleanupDeviceSensors(devices);
    }

    // Resolve the SoCModePropertyInfo for the MaxACPowerRampRate entry out of
    // the given map. Returns nullptr if the map is missing the entry or has an
    // unexpected shape.
    static const SoCModePropertyInfo*
        rampRateInfo(const SoCModePropertyMap& map)
    {
        auto it = map.find(DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE);
        if (it == map.end() || it->second.size() != 1)
        {
            return nullptr;
        }
        return &it->second.front();
    }

    // Build a bare sdbusplus dbus_interface for this test's objPath under a
    // unique interface name so repeated test invocations do not collide.
    std::shared_ptr<sdbusplus::asio::dbus_interface>
        makeIntf(const std::string& suffix)
    {
        auto intf = sObjServer->add_interface(
            objPath, std::string("com.nvidia.PowerSmoothing.Test.") + suffix);
        EXPECT_NE(intf, nullptr);
        return intf;
    }

    // Build a canned NSM_SUCCESS response for set_device_mode_settings_v2 so
    // the postPatchIO mock can hand it back to the coroutine.
    static std::vector<uint8_t> makeSetResp(uint8_t cc = NSM_SUCCESS)
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_set_device_mode_settings_v2_resp(0, cc, ERR_NULL, msg);
        return buf;
    }
};

// =============================================================================
// Property map entries expose the new name and correct mode index
// =============================================================================
TEST_F(NsmSoCPowerSmoothingTest, GpuMap_RampRateEntry_UsesNewPropertyName)
{
    const auto& map = getSoCModePropertyMap(SoCDeviceType::GPU, 1);
    const auto* info = rampRateInfo(map);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->propertyName, kPropName);
    EXPECT_NE(info->registrar, nullptr);
    EXPECT_NE(info->updater, nullptr);
}

TEST_F(NsmSoCPowerSmoothingTest, McuMap_RampRateEntry_UsesNewPropertyName)
{
    const auto& map = getSoCModePropertyMap(SoCDeviceType::MCU, 1);
    const auto* info = rampRateInfo(map);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->propertyName, kPropName);
    EXPECT_NE(info->registrar, nullptr);
    EXPECT_NE(info->updater, nullptr);
}

// Unknown version returns an empty map; the ramp-rate entry is not present.
TEST_F(NsmSoCPowerSmoothingTest, UnknownVersion_ReturnsEmptyMap)
{
    const auto& map = getSoCModePropertyMap(SoCDeviceType::GPU, 99);
    EXPECT_TRUE(map.empty());
}

// =============================================================================
// Registrar: publishes uint32 property with UINT32_MAX sentinel default
// =============================================================================
TEST_F(NsmSoCPowerSmoothingTest, Registrar_RegistersUint32WithSentinelDefault)
{
    const auto& map = getSoCModePropertyMap(SoCDeviceType::GPU, 1);
    const auto* info = rampRateInfo(map);
    ASSERT_NE(info, nullptr);

    auto intf = makeIntf("Registrar");
    EXPECT_NO_THROW(info->registrar(intf));
    EXPECT_TRUE(intf->initialize());

    // Setting the property to the sentinel is a no-op write; the fact that
    // set_property accepts a uint32 here confirms the registered type.
    EXPECT_NO_THROW(intf->set_property(
        kPropName, uint32_t{std::numeric_limits<uint32_t>::max()}));
}

// =============================================================================
// Updater: happy path writes LE-decoded uint32 into the property
// =============================================================================
TEST_F(NsmSoCPowerSmoothingTest, Updater_LittleEndianBytes_DecodesToUint32)
{
    const auto& map = getSoCModePropertyMap(SoCDeviceType::GPU, 1);
    const auto* info = rampRateInfo(map);
    ASSERT_NE(info, nullptr);

    auto intf = makeIntf("UpdaterLE");
    info->registrar(intf);
    ASSERT_TRUE(intf->initialize());

    // Bytes 04 03 02 01 (LE) -> 0x01020304 host order.
    const uint8_t bytes[4] = {0x04, 0x03, 0x02, 0x01};
    EXPECT_NO_THROW(info->updater(intf, bytes, sizeof(bytes)));

    // Sanity: property now accepts a uint32 assignment; it would have been
    // re-registered as double under the old name.
    EXPECT_NO_THROW(intf->set_property(kPropName, uint32_t{0x01020304}));
}

// =============================================================================
// Updater: short buffer is rejected without mutating the property
// =============================================================================
TEST_F(NsmSoCPowerSmoothingTest, Updater_ShortBuffer_IsIgnored)
{
    const auto& map = getSoCModePropertyMap(SoCDeviceType::GPU, 1);
    const auto* info = rampRateInfo(map);
    ASSERT_NE(info, nullptr);

    auto intf = makeIntf("UpdaterShort");
    info->registrar(intf);
    ASSERT_TRUE(intf->initialize());

    const uint8_t bytes[3] = {0x04, 0x03, 0x02};
    EXPECT_NO_THROW(info->updater(intf, bytes, sizeof(bytes)));
}

// =============================================================================
// setSoCSetting: uint32 value is encoded little-endian into the wire payload
// =============================================================================
TEST_F(NsmSoCPowerSmoothingTest, SetSoCSetting_Uint32_EncodesLEPayload)
{
    auto intf = makeIntf("Set_Happy");
    auto sensor = std::make_shared<NsmSoCPowerSmoothing>(
        "SoCRamp", "NSM_SOC", objPath, intf,
        static_cast<uint32_t>(DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE),
        [](const uint8_t*, uint16_t) {});

    Request capturedRequest;
    auto resp = makeSetResp();
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            Invoke([&, resp](eid_t, Request& req,
                             std::shared_ptr<const nsm_msg>& responseMsg,
                             size_t& responseLen) -> requester::Coroutine {
        capturedRequest = req;
        responseLen = resp.size();
        auto* buf = new uint8_t[responseLen];
        std::memcpy(buf, resp.data(), responseLen);
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(buf), [](const nsm_msg* p) {
            delete[] reinterpret_cast<const uint8_t*>(p);
        });
        // coverity[missing_return]
        co_return NSM_SUCCESS;
    }));

    AsyncOperationStatusType status = AsyncOperationStatusType::WriteFailure;
    AsyncSetOperationValueType value{uint32_t{0x01020304}};
    auto coro = sensor->setSoCSetting(value, &status, gpu);

    EXPECT_EQ(coro.exception(), nullptr);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);

    // Decode the captured request and confirm the payload is the LE uint32 we
    // asked the sensor to send.
    ASSERT_GE(capturedRequest.size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_set_device_mode_settings_v2_req));
    uint32_t decodedIdx = 0;
    std::vector<uint8_t> decodedData(sizeof(uint32_t), 0);
    uint16_t decodedLen = 0;
    auto reqMsg = reinterpret_cast<const nsm_msg*>(capturedRequest.data());
    auto rc = decode_set_device_mode_settings_v2_req(
        reqMsg, capturedRequest.size(), &decodedIdx, decodedData.data(),
        &decodedLen);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(decodedIdx,
              static_cast<uint32_t>(DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE));
    EXPECT_EQ(decodedLen, sizeof(uint32_t));
    EXPECT_EQ(decodedData[0], 0x04);
    EXPECT_EQ(decodedData[1], 0x03);
    EXPECT_EQ(decodedData[2], 0x02);
    EXPECT_EQ(decodedData[3], 0x01);
}

// =============================================================================
// setSoCSetting: wrong variant alternative -> InvalidArgument
// =============================================================================
TEST_F(NsmSoCPowerSmoothingTest, SetSoCSetting_WrongVariant_Throws)
{
    auto intf = makeIntf("Set_BadVariant");
    auto sensor = std::make_shared<NsmSoCPowerSmoothing>(
        "SoCRamp", "NSM_SOC", objPath, intf,
        static_cast<uint32_t>(DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE),
        [](const uint8_t*, uint16_t) {});

    // postPatchIO must never be reached on the rejected value path.
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).Times(0);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value{double{25.0}};

#ifdef COVERAGE_DISABLE_COROUTINES
    EXPECT_THROW(
        sensor->setSoCSetting(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
#else
    auto coro = sensor->setSoCSetting(value, &status, gpu);
    ASSERT_NE(coro.exception(), nullptr);
    EXPECT_THROW(
        std::rethrow_exception(coro.exception()),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
#endif
}

// =============================================================================
// MCU property map: every entry has a registrar and an updater
// =============================================================================
namespace
{

const SoCModePropertyInfo* entryFor(const SoCModePropertyMap& map,
                                    uint32_t modeIndex)
{
    auto it = map.find(modeIndex);
    if (it == map.end() || it->second.size() != 1)
    {
        return nullptr;
    }
    return &it->second.front();
}

} // namespace

TEST_F(NsmSoCPowerSmoothingTest, McuMap_HasEveryMode_GpuMapOnlyTheFirstTwo)
{
    const auto& mcu = getSoCModePropertyMap(SoCDeviceType::MCU, 1);
    const auto& gpuMap = getSoCModePropertyMap(SoCDeviceType::GPU, 1);

    EXPECT_EQ(mcu.size(), 4u);
    EXPECT_EQ(gpuMap.size(), 2u);
    for (auto index : {DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE,
                       DEVICE_MODE_SOC_POWER_SMOOTHING_ENABLED,
                       DEVICE_MODE_SOC_POWER_SMOOTHING_PRESET_INDEX,
                       DEVICE_MODE_SOC_POWER_BRAKE_ENABLED})
    {
        EXPECT_NE(entryFor(mcu, index), nullptr) << index;
    }
    EXPECT_EQ(entryFor(gpuMap, DEVICE_MODE_SOC_POWER_BRAKE_ENABLED), nullptr);
}

// The updaters write through set_property; a payload that is too short must
// be ignored and none of them may throw.
TEST_F(NsmSoCPowerSmoothingTest, Updater_PowerSmoothingEnabled)
{
    const auto* info = entryFor(getSoCModePropertyMap(SoCDeviceType::MCU, 1),
                                DEVICE_MODE_SOC_POWER_SMOOTHING_ENABLED);
    ASSERT_NE(info, nullptr);
    auto intf = makeIntf("PowerSmoothingEnabled");
    info->registrar(intf);
    ASSERT_TRUE(intf->initialize());

    const uint8_t on[1] = {1};
    const uint8_t off[1] = {0};
    EXPECT_NO_THROW(info->updater(intf, on, 1));
    EXPECT_NO_THROW(info->updater(intf, off, 1));
    // An empty payload is ignored
    EXPECT_NO_THROW(info->updater(intf, on, 0));
}

TEST_F(NsmSoCPowerSmoothingTest, Updater_PowerBrakeEnabled)
{
    const auto* info = entryFor(getSoCModePropertyMap(SoCDeviceType::MCU, 1),
                                DEVICE_MODE_SOC_POWER_BRAKE_ENABLED);
    ASSERT_NE(info, nullptr);
    auto intf = makeIntf("PowerBrakeEnabled");
    info->registrar(intf);
    ASSERT_TRUE(intf->initialize());

    const uint8_t on[1] = {7};
    EXPECT_NO_THROW(info->updater(intf, on, 1));
    EXPECT_NO_THROW(info->updater(intf, on, 0));
}

TEST_F(NsmSoCPowerSmoothingTest, Updater_ProfileName_PublishesNameAndAvailable)
{
    const auto* info = entryFor(getSoCModePropertyMap(SoCDeviceType::MCU, 1),
                                DEVICE_MODE_SOC_POWER_SMOOTHING_PRESET_INDEX);
    ASSERT_NE(info, nullptr);
    auto intf = makeIntf("ProfileName");
    info->registrar(intf);
    ASSERT_TRUE(intf->initialize());

    // Profile 3 is active; bits 0 and 2 are available
    const uint8_t data[2] = {3, 0x05};
    EXPECT_NO_THROW(info->updater(intf, data, 2));

    // A payload shorter than two bytes is ignored
    const uint8_t shortData[1] = {9};
    EXPECT_NO_THROW(info->updater(intf, shortData, 1));
}

// =============================================================================
// NsmSoCPowerSmoothing: cold-start read
// =============================================================================
TEST_F(NsmSoCPowerSmoothingTest, GenRequestMsg_EncodesTheModeIndex)
{
    auto sensor = std::make_shared<NsmSoCPowerSmoothing>(
        "SoC", "NSM_SOC", objPath, makeIntf("GenReq"),
        static_cast<uint32_t>(DEVICE_MODE_SOC_POWER_BRAKE_ENABLED),
        [](const uint8_t*, uint16_t) {});

    auto request = sensor->genRequestMsg(8, 1);

    ASSERT_TRUE(request.has_value());
    uint32_t index = 0;
    EXPECT_EQ(decode_get_device_mode_settings_v2_req(
                  reinterpret_cast<const nsm_msg*>(request->data()),
                  request->size(), &index),
              NSM_SW_SUCCESS);
    EXPECT_EQ(index,
              static_cast<uint32_t>(DEVICE_MODE_SOC_POWER_BRAKE_ENABLED));
}

namespace
{

std::vector<uint8_t> getModeResponse(const std::vector<uint8_t>& current,
                                     uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_device_mode_settings_v2_resp) +
                                 current.size() + 16,
                             0);
    uint8_t pending[1] = {0};
    encode_get_device_mode_settings_v2_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : 0x0101,
        current.empty() ? nullptr : current.data(),
        static_cast<uint16_t>(current.size()), pending, 1,
        reinterpret_cast<nsm_msg*>(buf.data()));
    return buf;
}

} // namespace

TEST_F(NsmSoCPowerSmoothingTest, HandleResponse_PassesTheCurrentModeToUpdater)
{
    std::vector<uint8_t> seen;
    auto sensor = std::make_shared<NsmSoCPowerSmoothing>(
        "SoC", "NSM_SOC", objPath, makeIntf("HandleOk"),
        static_cast<uint32_t>(DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE),
        [&seen](const uint8_t* data, uint16_t len) {
        seen.assign(data, data + len);
    });
    auto response = getModeResponse({0x2A, 0, 0, 0});

    EXPECT_EQ(
        sensor->handleResponseMsg(
            reinterpret_cast<const nsm_msg*>(response.data()), response.size()),
        NSM_SW_SUCCESS);

    EXPECT_EQ(seen, (std::vector<uint8_t>{0x2A, 0, 0, 0}));
}

TEST_F(NsmSoCPowerSmoothingTest,
       HandleResponse_NoUpdaterCall_WhenNothingToApply)
{
    bool called = false;
    auto sensor = std::make_shared<NsmSoCPowerSmoothing>(
        "SoC", "NSM_SOC", objPath, makeIntf("HandleNone"),
        static_cast<uint32_t>(DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE),
        [&called](const uint8_t*, uint16_t) { called = true; });

    // Empty current mode
    auto empty = getModeResponse({});
    EXPECT_EQ(sensor->handleResponseMsg(
                  reinterpret_cast<const nsm_msg*>(empty.data()), empty.size()),
              NSM_SW_SUCCESS);
    EXPECT_FALSE(called);

    // Device error
    auto error = getModeResponse({}, NSM_ERROR);
    EXPECT_EQ(sensor->handleResponseMsg(
                  reinterpret_cast<const nsm_msg*>(error.data()), error.size()),
              NSM_ERROR);
    EXPECT_FALSE(called);

    // Too short to decode
    EXPECT_NE(sensor->handleResponseMsg(
                  reinterpret_cast<const nsm_msg*>(empty.data()), 2),
              NSM_SW_SUCCESS);
    EXPECT_FALSE(called);

    // No interface: nothing to update
    auto noIntf = std::make_shared<NsmSoCPowerSmoothing>(
        "SoC", "NSM_SOC", objPath, nullptr,
        static_cast<uint32_t>(DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE),
        [&called](const uint8_t*, uint16_t) { called = true; });
    auto response = getModeResponse({1, 0, 0, 0});
    EXPECT_EQ(
        noIntf->handleResponseMsg(
            reinterpret_cast<const nsm_msg*>(response.data()), response.size()),
        NSM_SW_SUCCESS);
    EXPECT_FALSE(called);
}

namespace
{

/** @brief Whether running the coroutine ends in an exception. Coverage builds
 *  run coroutines inline, so the exception surfaces in the call itself instead
 *  of in the coroutine's exception().
 */
template <typename Fn>
bool endsInException(Fn&& call)
{
    try
    {
        auto coro = call();
        return coro.exception() != nullptr;
    }
    catch (const std::exception&)
    {
        return true;
    }
}

} // namespace

// =============================================================================
// setSoCSetting: every mode, success and rejected inputs
// =============================================================================
TEST_F(NsmSoCPowerSmoothingTest, SetSoCSetting_EnabledAndBrake_AcceptBool)
{
    for (auto index : {DEVICE_MODE_SOC_POWER_SMOOTHING_ENABLED,
                       DEVICE_MODE_SOC_POWER_BRAKE_ENABLED})
    {
        auto sensor = std::make_shared<NsmSoCPowerSmoothing>(
            "SoC", "NSM_SOC", objPath,
            makeIntf("SetBool" + std::to_string(index)),
            static_cast<uint32_t>(index), [](const uint8_t*, uint16_t) {});
        EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
            .WillOnce(mockPostPatchIO(makeSetResp()));

        AsyncOperationStatusType status =
            AsyncOperationStatusType::WriteFailure;
        auto coro = sensor->setSoCSetting(AsyncSetOperationValueType{true},
                                          &status, gpu);

        EXPECT_EQ(coro.exception(), nullptr) << index;
        EXPECT_EQ(status, AsyncOperationStatusType::Success) << index;
    }
}

TEST_F(NsmSoCPowerSmoothingTest, SetSoCSetting_EnabledAndBrake_RejectNonBool)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).Times(0);

    for (auto index : {DEVICE_MODE_SOC_POWER_SMOOTHING_ENABLED,
                       DEVICE_MODE_SOC_POWER_BRAKE_ENABLED})
    {
        auto sensor = std::make_shared<NsmSoCPowerSmoothing>(
            "SoC", "NSM_SOC", objPath,
            makeIntf("SetBoolBad" + std::to_string(index)),
            static_cast<uint32_t>(index), [](const uint8_t*, uint16_t) {});
        AsyncOperationStatusType status = AsyncOperationStatusType::Success;

        EXPECT_TRUE(endsInException([&] {
            return sensor->setSoCSetting(
                AsyncSetOperationValueType{uint32_t{1}}, &status, gpu);
        })) << index;
    }
}

TEST_F(NsmSoCPowerSmoothingTest, SetSoCSetting_PresetIndex_ParsesTheString)
{
    auto sensor = std::make_shared<NsmSoCPowerSmoothing>(
        "SoC", "NSM_SOC", objPath, makeIntf("SetPreset"),
        static_cast<uint32_t>(DEVICE_MODE_SOC_POWER_SMOOTHING_PRESET_INDEX),
        [](const uint8_t*, uint16_t) {});
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::WriteFailure;
    auto coro = sensor->setSoCSetting(
        AsyncSetOperationValueType{std::string("3")}, &status, gpu);

    EXPECT_EQ(coro.exception(), nullptr);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSoCPowerSmoothingTest, SetSoCSetting_PresetIndex_RejectsBadValues)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).Times(0);
    auto sensor = std::make_shared<NsmSoCPowerSmoothing>(
        "SoC", "NSM_SOC", objPath, makeIntf("SetPresetBad"),
        static_cast<uint32_t>(DEVICE_MODE_SOC_POWER_SMOOTHING_PRESET_INDEX),
        [](const uint8_t*, uint16_t) {});

    for (const AsyncSetOperationValueType& value :
         {AsyncSetOperationValueType{uint32_t{3}},
          AsyncSetOperationValueType{std::string("abc")},
          AsyncSetOperationValueType{std::string("256")}})
    {
        AsyncOperationStatusType status = AsyncOperationStatusType::Success;
        EXPECT_TRUE(endsInException(
            [&] { return sensor->setSoCSetting(value, &status, gpu); }));
    }
}

TEST_F(NsmSoCPowerSmoothingTest, SetSoCSetting_UnsupportedModeIndex_Fails)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).Times(0);
    auto sensor = std::make_shared<NsmSoCPowerSmoothing>(
        "SoC", "NSM_SOC", objPath, makeIntf("SetUnsupported"), 0xFFu,
        [](const uint8_t*, uint16_t) {});

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setSoCSetting(AsyncSetOperationValueType{true}, &status,
                                      gpu);

    EXPECT_EQ(coro.data(), NSM_SW_ERROR_DATA);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmSoCPowerSmoothingTest, SetSoCSetting_TransportAndDeviceErrors)
{
    auto sensor = std::make_shared<NsmSoCPowerSmoothing>(
        "SoC", "NSM_SOC", objPath, makeIntf("SetErrors"),
        static_cast<uint32_t>(DEVICE_MODE_SOC_POWER_SMOOTHING_ENABLED),
        [](const uint8_t*, uint16_t) {});

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetResp(), NSM_ERROR));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setSoCSetting(AsyncSetOperationValueType{true}, &status,
                                      gpu);
    EXPECT_EQ(coro.data(), NSM_ERROR);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetResp(NSM_ERROR)));
    status = AsyncOperationStatusType::Success;
    auto coro2 = sensor->setSoCSetting(AsyncSetOperationValueType{true},
                                       &status, gpu);
    EXPECT_NE(coro2.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}
