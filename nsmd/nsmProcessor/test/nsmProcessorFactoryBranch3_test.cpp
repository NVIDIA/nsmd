/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Additional factory coverage for createNsmProcessorSensor:
 * - NSM_Processor_Attributes with optional props TRUE
 * - NSM_PowerCap type
 * - NSM_ReconfigPermissions type
 * - NSM_WorkloadPowerProfile type
 * - NSM_Processor type with DEVICE_UUID
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"
#include "device-configuration.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmProcessor.hpp"

#undef private
#undef protected

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmProcessorSensor(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath);
} // namespace nsm

struct NsmProcessorFactory3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Processor";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:8";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorFactory3Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorFactory3Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NSM_Processor_Attributes with LocationType and LocationCode
// ============================================================================
TEST_F(NsmProcessorFactory3Test, Attributes_WithLocation)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F3_Loc");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F3_Loc");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    attr["LocationCode"] = std::string("GPU_Slot_0");

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
}

// ============================================================================
// NSM_Processor_Attributes with MIGMode, ECC, EDPp enabled
// ============================================================================
TEST_F(NsmProcessorFactory3Test, Attributes_WithMIGAndECC)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F3_MIG");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F3_MIG");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MIGModeSupported"] = true;
    attr["ECCModeSupported"] = true;
    attr["EDPpScalingFactorSupported"] = true;
    attr["PortDisableFutureSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
}

// ============================================================================
// NSM_Processor_Attributes with remaining supported flags
// ============================================================================
TEST_F(NsmProcessorFactory3Test, Attributes_WithRemainingFlags)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F3_Flags");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F3_Flags");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["CpuOperatingConfigSupported"] = true;
    attr["MemCapacityUtilSupported"] = true;
    attr["TotalNvLinksCountSupported"] = true;
    attr["EGMModeSupported"] = true;
    attr["MNNVLTopologySupported"] = true;
    attr["MctpNsmOperationalStatusSupported"] = true;
    attr["GPUBasePowerLimitSupported"] = true;
    attr["GPUCopyCPUPowerLimitSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
}

// ============================================================================
// NSM_PowerCap type
// ============================================================================
TEST_F(NsmProcessorFactory3Test, PowerCap_Type)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F3_PCap");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F3_PCap");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PowerCap");
    cur["Type"] = std::string("NSM_PowerCap");

    createNsmProcessorSensor(mockManager, baseIntf + ".PowerCap", path);
}

// ============================================================================
// NSM_ReconfigPermissions type
// ============================================================================
TEST_F(NsmProcessorFactory3Test, ReconfigPermissions_Type)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F3_Reconfig");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F3_Reconfig");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    cur["Type"] = std::string("NSM_ReconfigPermissions");

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
}

// ============================================================================
// NSM_WorkloadPowerProfile type
// ============================================================================
TEST_F(NsmProcessorFactory3Test, WorkloadPowerProfile_Type)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F3_WPP");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F3_WPP");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".WorkloadPowerProfile");
    cur["Type"] = std::string("NSM_WorkloadPowerProfile");

    createNsmProcessorSensor(mockManager, baseIntf + ".WorkloadPowerProfile",
                             path);
}

// ============================================================================
// Unknown type - no sensor created
// ============================================================================
TEST_F(NsmProcessorFactory3Test, UnknownType_NoSensor)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F3_Unknown");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F3_Unknown");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(path, baseIntf + ".Unknown");
    cur["Type"] = std::string("NSM_Unknown_Type");

    const size_t before = gpu->deviceSensors.size() +
                          gpu->roundRobinSensors.size() +
                          gpu->staticSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".Unknown", path);
    EXPECT_EQ(gpu->deviceSensors.size() + gpu->roundRobinSensors.size() +
                  gpu->staticSensors.size(),
              before);
}

// ============================================================================
// Base properties fail (no cached properties)
// ============================================================================
TEST_F(NsmProcessorFactory3Test, BasePropertiesFail)
{
    const std::string path = "/test/proc_f3/no_base";
    createNsmProcessorSensor(mockManager, baseIntf, path);
}
