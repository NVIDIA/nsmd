/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Additional branch coverage for the processor feature creators
 * (nsm::features::create*Feature in nsmd/nsmProcessor/nsmProcessor.cpp):
 * - ProcessorPerformance with and without DeviceId
 * - WorkloadPowerProfile with the built-in profile id map
 * - ReconfigPermissions with available / disabled knobs
 * - PowerCap
 * - Processor with PCIeEnabled, PCIeDeviceId and PCIePortCount (loop body)
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

struct NsmProcessorFactory4Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
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
// ProcessorPerformance: DeviceId present
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ProcessorPerf_WithDeviceId)
{
    const size_t before = gpu->roundRobinSensors.size();
    auto rc = runFeature(nsm::features::createProcessorPerformanceFeature,
                         "GPU_F4_Perf_DevId", {{"DeviceId", uint64_t(0xABCD)}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // createProcessorPerformance adds 4 sensors to roundRobin/longRunning:
    // NsmAccumGpuUtilTime, NsmPciGroup5, NsmProcessorThrottleReason,
    // NsmProcessorThrottleDuration
    EXPECT_GT(gpu->roundRobinSensors.size() + gpu->longRunningSensors.size(),
              before);
    EXPECT_EQ(countOf<NsmAccumGpuUtilTime>(gpu->roundRobinSensors), 1u);
    EXPECT_EQ(countOf<NsmProcessorThrottleDuration>(gpu->longRunningSensors),
              1u);
}

// ============================================================================
// ProcessorPerformance: DeviceId absent (FALSE branch)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ProcessorPerf_NoDeviceId)
{
    const size_t before = gpu->roundRobinSensors.size();
    auto rc = runFeature(nsm::features::createProcessorPerformanceFeature,
                         "GPU_F4_Perf_NoDevId");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_GT(gpu->roundRobinSensors.size() + gpu->longRunningSensors.size(),
              before);
}

// ============================================================================
// WorkloadPowerProfile: the profile id map is the built-in
// workloadProfileIdMap
// ============================================================================
TEST_F(NsmProcessorFactory4Test, WorkloadPowerProfile_BuiltinProfileIdMap)
{
    const size_t staticBefore = gpu->staticSensors.size();
    const size_t rrBefore = gpu->roundRobinSensors.size();
    auto rc = runFeature(nsm::features::createWorkloadPowerProfileFeature,
                         "GPU_F4_WPP_ProfMap");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Creates: NsmWorkLoadProfileEnum (static), NsmWorkLoadProfileStatus (RR),
    // NsmWorkloadPowerProfileCollection (static),
    // NsmWorkloadPowerProfilePageCollection (static),
    // NsmWorkloadPowerProfilePage (RR)
    EXPECT_EQ(gpu->staticSensors.size(), staticBefore + 3);
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore + 2);

    std::shared_ptr<NsmWorkLoadProfileEnum> profileMapper;
    for (const auto& sensor : gpu->staticSensors)
    {
        if (auto mapper =
                std::dynamic_pointer_cast<NsmWorkLoadProfileEnum>(sensor))
        {
            profileMapper = mapper;
        }
    }
    ASSERT_NE(profileMapper, nullptr);
    EXPECT_EQ(profileMapper->enumToStringMap.size(),
              workloadProfileIdMap.size());
}

// ============================================================================
// PowerCap feature
// ============================================================================
TEST_F(NsmProcessorFactory4Test, PowerCap_Created)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();
    auto rc = runFeature(nsm::features::createPowerCapFeature,
                         "GPU_F4_PCap_NoComp");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Creates: NsmPowerCap (RR), NsmDefaultPowerCap (static),
    // NsmMaxPowerCap (static), NsmMinPowerCap (static)
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore + 1);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// ReconfigPermissions: knobs present (loop body)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ReconfigPermissions_WithFeatures)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature, "GPU_F4_Reconfig_Feat",
        {{"AvailableKnobs", std::vector<std::string>{"CCMode", "ECCEnable"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Each knob creates one NsmReconfigPermissions sensor
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore + 2);
}

// ============================================================================
// ReconfigPermissions: a disabled knob gets no sensor
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ReconfigPermissions_DisabledKnobFiltered)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature,
        "GPU_F4_Reconfig_Disabled",
        {{"AvailableKnobs", std::vector<std::string>{"CCMode", "ECCEnable"}},
         {"DisabledKnobs", std::vector<std::string>{"ECCEnable"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore + 1);
}

// ============================================================================
// ReconfigPermissions: AvailableKnobs absent
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ReconfigPermissions_NoFeatures)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();
    auto rc = runFeature(nsm::features::createReconfigPermissionsFeature,
                         "GPU_F4_Reconfig_NoFeat");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // No sensors added when no knob is available
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// ReconfigPermissions: empty AvailableKnobs list
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ReconfigPermissions_EmptyFeatures)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();
    auto rc = runFeature(nsm::features::createReconfigPermissionsFeature,
                         "GPU_F4_Reconfig_EmptyFeat",
                         {{"AvailableKnobs", std::vector<std::string>{}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// Processor PCIe: PCIeDeviceId and PCIePortCount > 0 (loop body coverage)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, PCIe_WithDeviceIdAndCount)
{
    auto rc = runFeature(nsm::features::createProcessorFeature,
                         "GPU_F4_PCIe_DevIdCnt",
                         {{"PCIeEnabled", true},
                          {"PCIeDeviceId", uint64_t(0x5678)},
                          {"PCIePortCount", uint64_t(2)}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Per port: NsmPciePortIntf (device sensor) + 3 PCIe group sensors (RR)
    EXPECT_EQ(countOf<NsmPciePortIntf>(gpu->deviceSensors), 2u);
    EXPECT_EQ(countOf<NsmPciGroup2>(gpu->roundRobinSensors), 2u);
    EXPECT_EQ(countOf<NsmPciGroup3>(gpu->roundRobinSensors), 2u);
    EXPECT_EQ(countOf<NsmPciGroup4>(gpu->roundRobinSensors), 2u);
}

// ============================================================================
// Processor PCIe: PCIeDeviceId absent, PCIePortCount absent (FALSE branches)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, PCIe_NoDeviceIdNoCount)
{
    auto rc = runFeature(nsm::features::createProcessorFeature,
                         "GPU_F4_PCIe_NoDevIdCnt", {{"PCIeEnabled", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Only NsmPCIeLinkSpeed added (no ports since the count defaults to 0)
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
    EXPECT_EQ(countOf<NsmPciGroup2>(gpu->roundRobinSensors), 0u);
}

// ============================================================================
// Processor PCIe: PCIePortCount = 0 (loop not entered)
// ============================================================================
TEST_F(NsmProcessorFactory4Test, PCIe_CountZero)
{
    auto rc = runFeature(nsm::features::createProcessorFeature,
                         "GPU_F4_PCIe_Cnt0",
                         {{"PCIeEnabled", true},
                          {"PCIeDeviceId", uint64_t(0x1111)},
                          {"PCIePortCount", uint64_t(0)}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // NsmPCIeLinkSpeed added to RR, no port sensors since the count is 0
    EXPECT_GE(gpu->roundRobinSensors.size(), 1u);
    EXPECT_EQ(countOf<NsmPciePortIntf>(gpu->deviceSensors), 0u);
}

// ============================================================================
// Processor: main processor objects, PCIe disabled
// ============================================================================
TEST_F(NsmProcessorFactory4Test, Processor_Created)
{
    auto rc = runFeature(nsm::features::createProcessorFeature, "GPU_F4_Proc");
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // NsmUuidIntf starts empty and is a static sensor
    EXPECT_EQ(countOf<NsmUuidIntf>(gpu->staticSensors), 1u);
    EXPECT_EQ(countOf<NsmProcessorAssociation>(gpu->deviceSensors), 1u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
    // PCIeEnabled absent: no PCIe sensors
    EXPECT_EQ(countOf<NsmPciGroup2>(gpu->roundRobinSensors), 0u);
}

// ============================================================================
// ProcessorAttributes: PowerSmoothingSupported = true
// ============================================================================
TEST_F(NsmProcessorFactory4Test, Attributes_PowerSmoothingTrue)
{
    auto rc = runFeature(nsm::features::createProcessorAttributesFeature,
                         "GPU_F4_Attr_PwrSmooth",
                         {{"PowerSmoothingSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // PowerSmoothing creates multiple sensors
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// ReconfigPermissions: single knob for minimal loop iteration
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ReconfigPermissions_SingleFeature)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();
    auto rc =
        runFeature(nsm::features::createReconfigPermissionsFeature,
                   "GPU_F4_Reconfig_1Feat",
                   {{"AvailableKnobs", std::vector<std::string>{"CCMode"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // One knob = one NsmReconfigPermissions sensor
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore + 1);
}

// ============================================================================
// ReconfigPermissions: multiple knobs with duplicates
// ============================================================================
TEST_F(NsmProcessorFactory4Test, ReconfigPermissions_DuplicateFeatures)
{
    const size_t rrBefore = gpu->roundRobinSensors.size();
    // Duplicate knobs - map deduplicates by FeatureType
    auto rc = runFeature(
        nsm::features::createReconfigPermissionsFeature,
        "GPU_F4_Reconfig_DupFeat",
        {{"AvailableKnobs",
          std::vector<std::string>{"CCMode", "ECCEnable", "CCMode"}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Duplicates are collapsed by the map: CCMode + ECCEnable = 2 unique
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore + 2);
}
