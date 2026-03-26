/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Factory branch coverage batch 8 for createNsmProcessorSensor:
 *
 * Targets additional branch permutations not covered in batches 1-7:
 *   - NSM_ReconfigPermissions with various Feature combos
 *     (InSystemTest, FusingMode, BAR0Firewall, CCDevMode, TGPCurrentLimit,
 *      TGPRatedLimit, TGPMaxLimit, TGPMinLimit, ClockLimit, NVLinkDisable,
 *      PCIeVFConfiguration, RowRemappingAllowed, RowRemappingFeature,
 *      HBMFrequencyChange, HULKLicenseUpdate, ForceTestCoupling,
 *      BAR0TypeConfig, EDPpScalingFactor, PowerSmoothingPrivilegeLevel1,
 *      PowerSmoothingPrivilegeLevel2, EGMMode, InfoROMFileSystemRecreate)
 *   - NSM_PCIe with large DeviceId values and Count=3
 *   - NSM_PowerCap with empty CompositeNumericSensors vector
 *   - NSM_ProcessorPerformance with large DeviceId
 *   - NSM_WorkloadPowerProfile with empty ProfileIdMap vector
 *   - NSM_Processor with empty DEVICE_UUID string
 *   - NSM_Processor_Attributes with all supported TRUE (full combo)
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

struct NsmProcessorFactory8Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Processor";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:88";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorFactory8Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorFactory8Test()
    {
        cleanupDeviceSensors(devices);
    }

    void setupBase(const std::string& path, const std::string& invPath)
    {
        auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
        base["Name"] = std::string("GPU_F8");
        base["UUID"] = gpuUuid;
        base["InventoryObjPath"] = invPath;
    }
};

// ============================================================================
// NSM_ReconfigPermissions: additional Feature combinations
// ============================================================================

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_InSystemTest)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_IST");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"InSystemTest"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_FusingMode)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_FM");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"FusingMode"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_BAR0Firewall)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_B0FW");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"BAR0Firewall"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_CCDevMode)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_CCDM");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"CCDevMode"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_TGPLimits)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_TGP");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{
        "TGPCurrentLimit", "TGPRatedLimit", "TGPMaxLimit", "TGPMinLimit"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 4u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_ClockLimitNVLink)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_CLNV");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"ClockLimit",
                                                  "NVLinkDisable"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 2u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_PCIeVFRowRemap)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_PCVF");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{
        "PCIeVFConfiguration", "RowRemappingAllowed", "RowRemappingFeature"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 3u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_HBMHULKForce)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_HBM");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{
        "HBMFrequencyChange", "HULKLicenseUpdate", "ForceTestCoupling"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 3u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_BAR0TypeEDPpPwrSmooth)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_B0TE");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{
        "BAR0TypeConfig", "EDPpScalingFactor", "PowerSmoothingPrivilegeLevel1",
        "PowerSmoothingPrivilegeLevel2"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 4u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_EGMModeInfoROM)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_EGIR");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"EGMMode",
                                                  "InfoROMFileSystemRecreate"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 2u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_AllFeatures)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_AllFeat");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] =
        std::vector<std::string>{"InSystemTest",
                                 "FusingMode",
                                 "CCMode",
                                 "BAR0Firewall",
                                 "CCDevMode",
                                 "TGPCurrentLimit",
                                 "TGPRatedLimit",
                                 "TGPMaxLimit",
                                 "TGPMinLimit",
                                 "ClockLimit",
                                 "NVLinkDisable",
                                 "ECCEnable",
                                 "PCIeVFConfiguration",
                                 "RowRemappingAllowed",
                                 "RowRemappingFeature",
                                 "HBMFrequencyChange",
                                 "HULKLicenseUpdate",
                                 "ForceTestCoupling",
                                 "BAR0TypeConfig",
                                 "EDPpScalingFactor",
                                 "PowerSmoothingPrivilegeLevel1",
                                 "PowerSmoothingPrivilegeLevel2",
                                 "EGMMode",
                                 "InfoROMFileSystemRecreate"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 24u);
}

// ============================================================================
// NSM_PCIe: large DeviceId and Count=3
// ============================================================================

