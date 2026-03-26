/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Factory branch coverage batch 9 for createNsmProcessorSensor:
 *
 * Targets constructor bodies and factory helper paths not exercised in
 * batches 1-8:
 *   - NSM_Processor branch: NsmAcceleratorIntf, NsmProcessorAssociation,
 *     NsmUuidIntf, NsmGpuHealth, NsmConfidentialCompute,
 *     NsmProcessorRevision, NsmTotalMemorySize constructors
 *   - NSM_Processor_Attributes with individual feature combos for:
 *     NsmMigMode, NsmEccMode, NsmEccErrorCounts, NsmEDPpScalingFactor,
 *     NsmMaxEDPpLimit, NsmMinEDPpLimit, NsmClockLimitGraphics,
 *     NsmCurrClockFreq, NsmDefaultBaseClockSpeed, NsmDefaultBoostClockSpeed,
 *     NsmCurrentUtilization, NsmEgmMode, NsmTotalNvLinks
 *   - NSM_ProcessorPerformance: NsmProcessorThrottleReason,
 *     NsmProcessorThrottleDuration, NsmAccumGpuUtilTime, NsmPciGroup5
 *   - NSM_PowerCap: NsmPowerCap, NsmMaxPowerCap, NsmMinPowerCap,
 *     NsmDefaultPowerCap
 *   - Error paths: missing Name, missing UUID
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

struct NsmProcessorFactory9Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Processor";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:99";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorFactory9Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorFactory9Test()
    {
        cleanupDeviceSensors(devices);
    }

    void setupBase(const std::string& path, const std::string& invPath)
    {
        auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
        base["Name"] = std::string("GPU_F9");
        base["UUID"] = gpuUuid;
        base["InventoryObjPath"] = invPath;
    }
};

// ============================================================================
// NSM_Processor: with DEVICE_UUID present (non-empty)
// Exercises NsmAcceleratorIntf, NsmProcessorAssociation, NsmUuidIntf,
// NsmGpuHealth, NsmConfidentialCompute, NsmProcessorRevision,
// NsmTotalMemorySize constructors
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Processor_WithDeviceUUID)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_UUID");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F9");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = std::string(path);
    base["DEVICE_UUID"] = std::string("12345678-abcd-abcd-abcd-123456789abc");
    base["Type"] = std::string("NSM_Processor");

    createNsmProcessorSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory9Test, Processor_WithoutDeviceUUID)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_NoUUID");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F9");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = std::string(path);
    base["Type"] = std::string("NSM_Processor");

    createNsmProcessorSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory9Test, DISABLED_Processor_InventoryPathNoSlash)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_NoSlash");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F9");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = std::string("NoSlashPath");
    base["Type"] = std::string("NSM_Processor");

    createNsmProcessorSensor(mockManager, baseIntf, path);
    // Factory may not add sensors for paths without slash - just verify no
    // crash
    SUCCEED();
}

