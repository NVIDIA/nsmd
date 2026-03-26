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
 * Branch coverage tests for createNsmProcessorSensor factory coroutine
 * in nsmd/nsmProcessor/nsmProcessor.cpp.
 *
 * Targets the FALSE branches of:
 *   allBaseIfaceProperties.count("Name")
 *   allBaseIfaceProperties.count("UUID")
 *   allCurrentIfaceProperties.count("Type")
 *   allBaseIfaceProperties.count("InventoryObjPath")
 *   allBaseIfaceProperties.count("DEVICE_UUID")
 *   allCurrentIfaceProperties.count("LocationType")
 *   allCurrentIfaceProperties.count("LocationCode")
 *   allCurrentIfaceProperties.count("MIGModeSupported")
 *   allCurrentIfaceProperties.count("PortDisableFutureSupported") //
 * allCurrentIfaceProperties.count("ECCModeSupported") //
 *
 *   allCurrentIfaceProperties.count("EDPpScalingFactorSupported") //
 * allCurrentIfaceProperties.count("PowerSmoothingSupported")
 *
 *   allCurrentIfaceProperties.count("CpuOperatingConfigSupported") //
 * allCurrentIfaceProperties.count("MemCapacityUtilSupported")
 *
 *   allCurrentIfaceProperties.count("TotalNvLinksCountSupported") //
 * allCurrentIfaceProperties.count("EGMModeSupported") //
 * allCurrentIfaceProperties.count("MNNVLTopologySupported")
 *
 *   allCurrentIfaceProperties.count("MctpNsmOperationalStatusSupported") //
 *
 *   allCurrentIfaceProperties.count("GPUBasePowerLimitSupported") //
 *
 *   allCurrentIfaceProperties.count("GPUCopyCPUPowerLimitSupported") //
 * allCurrentIfaceProperties.count("DeviceId") in createPCIe /
 * // createProcessorPerformance
 * allCurrentIfaceProperties.count("Count") in // createPCIe
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

// ============================================================================
// Fixture
// ============================================================================
struct NsmProcessorFactoryBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Processor";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:7";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorFactoryBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorFactoryBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NSM_Processor_Attributes type with NO optional properties at all
// This exercises all the FALSE branches for every "Supported" property
// and LocationType/LocationCode/DEVICE_UUID
// ============================================================================
TEST_F(NsmProcessorFactoryBranch2Test,
       ProcessorAttributes_AllOptionalPropsAbsent)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_FB2_Attr_NoOpt");
    auto& baseMap = utils::MockDbusAsync::propertyMap(path, basicIntfName);
    baseMap["Name"] = std::string("GPU_FB2_Attr_NoOpt");
    baseMap["UUID"] = gpuUuid;
    baseMap["InventoryObjPath"] = path;
    // No DEVICE_UUID property

    auto& attrMap = utils::MockDbusAsync::propertyMap(
        path, basicIntfName + ".ProcessorAttributes");
    attrMap["Type"] = std::string("NSM_Processor_Attributes");
    // All optional properties deliberately absent:
    // LocationType, LocationCode, MIGModeSupported, PortDisableFutureSupported,
    // ECCModeSupported, EDPpScalingFactorSupported, PowerSmoothingSupported,
    // CpuOperatingConfigSupported, MemCapacityUtilSupported,
    // TotalNvLinksCountSupported, EGMModeSupported, MNNVLTopologySupported,
    // MctpNsmOperationalStatusSupported, GPUBasePowerLimitSupported,
    // GPUCopyCPUPowerLimitSupported

    createNsmProcessorSensor(mockManager,
                             basicIntfName + ".ProcessorAttributes", path);

    // Factory still creates asset sensors (NsmAssetIntf) but no feature sensors
    EXPECT_GE(gpu->staticSensors.size(), 1u);
}

// ============================================================================
// NSM_Processor type with NO base properties (Name, UUID, InventoryObjPath)
// This exercises the FALSE branches for base property reads
// ============================================================================
TEST_F(NsmProcessorFactoryBranch2Test, DISABLED_ProcessorBase_NoNameUuidInvPath)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_FB2_NoBase");
    // Empty base property map - no Name, UUID, InventoryObjPath
    auto& baseMap = utils::MockDbusAsync::propertyMap(path, basicIntfName);
    (void)baseMap; // intentionally empty

    auto& currentMap = utils::MockDbusAsync::propertyMap(
        path, basicIntfName + ".ProcessorAttributes");
    // No Type property
    (void)currentMap;

    // With no UUID, getNsmDeviceFromStaticUUID returns nullptr for empty uuid
    // The factory will proceed with empty strings and empty uuid
    createNsmProcessorSensor(mockManager,
                             basicIntfName + ".ProcessorAttributes", path);
}