TEST_F(NsmProcessorFactory8Test, PCIe_LargeDeviceId)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_PCIeLg");
    setupBase(path, std::string(path));

    auto& pcie = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PCIe");
    pcie["Type"] = std::string("NSM_PCIe");
    pcie["DeviceId"] = uint64_t{255};
    pcie["Count"] = uint64_t{1};

    createNsmProcessorSensor(mockManager, baseIntf + ".PCIe", path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, PCIe_Count3)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_PCIe3");
    setupBase(path, std::string(path));

    auto& pcie = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PCIe");
    pcie["Type"] = std::string("NSM_PCIe");
    pcie["DeviceId"] = uint64_t{5};
    pcie["Count"] = uint64_t{3};

    createNsmProcessorSensor(mockManager, baseIntf + ".PCIe", path);
    // 1 PCIeLinkSpeed + 3*3 port sensors (group2,3,4 per port) = 10 RR
    EXPECT_GE(gpu->roundRobinSensors.size(), 4u);
    EXPECT_GE(gpu->deviceSensors.size(), 3u);
}

TEST_F(NsmProcessorFactory8Test, PCIe_DeviceId0Count1)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_PCIeD0C1");
    setupBase(path, std::string(path));

    auto& pcie = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PCIe");
    pcie["Type"] = std::string("NSM_PCIe");
    pcie["DeviceId"] = uint64_t{0};
    pcie["Count"] = uint64_t{1};

    createNsmProcessorSensor(mockManager, baseIntf + ".PCIe", path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_PowerCap: empty CompositeNumericSensors vector
// ============================================================================

TEST_F(NsmProcessorFactory8Test, PowerCap_EmptyCompositeNumericSensors)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_PwrCapE");
    setupBase(path, std::string(path));

    auto& pwrcap = utils::MockDbusAsync::propertyMap(path,
                                                     baseIntf + ".PowerCap");
    pwrcap["Type"] = std::string("NSM_PowerCap");
    pwrcap["CompositeNumericSensors"] = std::vector<std::string>{};

    createNsmProcessorSensor(mockManager, baseIntf + ".PowerCap", path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, PowerCap_ThreeCompositeNumericSensors)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_PwrCap3");
    setupBase(path, std::string(path));

    auto& pwrcap = utils::MockDbusAsync::propertyMap(path,
                                                     baseIntf + ".PowerCap");
    pwrcap["Type"] = std::string("NSM_PowerCap");
    pwrcap["CompositeNumericSensors"] =
        std::vector<std::string>{"sensorA", "sensorB", "sensorC"};

    createNsmProcessorSensor(mockManager, baseIntf + ".PowerCap", path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// NSM_ProcessorPerformance: large DeviceId
// ============================================================================

TEST_F(NsmProcessorFactory8Test, ProcessorPerformance_LargeDeviceId)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_PerfLg");
    setupBase(path, std::string(path));

    auto& perf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorPerformance");
    perf["Type"] = std::string("NSM_ProcessorPerformance");
    perf["DeviceId"] = uint64_t{127};

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorPerformance",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, ProcessorPerformance_DeviceId0)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_PerfD0");
    setupBase(path, std::string(path));

    auto& perf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorPerformance");
    perf["Type"] = std::string("NSM_ProcessorPerformance");
    perf["DeviceId"] = uint64_t{0};

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorPerformance",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_WorkloadPowerProfile: empty ProfileIdMap vector
// ============================================================================

TEST_F(NsmProcessorFactory8Test, WorkloadPowerProfile_EmptyProfileIdMap)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_WPPE");
    setupBase(path, std::string(path));

    auto& wpp = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".WorkloadPowerProfile");
    wpp["Type"] = std::string("NSM_WorkloadPowerProfile");
    wpp["ProfileIdMap"] = std::vector<std::string>{};

    createNsmProcessorSensor(mockManager, baseIntf + ".WorkloadPowerProfile",
                             path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, WorkloadPowerProfile_ThreeProfiles)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_WPP3");
    setupBase(path, std::string(path));

    auto& wpp = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".WorkloadPowerProfile");
    wpp["Type"] = std::string("NSM_WorkloadPowerProfile");
    wpp["ProfileIdMap"] = std::vector<std::string>{"ProfileA", "ProfileB",
                                                   "ProfileC"};

    createNsmProcessorSensor(mockManager, baseIntf + ".WorkloadPowerProfile",
                             path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor: empty DEVICE_UUID string
// ============================================================================

TEST_F(NsmProcessorFactory8Test, Processor_EmptyDeviceUUID)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_EUUID");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F8");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = std::string(path);
    base["DEVICE_UUID"] = std::string("");
    base["Type"] = std::string("NSM_Processor");

    createNsmProcessorSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: all supported TRUE at once (full coverage)
// ============================================================================