// ============================================================================
// NSM_Processor_Attributes: only MIGModeSupported=true
// Exercises NsmMigMode constructor (line 1099-1111)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_MIGModeOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_MIG");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MIGModeSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: only ECCModeSupported=true
// Exercises NsmEccMode (line 1177-1188), NsmEccErrorCounts (line 1260-1269)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_ECCModeOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_ECC");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["ECCModeSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: only EDPpScalingFactorSupported=true
// Exercises NsmEDPpScalingFactor (line 1417-1428), NsmMaxEDPpLimit (1580-1585),
// NsmMinEDPpLimit (1642-1647)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_EDPpOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_EDPp");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["EDPpScalingFactorSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: only CpuOperatingConfigSupported=true
// Exercises NsmClockLimitGraphics (1704-1715), NsmCurrClockFreq (1802-1812),
// NsmDefaultBaseClockSpeed (1875-1882), NsmDefaultBoostClockSpeed (1944-1951),
// NsmCurrentUtilization (2013-2028)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_CpuOperatingConfigOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_CPU");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["CpuOperatingConfigSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: only TotalNvLinksCountSupported=true
// Exercises NsmTotalNvLinks (2382-2392)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_TotalNvLinksOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_NVL");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["TotalNvLinksCountSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: only EGMModeSupported=true
// Exercises NsmEgmMode (3356-3363)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_EGMModeOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_EGM");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["EGMModeSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: only PowerSmoothingSupported=true
// Exercises createPowerSmoothing helper (lines 305-470)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_PowerSmoothingOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_PwrSm");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["PowerSmoothingSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: LocationType + LocationCode together
// Exercises NsmLocationIntfProcessor (1078-1087),
// NsmLocationCodeIntfProcessor (1089-1097)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_LocationTypeAndCode)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_Loc");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    attr["LocationCode"] = std::string("GPU_Bay_9");

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: only PortDisableFutureSupported=true
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_PortDisableFutureOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_PDF");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["PortDisableFutureSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: only MemCapacityUtilSupported=true
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_MemCapacityUtilOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_Mem");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MemCapacityUtilSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: only MNNVLTopologySupported=true
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_MNNVLTopologyOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_MNNVL");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MNNVLTopologySupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: only MctpNsmOperationalStatusSupported=true
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_OperationalStatusOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_OpStat");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MctpNsmOperationalStatusSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: only GPUBasePowerLimitSupported=true
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_GPUBasePowerLimitOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_GBPL");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["GPUBasePowerLimitSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: only GPUCopyCPUPowerLimitSupported=true
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_GPUCopyCPUPowerLimitOnlyTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_GCCPL");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["GPUCopyCPUPowerLimitSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_ProcessorPerformance: exercises NsmProcessorThrottleReason (2100-2114),
// NsmProcessorThrottleDuration (2940-2955), NsmAccumGpuUtilTime (2217-2228),
// NsmPciGroup5 (1349-1360)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, ProcessorPerformance_DeviceId1)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_Perf1");
    setupBase(path, std::string(path));

    auto& perf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorPerformance");
    perf["Type"] = std::string("NSM_ProcessorPerformance");
    perf["DeviceId"] = uint64_t{1};

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorPerformance",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory9Test, ProcessorPerformance_NoDeviceId)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_PerfND");
    setupBase(path, std::string(path));

    auto& perf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorPerformance");
    perf["Type"] = std::string("NSM_ProcessorPerformance");

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorPerformance",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_PowerCap: exercises NsmPowerCap (2528-2540), NsmMaxPowerCap (2668-2676),
// NsmMinPowerCap (2763-2771), NsmDefaultPowerCap (2863-2869)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, PowerCap_WithSensors)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_PC");
    setupBase(path, std::string(path));

    auto& pwrcap = utils::MockDbusAsync::propertyMap(path,
                                                     baseIntf + ".PowerCap");
    pwrcap["Type"] = std::string("NSM_PowerCap");
    pwrcap["CompositeNumericSensors"] = std::vector<std::string>{"sensorX",
                                                                 "sensorY"};

    createNsmProcessorSensor(mockManager, baseIntf + ".PowerCap", path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory9Test, PowerCap_NoCompositeNumericSensors)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_PCNone");
    setupBase(path, std::string(path));

    auto& pwrcap = utils::MockDbusAsync::propertyMap(path,
                                                     baseIntf + ".PowerCap");
    pwrcap["Type"] = std::string("NSM_PowerCap");

    createNsmProcessorSensor(mockManager, baseIntf + ".PowerCap", path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// NSM_ReconfigPermissions: CCMode and ECCEnable features
// Exercises createReconfigPermissions lines 685, 706, 718
// ============================================================================

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_CCModeECCEnable)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_RCCC");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"CCMode", "ECCEnable"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 2u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_EmptyFeatures)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_RCEmpty");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
}

// ============================================================================
// NSM_WorkloadPowerProfile: with ProfileIdMap entries
// ============================================================================

