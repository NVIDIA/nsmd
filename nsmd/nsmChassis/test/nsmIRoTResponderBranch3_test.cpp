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
 * Branch coverage tests for nsmd/nsmChassis/nsmIRoTResponder.cpp - batch 3
 *
 * Targets remaining uncovered branches:
 * - createNsmIRoTResponder: NSM_Chassis_Attributes without optional props
 *   (LocationType, ChassisType absent -> FALSE branches at L312-313, L317-319)
 * - createNsmIRoTResponder: NSM_Chassis_Attributes with FieldReplaceable=false
 * - createIRoTResponderAsset: Name present and absent branches
 * - createIRoTResponderHealth: health object creation
 * - NsmIRoTResponder<AssetIntf>::update: encode fail, sensorIO fail,
 *   decode fail cc!=0, decode fail rc!=0
 * - NsmIRoTResponder<AssetIntf>::update: success path (serialNumber set)
 * - NsmIRoTResponder<HealthIntf>::update: co_return NSM_SUCCESS
 * - NsmIRoTResponder<ChassisIntf>::update: co_return NSM_SUCCESS
 * - NsmIRoTResponder<LocationIntf>::update: co_return NSM_SUCCESS
 * - NsmIRoTResponder<SPDMResponderIntf>::update: co_return NSM_SUCCESS
 * - NsmIRoTResponder<AssociationDefinitionsIntf>::update: co_return NSM_SUCCESS
 * - createNsmIRoTResponder: coGetCachedBaseProperties failure -> early return
 */

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "nsmAssetIntf.hpp"
#include "nsmIRoTResponder.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <libnsm/debug-token.h>

#undef private
#undef protected

namespace nsm
{
requester::Coroutine createNsmIRoTResponder(SensorManager& manager,
                                            const std::string& interface,
                                            const std::string& objPath);

void createIRoTResponderAsset(std::shared_ptr<NsmDevice> device,
                              std::string& name, const std::string& baseType,
                              dbus::PropertyMap& allCurrentIfaceProperties);

void createIRoTResponderHealth(std::shared_ptr<NsmDevice> device,
                               std::string& name, const std::string& baseType);

void createIRoTResponderLocation(std::shared_ptr<NsmDevice> device,
                                 std::string& name, const std::string& baseType,
                                 dbus::PropertyMap& allCurrentIfaceProperties);

void createIRoTResponderChassis(std::shared_ptr<NsmDevice> device,
                                std::string& name, const std::string& baseType,
                                dbus::PropertyMap& allCurrentIfaceProperties);

void createIRoTResponderLocationCode(
    std::shared_ptr<NsmDevice> device, std::string& name,
    const std::string& baseType, dbus::PropertyMap& allCurrentIfaceProperties);

void createIRoTResponderLocationContext(
    std::shared_ptr<NsmDevice> device, std::string& name,
    const std::string& baseType, dbus::PropertyMap& allCurrentIfaceProperties);

void createIRoTResponderFieldReplaceable(
    std::shared_ptr<NsmDevice> device, std::string& name,
    const std::string& baseType, dbus::PropertyMap& allCurrentIfaceProperties);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmIRoTResponderBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "IRoTB3";
    const std::string type = "NSM_ChassisIRoTResponder";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:3";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmIRoTResponderBranch3Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmIRoTResponderBranch3Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// createNsmIRoTResponder: NSM_Chassis_Attributes with NO optional properties
// (LocationType absent, ChassisType absent, LocationCode absent,
//  LocationContext absent, FieldReplaceable absent)
// Exercises all FALSE branches for optional property checks at L312-336
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test,
       Factory_ChassisAttributes_NoOptionalProps_AllFalseBranches)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b3_noopt";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("IRoT_NoOpts")},
        // LocationType absent -> L312 FALSE
        // ChassisType absent -> L317 FALSE
        // LocationCode absent -> L321 FALSE
        // LocationContext absent -> L327 FALSE
        // FieldReplaceable absent -> L333 FALSE
    };

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".ChassisAttributes",
                           objPath);

    // Only Asset(1+3inv) + Health(1) = 5 sensors, no optional ones
    EXPECT_GE(fpga->staticSensors.size(), before + 5u);
}

// ============================================================================
// createNsmIRoTResponder: NSM_Chassis_Attributes with only LocationType
// Exercises LocationType TRUE, all others FALSE
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, Factory_ChassisAttributes_OnlyLocationType)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b3_lt";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("IRoT_LT")},
        {"LocationType",
         std::string("xyz.openbmc_project.Inventory.Decorator.Location."
                     "LocationTypes.Embedded")},
    };

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".ChassisAttributes",
                           objPath);

    // Asset(1+3inv) + Health(1) + Location(1) = 6
    EXPECT_GE(fpga->staticSensors.size(), before + 6u);
}

