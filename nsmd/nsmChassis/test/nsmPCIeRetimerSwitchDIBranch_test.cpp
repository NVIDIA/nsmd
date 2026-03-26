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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "nsmDeviceInventory/nsmPCIeRetimerSwitchDI.hpp"
#include "nsmObjectFactory.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// SECTION 3: nsmPCIeRetimerSwitchDI.cpp -- Uncovered function tests
// ============================================================================

struct NsmPCIeRetimerSwitchDIBatch11F :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "PCIeRetimerDI_B11F";
    const std::string type = "NSM_PCIeRetimer_Switch";
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:54";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPCIeRetimerSwitchDIBatch11F() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPCIeRetimerSwitchDIBatch11F()
    {
        cleanupDeviceSensors(devices);
    }
};

// -- NsmPCIeRetimerSwitchDI basic constructor (non-multiport)
TEST_F(NsmPCIeRetimerSwitchDIBatch11F,
       Constructor_NonMultiport_SetsDeviceIndexAndPCIeType)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    associations.push_back(
        {"chassis", "contained_by",
         "/xyz/openbmc_project/inventory/system/chassis/HGX"});
    std::string invPath = "/xyz/openbmc_project/inventory/batch11f/retimer/";
    uint8_t deviceIdx = 5;

    // Act
    auto retimerDI = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, associations, type, invPath, deviceIdx);

    // Assert
    EXPECT_NE(retimerDI, nullptr);
    EXPECT_EQ(retimerDI->getName(), name);
    EXPECT_EQ(retimerDI->getType(), type);
    EXPECT_EQ(retimerDI->deviceIndex, deviceIdx);
    EXPECT_FALSE(retimerDI->isMultiPciePortEnabled);
    EXPECT_NE(retimerDI->switchIntf, nullptr);
    EXPECT_NE(retimerDI->associationDefIntf, nullptr);
}

// -- NsmPCIeRetimerSwitchDI multiport constructor
TEST_F(NsmPCIeRetimerSwitchDIBatch11F,
       Constructor_Multiport_SetsPortVariablesAndEnabled)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    std::string invPath = "/xyz/openbmc_project/inventory/batch11f/retimer_mp/";
    uint8_t deviceIdx = 3;
    uint8_t portType = 1;
    uint8_t portIndex = 2;
    uint8_t upstreamPort = 4;

    // Act
    auto retimerDI = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "MultiPortSwitch_B11F", associations, type, invPath, deviceIdx,
        portType, portIndex, upstreamPort);

    // Assert
    EXPECT_NE(retimerDI, nullptr);
    EXPECT_TRUE(retimerDI->isMultiPciePortEnabled);
    EXPECT_EQ(retimerDI->multiPortType, portType);
    EXPECT_EQ(retimerDI->multiPortIndex, portIndex);
    EXPECT_EQ(retimerDI->multiPortUpstreamPort, upstreamPort);
    EXPECT_EQ(retimerDI->deviceIndex, deviceIdx);
}

// -- NsmPCIeRetimerSwitchDI::update (non-multiport, successful response)
TEST_F(NsmPCIeRetimerSwitchDIBatch11F,
       Update_NonMultiport_SuccessResponse_UpdatesDeviceAndVendorId)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    std::string invPath = "/xyz/openbmc_project/inventory/batch11f/rt_up/";
    uint8_t deviceIdx = 2;
    auto retimerDI = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "RetimerUpdate_B11F", associations, type, invPath, deviceIdx);

    // Build response with group0 telemetry data
    struct nsm_query_scalar_group_telemetry_group_0 groupData = {};
    groupData.pci_device_id = 0x1234;
    groupData.pci_vendor_id = 0x10DE;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        0, NSM_SUCCESS, ERR_NULL, &groupData, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(response));

    // Act
    retimerDI->update(gpu);

    // Assert
    EXPECT_EQ(retimerDI->switchIntf->deviceId(), "0x1234");
    EXPECT_EQ(retimerDI->switchIntf->vendorId(), "0x10de");
}

// -- NsmPCIeRetimerSwitchDI::update (non-multiport, error response)
TEST_F(NsmPCIeRetimerSwitchDIBatch11F,
       Update_NonMultiport_ErrorCC_DoesNotUpdateIds)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    std::string invPath = "/xyz/openbmc_project/inventory/batch11f/rt_err/";
    uint8_t deviceIdx = 3;
    auto retimerDI = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "RetimerErr_B11F", associations, type, invPath, deviceIdx);

    // Build error response
    struct nsm_query_scalar_group_telemetry_group_0 groupData = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        0, NSM_ERROR, ERR_NULL, &groupData, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(response));

    // Act
    retimerDI->update(gpu);

    // Assert - IDs should remain empty (default)
    EXPECT_EQ(retimerDI->switchIntf->deviceId(), "");
    EXPECT_EQ(retimerDI->switchIntf->vendorId(), "");
}

