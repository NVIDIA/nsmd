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
 * Branch coverage for nsmChassisAssembly.cpp
 *
 * Covers:
 * - NsmInventoryProperty<NsmAssetIntf> genRequestMsg (encode fail, success)
 * - NsmInventoryProperty<NsmAssetIntf> handleResponseMsg
 *   (success, errorCC, decode fail)
 * - Factory with missing optional properties (PhysicalContext, LocationType)
 * - AssemblyType branches: Device, Board, FRU, invalid default
 * - createChassisAssemblyAsset: Name present/absent in current props
 * - createChassisAssemblyAsset: AssemblyType present/absent in base props
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "nsmAssetIntf.hpp"
#include "nsmChassisAssembly.hpp"
#include "nsmInventoryProperty.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine
    nsmChassisAssemblyCreateSensors(SensorManager& manager,
                                    const std::string& interface,
                                    const std::string& objPath);
void createChassisAssemblyArea(std::shared_ptr<NsmDevice> device,
                               const std::string& chassisName,
                               const std::string& name,
                               dbus::PropertyMap& allCurrentIfaceProperties);
void createChassisAssemblyAsset(std::shared_ptr<NsmDevice> device,
                                const std::string& chassisName,
                                const std::string& name,
                                dbus::PropertyMap& allCurrentIfaceProperties,
                                dbus::PropertyMap& allBaseIfaceProperties);
void createChassisAssemblyHealth(std::shared_ptr<NsmDevice> device,
                                 const std::string& chassisName,
                                 const std::string& name);
void createChassisAssemblyLocation(
    std::shared_ptr<NsmDevice> device, const std::string& chassisName,
    const std::string& name, dbus::PropertyMap& allCurrentIfaceProperties);
} // namespace nsm

using namespace nsm;

static const std::string kBaseIntf =
    "xyz.openbmc_project.Configuration.NSM_ChassisAssembly";
static const std::string kAttrIntf =
    "xyz.openbmc_project.Configuration.NSM_ChassisAssembly.ChassisAttributes";
static const uuid_t kGpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

struct NsmChassisAssemblyBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_GPU_SXM_B2";
    const std::string name = "AssemblyB2";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmChassisAssemblyBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(kGpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmChassisAssemblyBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// Direct helper tests
// ============================================================================

// createChassisAssemblyHealth: direct call
TEST_F(NsmChassisAssemblyBranch2Test, CreateChassisAssemblyHealth_DirectCall)
{
    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyHealth(gpu, chassisName, name);
    EXPECT_EQ(gpu->staticSensors.size(), before + 1);
}

// createChassisAssemblyArea: direct call with valid PhysicalContext
TEST_F(NsmChassisAssemblyBranch2Test, CreateChassisAssemblyArea_ValidContext)
{
    dbus::PropertyMap props;
    props["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyArea(gpu, chassisName, name, props);
    EXPECT_EQ(gpu->staticSensors.size(), before + 1);
}

// createChassisAssemblyLocation: direct call with valid LocationType
TEST_F(NsmChassisAssemblyBranch2Test,
       CreateChassisAssemblyLocation_ValidLocation)
{
    dbus::PropertyMap props;
    props["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyLocation(gpu, chassisName, name, props);
    EXPECT_EQ(gpu->staticSensors.size(), before + 1);
}

// ============================================================================
// createChassisAssemblyAsset: AssemblyType branches
// ============================================================================

// AssemblyType == "Device" -> partNumberId = DEVICE_PART_NUMBER
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_AssemblyType_Device)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("DeviceAsset");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Device");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    // partNumber + serialNumber + model + buildDate = 4
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// AssemblyType == "Board" -> partNumberId = BOARD_PART_NUMBER
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_AssemblyType_Board)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("BoardAsset");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Board");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// AssemblyType == "FRU" -> partNumberId = FRU_PART_NUMBER
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_AssemblyType_FRU)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("FRUAsset");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("FRU");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// AssemblyType == "InvalidType" -> else branch -> default BOARD_PART_NUMBER
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_AssemblyType_Invalid)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("InvalidAsset");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("InvalidType");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// AssemblyType absent -> assemblyType="" -> else branch
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_AssemblyType_Missing)
{
    dbus::PropertyMap currProps;
    dbus::PropertyMap baseProps;
    // No "AssemblyType" key -> FALSE branch at L65 -> assemblyType=""

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// Name present in current props -> assetsName set (TRUE branch at L58)
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_NamePresent)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("NamedAsset");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Device");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// Name absent in current props -> assetsName="" (FALSE branch at L58)
TEST_F(NsmChassisAssemblyBranch2Test, CreateAsset_NameAbsent)
{
    dbus::PropertyMap currProps;
    // No "Name" key -> FALSE branch
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Device");

    const size_t before = gpu->staticSensors.size();
    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    EXPECT_EQ(gpu->staticSensors.size(), before + 4);
}

