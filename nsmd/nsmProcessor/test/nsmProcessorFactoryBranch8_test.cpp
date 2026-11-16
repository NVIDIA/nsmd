/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage batch 8 for the processor feature creators
 * (nsm::features::create*Feature in nsmd/nsmProcessor/nsmProcessor.cpp):
 *
 * Targets additional branch permutations not covered in batches 1-7:
 *   - ReconfigPermissions with various knob combos
 *     (InSystemTest, FusingMode, BAR0Firewall, CCDevMode, TGPCurrentLimit,
 *      TGPRatedLimit, TGPMaxLimit, TGPMinLimit, ClockLimit, NVLinkDisable,
 *      PCIeVFConfiguration, RowRemappingAllowed, RowRemappingFeature,
 *      HBMFrequencyChange, HULKLicenseUpdate, ForceTestCoupling,
 *      BAR0TypeConfig, EDPpScalingFactor, PowerSmoothingPrivilegeLevel1,
 *      PowerSmoothingPrivilegeLevel2, EGMMode, InfoROMFileSystemRecreate)
 *   - Processor PCIe with large PCIeDeviceId values and PCIePortCount=3
 *   - ProcessorPerformance with large DeviceId
 *   - Processor association params
 *   - ProcessorAttributes with all supported TRUE (full combo)
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

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmReconfigPermissions.hpp"

#include <algorithm>

using namespace nsm;

struct NsmProcessorFactory8Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
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

    /** @brief Run a processor feature creator for the GPU `name` on the mock
     *  NsmDevice; the inventory object is accelerator/<name>
     */
    requester::Coroutine runFeature(nsm::features::CreatorFn create,
                                    const std::string& name,
                                    dbus::PropertyMap params = {})
    {
        nsm::features::DeviceContext dev;
        dev.name = name;
        dev.kind = "gpu";
        dev.staticUuid = gpuUuid;
        dev.device = gpu;
        params["Name"] = name;
        return create(mockManager, dev, params);
    }

    /** @brief Number of sensors of type T in the queue */
    template <typename T, typename Queue>
    static size_t countOf(const Queue& queue)
    {
        return std::count_if(queue.begin(), queue.end(), [](const auto& s) {
            return std::dynamic_pointer_cast<T>(s) != nullptr;
        });
    }
};

