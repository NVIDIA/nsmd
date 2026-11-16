/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage batch 9 for the processor feature creators
 * (nsm::features::create*Feature in nsmd/nsmProcessor/nsmProcessor.cpp):
 *
 * Targets constructor bodies and helper paths not exercised in batches 1-8:
 *   - Processor: NsmAcceleratorIntf, NsmProcessorAssociation,
 *     NsmUuidIntf, NsmGpuHealth, NsmConfidentialCompute,
 *     NsmProcessorRevision, NsmTotalMemorySize constructors
 *   - ProcessorAttributes with individual feature combos for:
 *     NsmMigMode, NsmEccMode, NsmEccErrorCounts, NsmEDPpScalingFactor,
 *     NsmMaxEDPpLimit, NsmMinEDPpLimit, NsmClockLimitGraphics,
 *     NsmCurrClockFreq, NsmDefaultBaseClockSpeed, NsmDefaultBoostClockSpeed,
 *     NsmCurrentUtilization, NsmEgmMode, NsmTotalNvLinks, and the
 *     LocationContext / FieldReplaceable params
 *   - ProcessorPerformance: NsmProcessorThrottleReason,
 *     NsmProcessorThrottleDuration, NsmAccumGpuUtilTime, NsmPciGroup5
 *   - PowerCap: NsmPowerCap, NsmMaxPowerCap, NsmMinPowerCap,
 *     NsmDefaultPowerCap
 *   - Name param absent
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

