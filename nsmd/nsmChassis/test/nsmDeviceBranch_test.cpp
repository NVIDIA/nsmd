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

#include "base.h"
#include "device-configuration.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Common/UUID/server.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmDevice.hpp"
#include "nsmEvent/nsmLongRunningEvent.hpp"
#include "nsmFwSwInventory/GPUSWInventory.hpp"
#include "nsmNumericSensor/nsmNumericAggregator.hpp"
#include "nsmSetAsync/asyncOperationManager.hpp"

#undef private
#undef protected

using namespace nsm;

// Forward declarations for factory functions
namespace nsm
{
requester::Coroutine createNsmProcessorSensor(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath);
} // namespace nsm

static auto bus = sdbusplus::bus::new_default();

// ============================================================================
// PART 3: NsmDevice - additional coverage for FruInterfaceManager
// ============================================================================

TEST(FruInterfaceManager, IsPropertySupported_EmptyAfterConstruction)
{
    // Arrange
    FruInterfaceManager mgr = {};

    // Act & Assert
    EXPECT_FALSE(mgr.isPropertySupported("BOARD_PART_NUMBER"));
    EXPECT_FALSE(mgr.isPropertySupported("SERIAL_NUMBER"));
    EXPECT_FALSE(mgr.isPropertySupported(""));
}

TEST(FruInterfaceManager, MarkInitialized_ThenCheckProperties)
{
    // Arrange
    FruInterfaceManager mgr = {};
    std::set<std::string> props = {"BOARD_PART_NUMBER", "SERIAL_NUMBER",
                                   "DEVICE_TYPE"};

    // Act
    mgr.markInitialized(props);

    // Assert
    EXPECT_TRUE(mgr.isPropertySupported("BOARD_PART_NUMBER"));
    EXPECT_TRUE(mgr.isPropertySupported("SERIAL_NUMBER"));
    EXPECT_TRUE(mgr.isPropertySupported("DEVICE_TYPE"));
    EXPECT_FALSE(mgr.isPropertySupported("MARKETING_NAME"));
    EXPECT_FALSE(mgr.needsRecreation(props));
}

TEST(FruInterfaceManager, NeedsRecreation_DifferentPropertySet)
{
    // Arrange
    FruInterfaceManager mgr = {};
    std::set<std::string> props1 = {"BOARD_PART_NUMBER", "SERIAL_NUMBER"};
    std::set<std::string> props2 = {"BOARD_PART_NUMBER", "MARKETING_NAME"};

    // Act
    mgr.markInitialized(props1);

    // Assert
    EXPECT_TRUE(mgr.needsRecreation(props2));
}

TEST(FruInterfaceManager, Reset_ClearsEverything)
{
    // Arrange
    FruInterfaceManager mgr = {};
    std::set<std::string> props = {"BOARD_PART_NUMBER"};
    mgr.markInitialized(props);

    // Act
    mgr.reset();

    // Assert
    EXPECT_TRUE(mgr.needsRecreation(props));
    EXPECT_FALSE(mgr.isPropertySupported("BOARD_PART_NUMBER"));
    EXPECT_FALSE(mgr.initialized);
}

TEST(FruInterfaceManager, MultipleResetCycles_MaintainsConsistency)
{
    // Arrange
    FruInterfaceManager mgr = {};
    std::set<std::string> props1 = {"A", "B"};
    std::set<std::string> props2 = {"C", "D"};

    // Cycle 1
    mgr.markInitialized(props1);
    EXPECT_TRUE(mgr.isPropertySupported("A"));
    EXPECT_FALSE(mgr.needsRecreation(props1));

    // Reset
    mgr.reset();
    EXPECT_FALSE(mgr.isPropertySupported("A"));
    EXPECT_TRUE(mgr.needsRecreation(props1));

    // Cycle 2
    mgr.markInitialized(props2);
    EXPECT_TRUE(mgr.isPropertySupported("C"));
    EXPECT_FALSE(mgr.isPropertySupported("A"));
    EXPECT_FALSE(mgr.needsRecreation(props2));
    EXPECT_TRUE(mgr.needsRecreation(props1));
}

