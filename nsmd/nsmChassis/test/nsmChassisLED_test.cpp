/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "network-ports.h"

#include "nsmChassisLED.hpp"

namespace nsm
{
requester::Coroutine createNsmChassisLEDSensor(SensorManager& manager,
                                               const std::string& interface,
                                               const std::string& objPath);
} // namespace nsm

using namespace nsm;

struct NsmChassisLEDTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisLED";
    const std::string name = "HGX_GPU_SXM_1";
    const std::string type = "NSM_ChassisLED";
    const std::string objPath = chassisInventoryBasePath / name;
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/" + name;

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmChassisLEDTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(gpu, nullptr);
        EXPECT_EQ(NSM_DEV_ID_GPU, gpu->getDeviceType());
    }

    dbus::PropertyMap chassisLED = {
        {"Type", "NSM_ChassisLED"},
        {"Name", name},
        {"UUID", gpuUuid},
        {"InventoryObjPath", inventoryObjPath},
    };
};

TEST_F(NsmChassisLEDTest, goodTestCreateSensor)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap["Type"] = chassisLED["Type"];
    propertyMap["Name"] = chassisLED["Name"];
    propertyMap["UUID"] = chassisLED["UUID"];
    propertyMap["InventoryObjPath"] = chassisLED["InventoryObjPath"];

    size_t initialSensorCount = gpu->deviceSensors.size();
    // Mock D-Bus resolves co_await synchronously, so the coroutine completes
    // before return and assertions below see the final state.
    createNsmChassisLEDSensor(mockManager, basicIntfName, objPath);

    EXPECT_EQ(1, devices.size());
    EXPECT_GT(gpu->deviceSensors.size(), initialSensorCount);

    auto nvlinkLedSensor =
        dynamic_pointer_cast<NsmNvlinkLedIntf>(gpu->deviceSensors.back());
    EXPECT_NE(nullptr, nvlinkLedSensor);
    EXPECT_EQ(name, nvlinkLedSensor->getName());
    EXPECT_EQ(type, nvlinkLedSensor->getType());
}

struct NsmNvlinkLedIntfTest : public NsmChassisLEDTest
{
    std::shared_ptr<NsmNvlinkLedIntf> sensor;

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string nameCopy = name;
        std::string typeCopy = type;
        std::string inventoryObjPathCopy = inventoryObjPath;
        sensor = std::make_shared<NsmNvlinkLedIntf>(bus, nameCopy, typeCopy,
                                                    inventoryObjPathCopy);
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->getType(), type);
    }
};

TEST_F(NsmNvlinkLedIntfTest, goodTestConstructor)
{
    EXPECT_NE(sensor->nvlinkledIntf, nullptr);
}

TEST_F(NsmNvlinkLedIntfTest, goodTestRequest)
{
    EXPECT_CALL(*gpu, sensorIO).Times(1).WillOnce(mockSensorIO());
    sensor->update(gpu);
}

// Tests for NsmNvlinkLedIntf::update() response path branches.
// Although encode_get_nvlink_agg_led_status_resp is not available, we can
// construct raw response buffers directly using the struct layout.

// Helper to build a valid nsm_get_nvlink_agg_led_status_resp response buffer.
// Sets completion_code=0 (NSM_SUCCESS), data_size=9, and LED_State_Aggregate
// to the given value. If shortBuffer=true, omits data_size to trigger a
// decode failure (NSM_SW_ERROR_DATA).
static std::vector<uint8_t> buildLedResponse(uint8_t ledAggregate,
                                             bool shortBuffer = false)
{
    size_t respLen = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_nvlink_agg_led_status_resp);
    std::vector<uint8_t> response(respLen, 0);
    auto* resp = reinterpret_cast<nsm_get_nvlink_agg_led_status_resp*>(
        response.data() + sizeof(nsm_msg_hdr));
    resp->hdr.completion_code = NSM_SUCCESS;
    if (!shortBuffer)
    {
        resp->hdr.data_size = 9; // must be >= 9 for decode to succeed
    }
    resp->LED_State_Aggregate = ledAggregate;
    return response;
}

TEST_F(NsmNvlinkLedIntfTest, Update_SensorIOFailure_ReturnsError)
{
    // When sensorIO fails, update() returns early without changing LED state
    EXPECT_CALL(*gpu, sensorIO).Times(1).WillOnce(mockSensorIO(NSM_ERROR));
    sensor->update(gpu);
}

TEST_F(NsmNvlinkLedIntfTest, Update_DecodeFailure_SetsReadError)
{
    // data_size=0 causes NSM_SW_ERROR_DATA from decode, triggering else branch
    auto response = buildLedResponse(0x00, /*shortBuffer=*/true);
    EXPECT_CALL(*gpu, sensorIO).Times(1).WillOnce(mockSensorIO(response));
    sensor->update(gpu);
    using LedState =
        sdbusplus::common::com::nvidia::nv_link::NVLinkLED::LedState;
    EXPECT_EQ(sensor->nvlinkledIntf->ledState(), LedState::Read_Error);
}