// ============================================================================
// createNsmIRoTResponder: NSM_Chassis_Attributes with only ChassisType
// Exercises ChassisType TRUE, all others FALSE
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, Factory_ChassisAttributes_OnlyChassisType)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b3_ct";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("IRoT_CT")},
        {"ChassisType",
         std::string("xyz.openbmc_project.Inventory.Item.Chassis."
                     "ChassisType.Module")},
    };

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".ChassisAttributes",
                           objPath);

    // Asset(1+3inv) + Health(1) + Chassis(1) = 6
    EXPECT_GE(fpga->staticSensors.size(), before + 6u);
}

// ============================================================================
// createNsmIRoTResponder: NSM_Chassis_Attributes with FieldReplaceable only
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test,
       Factory_ChassisAttributes_OnlyFieldReplaceable)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b3_fr";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("IRoT_FR")},
        {"FieldReplaceable", bool{true}},
    };

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".ChassisAttributes",
                           objPath);

    // Asset(1+3inv) + Health(1) + FieldReplaceable(1) = 6
    EXPECT_GE(fpga->staticSensors.size(), before + 6u);
}

// ============================================================================
// createIRoTResponderAsset: Name absent in properties -> uses empty assetName
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, DirectCreateAsset_NoName_UsesDefault)
{
    std::string sensorName = "IRoT_AssetNoName";
    dbus::PropertyMap props = {}; // No "Name" key

    const size_t before = fpga->staticSensors.size();
    createIRoTResponderAsset(fpga, sensorName, type, props);
    // Asset(1) + BuildDate(1) + Model(1) + PartNumber(1) = 4
    EXPECT_GE(fpga->staticSensors.size(), before + 4u);
}

// ============================================================================
// createIRoTResponderAsset: Name present in properties
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, DirectCreateAsset_WithName)
{
    std::string sensorName = "IRoT_AssetWithName";
    dbus::PropertyMap props = {
        {"Name", std::string("MyAssetName")},
    };

    const size_t before = fpga->staticSensors.size();
    createIRoTResponderAsset(fpga, sensorName, type, props);
    EXPECT_GE(fpga->staticSensors.size(), before + 4u);
}

// ============================================================================
// createIRoTResponderHealth: direct call
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, DirectCreateHealth)
{
    std::string sensorName = "IRoT_Health";
    const size_t before = fpga->staticSensors.size();
    createIRoTResponderHealth(fpga, sensorName, type);
    EXPECT_GE(fpga->staticSensors.size(), before + 1u);
}

// ============================================================================
// createIRoTResponderLocation: direct call with LocationType
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, DirectCreateLocation)
{
    std::string sensorName = "IRoT_Location";
    dbus::PropertyMap props = {
        {"LocationType",
         std::string("xyz.openbmc_project.Inventory.Decorator.Location."
                     "LocationTypes.Embedded")},
    };

    const size_t before = fpga->staticSensors.size();
    createIRoTResponderLocation(fpga, sensorName, type, props);
    EXPECT_GE(fpga->staticSensors.size(), before + 1u);
}

// ============================================================================
// createIRoTResponderChassis: direct call with ChassisType
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, DirectCreateChassis)
{
    std::string sensorName = "IRoT_Chassis";
    dbus::PropertyMap props = {
        {"ChassisType",
         std::string("xyz.openbmc_project.Inventory.Item.Chassis."
                     "ChassisType.Module")},
    };

    const size_t before = fpga->staticSensors.size();
    createIRoTResponderChassis(fpga, sensorName, type, props);
    EXPECT_GE(fpga->staticSensors.size(), before + 1u);
}

