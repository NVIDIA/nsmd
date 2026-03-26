/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved. SPDX-License-Identifier: Apache-2.0
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
 * Coverage targets:
 *   - NsmInterfaces<T>::moveInterfaces for 14 concrete interface types
 *     (triggered by adding two sensors of identical type + request bytes
 *      to the same NsmDevice via the constrained addSensor<T> overload)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "nsmChassis/nsmChassis.hpp"
#include "nsmChassis/nsmChassisPCIeDevice.hpp"
#include "nsmChassis/nsmChassisPCIeSlot.hpp"
#include "nsmChassis/nsmClockOutputEnableState.hpp"
#include "nsmChassis/nsmNVSwitchAndNVMgmtNICChassis.hpp"
#include "nsmChassis/nsmNVSwitchAndNVMgmtNICChassisAssembly.hpp"
#include "nsmChassis/nsmPCIeLTSSMState.hpp"
#include "nsmChassis/nsmWriteProtectedJumper.hpp"
#include "nsmDbusIfaceOverride/nsmAssetIntf.hpp"
#include "nsmDbusIfaceOverride/nsmMNNVLinkTopologyIntf.hpp"
#include "nsmDeviceInventory/nsmSwitch.hpp"
#include "nsmErrorInjection/nsmErrorInjection.hpp"
#include "nsmFwSwInventory/nsmFirmwareInventory.hpp"
#include "nsmInterface.hpp"
#include "nsmNumericSensor/nsmNumericSensorComposite.hpp"
#include "nsmPort/nsmFpgaPort.hpp"
#include "nsmPort/nsmPCIePort.hpp"
#include "nsmPort/nsmPortDisableFuture.hpp"
#include "nsmPort/nsmPortInfo.hpp"
#include "nsmProcessor/nsmProcessor.hpp"
#include "nsmSensors/nsmInventoryProperty.hpp"
#include "nsmSensors/nsmPCIeLinkSpeed.hpp"

using namespace nsm;