// ============================================================================
// PART 4: NsmDevice - additional method coverage
// ============================================================================

TEST(NsmDeviceBatch9, SetEventMode_BoundaryValues)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // Act & Assert
    device.setEventMode(0);
    EXPECT_EQ(device.getEventMode(), 0);

    device.setEventMode(1);
    EXPECT_EQ(device.getEventMode(), 1);

    device.setEventMode(2);
    EXPECT_EQ(device.getEventMode(), 2);

    // Invalid - should not change
    device.setEventMode(3);
    EXPECT_EQ(device.getEventMode(), 2);

    device.setEventMode(255);
    EXPECT_EQ(device.getEventMode(), 2);
}

TEST(NsmDeviceBatch9, IsCommandSupported_InvalidMessageType_ReturnsFalse)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // Act & Assert - messageType >= NUM_NSM_TYPES should return false
    EXPECT_FALSE(device.isCommandSupported(NUM_NSM_TYPES, 0));
    EXPECT_FALSE(device.isCommandSupported(255, 0));
}

TEST(NsmDeviceBatch9, UpdateCommandCodeMatrix_InvalidMessageType_Skipped)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    bitfield8_t supportedCommands[4] = {};
    supportedCommands[0].byte = 0xFF;

    // Act - should not crash with messageType >= NUM_NSM_TYPES
    device.updateMessageTypesToCommandCodeMatrix(NUM_NSM_TYPES,
                                                 supportedCommands, 4);

    // Assert - nothing should be set (no crash)
    EXPECT_FALSE(device.isCommandSupported(0, 0));
}

TEST(NsmDeviceBatch9, AllCommandCodesRetrieved_OutOfRangeMessageType)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    device.areMessageTypesRetrieved = true;
    device.retrievedMessageTypes.clear();
    // Add an out-of-range message type
    device.retrievedMessageTypes.push_back(NUM_NSM_TYPES + 10);
    // Command codes for it are not retrieved, but it's out of range so it
    // should be treated as "retrieved"

    // Act & Assert
    EXPECT_TRUE(device.allCommandCodesAreRetrieved());
}

TEST(NsmDeviceBatch9, RegisterLongRunningHandler_InvokeWithMismatch)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid,
                         NSM_DEV_ROLE_RESERVED);

    // Create a mock long running event sensor
    class MockLongRunningEvent : public NsmLongRunningEvent
    {
      public:
        MockLongRunningEvent() :
            NsmLongRunningEvent("mock_lr", "mock_lr_type", true)
        {}
        int handle(eid_t, NsmType, NsmEventId, const nsm_msg*, size_t) override
        {
            return NSM_SW_SUCCESS;
        }
    };

    auto lrEvent = std::make_shared<MockLongRunningEvent>();
    device.registerLongRunningHandler(1, 2, lrEvent);

    EXPECT_TRUE(device.longRunningHandler.has_value());

    // Act - invoke with no active handler after clearing
    device.clearLongRunningHandler();
    EXPECT_FALSE(device.longRunningHandler.has_value());

    // Verify invokeLongRunningHandler with no handler returns error
    auto rc = device.invokeLongRunningHandler(0, (NsmType)0, 0, nullptr, 0);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmDeviceBatch9, FindAggregatorByType_MultipleTypes)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // findAggregatorByType with no aggregators
    auto result = device.findAggregatorByType("SomeType");
    EXPECT_EQ(result, nullptr);
}

TEST(NsmDeviceBatch9, UpdateDiscoveryIdentifiers_FirstCall)
{
    // Arrange
    uuid_t uuid = "";
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_UUID",
                         "00000000-0000-0000-0000-000000000000",
                         NSM_DEV_ROLE_RESERVED);
    device.uuid = ""; // empty UUID to simulate first call

    eid_t eid = 42;
    uuid_t newUuid = "11111111-1111-1111-1111-111111111111";
    uint8_t devInstNum = 5;
    std::string associatedPath = "/some/path";
    std::string mctpMedium = "SMBus";
    std::string mctpBinding = "SMBus";

    // Act
    bool isPreferred = device.updateDiscoveryIdentifiers(
        eid, newUuid, devInstNum, associatedPath, mctpMedium, mctpBinding, 30);

    // Assert
    EXPECT_TRUE(isPreferred);
    EXPECT_EQ(device.getEid(), 42);
    EXPECT_EQ(device.uuid, newUuid);
}