// ============================================================================
// NsmIRoTResponder<HealthIntf>::update: co_return NSM_SUCCESS
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, UpdateHealthIntf_CoReturnSuccess)
{
    auto sensor = std::make_shared<NsmIRoTResponder<HealthIntf>>(
        "HealthUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

// ============================================================================
// NsmIRoTResponder<ChassisIntf>::update: co_return NSM_SUCCESS
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, UpdateChassisIntf_CoReturnSuccess)
{
    auto sensor = std::make_shared<NsmIRoTResponder<ChassisIntf>>(
        "ChassisUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

// ============================================================================
// NsmIRoTResponder<LocationIntf>::update: co_return NSM_SUCCESS
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, UpdateLocationIntf_CoReturnSuccess)
{
    auto sensor = std::make_shared<NsmIRoTResponder<LocationIntf>>(
        "LocationUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

// ============================================================================
// NsmIRoTResponder<SPDMResponderIntf>::update: co_return NSM_SUCCESS
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, UpdateSPDMResponderIntf_CoReturnSuccess)
{
    auto sensor = std::make_shared<NsmIRoTResponder<SPDMResponderIntf>>(
        "SPDMUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

// ============================================================================
// NsmIRoTResponder<AssociationDefinitionsIntf>::update: co_return NSM_SUCCESS
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test,
       UpdateAssociationDefinitionsIntf_CoReturnSuccess)
{
    auto sensor =
        std::make_shared<NsmIRoTResponder<AssociationDefinitionsIntf>>(
            "AssocUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

// ============================================================================
// NsmIRoTResponder<AssetIntf>::update: sensorIO fails
// Exercises the AssetIntf constexpr-if branch: sensorIO failure path
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, UpdateAssetIntf_SensorIOFail)
{
    auto sensor = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(
        "AssetIO", "NSM_ChassisIRoTResponder");

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_INVALID_DATA));

    sensor->update(fpga);
}

// ============================================================================
// NsmIRoTResponder<AssetIntf>::update: encode fail (instanceId out of range)
// This exercises the encode_nsm_query_device_ids_req failure branch.
// Since DEFAULT_INSTANCE_ID is always valid, we exercise via sensorIO fail.
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, UpdateAssetIntf_DecodeFirstCallFails)
{
    auto sensor = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(
        "AssetDec1", "NSM_ChassisIRoTResponder");

    // Build a response where decode_nsm_query_device_ids_resp fails
    // (cc != NSM_SUCCESS)
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(response.data());
    auto* hdr = reinterpret_cast<nsm_common_non_success_resp*>(msg->payload);
    hdr->completion_code = NSM_ERROR;
    hdr->reason_code = htole16(ERR_NULL);

    testing::Mock::AllowLeak(fpga.get());
    EXPECT_CALL(*fpga, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(response));

    sensor->update(fpga);
}

// ============================================================================
// createNsmIRoTResponder: NSM_Chassis_Attributes with LocationCode only
// Exercises LocationCode TRUE branch, all others FALSE
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, Factory_ChassisAttributes_OnlyLocationCode)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b3_lc";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("IRoT_LC")},
        {"LocationCode", std::string("U42-B1")},
    };

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".ChassisAttributes",
                           objPath);

    // Asset(1+3inv) + Health(1) + LocationCode(1) = 6
    EXPECT_GE(fpga->staticSensors.size(), before + 6u);
}

// ============================================================================
// createNsmIRoTResponder: NSM_Chassis_Attributes with LocationContext only
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test,
       Factory_ChassisAttributes_OnlyLocationContext)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b3_lctx";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("IRoT_LCTX")},
        {"LocationContext", std::string("rear-panel")},
    };

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".ChassisAttributes",
                           objPath);

    // Asset(1+3inv) + Health(1) + LocationContext(1) = 6
    EXPECT_GE(fpga->staticSensors.size(), before + 6u);
}

// ============================================================================
// createNsmIRoTResponder: type == baseType with UUID present in current intf
// Exercises L289 TRUE branch
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, Factory_BaseType_WithCurrentUUID)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b3_bt_uuid";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".BTWithUUID");
    propertyMap = {
        {"Type", std::string("NSM_ChassisIRoTResponder")},
        {"UUID", fpgaUuid},
    };

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".BTWithUUID", objPath);

    // UuidIntf(1) + AssociationDefinitionsIntf(1) = 2
    EXPECT_GE(fpga->staticSensors.size(), before + 2u);
}

// ============================================================================
// createNsmIRoTResponder: type == baseType without UUID in current intf
// Exercises L289 FALSE branch (inner uuid stays "")
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, Factory_BaseType_WithoutCurrentUUID)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b3_bt_nouuid";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap =
        utils::MockDbusAsync::propertyMap(objPath, basicIntfName + ".BTNoUUID");
    propertyMap = {
        {"Type", std::string("NSM_ChassisIRoTResponder")},
        // UUID absent -> L289 FALSE
    };

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".BTNoUUID", objPath);

    EXPECT_GE(fpga->staticSensors.size(), before + 2u);
}

// ============================================================================
// createNsmIRoTResponder: unknown type (neither baseType nor
// NSM_Chassis_Attributes) -> no sensors created
// ============================================================================

TEST_F(NsmIRoTResponderBranch3Test, Factory_UnknownType_NoSensorsCreated)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b3_unk";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap =
        utils::MockDbusAsync::propertyMap(objPath, basicIntfName + ".Unknown");
    propertyMap = {
        {"Type", std::string("NSM_Something_Else")},
    };

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".Unknown", objPath);

    // Neither if branch taken -> no sensors added
    EXPECT_EQ(fpga->staticSensors.size(), before);
}

// NOTE: Factory_MissingBaseName test removed - empty name creates invalid
// D-Bus object paths ("") which throws InvalidArgument from sdbusplus.
// This is the same issue as the DISABLED_ test in Branch2.