struct NsmMoveInterfacesTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    NsmMoveInterfacesTest() : SensorManagerTest(devices)
    {
        device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 0, "MCTP_EID",
                                                 "12", 0);
    }

    ~NsmMoveInterfacesTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// DimensionIntf: NsmInventoryProperty<DimensionIntf>
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_DimensionIntf)
{
    NsmChassis<DimensionIntf> prov1{"moveDim1"};
    NsmChassis<DimensionIntf> prov2{"moveDim2"};
    auto s1 = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        prov1, PRODUCT_LENGTH);
    auto s2 = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        prov2, PRODUCT_LENGTH);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// PowerLimitIntf: NsmInventoryProperty<PowerLimitIntf>
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_PowerLimitIntf)
{
    NsmChassis<PowerLimitIntf> prov1{"movePwr1"};
    NsmChassis<PowerLimitIntf> prov2{"movePwr2"};
    auto s1 = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        prov1, MINIMUM_DEVICE_POWER_LIMIT);
    auto s2 = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        prov2, MINIMUM_DEVICE_POWER_LIMIT);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// VersionIntf: NsmInventoryProperty<VersionIntf>
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_VersionIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string path1 = "/xyz/openbmc_project/inventory/move/ver1";
    const std::string path2 = "/xyz/openbmc_project/inventory/move/ver2";
    auto pdi1 = std::make_shared<VersionIntf>(bus, path1.c_str());
    auto pdi2 = std::make_shared<VersionIntf>(bus, path2.c_str());
    NsmInterfaceProvider<VersionIntf> prov1("moveVer1", "NSM_Version",
                                            std::filesystem::path(path1), pdi1);
    NsmInterfaceProvider<VersionIntf> prov2("moveVer2", "NSM_Version",
                                            std::filesystem::path(path2), pdi2);
    auto s1 = std::make_shared<NsmInventoryProperty<VersionIntf>>(
        prov1, PCIERETIMER_0_EEPROM_VERSION);
    auto s2 = std::make_shared<NsmInventoryProperty<VersionIntf>>(
        prov2, PCIERETIMER_0_EEPROM_VERSION);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// AssetTagIntf: NsmInventoryProperty<AssetTagIntf>
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_AssetTagIntf)
{
    NsmNVSwitchAndNicChassis<AssetTagIntf> prov1{"moveAtag1", "NSM_Chassis"};
    NsmNVSwitchAndNicChassis<AssetTagIntf> prov2{"moveAtag2", "NSM_Chassis"};
    auto s1 = std::make_shared<NsmInventoryProperty<AssetTagIntf>>(prov1,
                                                                   ASSET_TAG);
    auto s2 = std::make_shared<NsmInventoryProperty<AssetTagIntf>>(prov2,
                                                                   ASSET_TAG);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// NsmMNNVLinkTopologyIntf: NsmInventoryProperty<NsmMNNVLinkTopologyIntf>
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_NsmMNNVLinkTopologyIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string path1 = "/xyz/openbmc_project/inventory/move/mnn1";
    const std::string path2 = "/xyz/openbmc_project/inventory/move/mnn2";
    auto pdi1 = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path1.c_str());
    auto pdi2 = std::make_shared<NsmMNNVLinkTopologyIntf>(bus, path2.c_str());
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> prov1(
        "moveMNN1", "NSM_MNNVLink", std::filesystem::path(path1), pdi1);
    NsmInterfaceProvider<NsmMNNVLinkTopologyIntf> prov2(
        "moveMNN2", "NSM_MNNVLink", std::filesystem::path(path2), pdi2);
    auto s1 = std::make_shared<NsmInventoryProperty<NsmMNNVLinkTopologyIntf>>(
        prov1, GPU_IBGUID);
    auto s2 = std::make_shared<NsmInventoryProperty<NsmMNNVLinkTopologyIntf>>(
        prov2, GPU_IBGUID);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// PCIeSlotIntf: NsmPCIeLinkSpeed<PCIeSlotIntf>
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_PCIeSlotIntf)
{
    NsmChassisPCIeSlot<PCIeSlotIntf> prov1{"moveSlotCh1", "moveSlot1"};
    NsmChassisPCIeSlot<PCIeSlotIntf> prov2{"moveSlotCh2", "moveSlot2"};
    auto s1 = std::make_shared<NsmPCIeLinkSpeed<PCIeSlotIntf>>(prov1, 0, false);
    auto s2 = std::make_shared<NsmPCIeLinkSpeed<PCIeSlotIntf>>(prov2, 0, false);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// PCIeECCIntf: NsmPCIeLinkSpeed<PCIeEccIntf>
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_PCIeECCIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string path1 = "/xyz/openbmc_project/inventory/move/pcieECC1";
    const std::string path2 = "/xyz/openbmc_project/inventory/move/pcieECC2";
    auto pdi1 = std::make_shared<PCIeEccIntf>(bus, path1.c_str());
    auto pdi2 = std::make_shared<PCIeEccIntf>(bus, path2.c_str());
    NsmInterfaceProvider<PCIeEccIntf> prov1("movePCIeECC1", "NSM_PCIeECC",
                                            std::filesystem::path(path1), pdi1);
    NsmInterfaceProvider<PCIeEccIntf> prov2("movePCIeECC2", "NSM_PCIeECC",
                                            std::filesystem::path(path2), pdi2);
    auto s1 = std::make_shared<NsmPCIeLinkSpeed<PCIeEccIntf>>(prov1, 0, false);
    auto s2 = std::make_shared<NsmPCIeLinkSpeed<PCIeEccIntf>>(prov2, 0, false);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// NsmPortInfoIntf: NsmPCIeLinkSpeed<NsmPortInfoIntf>
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_NsmPortInfoIntf)
{
    NsmPCIePort<NsmPortInfoIntf> prov1{
        "/xyz/openbmc_project/inventory/move/portInfo1"};
    NsmPCIePort<NsmPortInfoIntf> prov2{
        "/xyz/openbmc_project/inventory/move/portInfo2"};
    auto s1 = std::make_shared<NsmPCIeLinkSpeed<NsmPortInfoIntf>>(prov1, 0,
                                                                  false);
    auto s2 = std::make_shared<NsmPCIeLinkSpeed<NsmPortInfoIntf>>(prov2, 0,
                                                                  false);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// LTSSMStateIntf: NsmPCIeLTSSMState
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_LTSSMStateIntf)
{
    NsmChassisPCIeDevice<LTSSMStateIntf> prov1{"moveLTSSMCh1", "moveLTSSM1"};
    NsmChassisPCIeDevice<LTSSMStateIntf> prov2{"moveLTSSMCh2", "moveLTSSM2"};
    auto s1 = std::make_shared<NsmPCIeLTSSMState>(prov1, 0);
    auto s2 = std::make_shared<NsmPCIeLTSSMState>(prov2, 0);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// PCIeRefClockIntf: NsmClockOutputEnableState<PCIeRefClockIntf>
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_PCIeRefClockIntf)
{
    NsmChassis<PCIeRefClockIntf> prov1{"movePCIeRefClock1"};
    NsmChassis<PCIeRefClockIntf> prov2{"movePCIeRefClock2"};
    auto s1 = std::make_shared<NsmClockOutputEnableState<PCIeRefClockIntf>>(
        prov1, PCIE_CLKBUF_INDEX, NSM_DEV_ID_GPU, 0);
    auto s2 = std::make_shared<NsmClockOutputEnableState<PCIeRefClockIntf>>(
        prov2, PCIE_CLKBUF_INDEX, NSM_DEV_ID_GPU, 0);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// NVLinkRefClockIntf: NsmClockOutputEnableState<NVLinkRefClockIntf>
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_NVLinkRefClockIntf)
{
    NsmChassisPCIeDevice<NVLinkRefClockIntf> prov1{"moveNVLinkCh1",
                                                   "moveNVLink1"};
    NsmChassisPCIeDevice<NVLinkRefClockIntf> prov2{"moveNVLinkCh2",
                                                   "moveNVLink2"};
    auto s1 = std::make_shared<NsmClockOutputEnableState<NVLinkRefClockIntf>>(
        prov1, NVHS_CLKBUF_INDEX, NSM_DEV_ID_GPU, 0);
    auto s2 = std::make_shared<NsmClockOutputEnableState<NVLinkRefClockIntf>>(
        prov2, NVHS_CLKBUF_INDEX, NSM_DEV_ID_GPU, 0);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// ErrorInjectionIntf: NsmErrorInjection
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_ErrorInjectionIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string path1 = "/xyz/openbmc_project/inventory/move/ei1";
    const std::string path2 = "/xyz/openbmc_project/inventory/move/ei2";
    auto pdi1 = std::make_shared<ErrorInjectionIntf>(bus, path1.c_str());
    auto pdi2 = std::make_shared<ErrorInjectionIntf>(bus, path2.c_str());
    NsmInterfaceProvider<ErrorInjectionIntf> prov1(
        "moveEI1", "NSM_EI", std::filesystem::path(path1), pdi1);
    NsmInterfaceProvider<ErrorInjectionIntf> prov2(
        "moveEI2", "NSM_EI", std::filesystem::path(path2), pdi2);
    auto s1 = std::make_shared<NsmErrorInjection>(prov1);
    auto s2 = std::make_shared<NsmErrorInjection>(prov2);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// ErrorInjectionPayloadIntf: NsmErrorInjectionPayload
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_ErrorInjectionPayloadIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string path1 = "/xyz/openbmc_project/inventory/move/eip1";
    const std::string path2 = "/xyz/openbmc_project/inventory/move/eip2";
    auto pdi1 = std::make_shared<ErrorInjectionPayloadIntf>(bus, path1.c_str());
    auto pdi2 = std::make_shared<ErrorInjectionPayloadIntf>(bus, path2.c_str());
    NsmInterfaceProvider<ErrorInjectionPayloadIntf> prov1(
        "moveEIP1", "NSM_EIP", std::filesystem::path(path1), pdi1);
    NsmInterfaceProvider<ErrorInjectionPayloadIntf> prov2(
        "moveEIP2", "NSM_EIP", std::filesystem::path(path2), pdi2);
    // Same errorInjectionType and errorInjectionSubtype -> same request bytes
    auto s1 = std::make_shared<NsmErrorInjectionPayload>(prov1, 1, 0);
    auto s2 = std::make_shared<NsmErrorInjectionPayload>(prov2, 1, 0);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// SettingsIntf: NsmWriteProtectedJumper
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_SettingsIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string path1 = "/xyz/openbmc_project/inventory/move/settings1";
    const std::string path2 = "/xyz/openbmc_project/inventory/move/settings2";
    auto pdi1 = std::make_shared<SettingsIntf>(bus, path1.c_str());
    auto pdi2 = std::make_shared<SettingsIntf>(bus, path2.c_str());
    NsmInterfaceProvider<SettingsIntf> prov1(
        "moveSettings1", "NSM_Settings", std::filesystem::path(path1), pdi1);
    NsmInterfaceProvider<SettingsIntf> prov2(
        "moveSettings2", "NSM_Settings", std::filesystem::path(path2), pdi2);
    // NsmWriteProtectedJumper always sends GET_WP_JUMPER_PRESENCE -> same bytes
    auto s1 = std::make_shared<NsmWriteProtectedJumper>(prov1);
    auto s2 = std::make_shared<NsmWriteProtectedJumper>(prov2);
    device->addSensor(s1, PollingType::RoundRobin);
    const size_t sizeBefore = device->deviceSensors.size();
    device->addSensor(s2, PollingType::RoundRobin);
    EXPECT_EQ(device->deviceSensors.size(), sizeBefore);
}

