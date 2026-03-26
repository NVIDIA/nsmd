/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Additional factory branch coverage for createNsmProcessorSensor:
 * - createProcessorPerformance with DeviceId present
 * - createWorkloadPowerProfile with ProfileIdMap present
 * - createReconfigPermissions with Features list
 * - createPowerCap with CompositeNumericSensors
 * - createPCIe with DeviceId and Count > 0 (loop body)
 * - createProcessorPerformance without DeviceId (false branch)
 * - createWorkloadPowerProfile without ProfileIdMap (false branch)
 * - createPowerCap without CompositeNumericSensors (false branch)
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

struct NsmProcessorFactory4Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Processor";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:9";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorFactory4Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorFactory4Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// createProcessorPerformance: DeviceId present (TRUE branch at line 566)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ProcessorPerf_WithDeviceId)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_Perf_DevId");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_Perf_DevId");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorPerformance");
    cur["Type"] = std::string("NSM_ProcessorPerformance");
    cur["DeviceId"] = uint64_t(0xABCD);

    const size_t before = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorPerformance",
                             path);

    // createProcessorPerformance adds 4 sensors to roundRobin/longRunning:
    // NsmAccumGpuUtilTime, NsmPciGroup5, NsmProcessorThrottleReason,
    // NsmProcessorThrottleDuration
    EXPECT_GT(gpu->roundRobinSensors.size() + gpu->longRunningSensors.size(),
              before);
}

// ============================================================================
// createProcessorPerformance: DeviceId absent (FALSE branch at line 566)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ProcessorPerf_NoDeviceId)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_Perf_NoDevId");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_Perf_NoDevId");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorPerformance");
    cur["Type"] = std::string("NSM_ProcessorPerformance");
    // No DeviceId - false branch

    const size_t before = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorPerformance",
                             path);

    EXPECT_GT(gpu->roundRobinSensors.size() + gpu->longRunningSensors.size(),
              before);
}

// ============================================================================
// createWorkloadPowerProfile: ProfileIdMap present (TRUE branch at line 782)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, WorkloadPowerProfile_WithProfileIdMap)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_WPP_ProfMap");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_WPP_ProfMap");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".WorkloadPowerProfile");
    cur["Type"] = std::string("NSM_WorkloadPowerProfile");
    cur["ProfileIdMap"] = std::vector<std::string>{"Profile_0", "Profile_1",
                                                   "Profile_2"};

    const size_t staticBefore = gpu->staticSensors.size();
    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".WorkloadPowerProfile",
                             path);

    // Creates: NsmWorkLoadProfileEnum (static), NsmWorkLoadProfileStatus (RR),
    // NsmWorkloadPowerProfileCollection (static),
    // NsmWorkloadPowerProfilePageCollection (static),
    // NsmWorkloadPowerProfilePage (RR)
    EXPECT_GT(gpu->staticSensors.size(), staticBefore);
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// createWorkloadPowerProfile: ProfileIdMap absent (FALSE branch at line 782)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, WorkloadPowerProfile_NoProfileIdMap)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_WPP_NoProfMap");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_WPP_NoProfMap");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".WorkloadPowerProfile");
    cur["Type"] = std::string("NSM_WorkloadPowerProfile");
    // No ProfileIdMap - false branch

    const size_t staticBefore = gpu->staticSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".WorkloadPowerProfile",
                             path);

    EXPECT_GT(gpu->staticSensors.size(), staticBefore);
}

// ============================================================================
// createWorkloadPowerProfile: empty ProfileIdMap
// ============================================================================
TEST_F(NsmProcessorFactory4Test, WorkloadPowerProfile_EmptyProfileIdMap)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_WPP_EmptyMap");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_WPP_EmptyMap");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".WorkloadPowerProfile");
    cur["Type"] = std::string("NSM_WorkloadPowerProfile");
    cur["ProfileIdMap"] = std::vector<std::string>{};

    const size_t staticBefore = gpu->staticSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".WorkloadPowerProfile",
                             path);

    EXPECT_GT(gpu->staticSensors.size(), staticBefore);
}

// ============================================================================
// createPowerCap: CompositeNumericSensors present (TRUE branch at line 602)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, PowerCap_WithCompositeNumericSensors)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_PCap_Comp");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_PCap_Comp");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PowerCap");
    cur["Type"] = std::string("NSM_PowerCap");
    cur["CompositeNumericSensors"] = std::vector<std::string>{"sensor_a",
                                                              "sensor_b"};

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".PowerCap", path);

    // Creates: NsmPowerCap (RR), NsmDefaultPowerCap (static),
    // NsmMaxPowerCap (static), NsmMinPowerCap (static)
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// createPowerCap: CompositeNumericSensors absent (FALSE branch at line 602)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, PowerCap_NoCompositeNumericSensors)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_PCap_NoComp");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_PCap_NoComp");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PowerCap");
    cur["Type"] = std::string("NSM_PowerCap");
    // No CompositeNumericSensors - false branch

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".PowerCap", path);

    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// createReconfigPermissions: Features present with values (TRUE branch + loop)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ReconfigPermissions_WithFeatures)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_Reconfig_Feat");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_Reconfig_Feat");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    cur["Type"] = std::string("NSM_ReconfigPermissions");
    cur["Features"] = std::vector<std::string>{"CCMode", "ECCEnable"};

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);

    // Each feature creates one NsmReconfigPermissions sensor
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// createReconfigPermissions: Features absent (FALSE branch at line 670)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ReconfigPermissions_NoFeatures)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_Reconfig_NoFeat");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_Reconfig_NoFeat");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    cur["Type"] = std::string("NSM_ReconfigPermissions");
    // No Features - false branch

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);

    // No sensors added when Features is absent
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// createReconfigPermissions: empty Features list
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ReconfigPermissions_EmptyFeatures)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_Reconfig_EmptyFeat");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_Reconfig_EmptyFeat");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    cur["Type"] = std::string("NSM_ReconfigPermissions");
    cur["Features"] = std::vector<std::string>{};

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);

    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// createPCIe: DeviceId and Count present with Count > 0 (loop body coverage)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, PCIe_WithDeviceIdAndCount)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_PCIe_DevIdCnt");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_PCIe_DevIdCnt");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PCIe");
    cur["Type"] = std::string("NSM_PCIe");
    cur["DeviceId"] = uint64_t(0x5678);
    cur["Count"] = uint64_t(2);

    const size_t rrBefore = gpu->roundRobinSensors.size();
    const size_t devBefore = gpu->deviceSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".PCIe", path);

    // NsmPCIeLinkSpeed (RR) + per-port sensors (Count * 3 groups + port intf)
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
    EXPECT_GT(gpu->deviceSensors.size(), devBefore);
}

