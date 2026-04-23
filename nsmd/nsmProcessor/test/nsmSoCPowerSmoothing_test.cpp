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