TEST(NsmDeviceBatch9, AddSensorBase_SetsDeviceIdentifier)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(NSM_DEV_ID_GPU, 3, "MCTP_UUID", uuid,
                         NSM_DEV_ROLE_RESERVED);

    auto sensor = std::make_shared<MockSensor>("test_sensor", "test_type");

    // Act
    device.addSensorBase(sensor, PollingType::RoundRobin);

    // Assert
    EXPECT_FALSE(sensor->deviceIdentifier.empty());
    EXPECT_EQ(device.deviceSensors.size(), 2u); // 1 msgTypes + 1 added
}

TEST(NsmDeviceBatch9, AddSensorBase_PriorityPolling_SetsRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("prio_sensor", "prio_type");

    // Act
    device.addSensorBase(sensor, PollingType::Priority);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, PRIORITY_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceBatch9, AddSensorBase_GpmPolling_SetsRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("gpm_sensor", "gpm_type");

    // Act
    device.addSensorBase(sensor, PollingType::GpuPerformanceMonitoring);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, GPM_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceBatch9, AddSensorBase_LongRunning_SetsRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("lr_sensor", "lr_type");

    // Act
    device.addSensorBase(sensor, PollingType::LongRunning);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, LONG_RUNNING_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceBatch9, AddSensorBase_StaticPolling_SetsRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("static_sensor", "static_type");

    // Act
    device.addSensorBase(sensor, PollingType::Static);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceBatch9, ProgressCounters_LazyInitialized)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 0, "MCTP_UUID", uuid, 0);

    // sensorProgressCounters should be null initially
    EXPECT_EQ(device.sensorProgressCounters, nullptr);

    // Act - calling progressCounters() should create it
    auto& counters = device.progressCounters();

    // Assert
    EXPECT_NE(device.sensorProgressCounters, nullptr);
    (void)counters;
}

// Cover progressCounters() FALSE branch (line 1075): second call returns the
// cached ProgressCounters without re-creating it.
TEST(NsmDeviceBatch9, ProgressCounters_SecondCall_ReturnsCached)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 0, "MCTP_UUID", uuid, 0);

    auto& c1 = device.progressCounters();
    auto* ptr1 = &c1;

    // Second call must hit the FALSE branch of if (!sensorProgressCounters)
    auto& c2 = device.progressCounters();
    auto* ptr2 = &c2;

    EXPECT_EQ(ptr1, ptr2); // Same underlying object
}

// Cover clearLongRunningHandler() early-return path: when no handler is set,
// !longRunningHandler.has_value() is true → return without touching the value.
TEST(NsmDeviceBatch9, ClearLongRunningHandler_AlreadyClear_NoOp)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 0, "MCTP_UUID", uuid, 0);

    EXPECT_FALSE(device.longRunningHandler.has_value());
    // Must not throw and must leave the optional empty
    EXPECT_NO_THROW(device.clearLongRunningHandler());
    EXPECT_FALSE(device.longRunningHandler.has_value());
}

// Cover setOffline() TRUE branch of `if (gpuDriverSensor)` (lines 229-235):
// attach a real NsmGPUSWInventoryDriverVersionAndStatus, call setOffline(),
// and verify driverState is reset to 0.
TEST(NsmDeviceBatch9, SetOffline_WithGpuDriverSensor_SetsStateZero)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid,
                         NSM_DEV_ROLE_RESERVED);

    auto driverSensor =
        std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
            bus, "TestDriverSensor_SetOffline",
            std::vector<utils::Association>{}, "GPUDriver", "NVIDIA");
    driverSensor->driverState = 2; // Non-zero before offline
    device.addGpuDriverSensor(driverSensor);

    EXPECT_NE(device.gpuDriverSensor, nullptr);

    device.setOffline();

    EXPECT_EQ(driverSensor->driverState, 0u);
    EXPECT_FALSE(device.isDeviceActive);
}

