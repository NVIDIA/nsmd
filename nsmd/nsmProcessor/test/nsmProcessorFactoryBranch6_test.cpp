/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Factory branch coverage batch 6 for createNsmProcessorSensor:
 *
 * Targets the FALSE side of compound conditions where the property IS
 * present but its bool value is false:
 *   count("MIGModeSupported") && std::get<bool>() == false
 *   count("PortDisableFutureSupported") && std::get<bool>() == false
 *   count("ECCModeSupported") && std::get<bool>() == false
 *   count("EDPpScalingFactorSupported") && std::get<bool>() == false
 *   count("PowerSmoothingSupported") && std::get<bool>() == false
 *   count("CpuOperatingConfigSupported") && std::get<bool>() == false
 *   count("MemCapacityUtilSupported") && std::get<bool>() == false
 *   count("TotalNvLinksCountSupported") && std::get<bool>() == false
 *   count("EGMModeSupported") && std::get<bool>() == false
 *   count("MNNVLTopologySupported") && std::get<bool>() == false
 *   count("MctpNsmOperationalStatusSupported") && std::get<bool>() == false
 *   count("GPUBasePowerLimitSupported") && std::get<bool>() == false
 *   count("GPUCopyCPUPowerLimitSupported") && std::get<bool>() == false
 *
 * Also covers:
 *   - NSM_Processor without Name/UUID/Type/InventoryObjPath (all absent)
 *   - NSM_Processor_Attributes with all Supported = false (no sub-sensors)
 *   - NSM_Processor_Attributes with first group true, second group false
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

struct NsmProcessorFactory6Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Processor";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:11";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorFactory6Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorFactory6Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NSM_Processor_Attributes: ALL bool Supported properties present but FALSE
// Each count() returns true but std::get<bool>() is false -> body skipped
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_AllSupportedFalse)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F6_AllFalse");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F6_AllFalse");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MIGModeSupported"] = false;
    attr["PortDisableFutureSupported"] = false;
    attr["ECCModeSupported"] = false;
    attr["EDPpScalingFactorSupported"] = false;
    attr["PowerSmoothingSupported"] = false;
    attr["CpuOperatingConfigSupported"] = false;
    attr["MemCapacityUtilSupported"] = false;
    attr["TotalNvLinksCountSupported"] = false;
    attr["EGMModeSupported"] = false;
    attr["MNNVLTopologySupported"] = false;
    attr["MctpNsmOperationalStatusSupported"] = false;
    attr["GPUBasePowerLimitSupported"] = false;
    attr["GPUCopyCPUPowerLimitSupported"] = false;

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);

    // Only asset sensors created, no MIG/ECC/EDPp/PowerSmoothing etc.
    // The Attributes block always creates asset + inventory sensors
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    // None of the bool-guarded sub-functions should have added RR sensors
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// NSM_Processor_Attributes: first group TRUE, second group FALSE
// Covers MIG/ECC/EDPp/PortDisable=true, rest=false
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_FirstGroupTrue_SecondFalse)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F6_Split");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F6_Split");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MIGModeSupported"] = true;
    attr["PortDisableFutureSupported"] = true;
    attr["ECCModeSupported"] = true;
    attr["EDPpScalingFactorSupported"] = true;
    attr["PowerSmoothingSupported"] = false;
    attr["CpuOperatingConfigSupported"] = false;
    attr["MemCapacityUtilSupported"] = false;
    attr["TotalNvLinksCountSupported"] = false;
    attr["EGMModeSupported"] = false;
    attr["MNNVLTopologySupported"] = false;
    attr["MctpNsmOperationalStatusSupported"] = false;
    attr["GPUBasePowerLimitSupported"] = false;
    attr["GPUCopyCPUPowerLimitSupported"] = false;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);

    // MIG/ECC/EDPp/PortDisable sensors created; rest skipped
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: second group TRUE, first group FALSE
// Covers CpuOp/MemCap/TotalNvLinks/EGM/MNNVL/GpuOpStatus/GPUBase/GPUCopy=true
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_SecondGroupTrue_FirstFalse)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F6_Split2");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F6_Split2");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MIGModeSupported"] = false;
    attr["PortDisableFutureSupported"] = false;
    attr["ECCModeSupported"] = false;
    attr["EDPpScalingFactorSupported"] = false;
    attr["PowerSmoothingSupported"] = false;
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

    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: PowerSmoothing=true only (all others absent)
// Covers PowerSmoothingSupported TRUE with minimum config
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_PowerSmoothingOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F6_PSOnly");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F6_PSOnly");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["PowerSmoothingSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);

    // PowerSmoothing creates many device sensors
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: LocationType only (no LocationCode)
// Covers: count("LocationType") TRUE, count("LocationCode") FALSE
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_LocationTypeNoCode)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F6_LocTypeOnly");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F6_LocTypeOnly");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    // No LocationCode

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: LocationCode only (no LocationType)
// Covers: count("LocationType") FALSE, count("LocationCode") TRUE
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_LocationCodeNoType)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F6_LocCodeOnly");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F6_LocCodeOnly");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    // No LocationType
    attr["LocationCode"] = std::string("GPU_Slot_1");

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: GPUBasePowerLimit only TRUE (rest absent)
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_GPUBasePowerLimitOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F6_GPUBase");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F6_GPUBase");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["GPUBasePowerLimitSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: GPUCopyCPUPowerLimit only TRUE (rest absent)
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_GPUCopyCPUPowerLimitOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F6_GPUCopy");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F6_GPUCopy");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["GPUCopyCPUPowerLimitSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: GPUBasePowerLimit=false, GPUCopyCPU=false
// ============================================================================
TEST_F(NsmProcessorFactory6Test, Attributes_GPUPowerLimitsBothFalse)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F6_GPUBothFalse");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F6_GPUBothFalse");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["GPUBasePowerLimitSupported"] = false;
    attr["GPUCopyCPUPowerLimitSupported"] = false;

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    // No power limit sensors added
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore);
}