TEST_F(NsmNvlinkLedIntfTest, Update_GreenLedState)
{
    // LED_State_Aggregate = 0x80: bit7=1, bits0-2=0 -> NSM_NVLINK_LED_GREEN
    auto response = buildLedResponse(0x80);
    EXPECT_CALL(*gpu, sensorIO).Times(1).WillOnce(mockSensorIO(response));
    sensor->update(gpu);
    using LedState =
        sdbusplus::common::com::nvidia::nv_link::NVLinkLED::LedState;
    EXPECT_EQ(sensor->nvlinkledIntf->ledState(), LedState::Steady_Green);
}

TEST_F(NsmNvlinkLedIntfTest, Update_GreenBlinkLedState)
{
    // LED_State_Aggregate = 0x81: bit7=1, bits0-2=1 ->
    // NSM_NVLINK_LED_GREEN_BLINK
    auto response = buildLedResponse(0x81);
    EXPECT_CALL(*gpu, sensorIO).Times(1).WillOnce(mockSensorIO(response));
    sensor->update(gpu);
    using LedState =
        sdbusplus::common::com::nvidia::nv_link::NVLinkLED::LedState;
    EXPECT_EQ(sensor->nvlinkledIntf->ledState(), LedState::Blinking_Green);
}

TEST_F(NsmNvlinkLedIntfTest, Update_AmberLedState)
{
    // LED_State_Aggregate = 0x85: bit7=1, bits0-2=5 -> NSM_NVLINK_LED_AMBER
    auto response = buildLedResponse(0x85);
    EXPECT_CALL(*gpu, sensorIO).Times(1).WillOnce(mockSensorIO(response));
    sensor->update(gpu);
    using LedState =
        sdbusplus::common::com::nvidia::nv_link::NVLinkLED::LedState;
    EXPECT_EQ(sensor->nvlinkledIntf->ledState(), LedState::Steady_Amber);
}

TEST_F(NsmNvlinkLedIntfTest, Update_AmberBlinkLedState)
{
    // LED_State_Aggregate = 0x87: bit7=1, bits0-2=7 ->
    // NSM_NVLINK_LED_AMBER_BLINK
    auto response = buildLedResponse(0x87);
    EXPECT_CALL(*gpu, sensorIO).Times(1).WillOnce(mockSensorIO(response));
    sensor->update(gpu);
    using LedState =
        sdbusplus::common::com::nvidia::nv_link::NVLinkLED::LedState;
    EXPECT_EQ(sensor->nvlinkledIntf->ledState(), LedState::Blinking_Amber);
}

TEST_F(NsmNvlinkLedIntfTest, Update_OffLedState)
{
    // LED_State_Aggregate = 0x86: bit7=1, bits0-2=6 -> NSM_NVLINK_LED_OFF
    auto response = buildLedResponse(0x86);
    EXPECT_CALL(*gpu, sensorIO).Times(1).WillOnce(mockSensorIO(response));
    sensor->update(gpu);
    using LedState =
        sdbusplus::common::com::nvidia::nv_link::NVLinkLED::LedState;
    EXPECT_EQ(sensor->nvlinkledIntf->ledState(), LedState::Off);
}

// DISABLED: The decode function handles all 3-bit values (0-7) exhaustively,
// so NSM_NVLINK_LED_ERROR can never be returned with NSM_SW_SUCCESS.
// 0x82 (bit7=1, bits0-2=2) maps to NSM_NVLINK_LED_GREEN_BLINK (case 1-4),
// not the default branch. The default: in nsmChassisLED.cpp is unreachable.
TEST_F(NsmNvlinkLedIntfTest, DISABLED_Update_DefaultLedState_SetsReadError)
{
    // LED_State_Aggregate = 0x82: bit7=1, bits0-2=2 → not a known enum case
    // → default branch → ledState set to Read_Error (and rc is NSM_SW_SUCCESS
    // so co_return rc returns NSM_SW_SUCCESS = 0)
    auto response = buildLedResponse(0x82);
    EXPECT_CALL(*gpu, sensorIO).Times(1).WillOnce(mockSensorIO(response));
    sensor->update(gpu);
    using LedState =
        sdbusplus::common::com::nvidia::nv_link::NVLinkLED::LedState;
    EXPECT_EQ(sensor->nvlinkledIntf->ledState(), LedState::Read_Error);
}