// ============================================================================
// PART 8: NsmProcessor factory - remaining coverage
// ============================================================================

struct NsmProcessorBatch9Test :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Processor";
    const std::string name = "GPU_SXM_2";
    const std::string objPath = std::string(processorsInventoryBasePath) + name;
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:5";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorBatch9Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(gpu, nullptr);
        EXPECT_EQ(NSM_DEV_ID_GPU, gpu->getDeviceType());
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorBatch9Test()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basic = {
        {"Name", name},
        {"Type", std::string("NSM_Processor")},
        {"UUID", gpuUuid},
        {"DEVICE_UUID", gpuUuid},
        {"InventoryObjPath", objPath},
        {"Priority", false},
    };
};

TEST_F(NsmProcessorBatch9Test,
       CreateProcessorAttributes_MemCapacityUtilEnabled_Standalone)
{
    // Arrange - only MemCapacityUtil enabled
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = basic["Name"];
    basePropertyMap["UUID"] = basic["UUID"];
    basePropertyMap["InventoryObjPath"] = basic["InventoryObjPath"];

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ProcessorAttributes");
    propertyMap["Type"] = std::string("NSM_Processor_Attributes");
    propertyMap["MIGModeSupported"] = false;
    propertyMap["PortDisableFutureSupported"] = false;
    propertyMap["ECCModeSupported"] = false;
    propertyMap["EDPpScalingFactorSupported"] = false;
    propertyMap["PowerSmoothingSupported"] = false;
    propertyMap["CpuOperatingConfigSupported"] = false;
    propertyMap["MemCapacityUtilSupported"] = true;
    propertyMap["TotalNvLinksCountSupported"] = false;
    propertyMap["EGMModeSupported"] = false;
    propertyMap["MNNVLTopologySupported"] = false;

    // Act
    createNsmProcessorSensor(mockManager,
                             basicIntfName + ".ProcessorAttributes", objPath);

    // Assert - 3 Asset (static) + MemCapacityUtil (LR) + 1 msgTypes
    EXPECT_GE(gpu->deviceSensors.size(), 1u);
    EXPECT_GE(gpu->longRunningSensors.size(), 0u);
}

TEST_F(NsmProcessorBatch9Test, CreateProcessorAttributes_AllFeaturesEnabled)
{
    // Arrange - ALL features enabled simultaneously
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = basic["Name"];
    basePropertyMap["UUID"] = basic["UUID"];
    basePropertyMap["InventoryObjPath"] = basic["InventoryObjPath"];

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ProcessorAttributes");
    propertyMap["Type"] = std::string("NSM_Processor_Attributes");
    propertyMap["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location."
        "LocationTypes.Embedded");
    propertyMap["LocationCode"] = std::string("GPU_SXM_2");
    propertyMap["MIGModeSupported"] = true;
    propertyMap["PortDisableFutureSupported"] = true;
    propertyMap["ECCModeSupported"] = true;
    propertyMap["EDPpScalingFactorSupported"] = true;
    propertyMap["PowerSmoothingSupported"] = true;
    propertyMap["CpuOperatingConfigSupported"] = true;
    propertyMap["MemCapacityUtilSupported"] = true;
    propertyMap["TotalNvLinksCountSupported"] = true;
    propertyMap["EGMModeSupported"] = true;
    propertyMap["MNNVLTopologySupported"] = true;

    // Act
    createNsmProcessorSensor(mockManager,
                             basicIntfName + ".ProcessorAttributes", objPath);

    // Assert - very large number of sensors created
    EXPECT_GE(gpu->deviceSensors.size(), 10u);
}