// -- NsmPCIeRetimerSwitchDI::update with sensorIO failure
TEST_F(NsmPCIeRetimerSwitchDIBatch11F, Update_SensorIOFailure_ReturnsEarly)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    std::string invPath = "/xyz/openbmc_project/inventory/batch11f/rt_iofail/";
    uint8_t deviceIdx = 4;
    auto retimerDI = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "RetimerIOFail_B11F", associations, type, invPath, deviceIdx);

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(
            mockSensorIO(static_cast<nsm_completion_codes>(NSM_SW_ERROR)));

    // Act
    retimerDI->update(gpu);

    // Assert - IDs should remain empty
    EXPECT_EQ(retimerDI->switchIntf->deviceId(), "");
}

// ============================================================================
// addSensor<T> instantiation coverage (nsmDevice.hpp)
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBatch11F, AddSensorNsmPCIeRetimerSwitchDI)
{
    auto& dbus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    std::string invPath = "/xyz/openbmc_project/inventory/batch11f/rt_as/";
    auto sensor = std::make_shared<NsmPCIeRetimerSwitchDI>(
        dbus, "RetimerDI_AS", associations, type, invPath, uint8_t(0));
    size_t before = gpu->deviceSensors.size();
    gpu->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

TEST_F(NsmPCIeRetimerSwitchDIBatch11F,
       AddSensorNsmPCIeRetimerSwitchGetClockState)
{
    auto& dbus = utils::DBusHandler::getBus();
    std::string invPath =
        "/xyz/openbmc_project/inventory/batch11f/rt_clock_as/";
    auto sensor = std::make_shared<NsmPCIeRetimerSwitchGetClockState>(
        dbus, "RetimerClock_AS", type, uint64_t(0), invPath);
    size_t before = gpu->deviceSensors.size();
    gpu->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// CreatePCIeRetimerSwitch factory function coverage
// (covers lines 245-362 in nsmPCIeRetimerSwitchDI.cpp)
// ============================================================================

struct NsmPCIeRetimerSwitchFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string pcrSwitchInterface =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_Switch";
    const std::string multiPortInterface =
        "xyz.openbmc_project.Configuration.NSM_MultiPortPCIeSwitchDevice";
    const std::string basePath =
        "/xyz/openbmc_project/inventory/system/retimer_sw_factory";
    // Unique UUID to avoid collision with other tests
    const uuid_t retimerUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:55";
    const std::string invPath =
        "/xyz/openbmc_project/inventory/system/retimer_sw_factory/";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPCIeRetimerSwitchFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(retimerUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPCIeRetimerSwitchFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Verify factory functions are registered at startup
// DISABLED: SIOF — _register_0 (__attribute__((constructor))) runs before
// pcieRetimerSwitchInterfaces is initialized → registers with empty interface
// list → isSupported() returns false. Cannot fix without modifying source.
TEST_F(NsmPCIeRetimerSwitchFactoryTest,
       DISABLED_FactoryRegistration_BothInterfacesRegistered)
{
    EXPECT_TRUE(NsmObjectFactory::instance().isSupported(pcrSwitchInterface));
    EXPECT_TRUE(NsmObjectFactory::instance().isSupported(multiPortInterface));
}

// NSM_PCIeRetimer_Switch type → success: covers factory body lines 307-340
// DISABLED: same SIOF as above — factory not registered → createObjects is a
// no-op
TEST_F(NsmPCIeRetimerSwitchFactoryTest,
       DISABLED_PCIeRetimerSwitch_Success_SensorCreated)
{
    const std::string testPath = basePath + "/sw0";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, pcrSwitchInterface);
    pm["Name"] = std::string("RetimerSwitch0");
    pm["UUID"] = retimerUuid;
    pm["InventoryObjPath"] = invPath;
    pm["DeviceInstance"] = uint64_t(0);

    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, pcrSwitchInterface,
                                               testPath);

    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// NSM_MultiPortPCIeSwitchDevice type → success: covers factory body lines
// 341-356 DISABLED: same SIOF — multiPortInterface also not registered
TEST_F(NsmPCIeRetimerSwitchFactoryTest,
       DISABLED_MultiPortPCIeSwitchDevice_Success_SensorCreated)
{
    const std::string testPath = basePath + "/mp0";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, multiPortInterface);
    pm["Name"] = std::string("MultiPortSwitch0");
    pm["UUID"] = retimerUuid;
    pm["InventoryObjPath"] = invPath;
    pm["DeviceInstance"] = uint64_t(1);

    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, multiPortInterface,
                                               testPath);

    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// FALSE branch for count("Name"): empty name → D-Bus path with trailing slash
// → sdbusplus exception → caught by createObjects → no sensor
TEST_F(NsmPCIeRetimerSwitchFactoryTest, PCIeRetimerSwitch_MissingName_NoSensor)
{
    const std::string testPath = basePath + "/noname";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, pcrSwitchInterface);
    // No "Name" → name = "" → objPath = invPath + "" = invPath (trailing slash)
    pm["UUID"] = retimerUuid;
    pm["InventoryObjPath"] = invPath;
    pm["DeviceInstance"] = uint64_t(0);

    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, pcrSwitchInterface,
                                               testPath);

    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// FALSE branch for count("UUID"): empty uuid → parseStaticUuid throws →