struct NsmProcessorFactory9Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
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
// Processor
// Exercises NsmAcceleratorIntf, NsmProcessorAssociation, NsmUuidIntf,
// NsmGpuHealth, NsmConfidentialCompute, NsmProcessorRevision,
// NsmTotalMemorySize constructors
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Processor_Created)
{
    auto rc = runFeature(nsm::features::createProcessorFeature, "GPU_F9_UUID");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only MIGModeSupported=true
// Exercises NsmMigMode constructor (line 1099-1111)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_MIGModeOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_MIG", {{"MIGModeSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only ECCModeSupported=true
// Exercises NsmEccMode (line 1177-1188), NsmEccErrorCounts (line 1260-1269)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_ECCModeOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_ECC", {{"ECCModeSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only EDPpScalingFactorSupported=true
// Exercises NsmEDPpScalingFactor (line 1417-1428), NsmMaxEDPpLimit (1580-1585),
// NsmMinEDPpLimit (1642-1647)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_EDPpOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_EDPp", {{"EDPpScalingFactorSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only CpuOperatingConfigSupported=true
// Exercises NsmClockLimitGraphics (1704-1715), NsmCurrClockFreq (1802-1812),
// NsmDefaultBaseClockSpeed (1875-1882), NsmDefaultBoostClockSpeed (1944-1951),
// NsmCurrentUtilization (2013-2028)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_CpuOperatingConfigOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_CPU", {{"CpuOperatingConfigSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only TotalNvLinksCountSupported=true
// Exercises NsmTotalNvLinks (2382-2392)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_TotalNvLinksOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_NVL", {{"TotalNvLinksCountSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only EGMModeSupported=true
// Exercises NsmEgmMode (3356-3363)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_EGMModeOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_EGM", {{"EGMModeSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only PowerSmoothingSupported=true
// Exercises createPowerSmoothing helper (lines 305-470)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_PowerSmoothingOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_PwrSm", {{"PowerSmoothingSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: LocationType + LocationCode together
// Exercises NsmLocationIntfProcessor (1078-1087),
// NsmLocationCodeIntfProcessor (1089-1097)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_LocationTypeAndCode)
{
    auto rc = runFeature(
        nsm::features::createProcessorAttributesFeature, "GPU_F9_Loc",
        {{"LocationType",
          std::string(
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
              "Embedded")},
         {"LocationCode", std::string("GPU_Bay_9")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only PortDisableFutureSupported=true
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_PortDisableFutureOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_PDF", {{"PortDisableFutureSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only MemCapacityUtilSupported=true
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_MemCapacityUtilOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_Mem", {{"MemCapacityUtilSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only MNNVLTopologySupported=true
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_MNNVLTopologyOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_MNNVL", {{"MNNVLTopologySupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only MctpNsmOperationalStatusSupported=true
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_OperationalStatusOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_OpStat",
                         {{"MctpNsmOperationalStatusSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only GPUBasePowerLimitSupported=true
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_GPUBasePowerLimitOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_GBPL", {{"GPUBasePowerLimitSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: only GPUCopyCPUPowerLimitSupported=true
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_GPUCopyCPUPowerLimitOnlyTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_GCCPL",
                         {{"GPUCopyCPUPowerLimitSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorPerformance: exercises NsmProcessorThrottleReason (2100-2114),
// NsmProcessorThrottleDuration (2940-2955), NsmAccumGpuUtilTime (2217-2228),
// NsmPciGroup5 (1349-1360)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, ProcessorPerformance_DeviceId1)
{
    auto rc = runFeature(nsm::features::createProcessorPerformanceFeature,
                         "GPU_F9_Perf1", {{"DeviceId", uint64_t{1}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory9Test, ProcessorPerformance_NoDeviceId)
{
    auto rc = runFeature(nsm::features::createProcessorPerformanceFeature,
                         "GPU_F9_PerfND");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory9Test, PowerCap_Created)
{
    auto rc = runFeature(nsm::features::createPowerCapFeature, "GPU_F9_PCNone");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// ReconfigPermissions: CCMode and ECCEnable features
// Exercises createReconfigPermissions lines 685, 706, 718
// ============================================================================

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_CCModeECCEnable)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_RCCC",
        {{"AvailableKnobs", std::vector<std::string>{"CCMode", "ECCEnable"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 2u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_EmptyFeatures)
{
    auto rc = runFeature(nsm::features::createReconfigPermissionsFeature,
                         "GPU_F9_RCEmpty",
                         {{"AvailableKnobs", std::vector<std::string>{}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
}

TEST_F(NsmProcessorFactory9Test, WorkloadPowerProfile_Created)
{
    auto rc = runFeature(nsm::features::createWorkloadPowerProfileFeature,
                         "GPU_F9_WPPNo");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: all supported flags = false (no sub-objects)
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_AllSupportedFalse)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_AllFalse",
                         {{"MIGModeSupported", false},
                          {"PortDisableFutureSupported", false},
                          {"ECCModeSupported", false},
                          {"EDPpScalingFactorSupported", false},
                          {"PowerSmoothingSupported", false},
                          {"CpuOperatingConfigSupported", false},
                          {"MemCapacityUtilSupported", false},
                          {"TotalNvLinksCountSupported", false},
                          {"EGMModeSupported", false},
                          {"MNNVLTopologySupported", false},
                          {"MctpNsmOperationalStatusSupported", false},
                          {"GPUBasePowerLimitSupported", false},
                          {"GPUCopyCPUPowerLimitSupported", false}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Only asset sensors should be created, no feature sensors
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: combined MIG + ECC + EDPp + CpuConfig
// Exercises multiple constructor paths in a single factory call
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_CombinedMIGECCEDPpCpu)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_Combo",
                         {{"MIGModeSupported", true},
                          {"ECCModeSupported", true},
                          {"EDPpScalingFactorSupported", true},
                          {"CpuOperatingConfigSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: combined EGM + TotalNvLinks + PowerSmoothing
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_CombinedEGMNvLinksPwrSmooth)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_Combo2",
                         {{"EGMModeSupported", true},
                          {"TotalNvLinksCountSupported", true},
                          {"PowerSmoothingSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: combined GPUBase + GPUCopyCPU power limits
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_BothGPUPowerLimits)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_BPWL",
                         {{"GPUBasePowerLimitSupported", true},
                          {"GPUCopyCPUPowerLimitSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// Processor PCIe: exercises PCIe branch with DeviceId and Count
// ============================================================================

TEST_F(NsmProcessorFactory9Test, PCIe_DeviceId10Count2)
{
    auto rc = runFeature(nsm::features::createProcessorFeature, "GPU_F9_PCIe",
                         {{"PCIeEnabled", true},
                          {"PCIeDeviceId", uint64_t{10}},
                          {"PCIePortCount", uint64_t{2}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// ReconfigPermissions: single features not tested in batch 8
// ============================================================================

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleTGPRatedLimit)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_TGPR",
        {{"AvailableKnobs", std::vector<std::string>{"TGPRatedLimit"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleTGPMaxLimit)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_TGPM",
        {{"AvailableKnobs", std::vector<std::string>{"TGPMaxLimit"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleTGPMinLimit)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_TGPN",
        {{"AvailableKnobs", std::vector<std::string>{"TGPMinLimit"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleClockLimit)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_CLK",
        {{"AvailableKnobs", std::vector<std::string>{"ClockLimit"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleHBMFreqChange)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_HBM",
        {{"AvailableKnobs", std::vector<std::string>{"HBMFrequencyChange"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleHULKLicenseUpdate)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_HULK",
        {{"AvailableKnobs", std::vector<std::string>{"HULKLicenseUpdate"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleForceTestCoupling)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_FTC",
        {{"AvailableKnobs", std::vector<std::string>{"ForceTestCoupling"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleBAR0TypeConfig)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_B0TC",
        {{"AvailableKnobs", std::vector<std::string>{"BAR0TypeConfig"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleEDPpScalingFactor)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_EDPSF",
        {{"AvailableKnobs", std::vector<std::string>{"EDPpScalingFactor"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test,
       ReconfigPermissions_SinglePowerSmoothingPrivLevel1)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_PSP1",
        {{"AvailableKnobs",
          std::vector<std::string>{"PowerSmoothingPrivilegeLevel1"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test,
       ReconfigPermissions_SinglePowerSmoothingPrivLevel2)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_PSP2",
        {{"AvailableKnobs",
          std::vector<std::string>{"PowerSmoothingPrivilegeLevel2"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test, ReconfigPermissions_SingleEGMMode)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_RCEGM",
        {{"AvailableKnobs", std::vector<std::string>{"EGMMode"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory9Test,
       ReconfigPermissions_SingleInfoROMFileSystemRecreate)
{
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F9_IRFS",
        {{"AvailableKnobs",
          std::vector<std::string>{"InfoROMFileSystemRecreate"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
}

// ============================================================================
// Error paths: missing Name param
// ============================================================================

TEST_F(NsmProcessorFactory9Test, ErrorPath_MissingName)
{
    // Name param absent: the sensors are created with an empty name
    nsm::features::DeviceContext dev;
    dev.name = "GPU_F9_NoName";
    dev.kind = "gpu";
    dev.staticUuid = gpuUuid;
    dev.device = gpu;
    auto rc = nsm::features::createProcessorAttributesFeature(mockManager, dev,
                                                              {});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Should still create asset sensors even with empty name
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// ProcessorAttributes: LocationContext and FieldReplaceable params
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_LocationContextAndReplaceable)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F9_CtxRepl",
                         {{"LocationContext", std::string("HGX_Baseboard_0")},
                          {"FieldReplaceable", false}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(countOf<NsmInterfaceProvider<LocationContextIntfProcessor>>(
                  gpu->deviceSensors),
              1u);
    EXPECT_EQ(countOf<NsmInterfaceProvider<ReplaceableIntfProcessor>>(
                  gpu->deviceSensors),
              1u);
}

// ============================================================================
// ProcessorAttributes: all features true with Location
// Full coverage of all attribute branches in a single test
// ============================================================================

TEST_F(NsmProcessorFactory9Test, Attributes_FullCoverage)
{
    auto rc = runFeature(
        nsm::features::createProcessorAttributesFeature, "GPU_F9_Full",
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
         {"LocationCode", std::string("GPU_Bay_Full")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}