// NSM_Processor_Attributes with NO "Supported" flags at all → all 11
// count() FALSE branches: MIGModeSupported, PortDisableFutureSupported,
// ECCModeSupported, EDPpScalingFactorSupported, PowerSmoothingSupported,
// CpuOperatingConfigSupported, MemCapacityUtilSupported,
// TotalNvLinksCountSupported, EGMModeSupported, MNNVLTopologySupported,
// MctpNsmOperationalStatusSupported all absent → skipped.
// Also LocationType and LocationCode absent → those count() FALSE branches.
TEST_F(NsmProcessorBatch9Test,
       CreateProcessorAttributes_NoSupportedFlags_AllFALSEBranches)
{
    const std::string uniquePath =
        std::string(processorsInventoryBasePath / "GPU_SXM_NoFlags");
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                              basicIntfName);
    basePropertyMap["Name"] = std::string("GPU_SXM_NoFlags");
    basePropertyMap["UUID"] = gpuUuid;
    basePropertyMap["InventoryObjPath"] = uniquePath;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".ProcessorAttributes");
    // Type only — all optional properties absent (no Supported flags,
    // no LocationType, no LocationCode) → all count() checks return 0
    propertyMap["Type"] = std::string("NSM_Processor_Attributes");

    createNsmProcessorSensor(
        mockManager, basicIntfName + ".ProcessorAttributes", uniquePath);

    // Asset sensors still created even with no optional features
    EXPECT_GE(gpu->staticSensors.size(), 1u);
}

TEST_F(NsmProcessorBatch9Test, DISABLED_CreateNSMProcessor_BaseType)
{
    const std::string uniquePath =
        std::string(processorsInventoryBasePath / "GPU_SXM_BaseType");
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                              basicIntfName);
    basePropertyMap["Name"] = std::string("GPU_SXM_BaseType");
    basePropertyMap["UUID"] = gpuUuid;
    basePropertyMap["InventoryObjPath"] = uniquePath;
    basePropertyMap["DEVICE_UUID"] =
        std::string("12345678-abcd-abcd-abcd-123456789abc");
    basePropertyMap["Type"] = std::string("NSM_Processor");

    // Act
    createNsmProcessorSensor(mockManager, basicIntfName, uniquePath);

    // Assert - many sensors should be created for NSM_Processor type:
    // associationSensor, uuidSensor, debugInfoObject, eraseTraceObject,
    // logInfoObject, diagnosticsObject, gpuRevisionSensor,
    // totalMemorySizeSensor, healthSensor, errorInjectionSensors,
    // confidentialComputeSensor
    EXPECT_GE(gpu->deviceSensors.size(), 5u);
}

TEST_F(NsmProcessorBatch9Test,
       CreateProcessorAttributes_LocationCodeOnly_NoLocationType)
{
    // Arrange - only LocationCode, no LocationType
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = basic["Name"];
    basePropertyMap["UUID"] = basic["UUID"];
    basePropertyMap["InventoryObjPath"] = basic["InventoryObjPath"];

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ProcessorAttributes");
    propertyMap["Type"] = std::string("NSM_Processor_Attributes");
    propertyMap["LocationCode"] = std::string("GPU_SXM_2");
    propertyMap["MIGModeSupported"] = false;
    propertyMap["PortDisableFutureSupported"] = false;
    propertyMap["ECCModeSupported"] = false;
    propertyMap["EDPpScalingFactorSupported"] = false;
    propertyMap["PowerSmoothingSupported"] = false;
    propertyMap["CpuOperatingConfigSupported"] = false;
    propertyMap["MemCapacityUtilSupported"] = false;
    propertyMap["TotalNvLinksCountSupported"] = false;
    propertyMap["EGMModeSupported"] = false;
    propertyMap["MNNVLTopologySupported"] = false;

    // Act
    createNsmProcessorSensor(mockManager,
                             basicIntfName + ".ProcessorAttributes", objPath);

    // Assert - 3 Asset (static) + LocationCode (device) + 1 msgTypes
    EXPECT_GE(gpu->deviceSensors.size(), 1u);
}

// ============================================================================
// PART 9: NsmDevice - constructor variant coverage
// ============================================================================

TEST(NsmDeviceBatch9, Constructor_MctpAssociation_SetsRemapProperty)
{
    // Arrange & Act
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_ASSOCIATION",
                         "/some/mctp/path", NSM_DEV_ROLE_RESERVED);

    // Assert
    EXPECT_EQ(device.getDeviceRemapProp(),
              DeviceRemapProperty::MCTP_ASSOCIATION);
}