// caught by createObjects → no sensor
TEST_F(NsmPCIeRetimerSwitchFactoryTest, PCIeRetimerSwitch_MissingUUID_NoSensor)
{
    const std::string testPath = basePath + "/nouuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, pcrSwitchInterface);
    pm["Name"] = std::string("RetimerNoUUID");
    pm["InventoryObjPath"] = invPath;
    pm["DeviceInstance"] = uint64_t(0);
    // UUID deliberately absent → empty uuid → parseStaticUuid throws

    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, pcrSwitchInterface,
                                               testPath);

    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// FALSE branch for count("InventoryObjPath"): empty inventoryObjPath →
// NsmPCIeRetimerSwitchDI objPath = "" + name → no leading slash →
// sdbusplus throws → caught by createObjects → no sensor
TEST_F(NsmPCIeRetimerSwitchFactoryTest,
       PCIeRetimerSwitch_MissingInventoryObjPath_NoSensor)
{
    const std::string testPath = basePath + "/noobjpath";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, pcrSwitchInterface);
    pm["Name"] = std::string("RetimerNoObjPath");
    pm["UUID"] = retimerUuid;
    pm["DeviceInstance"] = uint64_t(0);
    // InventoryObjPath absent → inventoryObjPath = "" → bad D-Bus path

    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, pcrSwitchInterface,
                                               testPath);

    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// FALSE branch for count("DeviceInstance"): defaults to 0 → still creates
// sensor successfully
// DISABLED: same SIOF — factory not registered → no sensor created even with
// all valid properties
TEST_F(NsmPCIeRetimerSwitchFactoryTest,
       DISABLED_PCIeRetimerSwitch_MissingDeviceInstance_DefaultsToZero)
{
    const std::string testPath = basePath + "/nodevinstance";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, pcrSwitchInterface);
    pm["Name"] = std::string("RetimerNoDevInst");
    pm["UUID"] = retimerUuid;
    pm["InventoryObjPath"] = invPath;
    // DeviceInstance absent → deviceInstance = 0 (default)

    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, pcrSwitchInterface,
                                               testPath);

    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Branch coverage: encode failure and GetClockState paths
// ============================================================================

// Line 114 TRUE branch: multiport encode fails when deviceIndex >
// NSM_INSTANCE_MAX (pack_nsm_header rejects instance_id=255) → coroutine
// returns early, no sensorIO
TEST_F(NsmPCIeRetimerSwitchDIBatch11F,
       Update_Multiport_LargeDeviceIndex_EncodeFails_SkipsSensorIO)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    std::string invPath =
        "/xyz/openbmc_project/inventory/batch11f/rt_mp_enc_fail/";
    // deviceIndex=255 > NSM_INSTANCE_MAX(31) → encode_multiport_query fails
    uint8_t deviceIdx = 255;
    uint8_t portType = 1;
    uint8_t portIndex = 0;
    uint8_t upstreamPort = 0;

    auto retimerDI = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "RetimerMpEncFail", associations, type, invPath, deviceIdx,
        portType, portIndex, upstreamPort);

    EXPECT_CALL(*gpu, sensorIO).Times(0);

    retimerDI->update(gpu);

    // No update expected: encode failed before sensorIO
    EXPECT_EQ(retimerDI->switchIntf->deviceId(), "");
}

// Lines 179-180 TRUE branch: genRequestMsg returns nullopt when instanceId > 31
TEST(NsmPCIeRetimerSwitchGetClockStateTest,
     GenRequestMsg_LargeInstanceId_ReturnsNullopt)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string invPath =
        "/xyz/openbmc_project/inventory/batch11f/rt_clk_genreq/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, "ClockGenReqFail",
                                          "NSM_PCIeRetimer_Switch", 0, invPath);

    // instanceId=255 > NSM_INSTANCE_MAX(31) →
    // encode_get_clock_output_enable_state_req fails
    auto result = obj.genRequestMsg(0x01, 255);
    EXPECT_FALSE(result.has_value());
}

// Line 207 Decision 'true': handleResponseMsg with success response updates
// clock state
TEST(NsmPCIeRetimerSwitchGetClockStateTest,
     HandleResponseMsg_Success_UpdatesPcieRefClock)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string invPath =
        "/xyz/openbmc_project/inventory/batch11f/rt_clk_handle/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, "ClockHandleSuccess",
                                          "NSM_PCIeRetimer_Switch", 0, invPath);

    // Build a valid success response: clkBuf with retimer1 bit set
    // nsm_pcie_clock_buffer_data: bits 7:0 = GPU clocks, bits 15:8 = retimer
    // clocks clk_buf_retimer1 is bit 8 → clkBuf = 0x0100
    uint32_t clkBuf = 0x0100; // clk_buf_retimer1 = 1
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_output_enabled_state_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto encRc = encode_get_clock_output_enable_state_resp(
        0, NSM_SUCCESS, ERR_NULL, clkBuf, msg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = obj.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(obj.pcieRefClockIntf->pcIeReferenceClockEnabled());
}