TEST_F(NsmProcessorFactory8Test, Attributes_AllSupportedTrue)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_AllTrue");
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
    attr["LocationCode"] = std::string("GPU_Bay_8");

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: PowerSmoothing false, rest mixed
// ============================================================================

TEST_F(NsmProcessorFactory8Test, Attributes_MixedFlagsCombination)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_Mixed");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MIGModeSupported"] = false;
    attr["PortDisableFutureSupported"] = true;
    attr["ECCModeSupported"] = false;
    attr["EDPpScalingFactorSupported"] = true;
    attr["PowerSmoothingSupported"] = false;
    attr["CpuOperatingConfigSupported"] = true;
    attr["MemCapacityUtilSupported"] = false;
    attr["TotalNvLinksCountSupported"] = true;
    attr["EGMModeSupported"] = false;
    attr["MNNVLTopologySupported"] = true;
    attr["MctpNsmOperationalStatusSupported"] = false;
    attr["GPUBasePowerLimitSupported"] = true;
    attr["GPUCopyCPUPowerLimitSupported"] = false;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, Attributes_OppositeFlags)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_OppFlags");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MIGModeSupported"] = true;
    attr["PortDisableFutureSupported"] = false;
    attr["ECCModeSupported"] = true;
    attr["EDPpScalingFactorSupported"] = false;
    attr["PowerSmoothingSupported"] = true;
    attr["CpuOperatingConfigSupported"] = false;
    attr["MemCapacityUtilSupported"] = true;
    attr["TotalNvLinksCountSupported"] = false;
    attr["EGMModeSupported"] = true;
    attr["MNNVLTopologySupported"] = false;
    attr["MctpNsmOperationalStatusSupported"] = true;
    attr["GPUBasePowerLimitSupported"] = false;
    attr["GPUCopyCPUPowerLimitSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor: deep inventory path (multiple slashes)
// ============================================================================

TEST_F(NsmProcessorFactory8Test, Processor_DeepInventoryPath)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_Deep");
    const std::string invPath =
        "/xyz/openbmc_project/inventory/system/chassis/gpu/GPU8";
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F8");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath;
    base["Type"] = std::string("NSM_Processor");

    createNsmProcessorSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// NSM_ReconfigPermissions: single uncommon features
// ============================================================================

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleTGPCurrent)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_TGPC");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"TGPCurrentLimit"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleNVLinkDisable)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_NVLnk");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"NVLinkDisable"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SinglePCIeVFConfig)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_VFCfg");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"PCIeVFConfiguration"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleRowRemapAllowed)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_RRA");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"RowRemappingAllowed"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleRowRemapFeature)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_RRF");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"RowRemappingFeature"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleHBMFreqChange)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_HBMF");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"HBMFrequencyChange"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleHULKLicense)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_HULK");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"HULKLicenseUpdate"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleForceTestCoupling)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_FTC");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"ForceTestCoupling"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleBAR0TypeConfig)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_B0TC");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"BAR0TypeConfig"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleEDPpScalingFactor)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_EDPF");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"EDPpScalingFactor"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SinglePwrSmoothL1)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_PSL1");
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

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SinglePwrSmoothL2)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_PSL2");
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

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleEGMMode)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_EGMM");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"EGMMode"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleInfoROMRecreate)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_IROM");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"InfoROMFileSystemRecreate"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleTGPRatedLimit)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_TGPR");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"TGPRatedLimit"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleTGPMaxLimit)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_TGPMx");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"TGPMaxLimit"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleTGPMinLimit)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_TGPMn");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"TGPMinLimit"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleClockLimit)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_CkLm");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"ClockLimit"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

// ============================================================================
// NSM_Processor: Associations subinterface
// ============================================================================

TEST_F(NsmProcessorFactory8Test, Processor_WithAssociations)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_Assoc");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F8");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = std::string(path);
    base["Type"] = std::string("NSM_Processor");

    // Set up associations subinterface
    auto& assocProps =
        utils::MockDbusAsync::propertyMap(path, baseIntf + ".Associations");
    assocProps["Associations"] =
        std::vector<std::tuple<std::string, std::string, std::string>>{
            {"parent", "child", "/xyz/openbmc_project/inventory/gpu/GPU8"}};

    createNsmProcessorSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor_Attributes: LocationType without LocationCode, different type
// ============================================================================

TEST_F(NsmProcessorFactory8Test, Attributes_LocationTypeSlot)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F8_LocSlot");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Slot");

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}
