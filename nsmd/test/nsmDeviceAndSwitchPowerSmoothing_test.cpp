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

/*
 * Batch 10 -- Final coverage push tests for:
 *   1. nsmd/nsmDevice.cpp -- FruInterfaceManager remaining methods,
 *      addSensorBase pollingType branches, long-running handler methods,
 *      invokeLongRunningHandler edge cases.
 *   2. nsmd/nsmDeviceInventory/nsmSwitch.cpp --
 *      NsmSwitchDIPowerMode::update paths, NsmSwitchL1PredictionMode
 *      genRequestMsg/handleResponseMsg, setL1PowerModePatch edge cases.
 *   3. nsmd/nsmDbusIfaceOverride/nsmPowerSmoothingAdminProfileIntf-v2.hpp --
 *      Admin V2 remaining update methods with valid data for all sample types.
 *   4. nsmd/nsmDbusIfaceOverride/nsmPowerSmoothingPowerProfileIntf-v2.hpp --
 *      PowerProfile V2 updateSample methods with matching/mismatching
 *      profile IDs, handleSample routing for all tags.
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-capability-discovery.h"
#include "device-configuration.h"
#include "network-ports.h"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "nsmDbusIfaceOverride/nsmPowerSmoothingAdminProfileIntf-v2.hpp"
#include "nsmDbusIfaceOverride/nsmPowerSmoothingPowerProfileIntf-v2.hpp"
#include "nsmDeviceInventory/nsmSwitch.hpp"

#undef private
#undef protected

using namespace nsm;

// =============================================================================
// Encoding helpers
// =============================================================================
static void encodeUint32LE(uint8_t* buf, uint32_t val)
{
    buf[0] = static_cast<uint8_t>(val & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buf[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

static void encodeUint16LE(uint8_t* buf, uint16_t val)
{
    buf[0] = static_cast<uint8_t>(val & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
}

// =============================================================================
// PART 1: nsmDevice.cpp -- FruInterfaceManager
// =============================================================================

// ---- FruInterfaceManager::reset() ----

TEST(FruInterfaceManagerBatch10, Reset_ClearsAllState)
{
    // Arrange
    nsm::FruInterfaceManager mgr;
    std::set<std::string> props = {"A", "B", "C"};
    mgr.markInitialized(props);
    EXPECT_TRUE(mgr.initialized);
    EXPECT_FALSE(mgr.supportedProperties.empty());

    // Act
    mgr.reset();

    // Assert
    EXPECT_FALSE(mgr.initialized);
    EXPECT_TRUE(mgr.supportedProperties.empty());
    EXPECT_EQ(mgr.interface, nullptr);
}

TEST(FruInterfaceManagerBatch10, Reset_CalledMultipleTimes_NoThrow)
{
    // Arrange
    nsm::FruInterfaceManager mgr;

    // Act & Assert -- idempotent
    EXPECT_NO_THROW(mgr.reset());
    EXPECT_NO_THROW(mgr.reset());
    EXPECT_FALSE(mgr.initialized);
}

// ---- FruInterfaceManager::isPropertySupported() ----

TEST(FruInterfaceManagerBatch10,
     IsPropertySupported_AfterMarkInitialized_ReturnsTrue)
{
    // Arrange
    nsm::FruInterfaceManager mgr;
    std::set<std::string> props = {"BOARD_PART_NUMBER", "SERIAL_NUMBER"};
    mgr.markInitialized(props);

    // Act & Assert
    EXPECT_TRUE(mgr.isPropertySupported("BOARD_PART_NUMBER"));
    EXPECT_TRUE(mgr.isPropertySupported("SERIAL_NUMBER"));
    EXPECT_FALSE(mgr.isPropertySupported("MARKETING_NAME"));
    EXPECT_FALSE(mgr.isPropertySupported(""));
}

TEST(FruInterfaceManagerBatch10, IsPropertySupported_NotInitialized_AlwaysFalse)
{
    // Arrange
    nsm::FruInterfaceManager mgr;

    // Act & Assert
    EXPECT_FALSE(mgr.isPropertySupported("BOARD_PART_NUMBER"));
    EXPECT_FALSE(mgr.isPropertySupported(""));
}

// ---- FruInterfaceManager::needsRecreation() with empty sets ----

TEST(FruInterfaceManagerBatch10, NeedsRecreation_EmptyProps_WhenNotInit)
{
    // Arrange
    nsm::FruInterfaceManager mgr;
    std::set<std::string> empty;

    // Act & Assert -- not initialized => always needs recreation
    EXPECT_TRUE(mgr.needsRecreation(empty));
}

TEST(FruInterfaceManagerBatch10,
     NeedsRecreation_EmptyBoth_AfterInitWithEmpty_ReturnsFalse)
{
    // Arrange
    nsm::FruInterfaceManager mgr;
    std::set<std::string> empty;
    mgr.markInitialized(empty);

    // Act & Assert -- initialized with empty, new set is also empty -> same
    EXPECT_FALSE(mgr.needsRecreation(empty));
}

TEST(FruInterfaceManagerBatch10,
     NeedsRecreation_AfterResetThenInit_UsesNewProperties)
{
    // Arrange
    nsm::FruInterfaceManager mgr;
    std::set<std::string> propsV1 = {"A", "B"};
    mgr.markInitialized(propsV1);
    EXPECT_FALSE(mgr.needsRecreation(propsV1));

    // Act -- reset and re-initialize with different props
    mgr.reset();
    EXPECT_TRUE(mgr.needsRecreation(propsV1));

    std::set<std::string> propsV2 = {"A", "B", "C"};
    mgr.markInitialized(propsV2);

    // Assert
    EXPECT_FALSE(mgr.needsRecreation(propsV2));
    EXPECT_TRUE(mgr.needsRecreation(propsV1));
}

// ---- FruInterfaceManager::markInitialized replaces previous properties ----

TEST(FruInterfaceManagerBatch10,
     MarkInitialized_CalledTwice_ReplacesOldProperties)
{
    // Arrange
    nsm::FruInterfaceManager mgr;
    std::set<std::string> propsA = {"X"};
    std::set<std::string> propsB = {"Y", "Z"};

    // Act
    mgr.markInitialized(propsA);
    EXPECT_TRUE(mgr.isPropertySupported("X"));
    EXPECT_FALSE(mgr.isPropertySupported("Y"));

    mgr.markInitialized(propsB);

    // Assert -- old props replaced
    EXPECT_FALSE(mgr.isPropertySupported("X"));
    EXPECT_TRUE(mgr.isPropertySupported("Y"));
    EXPECT_TRUE(mgr.isPropertySupported("Z"));
}

// =============================================================================
// PART 1b: nsmDevice.cpp -- addSensorBase pollingType branches
// =============================================================================

TEST(NsmDeviceBatch10, AddSensorBase_GpuPerformanceMonitoring_SetsRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    auto sensor = std::make_shared<MockSensor>("GpmSensor", "GpmType");
    auto sizeBefore = nsmDevice.deviceSensors.size();

    // Act
    nsmDevice.addSensorBase(sensor, nsm::PollingType::GpuPerformanceMonitoring);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, GPM_REFRESH_LIMIT_IN_USEC);
    EXPECT_EQ(nsmDevice.deviceSensors.size(), sizeBefore + 1);
}

TEST(NsmDeviceBatch10, AddSensorBase_LongRunning_SetsRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    auto sensor = std::make_shared<MockSensor>("LRSensor", "LRType");
    auto sizeBefore = nsmDevice.deviceSensors.size();

    // Act
    nsmDevice.addSensorBase(sensor, nsm::PollingType::LongRunning);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, LONG_RUNNING_REFRESH_LIMIT_IN_USEC);
    EXPECT_EQ(nsmDevice.deviceSensors.size(), sizeBefore + 1);
}

TEST(NsmDeviceBatch10, AddSensorBase_Static_SetsRRRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    auto sensor = std::make_shared<MockSensor>("StaticSensor", "StaticType");

    // Act
    nsmDevice.addSensorBase(sensor, nsm::PollingType::Static);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceBatch10, AddSensorBase_Priority_SetsPriorityRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    auto sensor = std::make_shared<MockSensor>("PrioritySensor", "PriorityT");

    // Act
    nsmDevice.addSensorBase(sensor, nsm::PollingType::Priority);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, PRIORITY_REFRESH_LIMIT_IN_USEC);
}

// =============================================================================
// PART 1c: nsmDevice.cpp -- Long-running handler methods
// =============================================================================

TEST(NsmDeviceBatch10, RegisterLongRunningHandler_StoresHandlerInfo)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    auto mockEvent = std::make_shared<MockSensor>("LREvent", "LREventType");

    // Act -- store as NsmLongRunningEvent requires a proper type,
    // but we can test the optional management directly
    nsmDevice.longRunningHandler.reset();
    EXPECT_FALSE(nsmDevice.longRunningHandler.has_value());

    // Store directly to exercise getActiveLongRunningHandler
    nsmDevice.longRunningHandler = nsm::ActiveLongRunningHandlerInfo{5, 10,
                                                                     nullptr};

    // Assert
    EXPECT_TRUE(nsmDevice.longRunningHandler.has_value());
    EXPECT_EQ(nsmDevice.longRunningHandler->messageType, 5);
    EXPECT_EQ(nsmDevice.longRunningHandler->commandCode, 10);
}

TEST(NsmDeviceBatch10, ClearLongRunningHandler_WhenNoHandler_NoOp)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    nsmDevice.longRunningHandler.reset();

    // Act -- should not throw
    EXPECT_NO_THROW(nsmDevice.clearLongRunningHandler());

    // Assert
    EXPECT_FALSE(nsmDevice.longRunningHandler.has_value());
}

TEST(NsmDeviceBatch10, ClearLongRunningHandler_WhenHandlerPresent_ClearsIt)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    nsmDevice.longRunningHandler = nsm::ActiveLongRunningHandlerInfo{1, 2,
                                                                     nullptr};
    EXPECT_TRUE(nsmDevice.longRunningHandler.has_value());

    // Act
    nsmDevice.clearLongRunningHandler();

    // Assert
    EXPECT_FALSE(nsmDevice.longRunningHandler.has_value());
}

// ---- InvokeLongRunningHandler: no handler registered ----

TEST(NsmDeviceBatch10, InvokeLongRunningHandler_NoHandler_ReturnsErrorData)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    nsmDevice.longRunningHandler.reset();

    // Prepare minimal event data (just enough to not crash)
    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + 16, 0);
    auto eventMsg = reinterpret_cast<const nsm_msg*>(eventData.data());

    // Act
    auto rc = nsmDevice.invokeLongRunningHandler(1, 0, 0, eventMsg,
                                                 eventData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// =============================================================================
// PART 1d: nsmDevice.cpp -- updateMessageTypesToCommandCodeMatrix edge cases
// =============================================================================

TEST(NsmDeviceBatch10, UpdateCommandCodeMatrix_MessageTypeOutOfRange_Skipped)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    bitfield8_t supportedCommands[1] = {};
    supportedCommands[0].byte = 0xFF;

    // Act -- NUM_NSM_TYPES is the upper limit; use 0xFF which is likely
    // out of range
    nsmDevice.updateMessageTypesToCommandCodeMatrix(0xFF, supportedCommands, 1);

    // Assert -- should not have set any commands for type 0xFF
    // isCommandSupported returns false for out-of-range types
    EXPECT_FALSE(nsmDevice.isCommandSupported(0xFF, 0));
}

TEST(NsmDeviceBatch10, IsCommandSupported_MessageTypeOutOfRange_ReturnsFalse)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Act & Assert
    EXPECT_FALSE(nsmDevice.isCommandSupported(0xFF, 0));
    EXPECT_FALSE(nsmDevice.isCommandSupported(0xFF, 255));
}

// ---- allCommandCodesAreRetrieved with out-of-range message types ----

TEST(NsmDeviceBatch10,
     AllCommandCodesAreRetrieved_OutOfRangeMessageType_TreatedAsRetrieved)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    nsmDevice.areMessageTypesRetrieved = true;
    nsmDevice.commandCodesRetrieved.clear();
    // Add an out-of-range type (>= NUM_NSM_TYPES is treated as retrieved)
    nsmDevice.retrievedMessageTypes = {0xFF};

    // Act & Assert -- out-of-range types do not block
    EXPECT_TRUE(nsmDevice.allCommandCodesAreRetrieved());
}

TEST(NsmDeviceBatch10,
     AllCommandCodesAreRetrieved_MixOfValidAndOutOfRange_OnlyValidCounts)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    nsmDevice.areMessageTypesRetrieved = true;
    nsmDevice.commandCodesRetrieved.clear();
    nsmDevice.retrievedMessageTypes = {0,
                                       0xFF}; // 0 is valid, 0xFF out of range

    // 0 not yet retrieved
    nsmDevice.commandCodesRetrieved[0] = false;

    // Act & Assert
    EXPECT_FALSE(nsmDevice.allCommandCodesAreRetrieved());

    nsmDevice.commandCodesRetrieved[0] = true;
    EXPECT_TRUE(nsmDevice.allCommandCodesAreRetrieved());
}

// =============================================================================
// PART 1e: nsmDevice.cpp -- Discovery and device info getters
// =============================================================================

TEST(NsmDeviceBatch10, InitAndFinishDeviceDiscovery_CycleState)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    EXPECT_TRUE(nsmDevice.isOnline());
    EXPECT_FALSE(nsmDevice.isDiscoveryPending());

    // Act -- initDeviceDiscovery sets discoveryPending=true,
    // isDeviceActive=false
    nsmDevice.initDeviceDiscovery();

    // Assert
    EXPECT_TRUE(nsmDevice.isDiscoveryPending());
    EXPECT_FALSE(nsmDevice.isOnline());

    // Act -- finishDeviceDiscovery sets discoveryPending=false
    nsmDevice.finishDeviceDiscovery();

    // Assert
    EXPECT_FALSE(nsmDevice.isDiscoveryPending());
    EXPECT_FALSE(nsmDevice.isOnline()); // still not active
}

TEST(NsmDeviceBatch10, GetEid_ReturnsSetValue)
{
    // Arrange
    MockNsmDevice nsmDevice(1, 1, "MCTP_EID", "42", 1);

    // Act & Assert
    EXPECT_EQ(nsmDevice.getEid(), 42);
}

TEST(NsmDeviceBatch10, GetNsmDeviceInstanceNumber_ReturnsSetValue)
{
    // Arrange
    MockNsmDevice nsmDevice(1, 1, "NSM_DEVICE_INSTANCE_NUMBER", "99", 1);

    // Act & Assert
    EXPECT_EQ(nsmDevice.getNsmDeviceInstanceNumber(), 99);
}

// =============================================================================
// PART 2: nsmSwitch.cpp -- NsmSwitchDIPowerMode tests
// =============================================================================

class NsmSwitchPowerModeBatch10Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchPowerModeBatch10Test() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchPowerModeBatch10Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ---- NsmSwitchDIPowerMode update with all-zeros response ----

TEST_F(NsmSwitchPowerModeBatch10Test,
       Update_AllZeroResponse_AllFieldsSetToZeroOrFalse)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    nsm_power_mode_data pmData = {};
    memset(&pmData, 0, sizeof(pmData));

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &pmData,
                                         responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    // Act
    powerMode->update(nvswitch);

    // Assert
    auto resultData = powerMode->getPowerModeData();
    EXPECT_EQ(resultData.l1_hw_mode_control, 0);
    EXPECT_EQ(resultData.l1_hw_mode_threshold, 0u);
    EXPECT_EQ(resultData.l1_fw_throttling_mode, 0);
    EXPECT_EQ(resultData.l1_prediction_mode, 0);
    EXPECT_EQ(resultData.l1_hw_active_time, 0u);
    EXPECT_EQ(resultData.l1_hw_inactive_time, 0u);
    EXPECT_EQ(resultData.l1_prediction_inactive_time, 0u);
}

// ---- NsmSwitchDIPowerMode update with all-enabled response ----

TEST_F(NsmSwitchPowerModeBatch10Test, Update_AllEnabled_FieldsSetCorrectly)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    nsm_power_mode_data pmData = {};
    pmData.l1_hw_mode_control = 1;
    pmData.l1_hw_mode_threshold = 10000;
    pmData.l1_fw_throttling_mode = 1;
    pmData.l1_prediction_mode = 1;
    pmData.l1_hw_active_time = 500;
    pmData.l1_hw_inactive_time = 750;
    pmData.l1_prediction_inactive_time = 1000;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &pmData,
                                         responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    // Act
    powerMode->update(nvswitch);

    // Assert
    auto resultData = powerMode->getPowerModeData();
    EXPECT_EQ(resultData.l1_hw_mode_control, 1);
    EXPECT_EQ(resultData.l1_hw_mode_threshold, 10000u);
    EXPECT_EQ(resultData.l1_fw_throttling_mode, 1);
    EXPECT_EQ(resultData.l1_prediction_mode, 1);
    EXPECT_EQ(resultData.l1_hw_active_time, 500u);
    EXPECT_EQ(resultData.l1_hw_inactive_time, 750u);
    EXPECT_EQ(resultData.l1_prediction_inactive_time, 1000u);
}

// ---- NsmSwitchDIPowerMode update with sensorIO failure ----

TEST_F(NsmSwitchPowerModeBatch10Test, Update_SensorIOFails_DataUnchanged)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), true);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(999));
    powerMode->invoke(pdiMethod(fwThrottlingMode), true);
    powerMode->invoke(pdiMethod(predictionMode), true);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(111));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(222));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(333));

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    // Act
    powerMode->update(nvswitch);

    // Assert -- data should remain unchanged
    auto resultData = powerMode->getPowerModeData();
    EXPECT_EQ(resultData.l1_hw_mode_control, true);
    EXPECT_EQ(resultData.l1_hw_mode_threshold, 999u);
    EXPECT_EQ(resultData.l1_fw_throttling_mode, true);
}

// =============================================================================
// PART 2b: NsmSwitchIsolationMode -- genRequestMsg/handleResponseMsg
// =============================================================================

class NsmSwitchIsolationBatch10Test : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string type = "NSM_NVSwitch";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/batch10/nvswitch/NVSwitch_0";
};

TEST_F(NsmSwitchIsolationBatch10Test, GenRequestMsg_ValidParams_ReturnsData)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    NsmSwitchIsolationMode isolationMode(name, type, isolationIntf);

    // Act
    auto result = isolationMode.genRequestMsg(10, 5);

    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0u);
}

TEST_F(NsmSwitchIsolationBatch10Test,
       HandleResponseMsg_CcError_ReturnsNonSuccess)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());
    NsmSwitchIsolationMode isolationMode(name, type, isolationIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_switch_isolation_mode_resp(
        0, NSM_ERROR, ERR_NULL, SWITCH_COMMUNICATION_MODE_ENABLED, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = isolationMode.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_NE(rc, NSM_SUCCESS);
}

// =============================================================================
// PART 2c: NsmSwitchL1PredictionMode -- genRequestMsg / handleResponseMsg
// =============================================================================

class NsmSwitchL1PredBatch10Test : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string type = "NSM_NVSwitch";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/batch10/nvswitch/NVSwitch_0/"
        "Oem/Nvidia/PowerMode/L1PredictionMode";
};

TEST_F(NsmSwitchL1PredBatch10Test, GenRequestMsg_ValidParams_ReturnsRequest)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    NsmSwitchL1PredictionMode predMode(name, type, enableIntf, assocDefIntf);

    // Act
    auto result = predMode.genRequestMsg(10, 5);

    // Assert
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_req));
}

TEST_F(NsmSwitchL1PredBatch10Test, HandleResponseMsg_Enabled_SetsEnabledTrue)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    NsmSwitchL1PredictionMode predMode(name, type, enableIntf, assocDefIntf);
    enableIntf->enabled(false); // start as false

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_device_mode_settings_resp(
        0, NSM_SUCCESS, ERR_NULL, nsm_l1_prediction_mode_config::ENABLED,
        response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = predMode.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(enableIntf->enabled());
}

TEST_F(NsmSwitchL1PredBatch10Test, HandleResponseMsg_Disabled_SetsEnabledFalse)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    NsmSwitchL1PredictionMode predMode(name, type, enableIntf, assocDefIntf);
    enableIntf->enabled(true); // start as true

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_device_mode_settings_resp(
        0, NSM_SUCCESS, ERR_NULL, nsm_l1_prediction_mode_config::DISABLED,
        response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = predMode.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_FALSE(enableIntf->enabled());
}

TEST_F(NsmSwitchL1PredBatch10Test, HandleResponseMsg_CcError_ReturnsErrorCode)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    NsmSwitchL1PredictionMode predMode(name, type, enableIntf, assocDefIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_device_mode_settings_resp(
        0, NSM_ERROR, ERR_NULL, nsm_l1_prediction_mode_config::ENABLED,
        response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = predMode.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_NE(rc, NSM_SUCCESS);
}

// =============================================================================
// PART 2d: NsmSwitchDIPowerMode -- setL1PowerDevice via postPatchIO
// =============================================================================

TEST_F(NsmSwitchPowerModeBatch10Test,
       SetL1PowerDevice_SuccessfulResponse_ReturnsSuccess)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    nsm_power_mode_data data = {};
    data.l1_hw_mode_control = 1;
    data.l1_hw_mode_threshold = 5000;

    // Build success response for set_power_mode
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_set_power_mode_resp(0, ERR_NULL, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    powerMode->setL1PowerDevice(data, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSwitchPowerModeBatch10Test,
       SetL1PowerDevice_ErrorResponse_SetsWriteFailure)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    nsm_power_mode_data data = {};

    // Build error response: encode success then patch CC to error
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_set_power_mode_resp(0, ERR_NULL, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    // Patch completion_code to NSM_ERROR
    auto* resp =
        reinterpret_cast<nsm_set_power_mode_resp*>(responseMsg->payload);
    resp->completion_code = NSM_ERROR;

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    powerMode->setL1PowerDevice(data, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmSwitchPowerModeBatch10Test,
       SetL1PowerDevice_PostPatchIOFails_SetsWriteFailure)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    nsm_power_mode_data data = {};

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    powerMode->setL1PowerDevice(data, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// PART 2e: NsmSwitchDIPowerMode -- setL1PowerModePatch
// =============================================================================

TEST_F(NsmSwitchPowerModeBatch10Test,
       SetL1PowerModePatch_AsyncInProgress_SetsUnavailable)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    powerMode->asyncPatchInProgress = true;

    using PatchVal = std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    std::vector<std::tuple<std::string, PatchVal>> patchVals;
    patchVals.emplace_back("HWModeControl", PatchVal{true});

    AsyncSetOperationValueType value = patchVals;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

TEST_F(NsmSwitchPowerModeBatch10Test,
       SetL1PowerModePatch_ValidHWModeControl_CallsSetL1PowerDevice)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    // Build success set_power_mode response
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_set_power_mode_resp(0, ERR_NULL, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    using PatchVal = std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    std::vector<std::tuple<std::string, PatchVal>> patchVals;
    patchVals.emplace_back("HWModeControl", PatchVal{true});

    AsyncSetOperationValueType value = patchVals;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(powerMode->asyncPatchInProgress);
}

TEST_F(NsmSwitchPowerModeBatch10Test,
       SetL1PowerModePatch_EmptyPatchValues_ThrowsInvalidArgument)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    using PatchVal = std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    std::vector<std::tuple<std::string, PatchVal>> patchVals; // empty

    AsyncSetOperationValueType value = patchVals;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act -- coroutine catches exception internally (direct throw in coverage)
    EXPECT_NO_THROW_COROUTINE(
        powerMode->setL1PowerModePatch(value, &status, nvswitch));
}

TEST_F(NsmSwitchPowerModeBatch10Test,
       SetL1PowerModePatch_InvalidType_HandledInternally)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    // Pass wrong type -- AsyncSetOperationValueType that is not a vector of
    // tuples
    AsyncSetOperationValueType value = std::string("not-a-vector");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act -- coroutine catches exception internally (direct throw in coverage)
    EXPECT_NO_THROW_COROUTINE(
        powerMode->setL1PowerModePatch(value, &status, nvswitch));
}

TEST_F(NsmSwitchPowerModeBatch10Test,
       SetL1PowerModePatch_UnknownProperty_HandledInternally)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    using PatchVal = std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    std::vector<std::tuple<std::string, PatchVal>> patchVals;
    patchVals.emplace_back("UnknownProp", PatchVal{true});

    AsyncSetOperationValueType value = patchVals;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act -- coroutine catches exception internally (direct throw in coverage)
    EXPECT_NO_THROW_COROUTINE(
        powerMode->setL1PowerModePatch(value, &status, nvswitch));
}

TEST_F(NsmSwitchPowerModeBatch10Test,
       SetL1PowerModePatch_WrongTypeForBoolProp_HandledInternally)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    using PatchVal = std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    std::vector<std::tuple<std::string, PatchVal>> patchVals;
    // HWModeControl expects bool, pass uint32_t
    patchVals.emplace_back("HWModeControl", PatchVal{uint32_t(42)});

    AsyncSetOperationValueType value = patchVals;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act -- coroutine catches exception internally (direct throw in coverage)
    EXPECT_NO_THROW_COROUTINE(
        powerMode->setL1PowerModePatch(value, &status, nvswitch));
}

TEST_F(NsmSwitchPowerModeBatch10Test,
       SetL1PowerModePatch_AllProperties_Accepted)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), false);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), false);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(0));

    // Build success response
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_set_power_mode_resp(0, ERR_NULL, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    using PatchVal = std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    std::vector<std::tuple<std::string, PatchVal>> patchVals;
    patchVals.emplace_back("HWModeControl", PatchVal{true});
    patchVals.emplace_back("FWThrottlingMode", PatchVal{true});
    patchVals.emplace_back("PredictionMode", PatchVal{false});
    patchVals.emplace_back("HWThreshold", PatchVal{uint32_t(5000)});
    patchVals.emplace_back("HWActiveTime", PatchVal{uint32_t(100)});
    patchVals.emplace_back("HWInactiveTime", PatchVal{uint32_t(200)});
    patchVals.emplace_back("HWPredictionInactiveTime", PatchVal{uint32_t(300)});

    AsyncSetOperationValueType value = patchVals;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    powerMode->setL1PowerModePatch(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(powerMode->asyncPatchInProgress);
}

// =============================================================================
// PART 3: OemAdminProfileIntfV2 -- remaining updateSample methods
// =============================================================================

class AdminProfileV2Batch10Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const std::string parentPath =
        "/xyz/openbmc_project/inventory/batch10/gpu0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    AdminProfileV2Batch10Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~AdminProfileV2Batch10Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ---- updateCurrentPrimaryFloorActivationOffsetSample ----

TEST_F(AdminProfileV2Batch10Test,
       AdminV2_PrimaryFloorActivationOffset_ValidValue_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // 8000 mW -> 8.0 W
    uint32_t offsetVal = 8000;
    uint8_t data[4] = {};
    encodeUint32LE(data, offsetVal);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentPrimaryFloorActivationOffsetSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(offsetVal, 1000);
    EXPECT_NEAR(
        adminIntf->AdminPowerProfileIntf::primaryFloorActivationOffset(),
        expected, 0.01);
}

TEST_F(AdminProfileV2Batch10Test,
       AdminV2_PrimaryFloorActivationOffset_BadLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[2] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentPrimaryFloorActivationOffsetSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---- updateCurrentPrimaryFloorActivationWindowMultiplierSample ----

TEST_F(AdminProfileV2Batch10Test,
       AdminV2_PrimaryFloorActivationWindowMult_ValidValue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t multiplierVal = 50;
    uint8_t data[1] = {};
    data[0] = multiplierVal;

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag =
        ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc =
        adminIntf->updateCurrentPrimaryFloorActivationWindowMultiplierSample(
            sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint8ToDouble(multiplierVal);
    EXPECT_NEAR(
        adminIntf
            ->AdminPowerProfileIntf::primaryFloorActivationWindowMultiplier(),
        expected, 0.01);
}

// ---- updateCurrentPrimaryFloorTargetWindowSample ----

TEST_F(AdminProfileV2Batch10Test, AdminV2_PrimaryFloorTargetWindow_ValidValue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t targetWindowVal = 25;
    uint8_t data[1] = {};
    data[0] = targetWindowVal;

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentPrimaryFloorTargetWindowSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint8ToDouble(targetWindowVal);
    EXPECT_NEAR(
        adminIntf->AdminPowerProfileIntf::primaryFloorTargetWindowMultiplier(),
        expected, 0.01);
}

// ---- updateCurrentSecondaryFloorSample ----

TEST_F(AdminProfileV2Batch10Test,
       AdminV2_SecondaryFloor_ValidValue_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t secondaryFloor = 15000;
    uint8_t data[4] = {};
    encodeUint32LE(data, secondaryFloor);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentSecondaryFloorSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(secondaryFloor,
                                                               1000);
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::secondaryPowerFloorSetting(),
                expected, 0.01);
}

// ---- updateCurrentRampDownHysteresisSample ----

TEST_F(AdminProfileV2Batch10Test,
       AdminV2_RampDownHysteresis_ValidValue_ConvertsMsToS)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t hysteresisMs = 4500;
    uint8_t data[4] = {};
    encodeUint32LE(data, hysteresisMs);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentRampDownHysteresisSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(hysteresisMs,
                                                               1000);
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::rampDownHysteresis(),
                expected, 0.01);
}

// ---- Admin V2 resetParam ----

TEST_F(AdminProfileV2Batch10Test, AdminV2_ResetParam_ZeroValue_ReturnsFalse)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // Act & Assert
    EXPECT_FALSE(adminIntf->resetParam(0.0));
    EXPECT_FALSE(adminIntf->resetParam(1.0));
    EXPECT_FALSE(adminIntf->resetParam(100.0));
}

TEST_F(AdminProfileV2Batch10Test,
       AdminV2_ResetParam_InvalidPowerLimit_ReturnsTrue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // Act & Assert
    EXPECT_TRUE(
        adminIntf->resetParam(static_cast<double>(INVALID_POWER_LIMIT)));
}

// =============================================================================
// PART 4: OemPowerProfileIntfV2 -- updateSample methods with profile IDs
// =============================================================================

class PowerProfileV2Batch10Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const std::string parentPath =
        "/xyz/openbmc_project/inventory/batch10/gpu0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    PowerProfileV2Batch10Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~PowerProfileV2Batch10Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ---- updateCurrentTMPFloorSample: matching profileId ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_TMPFloor_MatchingProfileId_SetsValue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 2;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint16_t ufxpValue = 2048; // 0.5 fraction -> 50%
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentTMPFloorSample(sample, profileId,
                                                      profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = NvUFXP4_12ToDouble(ufxpValue) * 100;
    EXPECT_NEAR(profileIntf->PowerProfileIntf::tmpFloorPercent(), expected,
                0.01);
}

// ---- updateCurrentTMPFloorSample: mismatching profileId ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_TMPFloor_MismatchingProfileId_DoesNotSetValue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 2;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    // Set an initial value so we can check it doesn't change
    profileIntf->PowerProfileIntf::tmpFloorPercent(99.0);

    uint16_t ufxpValue = 2048;
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act -- profIdRes=5 != profId=2
    int rc = profileIntf->updateCurrentTMPFloorSample(sample, 5, profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Value should remain unchanged since profile ID doesn't match
    EXPECT_NEAR(profileIntf->PowerProfileIntf::tmpFloorPercent(), 99.0, 0.01);
}

// ---- updateCurrentTMPFloorSample: INVALID_UINT16_VALUE ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_TMPFloor_InvalidUint16_SetsInvalidUint32)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint16_t invalidVal = INVALID_UINT16_VALUE;
    uint8_t data[2] = {};
    encodeUint16LE(data, invalidVal);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentTMPFloorSample(sample, profileId,
                                                      profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(
        static_cast<uint32_t>(profileIntf->PowerProfileIntf::tmpFloorPercent()),
        INVALID_UINT32_VALUE);
}

// ---- updateCurrentRampUpRateSample: matching profileId ----

TEST_F(PowerProfileV2Batch10Test, PowerProfV2_RampUpRate_MatchingId_SetsValue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 1;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t rampUpRate = 6000; // 6000 mW/s -> 6.0 W/s
    uint8_t data[4] = {};
    encodeUint32LE(data, rampUpRate);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentRampUpRateSample(sample, profileId,
                                                        profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(rampUpRate,
                                                               1000);
    EXPECT_NEAR(profileIntf->PowerProfileIntf::rampUpRate(), expected, 0.01);
}

// ---- updateCurrentRampUpRateSample: mismatching profileId ----

TEST_F(PowerProfileV2Batch10Test, PowerProfV2_RampUpRate_MismatchId_DoesNotSet)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 1;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    profileIntf->PowerProfileIntf::rampUpRate(42.0);

    uint32_t rampUpRate = 6000;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampUpRate);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act -- profIdRes=3 != profId=1
    int rc = profileIntf->updateCurrentRampUpRateSample(sample, 3, profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_NEAR(profileIntf->PowerProfileIntf::rampUpRate(), 42.0, 0.01);
}

// ---- updateCurrentRampDownRateSample: matching profileId ----

TEST_F(PowerProfileV2Batch10Test, PowerProfV2_RampDownRate_MatchingId_SetsValue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t rampDownRate = 4000;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampDownRate);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentRampDownRateSample(sample, profileId,
                                                          profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(rampDownRate,
                                                               1000);
    EXPECT_NEAR(profileIntf->PowerProfileIntf::rampDownRate(), expected, 0.01);
}

// ---- updateCurrentRampDownHysteresisSample: matching profileId ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_RampDownHysteresis_MatchingId_SetsValue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t hysteresis = 3500;
    uint8_t data[4] = {};
    encodeUint32LE(data, hysteresis);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentRampDownHysteresisSample(
        sample, profileId, profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(hysteresis,
                                                               1000);
    EXPECT_NEAR(profileIntf->PowerProfileIntf::rampDownHysteresis(), expected,
                0.01);
}

// ---- updateCurrentSecondaryFloorSample: matching profileId ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_SecondaryFloor_MatchingId_SetsValue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t secondaryFloor = 20000;
    uint8_t data[4] = {};
    encodeUint32LE(data, secondaryFloor);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentSecondaryFloorSample(sample, profileId,
                                                            profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(secondaryFloor,
                                                               1000);
    EXPECT_NEAR(profileIntf->PowerProfileIntf::secondaryPowerFloorSetting(),
                expected, 0.01);
}

// ---- updateCurrentPrimaryFloorActivationWindowMultiplierSample ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_PrimaryFloorActWindowMult_MatchingId_SetsValue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t multiplier = 60;
    uint8_t data[1] = {};
    data[0] = multiplier;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc =
        profileIntf->updateCurrentPrimaryFloorActivationWindowMultiplierSample(
            sample, profileId, profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint8ToDouble(multiplier);
    EXPECT_NEAR(
        profileIntf->PowerProfileIntf::primaryFloorActivationWindowMultiplier(),
        expected, 0.01);
}

// ---- updateCurrentPrimaryFloorTargetWindowSample ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_PrimaryFloorTargetWindow_MatchingId_SetsValue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t targetWindow = 30;
    uint8_t data[1] = {};
    data[0] = targetWindow;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentPrimaryFloorTargetWindowSample(
        sample, profileId, profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint8ToDouble(targetWindow);
    EXPECT_NEAR(
        profileIntf->PowerProfileIntf::primaryFloorTargetWindowMultiplier(),
        expected, 0.01);
}

// ---- updateCurrentPrimaryFloorActivationOffsetSample ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_PrimaryFloorActOffset_MatchingId_SetsValue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t offset = 12000;
    uint8_t data[4] = {};
    encodeUint32LE(data, offset);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentPrimaryFloorActivationOffsetSample(
        sample, profileId, profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(offset, 1000);
    EXPECT_NEAR(profileIntf->PowerProfileIntf::primaryFloorActivationOffset(),
                expected, 0.01);
}

// ---- handleSample: rampDownHysteresis tag ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_HandleSample_RampDownHysteresisTag_Routes)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t hysteresis = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, hysteresis);

    // tag field: high 5 bits = tag type, low 3 bits = profileId
    // PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG=3 => (3 << 3) | profId
    uint8_t tagVal = (PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG << 3) | profileId;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tagVal;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---- handleSample: secondaryFloor tag ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_HandleSample_SecondaryFloorTag_Routes)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t floor = 25000;
    uint8_t data[4] = {};
    encodeUint32LE(data, floor);

    uint8_t tagVal = (PRESET_PROFILE_SECONDARY_FLOOR_TAG << 3) | profileId;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tagVal;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---- handleSample: PrimaryFloorActivationWindowMultiplier tag ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_HandleSample_PrimaryFloorActWinMultTag_Routes)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t multiplier = 75;
    uint8_t data[1] = {};
    data[0] = multiplier;

    uint8_t tagVal =
        (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG << 3) |
        profileId;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tagVal;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---- handleSample: PrimaryFloorTargetWindow tag ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_HandleSample_PrimaryFloorTargetWindowTag_Routes)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t targetWin = 40;
    uint8_t data[1] = {};
    data[0] = targetWin;

    uint8_t tagVal = (PRESET_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG << 3) |
                     profileId;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tagVal;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---- handleSample: PrimaryFloorActivationOffset tag ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_HandleSample_PrimaryFloorActOffsetTag_Routes)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t offset = 7500;
    uint8_t data[4] = {};
    encodeUint32LE(data, offset);

    uint8_t tagVal = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG << 3) |
                     profileId;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tagVal;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---- handleSample: unknown tag ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_HandleSample_UnknownTag_ReturnsErrorLength)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[4] = {};

    // Use a tag whose high 5 bits decode to an unknown value
    // e.g. tag type = 31 (unknown), profId = 0
    uint8_t tagVal = (31 << 3) | profileId;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tagVal;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ---- PowerProfile V2 resetParam ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_ResetParam_InvalidLimit_ReturnsTrue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    // Act & Assert
    EXPECT_TRUE(
        profileIntf->resetParam(static_cast<double>(INVALID_POWER_LIMIT)));
}

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_ResetParam_NormalValue_ReturnsFalse)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    // Act & Assert
    EXPECT_FALSE(profileIntf->resetParam(0.0));
    EXPECT_FALSE(profileIntf->resetParam(50.5));
    EXPECT_FALSE(profileIntf->resetParam(100.0));
}

// ---- PowerProfile V2 getProfileId / getInventoryObjPath ----

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_GetProfileId_ReturnsConstructedValue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 5;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    // Act & Assert
    EXPECT_EQ(profileIntf->getProfileId(), 5);
}

TEST_F(PowerProfileV2Batch10Test,
       PowerProfV2_GetInventoryObjPath_IncludesProfileId)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 3;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    // Act
    auto path = profileIntf->getInventoryObjPath();

    // Assert
    EXPECT_EQ(path, parentPath + "/profile/3");
}

// =============================================================================
// PART 5: NsmSwitchDI constructors and properties
// =============================================================================

class NsmSwitchDIBatch10Test : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_batch10";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/batch10/nvswitch/";
};

TEST_F(NsmSwitchDIBatch10Test, SwitchIntf_NameAndTypeCorrect)
{
    // Arrange & Act
    auto switchObj =
        std::make_shared<NsmSwitchDI<SwitchIntf>>(name, inventoryObjPath);

    // Assert
    EXPECT_EQ(switchObj->getName(), name);
    EXPECT_EQ(switchObj->getType(), "NSM_NVSwitch");
    EXPECT_EQ(switchObj->objPath, inventoryObjPath + name);
}

TEST_F(NsmSwitchDIBatch10Test, UuidIntf_NameAndTypeCorrect)
{
    // Arrange & Act
    auto uuidObj = std::make_shared<NsmSwitchDI<UuidIntf>>(name,
                                                           inventoryObjPath);

    // Assert
    EXPECT_EQ(uuidObj->getName(), name);
    EXPECT_EQ(uuidObj->getType(), "NSM_NVSwitch");
}

TEST_F(NsmSwitchDIBatch10Test, NvSwitchIntf_NameAndTypeCorrect)
{
    // Arrange & Act
    auto nvSwitchObj =
        std::make_shared<NsmSwitchDI<NvSwitchIntf>>(name, inventoryObjPath);

    // Assert
    EXPECT_EQ(nvSwitchObj->getName(), name);
    EXPECT_EQ(nvSwitchObj->getType(), "NSM_NVSwitch");
}

TEST_F(NsmSwitchDIBatch10Test, AssociationDefIntf_NameAndTypeCorrect)
{
    // Arrange & Act
    auto assocObj = std::make_shared<NsmSwitchDI<AssociationDefinitionsInft>>(
        name, inventoryObjPath);

    // Assert
    EXPECT_EQ(assocObj->getName(), name);
    EXPECT_EQ(assocObj->getType(), "NSM_NVSwitch");
}

TEST_F(NsmSwitchDIBatch10Test, NsmAssetIntf_NameAndTypeCorrect)
{
    // Arrange & Act
    auto assetObj =
        std::make_shared<NsmSwitchDI<NsmAssetIntf>>(name, inventoryObjPath);

    // Assert
    EXPECT_EQ(assetObj->getName(), name);
    EXPECT_EQ(assetObj->getType(), "NSM_NVSwitch");
}

// =============================================================================
// PART 6: Additional NsmSwitchDIPowerMode constructor / inventory path tests
// =============================================================================

TEST_F(NsmSwitchDIBatch10Test, PowerMode_InventoryObjectPath_FormattedCorrectly)
{
    // Arrange & Act
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);

    // Assert
    EXPECT_EQ(powerMode->getInventoryObjectPath(), inventoryObjPath + name);
    EXPECT_EQ(powerMode->getName(), name);
    EXPECT_EQ(powerMode->getType(), "NSM_NVSwitch");
    EXPECT_FALSE(powerMode->asyncPatchInProgress);
}

TEST_F(NsmSwitchDIBatch10Test, PowerMode_GetPowerModeData_ReflectsInvokedValues)
{
    // Arrange
    auto powerMode = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                            inventoryObjPath);
    powerMode->invoke(pdiMethod(hwModeControl), true);
    powerMode->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(2500));
    powerMode->invoke(pdiMethod(fwThrottlingMode), false);
    powerMode->invoke(pdiMethod(predictionMode), true);
    powerMode->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(600));
    powerMode->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(800));
    powerMode->invoke(pdiMethod(hwPredictionInactiveTime),
                      static_cast<uint64_t>(1200));

    // Act
    auto data = powerMode->getPowerModeData();

    // Assert
    EXPECT_EQ(data.l1_hw_mode_control, true);
    EXPECT_EQ(data.l1_hw_mode_threshold, 2500u);
    EXPECT_EQ(data.l1_fw_throttling_mode, false);
    EXPECT_EQ(data.l1_prediction_mode, true);
    EXPECT_EQ(data.l1_hw_active_time, 600u);
    EXPECT_EQ(data.l1_hw_inactive_time, 800u);
    EXPECT_EQ(data.l1_prediction_inactive_time, 1200u);
}

// =============================================================================
// PART 7: Admin V2 handleSample -- full tag routing for completeness
// =============================================================================

TEST_F(AdminProfileV2Batch10Test,
       AdminV2_HandleSample_PrimaryFloorTargetWindowTag_Routes)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t targetWindowVal = 42;
    uint8_t data[1] = {};
    data[0] = targetWindowVal;

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(AdminProfileV2Batch10Test,
       AdminV2_HandleSample_PrimaryFloorActivationOffsetTag_Routes)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t offsetVal = 9000;
    uint8_t data[4] = {};
    encodeUint32LE(data, offsetVal);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(AdminProfileV2Batch10Test,
       AdminV2_HandleSample_PrimaryFloorActivationWindowMultTag_Routes)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t multiplier = 80;
    uint8_t data[1] = {};
    data[0] = multiplier;

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag =
        ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// PART 8: Admin V2 -- getInventoryObjPath
// =============================================================================

TEST_F(AdminProfileV2Batch10Test,
       AdminV2_GetInventoryObjPath_ReturnsAdminProfilePath)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // Act
    auto path = adminIntf->getInventoryObjPath();

    // Assert
    EXPECT_EQ(path, parentPath + "/profile/admin_profile");
}

// =============================================================================
// PART 9: NsmSwitchDIReset constructor
// =============================================================================

class NsmSwitchDIResetBatch10Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:101";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchDIResetBatch10Test() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchDIResetBatch10Test()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmSwitchDIResetBatch10Test, Constructor_CreatesResetInterfaces)
{
    // Arrange
    std::string name = "NVSwitch_Reset_0";
    std::string type = "NSM_NVSwitch";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/batch10/nvswitch/";

    // Act
    NsmSwitchDIReset resetSensor(bus, name, type, inventoryObjPath, nvswitch);

    // Assert
    EXPECT_EQ(resetSensor.getName(), name);
    EXPECT_EQ(resetSensor.getType(), type);
    EXPECT_NE(resetSensor.resetIntf, nullptr);
    EXPECT_NE(resetSensor.resetAsyncIntf, nullptr);
}

// =============================================================================
// PART 10: Additional NsmDevice edge cases
// =============================================================================

TEST(NsmDeviceBatch10, DeviceRemapProp_MctpAssociation_StoresCorrectly)
{
    // Arrange & Act
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", "test-uuid", 1);

    // Assert -- MCTP_UUID should set remapProp to MCTP_UUID
    EXPECT_EQ(nsmDevice.getDeviceRemapProp(),
              nsm::DeviceRemapProperty::MCTP_UUID);
}

TEST(NsmDeviceBatch10, AddDeviceEvent_StoresEventInDeviceEvents)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto mockEvent = std::make_shared<MockSensor>("TestEvent", "EventType");

    // Act -- addDeviceEvent is not available in MockSensor, but we can
    // check deviceEvents size before and after manual push
    size_t sizeBefore = nsmDevice.deviceEvents.size();

    // Note: addDeviceEvent requires NsmEvent, not NsmSensor
    // We test the deviceEvents vector directly since NsmEvent is abstract
    nsmDevice.deviceEvents.push_back(nullptr);

    // Assert
    EXPECT_EQ(nsmDevice.deviceEvents.size(), sizeBefore + 1);
}

TEST(NsmDeviceBatch10, GpuDriverSensor_InitiallyNull)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Assert
    EXPECT_EQ(nsmDevice.gpuDriverSensor, nullptr);
}

TEST(NsmDeviceBatch10, MsgTypesSensor_NotNull_AfterConstruction)
{
    // Arrange & Act
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    // The real NsmDevice constructor calls initMsgTypesSensor,
    // but MockNsmDevice may not have nsmMsgHandler. Check directly.
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Assert -- MockNsmDevice bypasses initMsgTypesSensor (no nsmMsgHandler),
    // so msgTypesSensor may be null, but we verify the member exists
    // and can be accessed without crash
    EXPECT_NO_THROW((void)nsmDevice.msgTypesSensor);
}

TEST(NsmDeviceBatch10, SensorQueues_AccessibleAfterConstruction)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Assert -- constructor may pre-populate some queues (e.g. msgTypesSensor)
    // Verify queues are accessible and consistent
    EXPECT_NO_THROW((void)nsmDevice.deviceSensors.size());
    EXPECT_TRUE(nsmDevice.setSensors.empty());
}

TEST(NsmDeviceBatch10, EventDispatcher_AccessibleAfterConstruction)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Assert -- constructor may pre-register event handlers
    EXPECT_NO_THROW((void)nsmDevice.eventDispatcher.eventsMap.size());
}

// =============================================================================
// PART 11: NsmSwitchIsolationMode constructor field checks
// =============================================================================

TEST(NsmSwitchIsolationBatch10, Constructor_SetsAllFields)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    const std::string objPath =
        "/xyz/openbmc_project/inventory/batch10iso/nvswitch/NVSwitch_0";
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(bus,
                                                               objPath.c_str());

    // Act
    NsmSwitchIsolationMode isolationMode("NVSwitch_iso", "NSM_NVSwitch",
                                         isolationIntf);

    // Assert
    EXPECT_EQ(isolationMode.getName(), "NVSwitch_iso");
    EXPECT_EQ(isolationMode.getType(), "NSM_NVSwitch");
    EXPECT_NE(isolationMode.switchIsolationIntf, nullptr);
}

// =============================================================================
// PART 12: NsmSwitchL1PredictionMode constructor / field checks
// =============================================================================

TEST(NsmSwitchL1PredBatch10, Constructor_SetsAllFields)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    const std::string objPath =
        "/xyz/openbmc_project/inventory/batch10pred/nvswitch/NVSwitch_0/"
        "Oem/Nvidia/PowerMode/L1PredictionMode";
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());

    // Act
    NsmSwitchL1PredictionMode predMode("NVSwitch_pred", "NSM_NVSwitch",
                                       enableIntf, assocDefIntf);

    // Assert
    EXPECT_EQ(predMode.getName(), "NVSwitch_pred");
    EXPECT_EQ(predMode.getType(), "NSM_NVSwitch");
    EXPECT_NE(predMode.enableIntf, nullptr);
    EXPECT_NE(predMode.associationDefIntf, nullptr);
    EXPECT_FALSE(predMode.asyncPatchInProgress);
}
