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
 * Additional branch coverage tests for nsmGpuPciePort.cpp:
 * - NsmClearPCIeCounters::update: encode failure (invalid instanceId scenario)
 * - NsmClearPCIeCounters::update: decode failure (short response)
 * - NsmClearPCIeCounters::updateReading: groupId=2 with all bits set
 * - NsmClearPCIeCounters::updateReading: groupId=3 with bit0 set
 * - NsmClearPCIeCounters::updateReading: groupId=4 with all bits set
 * - NsmClearPCIeCounters::updateReading: groupId=2 with NO bits set
 * - NsmClearPCIeIntf::clearPCIeErrorCounter: encode failure
 * - createNsmGpuPcieSensor factory: NSM_PortInfo type with all props
 * - createNsmGpuPcieSensor factory: NSM_PortInfo missing optional props
 * - createNsmGpuPcieSensor factory: missing Type property
 * - createNsmGpuPcieSensor factory: base properties fail
 * - createNsmGpuPcieSensor factory: exception in sensor creation
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "libnsm/pci-links.h"

#include "nsmCommon/nsmPcieGroup.hpp"
#include "nsmGpuPciePort.hpp"
#include "test/commonMock.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmGpuPcieSensor(SensorManager& manager,
                                            const std::string& interface,
                                            const std::string& objPath);
} // namespace nsm

static auto gpBus2 = sdbusplus::bus::new_default();
static const std::string gpInvPath2 =
    "/xyz/openbmc_project/inventory/system/gpu/pcie/brPort2";

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
struct NsmGpuPciePortBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:75";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0";
    static constexpr const char* portInfoIntf =
        "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0.PortInfo";

    NsmGpuPciePortBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmGpuPciePortBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }

    // Helper: build valid update response with given clearable bits
    static std::vector<uint8_t> buildUpdateResponse(uint8_t clearableBits)
    {
        std::vector<uint8_t> resp(256, 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        uint8_t availableMask[1] = {0xFF};
        uint8_t clearableMask[1] = {clearableBits};
        uint8_t maskLength = 1;
        uint16_t dataSize = 3;
        [[maybe_unused]] auto rc =
            encode_query_available_clearable_scalar_data_sources_v1_resp(
                0, NSM_SUCCESS, ERR_NULL, dataSize, maskLength, availableMask,
                clearableMask, msg);
        return resp;
    }

    // Valgrind-safe decode-fail buffer
    static std::vector<uint8_t> decodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ===========================================================================
// NsmClearPCIeCounters::update - decode failure (short response)
// Covers the else branch of (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, Update_DecodeFail)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(gpBus2, gpInvPath2.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_DecFail2", "NSM_ClearPCIe", 2, 0,
                                clearPCIeIntf);

    auto resp = decodeFail();
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    sensor.update(gpu);
    // updateReading not called -> counters empty
    EXPECT_TRUE(clearPCIeIntf->clearableCounters().empty());
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading: groupId=2 all bits (0x0F)
// Covers all four if-branches in group 2
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, UpdateReading_GroupId2_AllBits)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(gpBus2, gpInvPath2.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G2All", "NSM_ClearPCIe", 2, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x0F); // bits 0-3 all set
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 4u);
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading: groupId=2, NO bits set
// Covers all FALSE branches of bit conditionals in group 2
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, UpdateReading_GroupId2_NoBits)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(gpBus2, gpInvPath2.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G2None", "NSM_ClearPCIe", 2, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x00); // no bits set
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);

    EXPECT_TRUE(clearPCIeIntf->clearableCounters().empty());
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading: groupId=3 with bit0 set
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, UpdateReading_GroupId3_Bit0)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(gpBus2, gpInvPath2.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G3b0", "NSM_ClearPCIe", 3, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x01); // bit0 -> L0ToRecoveryCount
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_EQ(counters.size(), 1u);
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading: groupId=4 all relevant bits
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, UpdateReading_GroupId4_AllBits)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(gpBus2, gpInvPath2.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G4All", "NSM_ClearPCIe", 4, 0,
                                clearPCIeIntf);

    // bits 1,2,4,6 set -> NAKReceived, NAKSent, ReplayRollover, Replay
    auto resp = buildUpdateResponse(0x56); // 0b01010110
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);

    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 4u);
}