// ============================================================================
// ReconfigPermissions: additional Feature combinations
// ============================================================================

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_InSystemTest)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_IST",
        {{"AvailableKnobs", std::vector<std::string>{"InSystemTest"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_FusingMode)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_FM",
        {{"AvailableKnobs", std::vector<std::string>{"FusingMode"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_BAR0Firewall)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_B0FW",
        {{"AvailableKnobs", std::vector<std::string>{"BAR0Firewall"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_CCDevMode)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_CCDM",
        {{"AvailableKnobs", std::vector<std::string>{"CCDevMode"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_TGPLimits)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_TGP",
        {{"AvailableKnobs",
          std::vector<std::string>{"TGPCurrentLimit", "TGPRatedLimit",
                                   "TGPMaxLimit", "TGPMinLimit"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 4u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_ClockLimitNVLink)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_CLNV",
        {{"AvailableKnobs",
          std::vector<std::string>{"ClockLimit", "NVLinkDisable"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 2u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_PCIeVFRowRemap)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_PCVF",
        {{"AvailableKnobs",
          std::vector<std::string>{"PCIeVFConfiguration", "RowRemappingAllowed",
                                   "RowRemappingFeature"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 3u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_HBMHULKForce)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_HBM",
        {{"AvailableKnobs",
          std::vector<std::string>{"HBMFrequencyChange", "HULKLicenseUpdate",
                                   "ForceTestCoupling"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 3u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_BAR0TypeEDPpPwrSmooth)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_B0TE",
        {{"AvailableKnobs",
          std::vector<std::string>{"BAR0TypeConfig", "EDPpScalingFactor",
                                   "PowerSmoothingPrivilegeLevel1",
                                   "PowerSmoothingPrivilegeLevel2"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 4u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_EGMModeInfoROM)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_EGIR",
        {{"AvailableKnobs",
          std::vector<std::string>{"EGMMode", "InfoROMFileSystemRecreate"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 2u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_AllFeatures)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_AllFeat",
        {{"AvailableKnobs",
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
                                   "InfoROMFileSystemRecreate"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 24u);
}

// ============================================================================
// Processor PCIe: large DeviceId and Count=3
// ============================================================================

TEST_F(NsmProcessorFactory8Test, PCIe_LargeDeviceId)
{
    auto rc = runFeature(nsm::features::createProcessorFeature, "GPU_F8_PCIeLg",
                         {{"PCIeEnabled", true},
                          {"PCIeDeviceId", uint64_t{255}},
                          {"PCIePortCount", uint64_t{1}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, PCIe_Count3)
{
    auto rc = runFeature(nsm::features::createProcessorFeature, "GPU_F8_PCIe3",
                         {{"PCIeEnabled", true},
                          {"PCIeDeviceId", uint64_t{5}},
                          {"PCIePortCount", uint64_t{3}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // 1 PCIeLinkSpeed + 3*3 port sensors (group2,3,4 per port) = 10 RR
    EXPECT_GE(gpu->roundRobinSensors.size(), 10u);
    EXPECT_EQ(countOf<NsmPciGroup2>(gpu->roundRobinSensors), 3u);
    EXPECT_EQ(countOf<NsmPciGroup3>(gpu->roundRobinSensors), 3u);
    EXPECT_EQ(countOf<NsmPciGroup4>(gpu->roundRobinSensors), 3u);
    EXPECT_EQ(countOf<NsmPciePortIntf>(gpu->deviceSensors), 3u);
}

TEST_F(NsmProcessorFactory8Test, PCIe_DeviceId0Count1)
{
    auto rc = runFeature(nsm::features::createProcessorFeature,
                         "GPU_F8_PCIeD0C1",
                         {{"PCIeEnabled", true},
                          {"PCIeDeviceId", uint64_t{0}},
                          {"PCIePortCount", uint64_t{1}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// PowerCap
// ============================================================================

TEST_F(NsmProcessorFactory8Test, PowerCap_Created)
{
    auto rc = runFeature(nsm::features::createPowerCapFeature,
                         "GPU_F8_PwrCapE");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// ProcessorPerformance: large DeviceId
// ============================================================================

TEST_F(NsmProcessorFactory8Test, ProcessorPerformance_LargeDeviceId)
{
    auto rc = runFeature(nsm::features::createProcessorPerformanceFeature,
                         "GPU_F8_PerfLg", {{"DeviceId", uint64_t{127}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, ProcessorPerformance_DeviceId0)
{
    auto rc = runFeature(nsm::features::createProcessorPerformanceFeature,
                         "GPU_F8_PerfD0", {{"DeviceId", uint64_t{0}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// WorkloadPowerProfile
// ============================================================================

TEST_F(NsmProcessorFactory8Test, WorkloadPowerProfile_Created)
{
    auto rc = runFeature(nsm::features::createWorkloadPowerProfileFeature,
                         "GPU_F8_WPPE");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: all supported TRUE at once (full coverage)
// ============================================================================

TEST_F(NsmProcessorFactory8Test, Attributes_AllSupportedTrue)
{
    auto rc = runFeature(
        nsm::features::createProcessorAttributesFeature, "GPU_F8_AllTrue",
        {{"MIGModeSupported", true},
         {"PortDisableFutureSupported", true},
         {"ECCModeSupported", true},
         {"EDPpScalingFactorSupported", true},
         {"PowerSmoothingSupported", true},
         {"CpuOperatingConfigSupported", true},
         {"MemCapacityUtilSupported", true},
         {"TotalNvLinksCountSupported", true},
         {"EGMModeSupported", true},
         {"MNNVLTopologySupported", true},
         {"MctpNsmOperationalStatusSupported", true},
         {"GPUBasePowerLimitSupported", true},
         {"GPUCopyCPUPowerLimitSupported", true},
         {"LocationType",
          std::string(
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
              "Embedded")},
         {"LocationCode", std::string("GPU_Bay_8")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: PowerSmoothing false, rest mixed
// ============================================================================

TEST_F(NsmProcessorFactory8Test, Attributes_MixedFlagsCombination)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F8_Mixed",
                         {{"MIGModeSupported", false},
                          {"PortDisableFutureSupported", true},
                          {"ECCModeSupported", false},
                          {"EDPpScalingFactorSupported", true},
                          {"PowerSmoothingSupported", false},
                          {"CpuOperatingConfigSupported", true},
                          {"MemCapacityUtilSupported", false},
                          {"TotalNvLinksCountSupported", true},
                          {"EGMModeSupported", false},
                          {"MNNVLTopologySupported", true},
                          {"MctpNsmOperationalStatusSupported", false},
                          {"GPUBasePowerLimitSupported", true},
                          {"GPUCopyCPUPowerLimitSupported", false}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory8Test, Attributes_OppositeFlags)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F8_OppFlags",
                         {{"MIGModeSupported", true},
                          {"PortDisableFutureSupported", false},
                          {"ECCModeSupported", true},
                          {"EDPpScalingFactorSupported", false},
                          {"PowerSmoothingSupported", true},
                          {"CpuOperatingConfigSupported", false},
                          {"MemCapacityUtilSupported", true},
                          {"TotalNvLinksCountSupported", false},
                          {"EGMModeSupported", true},
                          {"MNNVLTopologySupported", false},
                          {"MctpNsmOperationalStatusSupported", true},
                          {"GPUBasePowerLimitSupported", false},
                          {"GPUCopyCPUPowerLimitSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// ReconfigPermissions: single uncommon features
// ============================================================================

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleTGPCurrent)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_TGPC",
        {{"AvailableKnobs", std::vector<std::string>{"TGPCurrentLimit"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleNVLinkDisable)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_NVLnk",
        {{"AvailableKnobs", std::vector<std::string>{"NVLinkDisable"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SinglePCIeVFConfig)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_VFCfg",
        {{"AvailableKnobs", std::vector<std::string>{"PCIeVFConfiguration"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleRowRemapAllowed)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_RRA",
        {{"AvailableKnobs", std::vector<std::string>{"RowRemappingAllowed"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleRowRemapFeature)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_RRF",
        {{"AvailableKnobs", std::vector<std::string>{"RowRemappingFeature"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleHBMFreqChange)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_HBMF",
        {{"AvailableKnobs", std::vector<std::string>{"HBMFrequencyChange"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleHULKLicense)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_HULK",
        {{"AvailableKnobs", std::vector<std::string>{"HULKLicenseUpdate"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleForceTestCoupling)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_FTC",
        {{"AvailableKnobs", std::vector<std::string>{"ForceTestCoupling"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleBAR0TypeConfig)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_B0TC",
        {{"AvailableKnobs", std::vector<std::string>{"BAR0TypeConfig"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleEDPpScalingFactor)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_EDPF",
        {{"AvailableKnobs", std::vector<std::string>{"EDPpScalingFactor"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SinglePwrSmoothL1)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_PSL1",
        {{"AvailableKnobs",
          std::vector<std::string>{"PowerSmoothingPrivilegeLevel1"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SinglePwrSmoothL2)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_PSL2",
        {{"AvailableKnobs",
          std::vector<std::string>{"PowerSmoothingPrivilegeLevel2"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleEGMMode)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_EGMM",
        {{"AvailableKnobs", std::vector<std::string>{"EGMMode"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleInfoROMRecreate)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_IROM",
        {{"AvailableKnobs",
          std::vector<std::string>{"InfoROMFileSystemRecreate"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleTGPRatedLimit)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_TGPR",
        {{"AvailableKnobs", std::vector<std::string>{"TGPRatedLimit"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleTGPMaxLimit)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_TGPMx",
        {{"AvailableKnobs", std::vector<std::string>{"TGPMaxLimit"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleTGPMinLimit)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_TGPMn",
        {{"AvailableKnobs", std::vector<std::string>{"TGPMinLimit"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory8Test, ReconfigPermissions_SingleClockLimit)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F8_CkLm",
        {{"AvailableKnobs", std::vector<std::string>{"ClockLimit"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

// ============================================================================
// Processor: association params
// ============================================================================

TEST_F(NsmProcessorFactory8Test, Processor_WithAssociations)
{
    auto rc = runFeature(nsm::features::createProcessorFeature, "GPU_F8_Assoc",
                         {{"ParentChassisAssociationPaths",
                           std::vector<std::string>{"chassis/HGX_GPU_8", ""}},
                          {"BridgingChassisAssociationPath",
                           std::string("chassis/HGX_Bridge_8")},
                          {"TotalPowerAssociationEnabled", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->staticSensors.size(), 0u);

    std::shared_ptr<NsmProcessorAssociation> associationSensor;
    for (const auto& sensor : gpu->deviceSensors)
    {
        if (auto assoc =
                std::dynamic_pointer_cast<NsmProcessorAssociation>(sensor))
        {
            associationSensor = assoc;
        }
    }
    ASSERT_NE(associationSensor, nullptr);

    // The empty ParentChassisAssociationPaths entry gives no link
    using Assoc = std::tuple<std::string, std::string, std::string>;
    const std::vector<Assoc> expected{
        {"parent_chassis", "all_processors",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_8"},
        {"bridged_processor", "bridging_processor",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_Bridge_8"},
        {"total_power", "all_processors", totalGpuPowerSensorPath.string()}};
    EXPECT_EQ(associationSensor->associationDef->associations(), expected);
}

TEST_F(NsmProcessorFactory8Test, Processor_NoAssociationParams)
{
    auto rc = runFeature(nsm::features::createProcessorFeature,
                         "GPU_F8_NoAssoc");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    std::shared_ptr<NsmProcessorAssociation> associationSensor;
    for (const auto& sensor : gpu->deviceSensors)
    {
        if (auto assoc =
                std::dynamic_pointer_cast<NsmProcessorAssociation>(sensor))
        {
            associationSensor = assoc;
        }
    }
    ASSERT_NE(associationSensor, nullptr);
    EXPECT_TRUE(associationSensor->associationDef->associations().empty());
}

// ============================================================================
// ProcessorAttributes: LocationType without LocationCode, different type
// ============================================================================

TEST_F(NsmProcessorFactory8Test, Attributes_LocationTypeSlot)
{
    auto rc = runFeature(
        nsm::features::createProcessorAttributesFeature, "GPU_F8_LocSlot",
        {{"LocationType",
          std::string(
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Slot")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}