// createNsmChassisLEDSensor: base interface NOT registered at unique path →
// coGetCachedBaseProperties returns error → co_return rc (no sensor created).
//
TEST_F(NsmChassisLEDTest, CreateSensor_BasePropertiesFail_ReturnsEarly)
{
    const std::string uniquePath = "/xyz/test/led/base_fail_unique";
    // Register a different sub-interface so the path exists but not
    // basicIntfName
    auto& other = utils::MockDbusAsync::propertyMap(uniquePath,
                                                    basicIntfName + ".Sub");
    other["Type"] = std::string("NSM_ChassisLED");

    const size_t before = gpu->deviceSensors.size();
    createNsmChassisLEDSensor(mockManager, basicIntfName, uniquePath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// createNsmChassisLEDSensor: type != "NSM_ChassisLED" →
// if (type == "NSM_ChassisLED") false → no sensor added.
TEST_F(NsmChassisLEDTest, CreateSensor_UnknownType_NoSensorCreated)
{
    const std::string uniquePath = "/xyz/test/led/unknown_type_unique";
    auto& propMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    propMap["Name"] = std::string("HGX_GPU_SXM_1");
    propMap["UUID"] = gpuUuid;
    propMap["Type"] = std::string("NSM_UnknownType");
    propMap["InventoryObjPath"] = inventoryObjPath;

    const size_t before = gpu->deviceSensors.size();
    createNsmChassisLEDSensor(mockManager, basicIntfName, uniquePath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// createNsmChassisLEDSensor: UUID not found in device table →
// getNsmDeviceFromStaticUUID returns nullptr → co_return NSM_ERROR. //

TEST_F(NsmChassisLEDTest, CreateSensor_UnknownUUID_ReturnsError)
{
    const std::string uniquePath = "/xyz/test/led/unknown_uuid_unique";
    const uuid_t unknownUuid = "ffffffff-ffff-ffff-ffff-ffffffffffff";
    auto& propMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    propMap["Name"] = std::string("HGX_GPU_SXM_1");
    propMap["UUID"] = unknownUuid;
    propMap["Type"] = std::string("NSM_ChassisLED");
    propMap["InventoryObjPath"] = inventoryObjPath;

    const size_t before = gpu->deviceSensors.size();
    createNsmChassisLEDSensor(mockManager, basicIntfName, uniquePath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// =============================================================================
// Branch coverage: FALSE branches for property count() checks
// =============================================================================

// Type absent → FALSE branch for count("Type") → type="" →
// if (type == "NSM_ChassisLED") FALSE → no sensor added
TEST_F(NsmChassisLEDTest, CreateSensor_MissingType_NoSensorCreated)
{
    const std::string uniquePath = "/xyz/test/led/missing_type";
    auto& propMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    // Intentionally omit "Type" → FALSE branch for count("Type")
    propMap["Name"] = std::string("HGX_GPU_SXM_1");
    propMap["UUID"] = gpuUuid;
    propMap["InventoryObjPath"] = inventoryObjPath;

    const size_t before = gpu->deviceSensors.size();
    createNsmChassisLEDSensor(mockManager, basicIntfName, uniquePath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// UUID absent → FALSE branch for count("UUID") → uuid="" →
// parseStaticUuid("") throws → caught by try/catch → no sensor
TEST_F(NsmChassisLEDTest, CreateSensor_MissingUUID_NoSensorCreated)
{
    const std::string uniquePath = "/xyz/test/led/missing_uuid";
    auto& propMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    // Intentionally omit "UUID" → FALSE branch for count("UUID")
    propMap["Name"] = std::string("HGX_GPU_SXM_1");
    propMap["Type"] = std::string("NSM_ChassisLED");
    propMap["InventoryObjPath"] = inventoryObjPath;

    const size_t before = gpu->deviceSensors.size();
    createNsmChassisLEDSensor(mockManager, basicIntfName, uniquePath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// Name absent → FALSE branch for count("Name") → name="" →
// NsmNvlinkLedIntf(bus, "", type, inventoryObjPath) succeeds in test env →
// sensor IS created with empty name
TEST_F(NsmChassisLEDTest, CreateSensor_MissingName_SensorCreatedWithEmptyName)
{
    const std::string uniquePath = "/xyz/test/led/missing_name";
    auto& propMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    // Intentionally omit "Name" → FALSE branch for count("Name") → name=""
    propMap["UUID"] = gpuUuid;
    propMap["Type"] = std::string("NSM_ChassisLED");
    propMap["InventoryObjPath"] = inventoryObjPath;

    const size_t before = gpu->deviceSensors.size();
    createNsmChassisLEDSensor(mockManager, basicIntfName, uniquePath);
    // name="" is accepted by NsmNvlinkLedIntf in test env → sensor added
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// InventoryObjPath absent → FALSE branch for count("InventoryObjPath") →
// inventoryObjPath="" → exception caught or sensor created with empty path
TEST_F(NsmChassisLEDTest, CreateSensor_MissingInventoryObjPath_NoSensorCreated)
{
    const std::string uniquePath = "/xyz/test/led/missing_inv_path";
    auto& propMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      basicIntfName);
    // Intentionally omit "InventoryObjPath" → FALSE branch for count
    propMap["Name"] = std::string("HGX_GPU_SXM_1");
    propMap["UUID"] = gpuUuid;
    propMap["Type"] = std::string("NSM_ChassisLED");

    const size_t before = gpu->deviceSensors.size();
    createNsmChassisLEDSensor(mockManager, basicIntfName, uniquePath);
    // D-Bus registration with empty inventoryObjPath throws (caught by
    // try/catch) → no sensor added, returns NSM_SUCCESS
    EXPECT_EQ(before, gpu->deviceSensors.size());
}