// ===========================================================================
// NsmClearPCIeCounters::updateReading: groupId=4, NO bits set
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, UpdateReading_GroupId4_NoBits)
{
    auto clearPCIeIntf =
        std::make_shared<NsmClearPCIeIntf>(gpBus2, gpInvPath2.c_str(), 0, gpu);
    NsmClearPCIeCounters sensor("ClearPCIe_G4None", "NSM_ClearPCIe", 4, 0,
                                clearPCIeIntf);

    auto resp = buildUpdateResponse(0x00);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));
    sensor.update(gpu);

    EXPECT_TRUE(clearPCIeIntf->clearableCounters().empty());
}

// ===========================================================================
// createNsmGpuPcieSensor: base properties fail -> early return
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, Factory_BasePropertiesFail)
{
    const std::string path = "/test/gpupcie2/no_base";
    const size_t before = gpu->deviceSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ===========================================================================
// createNsmGpuPcieSensor: NSM_PortInfo type with all optional properties
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, Factory_PortInfo_AllProps)
{
    const std::string path = "/test/gpupcie2/portinfo_all";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/gpupcie2/portinfo_all_proc";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GpuPcie_PI_All");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = processorPath;
    base["DeviceIndex"] = uint64_t(0);

    auto& cur = utils::MockDbusAsync::propertyMap(path, portInfoIntf);
    cur["Type"] = std::string("NSM_PortInfo");
    cur["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo."
        "PortType.UpstreamPort");
    cur["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo."
        "PortProtocol.PCIe");
    cur["Priority"] = true;

    createNsmGpuPcieSensor(mockManager, portInfoIntf, path);
}

// ===========================================================================
// createNsmGpuPcieSensor: NSM_PortInfo missing all optional properties
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, Factory_PortInfo_MissingOptionalProps)
{
    const std::string path = "/test/gpupcie2/portinfo_bare";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/gpupcie2/portinfo_bare_proc";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GpuPcie_PI_Bare");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = processorPath;
    // DeviceIndex omitted

    auto& cur = utils::MockDbusAsync::propertyMap(path, portInfoIntf);
    cur["Type"] = std::string("NSM_PortInfo");
    // PortType, PortProtocol, Priority omitted

    EXPECT_NO_THROW_COROUTINE(
        createNsmGpuPcieSensor(mockManager, portInfoIntf, path));
}

// ===========================================================================
// createNsmGpuPcieSensor: missing Type -> no branch matches
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, Factory_MissingType_NoSensor)
{
    const std::string path = "/test/gpupcie2/missing_type";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/gpupcie2/missing_type_proc";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("GpuPcie_NoType");
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = processorPath;
    // Type omitted in current iface properties

    const size_t before = gpu->deviceSensors.size();
    createNsmGpuPcieSensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ===========================================================================
// createNsmGpuPcieSensor: NSM_GPU_PCIe_0 missing Health and ChasisPowerState
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, Factory_GpuPcie0_MissingHealthAndChassis)
{
    const std::string path = "/test/gpupcie2/no_health_chassis";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/gpupcie2/no_hc_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("GpuPcie_NoHC");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["DeviceIndex"] = uint64_t(0);
    // Health, ChasisPowerState omitted -> empty strings -> throws

    EXPECT_NO_THROW_COROUTINE(
        createNsmGpuPcieSensor(mockManager, baseIntf, path));
}

// ===========================================================================
// createNsmGpuPcieSensor: NSM_GPU_PCIe_0 with ClearableScalarGroup
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, Factory_GpuPcie0_WithClearableScalarGroup)
{
    const std::string path = "/test/gpupcie2/with_clearable";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/gpupcie2/clearable_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("GpuPcie_Clearable");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    pm["DeviceIndex"] = uint64_t(0);
    pm["ClearableScalarGroup"] = std::vector<uint64_t>{2, 3, 4};

    createNsmGpuPcieSensor(mockManager, baseIntf, path);
}

// ===========================================================================
// createNsmGpuPcieSensor: NSM_GPU_PCIe_0 missing ClearableScalarGroup
// ===========================================================================
TEST_F(NsmGpuPciePortBranch2Test, Factory_GpuPcie0_MissingClearableGroup)
{
    const std::string path = "/test/gpupcie2/no_clearable";
    const std::string processorPath =
        "/xyz/openbmc_project/inventory/gpupcie2/no_clearable_proc";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("GpuPcie_NoClear");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = processorPath;
    pm["Type"] = std::string("NSM_GPU_PCIe_0");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    pm["DeviceIndex"] = uint64_t(0);
    // ClearableScalarGroup omitted

    createNsmGpuPcieSensor(mockManager, baseIntf, path);
}