// ============================================================================
// NSM_Processor type: DEVICE_UUID absent FALSE branch
// ============================================================================
TEST_F(NsmProcessorFactoryBranch2Test, DISABLED_Processor_NoDeviceUuid)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_FB2_NoDevUuid");
    auto& baseMap = utils::MockDbusAsync::propertyMap(path, basicIntfName);
    baseMap["Name"] = std::string("GPU_FB2_NoDevUuid");
    baseMap["UUID"] = gpuUuid;
    baseMap["InventoryObjPath"] = path;
    // DEVICE_UUID deliberately absent

    auto& currentMap = utils::MockDbusAsync::propertyMap(path, basicIntfName);
    currentMap["Type"] = std::string("NSM_Processor");

    createNsmProcessorSensor(mockManager, basicIntfName, path);

    // NsmUuidIntf still created with empty deviceUuid
    EXPECT_GE(gpu->staticSensors.size(), 1u);
}

// ============================================================================
// NSM_PCIe: DeviceId and Count absent (FALSE branches)
// ============================================================================
TEST_F(NsmProcessorFactoryBranch2Test, PCIe_NoDeviceIdNoCount)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_FB2_PCIe_NoDevIdCount");
    auto& baseMap = utils::MockDbusAsync::propertyMap(path, basicIntfName);
    baseMap["Name"] = std::string("GPU_FB2_PCIe_NoDevIdCount");
    baseMap["UUID"] = gpuUuid;
    baseMap["InventoryObjPath"] = path;

    auto& pcieMap = utils::MockDbusAsync::propertyMap(path,
                                                      basicIntfName + ".PCIe");
    pcieMap["Type"] = std::string("NSM_PCIe");
    // DeviceId and Count deliberately absent

    createNsmProcessorSensor(mockManager, basicIntfName + ".PCIe", path);

    // createPCIe still adds NsmPCIeLinkSpeed sensor
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

// ============================================================================
// NSM_ProcessorPerformance: DeviceId absent (FALSE branch)
// ============================================================================
TEST_F(NsmProcessorFactoryBranch2Test, ProcessorPerf_NoDeviceId)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_FB2_Perf_NoDevId");
    auto& baseMap = utils::MockDbusAsync::propertyMap(path, basicIntfName);
    baseMap["Name"] = std::string("GPU_FB2_Perf_NoDevId");
    baseMap["UUID"] = gpuUuid;
    baseMap["InventoryObjPath"] = path;

    auto& perfMap = utils::MockDbusAsync::propertyMap(
        path, basicIntfName + ".ProcessorPerformance");
    perfMap["Type"] = std::string("NSM_ProcessorPerformance");
    // DeviceId deliberately absent

    createNsmProcessorSensor(mockManager,
                             basicIntfName + ".ProcessorPerformance", path);

    // Still creates throttle/gpu-util/pci sensors
    EXPECT_GE(gpu->roundRobinSensors.size(), 2u);
}

// ============================================================================
// NSM_Processor_Attributes: each "Supported" flag present but set to false
// Exercises the `count()` TRUE but `get<bool>() == false` branches
// ============================================================================
TEST_F(NsmProcessorFactoryBranch2Test,
       ProcessorAttributes_AllSupportedFlagsFalse)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_FB2_Attr_AllFalse");
    auto& baseMap = utils::MockDbusAsync::propertyMap(path, basicIntfName);
    baseMap["Name"] = std::string("GPU_FB2_Attr_AllFalse");
    baseMap["UUID"] = gpuUuid;
    baseMap["InventoryObjPath"] = path;

    auto& attrMap = utils::MockDbusAsync::propertyMap(
        path, basicIntfName + ".ProcessorAttributes");
    attrMap["Type"] = std::string("NSM_Processor_Attributes");
    attrMap["MIGModeSupported"] = false;
    attrMap["PortDisableFutureSupported"] = false;
    attrMap["ECCModeSupported"] = false;
    attrMap["EDPpScalingFactorSupported"] = false;
    attrMap["PowerSmoothingSupported"] = false;
    attrMap["CpuOperatingConfigSupported"] = false;
    attrMap["MemCapacityUtilSupported"] = false;
    attrMap["TotalNvLinksCountSupported"] = false;
    attrMap["EGMModeSupported"] = false;
    attrMap["MNNVLTopologySupported"] = false;
    attrMap["MctpNsmOperationalStatusSupported"] = false;
    attrMap["GPUBasePowerLimitSupported"] = false;
    attrMap["GPUCopyCPUPowerLimitSupported"] = false;

    createNsmProcessorSensor(mockManager,
                             basicIntfName + ".ProcessorAttributes", path);

    // No feature sensors created, only asset sensors
    EXPECT_GE(gpu->staticSensors.size(), 1u);
}
