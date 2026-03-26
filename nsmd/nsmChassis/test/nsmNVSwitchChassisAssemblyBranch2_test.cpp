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
 * Branch coverage for nsmNVSwitchAndNVMgmtNICChassisAssembly.cpp
 *
 * Covers:
 * - NsmInventoryProperty<NsmAssetIntf> genRequestMsg encode fail
 * - NsmInventoryProperty<NsmAssetIntf> handleResponseMsg success/errorCC/decode
 * - NsmInventoryProperty<RevisionIntf> genRequestMsg/handleResponseMsg
 * - Factory: createNsmChassisAssembly with ChassisAttributes branch
 *   (PhysicalContext/LocationType present)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmChassis/nsmNVSwitchAndNVMgmtNICChassisAssembly.hpp"
#include "nsmInventoryProperty.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine createNsmNVSwitchChassisAssembly(SensorManager& manager,
                                                      const std::string& intf,
                                                      const std::string& path);
requester::Coroutine createNsmNVLinkMgmtNicChassisAssembly(
    SensorManager& manager, const std::string& intf, const std::string& path);
void createAssemblyAsset(std::shared_ptr<NsmDevice> device,
                         const std::string& chassisName,
                         const std::string& name, const std::string& baseType,
                         const dbus::PropertyMap& allCurrentIfaceProperties);
void createPhysicalContext(std::shared_ptr<NsmDevice> device,
                           const std::string& chassisName,
                           const std::string& name, const std::string& baseType,
                           const dbus::PropertyMap& allCurrentIfaceProperties);
void createAssemblyHealth(std::shared_ptr<NsmDevice> device,
                          const std::string& chassisName,
                          const std::string& name, const std::string& baseType);
void createLocationType(std::shared_ptr<NsmDevice> device,
                        const std::string& chassisName, const std::string& name,
                        const std::string& baseType,
                        const dbus::PropertyMap& allCurrentIfaceProperties);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmNVSwitchAssemblyBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "NVSwitch_B2";
    const std::string name = "NVSwitch0_AsmB2";
    const std::string baseType = "NSM_NVSwitch_ChassisAssembly";
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmNVSwitchAssemblyBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmNVSwitchAssemblyBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// createAssemblyAsset: genRequestMsg / handleResponseMsg via
// NsmInventoryProperty<NsmAssetIntf> sensors (DEVICE_PART_NUMBER, etc.)
// ============================================================================