// ============================================================================
// NsmInventoryProperty genRequestMsg / handleResponseMsg
// via sensors created by createChassisAssemblyAsset
// ============================================================================

TEST_F(NsmChassisAssemblyBranch2Test, InventoryProperty_GenRequestMsg_Success)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("SensorTest");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Device");

    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);
    auto request = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmChassisAssemblyBranch2Test,
       InventoryProperty_GenRequestMsg_EncodeFail)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("EncFail");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Device");

    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);
    auto request = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmChassisAssemblyBranch2Test,
       InventoryProperty_HandleResponseMsg_Success)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("RespOK");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Device");

    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    const std::string partNum = "900-00000-0000-000";
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            partNum.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, partNum.size(),
        reinterpret_cast<const uint8_t*>(partNum.data()), responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor->handleResponseMsg(responseMsg, responseData.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST_F(NsmChassisAssemblyBranch2Test,
       InventoryProperty_HandleResponseMsg_ErrorCC)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("RespCC");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("Board");

    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    // Build error CC response (min 7 bytes for valgrind safety)
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    responseMsg->payload[1] = NSM_ERROR;

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmChassisAssemblyBranch2Test,
       InventoryProperty_HandleResponseMsg_DecodeFail)
{
    dbus::PropertyMap currProps;
    currProps["Name"] = std::string("RespDec");
    dbus::PropertyMap baseProps;
    baseProps["AssemblyType"] = std::string("FRU");

    createChassisAssemblyAsset(gpu, chassisName, name, currProps, baseProps);
    ASSERT_GE(gpu->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(gpu->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    // Short buffer -> decode fail
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, sizeof(nsm_msg_hdr));
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// Factory function: nsmChassisAssemblyCreateSensors branch coverage
// ============================================================================

// Base properties fail -> early return
TEST_F(NsmChassisAssemblyBranch2Test, Factory_BasePropertiesFail)
{
    const std::string objPath = "/xyz/test/chassis_asm_b2/base_fail";
    // Do NOT register base interface property map
    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");

    const size_t before = gpu->staticSensors.size();
    nsmChassisAssemblyCreateSensors(mockManager, kAttrIntf, objPath);
    EXPECT_EQ(before, gpu->staticSensors.size());
}

// Missing ChassisName -> chassisName="" (FALSE branch at L160)
TEST_F(NsmChassisAssemblyBranch2Test, Factory_MissingChassisName)
{
    const std::string objPath = "/xyz/test/chassis_asm_b2/no_cname";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    // No "ChassisName" key
    baseMap["Name"] = std::string("Assembly1");
    baseMap["UUID"] = kGpuUuid;

    auto& attrMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    attrMap["Type"] = std::string("NSM_Chassis_Attributes");

    const size_t before = gpu->staticSensors.size();
    nsmChassisAssemblyCreateSensors(mockManager, kAttrIntf, objPath);
    EXPECT_GT(gpu->staticSensors.size(), before);
}

// Missing Name -> name="" (FALSE branch at L165)
TEST_F(NsmChassisAssemblyBranch2Test, Factory_MissingName_Throws)
{
    const std::string objPath = "/xyz/test/chassis_asm_b2/no_name";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["ChassisName"] = chassisName;
    baseMap["UUID"] = kGpuUuid;
    // No "Name" key -> name="" -> D-Bus path ending with "/" is invalid

    auto& attrMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    attrMap["Type"] = std::string("NSM_ChassisAssembly");

    EXPECT_THROW_COROUTINE(
        nsmChassisAssemblyCreateSensors(mockManager, kBaseIntf, objPath),
        std::exception);
}

// Missing Type -> type="" -> neither branch matches -> no sensors
TEST_F(NsmChassisAssemblyBranch2Test, Factory_MissingType_NoSensors)
{
    const std::string objPath = "/xyz/test/chassis_asm_b2/no_type";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["ChassisName"] = chassisName;
    baseMap["Name"] = name;
    baseMap["UUID"] = kGpuUuid;

    auto& attrMap [[maybe_unused]] =
        utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    // No "Type" key -> type=""

    const size_t before = gpu->staticSensors.size();
    nsmChassisAssemblyCreateSensors(mockManager, kAttrIntf, objPath);
    EXPECT_EQ(before, gpu->staticSensors.size());
}

// Missing UUID -> uuid="" -> parseStaticUuid throws
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmChassisAssemblyBranch2Test, Factory_MissingUUID_Throws)
{
    const std::string objPath = "/xyz/test/chassis_asm_b2/no_uuid";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["ChassisName"] = chassisName;
    baseMap["Name"] = name;
    // No "UUID" key

    auto& attrMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    attrMap["Type"] = std::string("NSM_ChassisAssembly");

    EXPECT_THROW_COROUTINE(
        nsmChassisAssemblyCreateSensors(mockManager, kBaseIntf, objPath),
        std::runtime_error);
}
#endif

// type == "NSM_ChassisAssembly" -> creates AssemblyIntf sensor
TEST_F(NsmChassisAssemblyBranch2Test, Factory_TypeAssembly_CreatesSensor)
{
    const std::string objPath = "/xyz/test/chassis_asm_b2/type_assembly";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["ChassisName"] = chassisName;
    baseMap["Name"] = name;
    baseMap["UUID"] = kGpuUuid;
    // When interface == baseInterface, both reads use the same property map
    baseMap["Type"] = std::string("NSM_ChassisAssembly");

    const size_t before = gpu->staticSensors.size();
    nsmChassisAssemblyCreateSensors(mockManager, kBaseIntf, objPath);
    EXPECT_EQ(gpu->staticSensors.size(), before + 1);
}

// type == "NSM_Chassis_Attributes" with all optional props
TEST_F(NsmChassisAssemblyBranch2Test, Factory_Attributes_AllOptional)
{
    const std::string objPath = "/xyz/test/chassis_asm_b2/attr_all";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["ChassisName"] = chassisName;
    baseMap["Name"] = name;
    baseMap["UUID"] = kGpuUuid;
    baseMap["AssemblyType"] = std::string("Device");

    auto& attrMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    attrMap["Type"] = std::string("NSM_Chassis_Attributes");
    attrMap["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");
    attrMap["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    attrMap["Name"] = std::string("GPU_Assembly");

    const size_t before = gpu->staticSensors.size();
    nsmChassisAssemblyCreateSensors(mockManager, kAttrIntf, objPath);
    // Asset(4) + Health(1) + Area(1) + Location(1) = 7
    EXPECT_GE(gpu->staticSensors.size(), before + 7);
}

// type == "NSM_Chassis_Attributes" without PhysicalContext or LocationType
TEST_F(NsmChassisAssemblyBranch2Test, Factory_Attributes_NoOptional)
{
    const std::string objPath = "/xyz/test/chassis_asm_b2/attr_none";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["ChassisName"] = chassisName;
    baseMap["Name"] = name;
    baseMap["UUID"] = kGpuUuid;
    baseMap["AssemblyType"] = std::string("Board");

    auto& attrMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    attrMap["Type"] = std::string("NSM_Chassis_Attributes");
    // No PhysicalContext -> FALSE branch at L193
    // No LocationType -> FALSE branch at L198

    const size_t before = gpu->staticSensors.size();
    nsmChassisAssemblyCreateSensors(mockManager, kAttrIntf, objPath);
    // Asset(4) + Health(1) = 5, no Area or Location
    EXPECT_EQ(gpu->staticSensors.size(), before + 5);
}

// type == "NSM_Chassis_Attributes" with only PhysicalContext (no LocationType)
TEST_F(NsmChassisAssemblyBranch2Test, Factory_Attributes_OnlyPhysicalContext)
{
    const std::string objPath = "/xyz/test/chassis_asm_b2/attr_phys_only";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["ChassisName"] = chassisName;
    baseMap["Name"] = name;
    baseMap["UUID"] = kGpuUuid;

    auto& attrMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    attrMap["Type"] = std::string("NSM_Chassis_Attributes");
    attrMap["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");
    // No LocationType

    const size_t before = gpu->staticSensors.size();
    nsmChassisAssemblyCreateSensors(mockManager, kAttrIntf, objPath);
    // Asset(4) + Health(1) + Area(1) = 6
    EXPECT_EQ(gpu->staticSensors.size(), before + 6);
}

// type == "NSM_Chassis_Attributes" with only LocationType (no PhysicalContext)
TEST_F(NsmChassisAssemblyBranch2Test, Factory_Attributes_OnlyLocationType)
{
    const std::string objPath = "/xyz/test/chassis_asm_b2/attr_loc_only";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["ChassisName"] = chassisName;
    baseMap["Name"] = name;
    baseMap["UUID"] = kGpuUuid;

    auto& attrMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    attrMap["Type"] = std::string("NSM_Chassis_Attributes");
    attrMap["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    // No PhysicalContext

    const size_t before = gpu->staticSensors.size();
    nsmChassisAssemblyCreateSensors(mockManager, kAttrIntf, objPath);
    // Asset(4) + Health(1) + Location(1) = 6
    EXPECT_EQ(gpu->staticSensors.size(), before + 6);
}
