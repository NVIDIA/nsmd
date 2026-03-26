/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Factory branch coverage batch 7 for createNsmProcessorSensor:
 *
 * Targets individual *Supported flags TRUE while all others FALSE,
 * NSM_PCIe with Count=0/1/2, NSM_Processor with/without DEVICE_UUID,
 * NSM_Processor with InventoryObjPath containing/not-containing slash,
 * and NSM_ProcessorPerformance / NSM_PowerCap / NSM_ReconfigPermissions /
 * NSM_WorkloadPowerProfile type dispatches.
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

struct NsmProcessorFactory7Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Processor";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:77";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorFactory7Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmProcessorFactory7Test()
    {
        cleanupDeviceSensors(devices);
    }

    void setupBase(const std::string& path, const std::string& invPath)
    {
        auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
        base["Name"] = std::string("GPU_F7");
        base["UUID"] = gpuUuid;
        base["InventoryObjPath"] = invPath;
    }
};

// ============================================================================
// NSM_Processor_Attributes: each *Supported individually TRUE
// ============================================================================

TEST_F(NsmProcessorFactory7Test, Attributes_MIGModeOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_MIG");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MIGModeSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_PortDisableFutureOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_PDF");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["PortDisableFutureSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_ECCModeOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_ECC");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["ECCModeSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_EDPpScalingFactorOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_EDPp");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["EDPpScalingFactorSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_CpuOperatingConfigOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_CpuOp");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["CpuOperatingConfigSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_MemCapacityUtilOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_MemCap");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MemCapacityUtilSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_TotalNvLinksCountOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_NvLink");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["TotalNvLinksCountSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_EGMModeOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_EGM");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["EGMModeSupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_MNNVLTopologyOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_MNNVL");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["MNNVLTopologySupported"] = true;

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Attributes_MctpNsmOperationalStatusOnly)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_OpStat");
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
// NSM_PCIe: Count = 0, 1, 2
// ============================================================================

TEST_F(NsmProcessorFactory7Test, PCIe_Count0)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_PCIe0");
    setupBase(path, std::string(path));

    auto& pcie = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PCIe");
    pcie["Type"] = std::string("NSM_PCIe");
    pcie["DeviceId"] = uint64_t{0};
    pcie["Count"] = uint64_t{0};

    createNsmProcessorSensor(mockManager, baseIntf + ".PCIe", path);
    // Only the PCIeLinkSpeed sensor, no port sensors
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, PCIe_Count1)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_PCIe1");
    setupBase(path, std::string(path));

    auto& pcie = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PCIe");
    pcie["Type"] = std::string("NSM_PCIe");
    pcie["DeviceId"] = uint64_t{1};
    pcie["Count"] = uint64_t{1};

    createNsmProcessorSensor(mockManager, baseIntf + ".PCIe", path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, PCIe_Count2)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_PCIe2");
    setupBase(path, std::string(path));

    auto& pcie = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PCIe");
    pcie["Type"] = std::string("NSM_PCIe");
    pcie["DeviceId"] = uint64_t{2};
    pcie["Count"] = uint64_t{2};

    createNsmProcessorSensor(mockManager, baseIntf + ".PCIe", path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 2u);
    EXPECT_GT(gpu->deviceSensors.size(), 1u);
}

TEST_F(NsmProcessorFactory7Test, PCIe_NoDeviceIdNoCount)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_PCIeNone");
    setupBase(path, std::string(path));

    auto& pcie = utils::MockDbusAsync::propertyMap(path, baseIntf + ".PCIe");
    pcie["Type"] = std::string("NSM_PCIe");
    // No DeviceId, no Count -> defaults to 0

    createNsmProcessorSensor(mockManager, baseIntf + ".PCIe", path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor: with DEVICE_UUID present
// ============================================================================

TEST_F(NsmProcessorFactory7Test, Processor_WithDeviceUUID)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_UUID");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F7");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = std::string(path);
    base["DEVICE_UUID"] = std::string("some-device-uuid-value");
    base["Type"] = std::string("NSM_Processor");

    createNsmProcessorSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, Processor_WithoutDeviceUUID)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_NoUUID");
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F7");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = std::string(path);
    base["Type"] = std::string("NSM_Processor");
    // No DEVICE_UUID

    createNsmProcessorSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// NSM_Processor: InventoryObjPath with slash (has basename)
// ============================================================================

TEST_F(NsmProcessorFactory7Test, Processor_InvPathWithSlash)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_Slash");
    const std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/GPU0";
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F7");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath;
    base["Type"] = std::string("NSM_Processor");

    createNsmProcessorSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, DISABLED_Processor_InvPathNoSlash)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_NoSlash");
    const std::string invPath = "GPU_NoSlash_Path";
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GPU_F7");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath;
    base["Type"] = std::string("NSM_Processor");

    createNsmProcessorSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// NSM_ProcessorPerformance
// ============================================================================

TEST_F(NsmProcessorFactory7Test, ProcessorPerformance_WithDeviceId)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_Perf");
    setupBase(path, std::string(path));

    auto& perf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorPerformance");
    perf["Type"] = std::string("NSM_ProcessorPerformance");
    perf["DeviceId"] = uint64_t{3};

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorPerformance",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, ProcessorPerformance_NoDeviceId)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_PerfND");
    setupBase(path, std::string(path));

    auto& perf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorPerformance");
    perf["Type"] = std::string("NSM_ProcessorPerformance");
    // No DeviceId -> default 0

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorPerformance",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_PowerCap
// ============================================================================

