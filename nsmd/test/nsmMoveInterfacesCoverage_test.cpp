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
#include "nsmChassis/nsmPCIeLTSSMState.hpp"
#include "nsmChassis/nsmWriteProtectedJumper.hpp"
#include "nsmDbusIfaceOverride/nsmMNNVLinkTopologyIntf.hpp"
#include "nsmErrorInjection/nsmErrorInjection.hpp"
#include "nsmInterface.hpp"
#include "nsmPort/nsmPCIePort.hpp"
#include "nsmPort/nsmPortInfo.hpp"
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