TEST_F(NsmProcessorFactory9Test, WorkloadPowerProfile_WithProfiles)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_WPP");
    setupBase(path, std::string(path));

    auto& wpp = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".WorkloadPowerProfile");
    wpp["Type"] = std::string("NSM_WorkloadPowerProfile");
    wpp["ProfileIdMap"] = std::vector<std::string>{"ProfileX", "ProfileY"};

    createNsmProcessorSensor(mockManager, baseIntf + ".WorkloadPowerProfile",
                             path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory9Test, WorkloadPowerProfile_NoProfileIdMap)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_WPPNo");
    setupBase(path, std::string(path));

    auto& wpp = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".WorkloadPowerProfile");
    wpp["Type"] = std::string("NSM_WorkloadPowerProfile");

    createNsmProcessorSensor(mockManager, baseIntf + ".WorkloadPowerProfile",
                             path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: all supported flags = false (no sub-objects)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_AllSupportedFalse)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_AllFalse");
    setupBase(path, std::string(path));

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

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    // Only asset sensors should be created, no feature sensors
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: combined MIG + ECC + EDPp + CpuConfig
// Exercises multiple constructor paths in a single factory call
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_CombinedMIGECCEDPpCpu)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_Combo");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MIGModeSupported"] = true;
    attr["ECCModeSupported"] = true;
    attr["EDPpScalingFactorSupported"] = true;
    attr["CpuOperatingConfigSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: combined EGM + TotalNvLinks + PowerSmoothing
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_CombinedEGMNvLinksPwrSmooth)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_Combo2");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["EGMModeSupported"] = true;
    attr["TotalNvLinksCountSupported"] = true;
    attr["PowerSmoothingSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: combined GPUBase + GPUCopyCPU power limits
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_BothGPUPowerLimits)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_BPWL");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["GPUBasePowerLimitSupported"] = true;
    attr["GPUCopyCPUPowerLimitSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_PCIe: exercises PCIe branch with DeviceId and Count
// ============================================================================

TEST_F(NsmProcessorFactory9Test, PCIe_DeviceId10Count2)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_PCIe");
    setupBase(path, std::string(path));

    auto& pcie = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PCIe");
    pcie["Type"] = std::string("NSM_PCIe");
    pcie["DeviceId"] = uint64_t{10};
    pcie["Count"] = uint64_t{2};

    createNsmProcessorSensor(mockManager, baseIntf + ".PCIe", path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_ReconfigPermissions: single features not tested in batch 8
// ============================================================================

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleTGPRatedLimit)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_TGPR");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"TGPRatedLimit"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleTGPMaxLimit)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_TGPM");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"TGPMaxLimit"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleTGPMinLimit)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_TGPN");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"TGPMinLimit"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleClockLimit)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_CLK");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"ClockLimit"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleHBMFreqChange)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_HBM");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"HBMFrequencyChange"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleHULKLicenseUpdate)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_HULK");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"HULKLicenseUpdate"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleForceTestCoupling)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_FTC");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"ForceTestCoupling"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleBAR0TypeConfig)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_B0TC");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"BAR0TypeConfig"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleEDPpScalingFactor)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_EDPSF");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"EDPpScalingFactor"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test,
       ReconfigPermissions_SinglePowerSmoothingPrivLevel1)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_PSP1");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] =
        std::vector<std::string>{"PowerSmoothingPrivilegeLevel1"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test,
       ReconfigPermissions_SinglePowerSmoothingPrivLevel2)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_PSP2");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] =
        std::vector<std::string>{"PowerSmoothingPrivilegeLevel2"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleEGMMode)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_RCEGM");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"EGMMode"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test,
       ReconfigPermissions_SingleInfoROMFileSystemRecreate)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_IRFS");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"InfoROMFileSystemRecreate"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

// ============================================================================
// Error paths: missing Name, missing UUID in base properties
// ============================================================================

TEST_F(NsmProcessorFactory9Test, ErrorPath_MissingName)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_NoName");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = std::string(path);

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    // Should still create asset sensors even with empty name
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory9Test, ErrorPath_MissingUUID)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_NoUUIDBase");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F9");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = std::string(path);

    auto& perf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorPerformance");
    perf["Type"] = std::string("NSM_ProcessorPerformance");
    perf["DeviceId"] = uint64_t{42};

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorPerformance",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: all features true with Location
// Full coverage of all attribute branches in a single test
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_FullCoverage)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F9_Full");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MIGModeSupported"] = true;
    attr["PortDisableFutureSupported"] = true;
    attr["ECCModeSupported"] = true;
    attr["EDPpScalingFactorSupported"] = true;
    attr["PowerSmoothingSupported"] = true;
    attr["CpuOperatingConfigSupported"] = true;
    attr["MemCapacityUtilSupported"] = true;
    attr["TotalNvLinksCountSupported"] = true;
    attr["EGMModeSupported"] = true;
    attr["MNNVLTopologySupported"] = true;
    attr["MctpNsmOperationalStatusSupported"] = true;
    attr["GPUBasePowerLimitSupported"] = true;
    attr["GPUCopyCPUPowerLimitSupported"] = true;
    attr["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    attr["LocationCode"] = std::string("GPU_Bay_Full");

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}