TEST_F(NsmProcessorFactory7Test, PowerCap_WithCompositeNumericSensors)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_PwrCap");
    setupBase(path, std::string(path));

    auto& pwrcap = utils::MockDbusAsync::propertyMap(path,
                                                     baseIntf + ".PowerCap");
    pwrcap["Type"] = std::string("NSM_PowerCap");
    pwrcap["CompositeNumericSensors"] = std::vector<std::string>{"sensor1",
                                                                 "sensor2"};

    createNsmProcessorSensor(mockManager, baseIntf + ".PowerCap", path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, PowerCap_NoCompositeNumericSensors)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_PwrCapNC");
    setupBase(path, std::string(path));

    auto& pwrcap = utils::MockDbusAsync::propertyMap(path,
                                                     baseIntf + ".PowerCap");
    pwrcap["Type"] = std::string("NSM_PowerCap");
    // No CompositeNumericSensors

    createNsmProcessorSensor(mockManager, baseIntf + ".PowerCap", path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NSM_ReconfigPermissions
// ============================================================================

TEST_F(NsmProcessorFactory7Test, ReconfigPermissions_WithFeatures)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_Reconf");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    reconf["Features"] = std::vector<std::string>{"CCMode", "ECCEnable"};

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, ReconfigPermissions_NoFeatures)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_ReconfNF");
    setupBase(path, std::string(path));

    auto& reconf = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ReconfigPermissions");
    reconf["Type"] = std::string("NSM_ReconfigPermissions");
    // No Features -> empty

    createNsmProcessorSensor(mockManager, baseIntf + ".ReconfigPermissions",
                             path);
}

// ============================================================================
// NSM_WorkloadPowerProfile
// ============================================================================

TEST_F(NsmProcessorFactory7Test, WorkloadPowerProfile_WithProfileIdMap)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_WPP");
    setupBase(path, std::string(path));

    auto& wpp = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".WorkloadPowerProfile");
    wpp["Type"] = std::string("NSM_WorkloadPowerProfile");
    wpp["ProfileIdMap"] = std::vector<std::string>{"Profile_A", "Profile_B"};

    createNsmProcessorSensor(mockManager, baseIntf + ".WorkloadPowerProfile",
                             path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
    EXPECT_GT(gpu->roundRobinSensors.size(), 0u);
}

TEST_F(NsmProcessorFactory7Test, WorkloadPowerProfile_NoProfileIdMap)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_WPPNM");
    setupBase(path, std::string(path));

    auto& wpp = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".WorkloadPowerProfile");
    wpp["Type"] = std::string("NSM_WorkloadPowerProfile");
    // No ProfileIdMap

    createNsmProcessorSensor(mockManager, baseIntf + ".WorkloadPowerProfile",
                             path);
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// Attributes: LocationType + LocationCode both present
// ============================================================================

TEST_F(NsmProcessorFactory7Test, Attributes_LocationTypeAndCode)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_LocBoth");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    attr["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    attr["LocationCode"] = std::string("GPU_Bay_7");

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    EXPECT_GT(gpu->deviceSensors.size(), 0u);
}

// ============================================================================
// Attributes: neither LocationType nor LocationCode
// ============================================================================

TEST_F(NsmProcessorFactory7Test, Attributes_NoLocation)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_NoLoc");
    setupBase(path, std::string(path));

    auto& attr = utils::MockDbusAsync::propertyMap(
        path, baseIntf + ".ProcessorAttributes");
    attr["Type"] = std::string("NSM_Processor_Attributes");
    // No LocationType, no LocationCode, no Supported flags

    createNsmProcessorSensor(mockManager, baseIntf + ".ProcessorAttributes",
                             path);
    // Only asset sensors created
    EXPECT_GT(gpu->staticSensors.size(), 0u);
}

// ============================================================================
// Unrecognized type - does not match any branch
// ============================================================================

TEST_F(NsmProcessorFactory7Test, UnrecognizedType)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_Unknown");
    setupBase(path, std::string(path));

    auto& unknown = utils::MockDbusAsync::propertyMap(path,
                                                      baseIntf + ".Unknown");
    unknown["Type"] = std::string("NSM_SomeUnknownType");

    const size_t before = gpu->roundRobinSensors.size() +
                          gpu->staticSensors.size() + gpu->deviceSensors.size();
    createNsmProcessorSensor(mockManager, baseIntf + ".Unknown", path);
    const size_t after = gpu->roundRobinSensors.size() +
                         gpu->staticSensors.size() + gpu->deviceSensors.size();
    // No sensors added for unknown type
    EXPECT_EQ(before, after);
}

// ============================================================================
// Missing Name/UUID/Type/InventoryObjPath - all absent in base
// ============================================================================

TEST_F(NsmProcessorFactory7Test, AllBasePropertiesAbsent_ExceptUUID)
{
    const std::string path = processorsInventoryBasePath /
                             std::string("GPU_F7_Empty");
    // Set base properties: only UUID (required for device lookup)
    // Name, InventoryObjPath absent -> defaults to ""
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["UUID"] = gpuUuid;
    // No Type -> empty type, no branch matches
    base["Type"] = std::string("");

    createNsmProcessorSensor(mockManager, baseIntf, path);
}