TEST_F(NsmNVSwitchAssemblyBranch2Test,
       AssetInventoryProperty_GenRequestMsg_Success)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("SensorOK");

    createAssemblyAsset(gpu, chassisName, name, baseType, currProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);
    auto request = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmNVSwitchAssemblyBranch2Test,
       AssetInventoryProperty_GenRequestMsg_EncodeFail)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("EncFail");

    createAssemblyAsset(gpu, chassisName, name, baseType, currProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);
    // NSM_INSTANCE_MAX + 1 triggers encode failure
    auto request = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmNVSwitchAssemblyBranch2Test,
       AssetInventoryProperty_HandleResponseMsg_Success)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("RespOK");

    createAssemblyAsset(gpu, chassisName, name, baseType, currProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    const std::string partNum = "900-12345-6789-000";
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            partNum.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    [[maybe_unused]] auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, partNum.size(),
        reinterpret_cast<const uint8_t*>(partNum.data()), responseMsg);

    rc = sensor->handleResponseMsg(responseMsg, responseData.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST_F(NsmNVSwitchAssemblyBranch2Test,
       AssetInventoryProperty_HandleResponseMsg_ErrorCC)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("RespCC");

    createAssemblyAsset(gpu, chassisName, name, baseType, currProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    // Valgrind-safe error CC response
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    responseMsg->payload[1] = NSM_ERROR;

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmNVSwitchAssemblyBranch2Test,
       AssetInventoryProperty_HandleResponseMsg_DecodeFail)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("RespDec");

    createAssemblyAsset(gpu, chassisName, name, baseType, currProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    // Valid buffer but short responseLen triggers decode failure
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, sizeof(nsm_msg_hdr));
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// RevisionIntf: NsmInventoryProperty<RevisionIntf> genRequestMsg /
// handleResponseMsg via sensors created by createNsmChassisAssembly
// when type == baseType
// ============================================================================

TEST_F(NsmNVSwitchAssemblyBranch2Test,
       RevisionInventoryProperty_GenRequestMsg_Success)
{
    const std::string baseIfaceName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";
    const std::string uniquePath = "/xyz/test/nvswasm/rev_gen_ok";

    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      baseIfaceName);
    baseMap["UUID"] = deviceUuid;
    baseMap["Name"] = std::string("NVSwitch0_RevOK");
    baseMap["ChassisName"] = std::string("HGX_Chassis_RevOK");
    baseMap["Type"] = std::string("NSM_NVSwitch_ChassisAssembly");

    const size_t before = gpu->staticSensors.size();
    nsm::createNsmNVSwitchChassisAssembly(mockManager, baseIfaceName,
                                          uniquePath);
    // type==baseType adds assemblyObject + versionSensor
    ASSERT_GE(gpu->staticSensors.size(), before + 2);

    // The version sensor is the last static sensor added
    auto sensor = std::dynamic_pointer_cast<NsmSensor>(
        gpu->staticSensors[gpu->staticSensors.size() - 1]);
    ASSERT_NE(sensor, nullptr);
    auto request = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmNVSwitchAssemblyBranch2Test,
       RevisionInventoryProperty_GenRequestMsg_EncodeFail)
{
    const std::string baseIfaceName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";
    const std::string uniquePath = "/xyz/test/nvswasm/rev_gen_fail";

    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      baseIfaceName);
    baseMap["UUID"] = deviceUuid;
    baseMap["Name"] = std::string("NVSwitch0_RevFail");
    baseMap["ChassisName"] = std::string("HGX_Chassis_RevFail");
    baseMap["Type"] = std::string("NSM_NVSwitch_ChassisAssembly");

    const size_t before = gpu->staticSensors.size();
    nsm::createNsmNVSwitchChassisAssembly(mockManager, baseIfaceName,
                                          uniquePath);
    ASSERT_GE(gpu->staticSensors.size(), before + 2);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(
        gpu->staticSensors[gpu->staticSensors.size() - 1]);
    ASSERT_NE(sensor, nullptr);
    auto request = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmNVSwitchAssemblyBranch2Test,
       RevisionInventoryProperty_HandleResponseMsg_Success)
{
    const std::string baseIfaceName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";
    const std::string uniquePath = "/xyz/test/nvswasm/rev_resp_ok";

    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      baseIfaceName);
    baseMap["UUID"] = deviceUuid;
    baseMap["Name"] = std::string("NVSwitch0_RevRespOK");
    baseMap["ChassisName"] = std::string("HGX_Chassis_RevRespOK");
    baseMap["Type"] = std::string("NSM_NVSwitch_ChassisAssembly");

    const size_t before = gpu->staticSensors.size();
    nsm::createNsmNVSwitchChassisAssembly(mockManager, baseIfaceName,
                                          uniquePath);
    ASSERT_GE(gpu->staticSensors.size(), before + 2);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(
        gpu->staticSensors[gpu->staticSensors.size() - 1]);
    ASSERT_NE(sensor, nullptr);

    const std::string version = "1.2.3";
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            version.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    [[maybe_unused]] auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, version.size(),
        reinterpret_cast<const uint8_t*>(version.data()), responseMsg);

    rc = sensor->handleResponseMsg(responseMsg, responseData.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST_F(NsmNVSwitchAssemblyBranch2Test,
       RevisionInventoryProperty_HandleResponseMsg_ErrorCC)
{
    const std::string baseIfaceName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";
    const std::string uniquePath = "/xyz/test/nvswasm/rev_resp_cc";

    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      baseIfaceName);
    baseMap["UUID"] = deviceUuid;
    baseMap["Name"] = std::string("NVSwitch0_RevRespCC");
    baseMap["ChassisName"] = std::string("HGX_Chassis_RevRespCC");
    baseMap["Type"] = std::string("NSM_NVSwitch_ChassisAssembly");

    const size_t before = gpu->staticSensors.size();
    nsm::createNsmNVSwitchChassisAssembly(mockManager, baseIfaceName,
                                          uniquePath);
    ASSERT_GE(gpu->staticSensors.size(), before + 2);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(
        gpu->staticSensors[gpu->staticSensors.size() - 1]);
    ASSERT_NE(sensor, nullptr);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    responseMsg->payload[1] = NSM_ERROR;

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmNVSwitchAssemblyBranch2Test,
       RevisionInventoryProperty_HandleResponseMsg_DecodeFail)
{
    const std::string baseIfaceName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";
    const std::string uniquePath = "/xyz/test/nvswasm/rev_resp_dec";

    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      baseIfaceName);
    baseMap["UUID"] = deviceUuid;
    baseMap["Name"] = std::string("NVSwitch0_RevRespDec");
    baseMap["ChassisName"] = std::string("HGX_Chassis_RevRespDec");
    baseMap["Type"] = std::string("NSM_NVSwitch_ChassisAssembly");

    const size_t before = gpu->staticSensors.size();
    nsm::createNsmNVSwitchChassisAssembly(mockManager, baseIfaceName,
                                          uniquePath);
    ASSERT_GE(gpu->staticSensors.size(), before + 2);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(
        gpu->staticSensors[gpu->staticSensors.size() - 1]);
    ASSERT_NE(sensor, nullptr);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, sizeof(nsm_msg_hdr));
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// Factory: createNsmChassisAssembly with NSM_Chassis_Attributes + both
// PhysicalContext and LocationType present (covers TRUE branches)
// ============================================================================