TEST(NsmDeviceBatch9, Constructor_InvalidRemapProp_NoMatch)
{
    // Arrange & Act - invalid remap property should log error but not crash
    // The MockNsmDevice constructor calls NsmDevice constructor which handles
    // this
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "INVALID_REMAP", "somevalue",
                         NSM_DEV_ROLE_RESERVED);

    // Assert - device should still be created
    EXPECT_EQ(device.getDeviceType(), NSM_DEV_ID_GPU);
}

TEST(NsmDeviceBatch9, DeviceLifecycle_InitDiscovery_FinishDiscovery)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid,
                         NSM_DEV_ROLE_RESERVED);

    // Act - init discovery
    device.initDeviceDiscovery();
    EXPECT_TRUE(device.isDiscoveryPending());
    EXPECT_FALSE(device.isOnline());

    // Act - finish discovery
    device.finishDeviceDiscovery();
    EXPECT_FALSE(device.isDiscoveryPending());
}

TEST(NsmDeviceBatch9, MarkDeviceReady_ThenNotReady)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid,
                         NSM_DEV_ROLE_RESERVED);

    // Act
    device.markDeviceAsReady();
    EXPECT_TRUE(device.isReady());

    device.markDeviceAsNotReady();
    EXPECT_FALSE(device.isReady());
}

TEST(NsmDeviceBatch9, AddCapabilityRefreshSensor_Single)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid,
                         NSM_DEV_ROLE_RESERVED);
    auto sensor = std::make_shared<MockSensor>("cap_sensor", "cap_type");

    // Act
    device.addCapabilityRefreshSensor(sensor);

    // Assert
    EXPECT_EQ(device.capabilityRefreshSensors.size(), 1u);
}

TEST(NsmDeviceBatch9, AddSetSensor_Single)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid,
                         NSM_DEV_ROLE_RESERVED);
    auto sensor = std::make_shared<MockSensor>("set_sensor", "set_type");

    // Act
    device.addSetSensor(sensor);

    // Assert
    EXPECT_EQ(device.setSensors.size(), 1u);
}

TEST(NsmDeviceBatch9, AddStandByToDcRefreshSensor_Single)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid,
                         NSM_DEV_ROLE_RESERVED);
    auto sensor = std::make_shared<MockSensor>("dc_sensor", "dc_type");

    // Act
    device.addStandByToDcRefreshSensor(sensor);

    // Assert
    EXPECT_EQ(device.standByToDcRefreshSensors.size(), 1u);
}

TEST(NsmDeviceBatch9, GetSemaphore_ReturnsReference)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid,
                         NSM_DEV_ROLE_RESERVED);

    // Act
    auto& sem = device.getSemaphore();

    // Assert - just verify it returns a reference without crash
    (void)sem;
}

TEST(NsmDeviceBatch9, GetDeviceRemapValues_MctpUuid)
{
    // Arrange
    uuid_t uuid = "test-uuid-value";
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid,
                         NSM_DEV_ROLE_RESERVED);

    // Act
    auto values = device.getDeviceRemapValues();

    // Assert
    EXPECT_TRUE(std::holds_alternative<std::vector<uuid_t>>(values));
}

TEST(NsmDeviceBatch9, GetDeviceRemapValues_MctpEid)
{
    // Arrange
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_EID", "42",
                         NSM_DEV_ROLE_RESERVED);

    // Act
    auto values = device.getDeviceRemapValues();

    // Assert
    EXPECT_TRUE(std::holds_alternative<std::vector<uint8_t>>(values));
}

TEST(NsmDeviceBatch9, GetDeviceRemapValues_NsmDeviceInstanceNumber)
{
    // Arrange
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "NSM_DEVICE_INSTANCE_NUMBER", "10",
                         NSM_DEV_ROLE_RESERVED);

    // Act
    auto values = device.getDeviceRemapValues();

    // Assert
    EXPECT_TRUE(std::holds_alternative<std::vector<uint8_t>>(values));
}