// ============================================================================
// NsmInterfaces<T> constructor — empty-map throw path (nsmInterface.hpp 57-61)
// Each template instantiation generates a separate copy of
// the constructor; passing an empty Interfaces<T> map triggers the throw branch
// for each type.
// ============================================================================

TEST_F(NsmMoveInterfacesTest, Constructor_Empty_DimensionIntf_Throws)
{
    Interfaces<DimensionIntf> m;
    EXPECT_THROW((NsmInterfaces<DimensionIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_PowerLimitIntf_Throws)
{
    Interfaces<PowerLimitIntf> m;
    EXPECT_THROW((NsmInterfaces<PowerLimitIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_AssetTagIntf_Throws)
{
    Interfaces<AssetTagIntf> m;
    EXPECT_THROW((NsmInterfaces<AssetTagIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_NsmMNNVLinkTopologyIntf_Throws)
{
    Interfaces<NsmMNNVLinkTopologyIntf> m;
    EXPECT_THROW((NsmInterfaces<NsmMNNVLinkTopologyIntf>(m)),
                 std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_PCIeSlotIntf_Throws)
{
    Interfaces<PCIeSlotIntf> m;
    EXPECT_THROW((NsmInterfaces<PCIeSlotIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_PCIeEccIntf_Throws)
{
    Interfaces<PCIeEccIntf> m;
    EXPECT_THROW((NsmInterfaces<PCIeEccIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_NsmPortInfoIntf_Throws)
{
    Interfaces<NsmPortInfoIntf> m;
    EXPECT_THROW((NsmInterfaces<NsmPortInfoIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_LTSSMStateIntf_Throws)
{
    Interfaces<LTSSMStateIntf> m;
    EXPECT_THROW((NsmInterfaces<LTSSMStateIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_PCIeRefClockIntf_Throws)
{
    Interfaces<PCIeRefClockIntf> m;
    EXPECT_THROW((NsmInterfaces<PCIeRefClockIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_NVLinkRefClockIntf_Throws)
{
    Interfaces<NVLinkRefClockIntf> m;
    EXPECT_THROW((NsmInterfaces<NVLinkRefClockIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_ErrorInjectionIntf_Throws)
{
    Interfaces<ErrorInjectionIntf> m;
    EXPECT_THROW((NsmInterfaces<ErrorInjectionIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest,
       Constructor_Empty_ErrorInjectionPayloadIntf_Throws)
{
    Interfaces<ErrorInjectionPayloadIntf> m;
    EXPECT_THROW((NsmInterfaces<ErrorInjectionPayloadIntf>(m)),
                 std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_SettingsIntf_Throws)
{
    Interfaces<SettingsIntf> m;
    EXPECT_THROW((NsmInterfaces<SettingsIntf>(m)), std::runtime_error);
}

// ============================================================================
// NsmInterfaces<T>::invoke() — error path (nsmInterface.hpp lines 128–136)
// Create two DimensionIntf instances with different depth() values; the
// non-void invoke() overload throws std::runtime_error when values differ.
// ============================================================================

TEST_F(NsmMoveInterfacesTest, Invoke_TwoInterfacesDifferentValues_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string path1 = "/xyz/openbmc_project/inventory/invk/dim1";
    const std::string path2 = "/xyz/openbmc_project/inventory/invk/dim2";
    auto pdi1 = std::make_shared<DimensionIntf>(bus, path1.c_str());
    auto pdi2 = std::make_shared<DimensionIntf>(bus, path2.c_str());
    pdi1->depth(10);
    pdi2->depth(20); // different value — invoke() must throw

    Interfaces<DimensionIntf> map = {
        {std::filesystem::path(path1), pdi1},
        {std::filesystem::path(path2), pdi2},
    };
    NsmInterfaces<DimensionIntf> interfaces(map);

    EXPECT_THROW(interfaces.invoke([](DimensionIntf& d) { return d.depth(); }),
                 std::runtime_error);
}

// ============================================================================
// NsmInterfaces<T>::invoke() non-void — same-value path (nsmInterface.hpp
// line 128): two interfaces with equal depth() — loop runs, no throw.
// ============================================================================

TEST_F(NsmMoveInterfacesTest, Invoke_TwoInterfaces_SameValue_NoThrow)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string path1 = "/xyz/openbmc_project/inventory/invk2/dim1";
    const std::string path2 = "/xyz/openbmc_project/inventory/invk2/dim2";
    auto pdi1 = std::make_shared<DimensionIntf>(bus, path1.c_str());
    auto pdi2 = std::make_shared<DimensionIntf>(bus, path2.c_str());
    pdi1->depth(42);
    pdi2->depth(42); // same value — loop runs but does NOT throw

    Interfaces<DimensionIntf> map = {
        {std::filesystem::path(path1), pdi1},
        {std::filesystem::path(path2), pdi2},
    };
    NsmInterfaces<DimensionIntf> interfaces(map);

    uint64_t result = 0;
    EXPECT_NO_THROW(
        result = interfaces.invoke([](DimensionIntf& d) { return d.depth(); }));
    EXPECT_EQ(result, 42u);
}

// ============================================================================
// NsmInterfaces<T>::moveInterfaces() — FALSE branch (nsmInterface.hpp line 80)
// When the destination already contains the same path key, the entry is NOT
// added and moveInterfaces() returns false.
// ============================================================================

TEST_F(NsmMoveInterfacesTest, MoveInterfaces_DuplicatePath_ReturnsFalse)
{
    auto& bus = utils::DBusHandler::getBus();
    // Two distinct D-Bus objects (different registration paths)
    auto pdi1 = std::make_shared<DimensionIntf>(
        bus, "/xyz/openbmc_project/inventory/dup/pdi1");
    auto pdi2 = std::make_shared<DimensionIntf>(
        bus, "/xyz/openbmc_project/inventory/dup/pdi2");

    // Both providers use THE SAME map key
    const std::filesystem::path sameKey{
        "/xyz/openbmc_project/inventory/dup/common"};

    Interfaces<DimensionIntf> map1 = {{sameKey, pdi1}};
    Interfaces<DimensionIntf> map2 = {{sameKey, pdi2}};
    NsmInterfaceProvider<DimensionIntf> prov1("dupDim1", "NSM_Test", map1);
    NsmInterfaceProvider<DimensionIntf> prov2("dupDim2", "NSM_Test", map2);

    // sameKey already in prov1 → moveInterfaces must skip it and return false
    bool moved = prov1.moveInterfaces(prov2);
    EXPECT_FALSE(moved);
    // prov1 still holds its original 1 interface
    EXPECT_EQ(prov1.size(), 1u);
    // prov2 was cleared by moveInterfaces
    EXPECT_EQ(prov2.size(), 0u);
}

// ============================================================================
// NsmInterfaces<T> constructor — empty-map throw path for additional types //
// (nsmChassis.hpp / nsmChassisPCIeDevice.hpp /
// nsmErrorInjection.hpp) Each template instantiation generates a separate copy;
// we must pass an empty Interfaces<T> map to trigger the throw branch for each
// type.
// ============================================================================

TEST_F(NsmMoveInterfacesTest, Constructor_Empty_UuidIntf_Throws)
{
    Interfaces<UuidIntf> m;
    EXPECT_THROW((NsmInterfaces<UuidIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest,
       Constructor_Empty_AssociationDefinitionsInft_Throws)
{
    Interfaces<AssociationDefinitionsInft> m;
    EXPECT_THROW((NsmInterfaces<AssociationDefinitionsInft>(m)),
                 std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_LocationIntf_Throws)
{
    Interfaces<LocationIntf> m;
    EXPECT_THROW((NsmInterfaces<LocationIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_LocationCodeIntf_Throws)
{
    Interfaces<LocationCodeIntf> m;
    EXPECT_THROW((NsmInterfaces<LocationCodeIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_LocationContextIntf_Throws)
{
    Interfaces<LocationContextIntf> m;
    EXPECT_THROW((NsmInterfaces<LocationContextIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_ReplaceableIntf_Throws)
{
    Interfaces<ReplaceableIntf> m;
    EXPECT_THROW((NsmInterfaces<ReplaceableIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_MctpUuidIntf_Throws)
{
    Interfaces<MctpUuidIntf> m;
    EXPECT_THROW((NsmInterfaces<MctpUuidIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_ChassisIntf_Throws)
{
    Interfaces<ChassisIntf> m;
    EXPECT_THROW((NsmInterfaces<ChassisIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_ItemIntf_Throws)
{
    Interfaces<ItemIntf> m;
    EXPECT_THROW((NsmInterfaces<ItemIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_PowerStateIntf_Throws)
{
    Interfaces<PowerStateIntf> m;
    EXPECT_THROW((NsmInterfaces<PowerStateIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_OperationalStatusIntf_Throws)
{
    Interfaces<OperationalStatusIntf> m;
    EXPECT_THROW((NsmInterfaces<OperationalStatusIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_HealthIntf_Throws)
{
    Interfaces<HealthIntf> m;
    EXPECT_THROW((NsmInterfaces<HealthIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_PCIeDeviceIntf_Throws)
{
    Interfaces<PCIeDeviceIntf> m;
    EXPECT_THROW((NsmInterfaces<PCIeDeviceIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest,
       Constructor_Empty_ErrorInjectionCapabilityIntf_Throws)
{
    Interfaces<ErrorInjectionCapabilityIntf> m;
    EXPECT_THROW((NsmInterfaces<ErrorInjectionCapabilityIntf>(m)),
                 std::runtime_error);
}

// ============================================================================
// NsmInterfaces<T>::invoke() void — TRUE constexpr branch (nsmInterface.hpp
// line 150-152): lambda accepting (const std::string& path, IntfType&)
// triggers func(path, *pdi) instead of func(*pdi, args...).
// ============================================================================

// ============================================================================
// NsmInterfaces<T> constructor — empty-map throw path for additional types //
// (nsmNVSwitchAndNVMgmtNICChassisAssembly,
// nsmFirmwareInventory,
//  nsmNumericSensorComposite, nsmProcessor, nsmSwitch, nsmAssetIntf,
//  nsmPortDisableFuture, nsmFpgaPort, nsmInventoryProperty)
// ============================================================================

TEST_F(NsmMoveInterfacesTest, Constructor_Empty_AreaIntf_Throws)
{
    Interfaces<AreaIntf> m;
    EXPECT_THROW((NsmInterfaces<AreaIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_AssemblyIntf_Throws)
{
    Interfaces<AssemblyIntf> m;
    EXPECT_THROW((NsmInterfaces<AssemblyIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest,
       Constructor_Empty_AssociationDefinitionsIntf_Throws)
{
    Interfaces<AssociationDefinitionsIntf> m;
    EXPECT_THROW((NsmInterfaces<AssociationDefinitionsIntf>(m)),
                 std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_DecoratorAreaIntf_Throws)
{
    Interfaces<DecoratorAreaIntf> m;
    EXPECT_THROW((NsmInterfaces<DecoratorAreaIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_DimmMemoryMetricsIntf_Throws)
{
    Interfaces<DimmMemoryMetricsIntf> m;
    EXPECT_THROW((NsmInterfaces<DimmMemoryMetricsIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_L1PowerModeIntf_Throws)
{
    Interfaces<L1PowerModeIntf> m;
    EXPECT_THROW((NsmInterfaces<L1PowerModeIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_NsmAssetIntf_Throws)
{
    Interfaces<NsmAssetIntf> m;
    EXPECT_THROW((NsmInterfaces<NsmAssetIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_NvLinkDisableFutureIntf_Throws)
{
    Interfaces<NvLinkDisableFutureIntf> m;
    EXPECT_THROW((NsmInterfaces<NvLinkDisableFutureIntf>(m)),
                 std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_PortStateIntf_Throws)
{
    Interfaces<PortStateIntf> m;
    EXPECT_THROW((NsmInterfaces<PortStateIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_RevisionIntf_Throws)
{
    Interfaces<RevisionIntf> m;
    EXPECT_THROW((NsmInterfaces<RevisionIntf>(m)), std::runtime_error);
}
TEST_F(NsmMoveInterfacesTest, Constructor_Empty_VersionIntf_Throws)
{
    Interfaces<VersionIntf> m;
    EXPECT_THROW((NsmInterfaces<VersionIntf>(m)), std::runtime_error);
}

TEST_F(NsmMoveInterfacesTest, Invoke_Void_WithPathArg_TrueConstexprBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string path = "/xyz/openbmc_project/inventory/invk_void/dim1";
    auto pdi = std::make_shared<DimensionIntf>(bus, path.c_str());
    pdi->depth(77);

    Interfaces<DimensionIntf> map = {{std::filesystem::path(path), pdi}};
    NsmInterfaces<DimensionIntf> interfaces(map);

    // Lambda taking (string, IntfType&): triggers TRUE constexpr branch
    std::string capturedPath;
    interfaces.invoke([&capturedPath](const std::string& p, DimensionIntf& d) {
        capturedPath = p;
        d.depth(99);
    });
    // depth was updated via the path-argument overload
    EXPECT_EQ(pdi->depth(), 99u);
    // The path string was passed to the lambda
    EXPECT_FALSE(capturedPath.empty());
}