TEST_F(NsmNVSwitchAssemblyBranch2Test,
       Factory_ChassisAttributes_WithPhysicalContextAndLocationType)
{
    const std::string baseIfaceName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";
    const std::string subIface = baseIfaceName + ".ChassisAttributes";
    const std::string uniquePath = "/xyz/test/nvswasm/attrs_full";

    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                      baseIfaceName);
    baseMap["UUID"] = deviceUuid;
    baseMap["Name"] = std::string("NVSwitch0_Full");
    baseMap["ChassisName"] = std::string("HGX_Chassis_Full");

    auto& attrMap = utils::MockDbusAsync::propertyMap(uniquePath, subIface);
    attrMap["Type"] = std::string("NSM_Chassis_Attributes");
    attrMap["Name"] = std::string("NVSwitch0_Full");
    attrMap["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area."
        "PhysicalContextType.Backplane");
    attrMap["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location."
        "LocationTypes.Slot");

    const size_t before = gpu->staticSensors.size();
    nsm::createNsmNVSwitchChassisAssembly(mockManager, subIface, uniquePath);
    // Health + 4 asset + physicalContext + locationType sensors
    EXPECT_GT(gpu->staticSensors.size(), before);
}

// ============================================================================
// NVLinkMgmtNic variant: createNsmNVLinkMgmtNicChassisAssembly with
// type==baseType (exercises the sibling factory path)
// ============================================================================

TEST_F(NsmNVSwitchAssemblyBranch2Test,
       NVLinkMgmtNic_TypeMatchesBase_AddsSensors)
{
    const std::string nicBaseIface =
        "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly";
    const std::string uniquePath = "/xyz/test/nvswasm/nic_base";

    auto& baseMap = utils::MockDbusAsync::propertyMap(uniquePath, nicBaseIface);
    baseMap["UUID"] = deviceUuid;
    baseMap["Name"] = std::string("NVMgmtNIC0_Asm");
    baseMap["ChassisName"] = std::string("HGX_Chassis_NIC");
    baseMap["Type"] = std::string("NSM_NVLinkMgmtNic_ChassisAssembly");

    const size_t before = gpu->staticSensors.size();
    nsm::createNsmNVLinkMgmtNicChassisAssembly(mockManager, nicBaseIface,
                                               uniquePath);
    // type==baseType → assemblyObject + versionSensor
    EXPECT_GE(gpu->staticSensors.size(), before + 2);
}