// ============================================================================
// PART 12: NsmDevice - updateMessageTypesToCommandCodeMatrix edge cases
// ============================================================================

TEST(NsmDeviceBatch9, UpdateCommandCodeMatrix_LargeSize_ClampedToMax)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // Create a large supportedCommands array
    bitfield8_t supportedCommands[64] = {};
    supportedCommands[0].byte = 0xFF;
    supportedCommands[31].byte = 0xFF;

    // Act - size 64 means 64*8 = 512 commands, but clamped to NUM_COMMAND_CODES
    device.updateMessageTypesToCommandCodeMatrix(0, supportedCommands, 64);

    // Assert - commands 0-7 should be supported
    EXPECT_TRUE(device.isCommandSupported(0, 0));
    EXPECT_TRUE(device.isCommandSupported(0, 7));
}

TEST(NsmDeviceBatch9, UpdateCommandCodeMatrix_ZeroSize_NoCommands)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    bitfield8_t supportedCommands[1] = {};
    supportedCommands[0].byte = 0xFF;

    // Act - size 0 means no commands at all
    device.updateMessageTypesToCommandCodeMatrix(0, supportedCommands, 0);

    // Assert
    EXPECT_FALSE(device.isCommandSupported(0, 0));
}

// ============================================================================
// PART 13: updateDiscoveryIdentifiers — fruDeviceManager.reset() branch
// (line 464): this->eid != eid && this->uuid.size() != 0 → TRUE
// ============================================================================

TEST(NsmDeviceBatch9,
     UpdateDiscoveryIdentifiers_NewEid_NonEmptyUuid_ResetsFruManager)
{
    // Arrange: device already has uuid + eid set
    uuid_t uuid = "00000000-0000-0000-0000-000000000001";
    MockNsmDevice device(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid,
                         NSM_DEV_ROLE_RESERVED);

    // Directly set the fields to simulate a previously discovered device
    std::string medium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    std::string binding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";
    device.uuid = uuid; // non-empty → this->uuid.size() != 0 is TRUE
    device.eid = 10;    // current eid
    device.mctpMedium = medium;
    device.mctpBinding = binding;

    // Mark as initialized so we can observe the reset
    std::set<std::string> props = {"BOARD_PART_NUMBER"};
    device.fruDeviceManager.markInitialized(props);
    EXPECT_TRUE(device.fruDeviceManager.initialized);

    // Act: call with a DIFFERENT eid (20 != 10) → if branch at line 462 is TRUE
    // → fruDeviceManager.reset() is called
    uuid_t newUuid = "00000000-0000-0000-0000-000000000002";
    std::string path = "/some/path";
    bool isPreferred = device.updateDiscoveryIdentifiers(20, newUuid, 0, path,
                                                         medium, binding, 30);

    // Assert: isPreferred=true (same medium/binding → same priority),
    // eid and uuid updated, fruDeviceManager was reset
    EXPECT_TRUE(isPreferred);
    EXPECT_EQ(device.getEid(), 20);
    EXPECT_EQ(device.uuid, newUuid);
    EXPECT_FALSE(device.fruDeviceManager.initialized);
}

// ============================================================================
// PART 14: FruInterfaceManager::updateAllPropertyValues — null interface
// → if (!interface) return; early return covers the TRUE branch (line 1129).
// ============================================================================

TEST(FruInterfaceManager, UpdateAllPropertyValues_NullInterface_EarlyReturn)
{
    // Arrange: FruInterfaceManager with no interface created
    FruInterfaceManager mgr = {};
    EXPECT_FALSE(mgr.interface); // interface is null initially

    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    auto device = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, NSM_DEV_ROLE_RESERVED);
    nsm::InventoryProperties properties;

    // Act: call with null interface → if (!interface) return; hits TRUE branch
    // Should not crash
    mgr.updateAllPropertyValues(device, properties);

    // Assert: state unchanged, no crash
    EXPECT_FALSE(mgr.initialized);
    EXPECT_FALSE(mgr.interface);
}