// ============================================================================
// createPCIe: DeviceId absent, Count absent (FALSE branches)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, PCIe_NoDeviceIdNoCount)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_PCIe_NoDevIdCnt");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_PCIe_NoDevIdCnt");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PCIe");
    cur["Type"] = std::string("NSM_PCIe");
    // No DeviceId, no Count

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".PCIe", path);

    // Only NsmPCIeLinkSpeed added (no ports since Count defaults to 0)
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// createPCIe: Count = 0 (loop not entered)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, PCIe_CountZero)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_PCIe_Cnt0");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_PCIe_Cnt0");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PCIe");
    cur["Type"] = std::string("NSM_PCIe");
    cur["DeviceId"] = uint64_t(0x1111);
    cur["Count"] = uint64_t(0);

    createNsmProcessorSensor(mockManager, baseIntf + ".PCIe", path);

    // NsmPCIeLinkSpeed added to RR, no port sensors since Count=0
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

// ============================================================================
// NSM_Processor type: exercises main branch with all base properties
// NOTE: When interface == baseIntf, base and current properties are the same
// map. Use a single propertyMap() call to avoid clearing previously set values.
// ============================================================================
TEST_F(NsmProcessorFactory4Test, Processor_WithDeviceUuid)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_Proc_DevUuid");
    auto& props = utils::MockDbusAsync::propertyMap(path, baseIntf);
    props["Name"] = std::string("GPU_F4_Proc_DevUuid");
    props["UUID"] = gpuUuid;
    props["InventoryObjPath"] = path;
    props["DEVICE_UUID"] = std::string("device-uuid-1234-5678");
    props["Type"] = std::string("NSM_Processor");

    createNsmProcessorSensor(mockManager, baseIntf, path);

    // NSM_Processor creates many sensors including NsmUuidIntf with deviceUuid
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor type: no DEVICE_UUID (FALSE branch at line 3496)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, Processor_NoDeviceUuid)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_Proc_NoDevUuid");
    auto& props = utils::MockDbusAsync::propertyMap(path, baseIntf);
    props["Name"] = std::string("GPU_F4_Proc_NoDevUuid");
    props["UUID"] = gpuUuid;
    props["InventoryObjPath"] = path;
    // No DEVICE_UUID
    props["Type"] = std::string("NSM_Processor");

    createNsmProcessorSensor(mockManager, baseIntf, path);

    // NsmUuidIntf still created with empty deviceUuid
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: PowerSmoothingSupported = true
// ============================================================================
TEST_F(NsmProcessorFactory4Test, Attributes_PowerSmoothingTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_Attr_PwrSmooth");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_Attr_PwrSmooth");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["PowerSmoothingSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);

    // PowerSmoothing creates multiple sensors
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor: inventoryObjPath with slash (line 3525 true branch)
// Uses a different inventoryObjPath than the test path to exercise the
// substring logic.
// ============================================================================
TEST_F(NsmProcessorFactory4Test, Processor_InvPathWithSlash)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_Slash");
    const std::string invPath = "/xyz/openbmc_project/inventory/GPU_F4_Sub";
    auto& props = utils::MockDbusAsync::propertyMap(path, baseIntf);
    props["Name"] = std::string("GPU_F4_Slash");
    props["UUID"] = gpuUuid;
    props["InventoryObjPath"] = invPath;
    props["Type"] = std::string("NSM_Processor");

    createNsmProcessorSensor(mockManager, baseIntf, path);

    // Should create sensors with basePath from inventoryObjPath
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// ReconfigPermissions: single Feature entry for minimal loop iteration
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ReconfigPermissions_SingleFeature)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_Reconfig_1Feat");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_Reconfig_1Feat");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    cur["Type"] = std::string("NSM_ReconfigPermissions");
    cur["Features"] = std::vector<std::string>{"CCMode"};

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);

    // One feature = one NsmReconfigPermissions sensor
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore + 1);
}

// ============================================================================
// ReconfigPermissions: multiple Features with duplicates
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ReconfigPermissions_DuplicateFeatures)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F4_Reconfig_DupFeat");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F4_Reconfig_DupFeat");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = path;

    auto& cur = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    cur["Type"] = std::string("NSM_ReconfigPermissions");
    // Duplicate features - map deduplicates by FeatureType
    cur["Features"] = std::vector<std::string>{"CCMode", "ECCEnable", "CCMode"};

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);

    // Duplicates are collapsed by the map: CCMode + ECCEnable = 2 unique
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore + 2);
}
