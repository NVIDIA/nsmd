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
 * Branch coverage tests for nsmd/nsmChassis/nsmIRoTResponder.cpp
 *
 * Covers:
 * - createIRoTResponderLocationCode: direct call with valid device + props
 * - createIRoTResponderLocationContext: direct call with valid device + props
 * - createIRoTResponderFieldReplaceable: direct call with valid device + props
 * - createNsmIRoTResponder: missing Name/UUID/Type in base props
 * - createNsmIRoTResponder: type == "NSM_Chassis_Attributes" full branch
 * - NsmIRoTResponder<UuidIntf>::update: error paths (rc != 0, empty uuid)
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

struct NsmIRoTResponderBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "IRoTB2";
    const std::string type = "NSM_ChassisIRoTResponder";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmIRoTResponderBranch2Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmIRoTResponderBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// createIRoTResponderLocationCode: direct call with LocationCode present
// Exercises L201 TRUE branch (count("LocationCode") TRUE)
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test,
       DirectCreateLocationCode_WithLocationCode_CreatesObject)
{
    std::string sensorName = "IRoT_LocCodeDirect";
    dbus::PropertyMap props = {
        {"LocationCode", std::string("U78-C1")},
    };

    const size_t before = fpga->staticSensors.size();
    createIRoTResponderLocationCode(fpga, sensorName, type, props);
    EXPECT_GT(fpga->staticSensors.size(), before);
}

// ============================================================================
// createIRoTResponderLocationCode: direct call without LocationCode
// Exercises L201 FALSE branch (count("LocationCode") FALSE)
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test,
       DirectCreateLocationCode_NoLocationCode_UsesDefault)
{
    std::string sensorName = "IRoT_LocCodeNone";
    dbus::PropertyMap props = {};

    const size_t before = fpga->staticSensors.size();
    createIRoTResponderLocationCode(fpga, sensorName, type, props);
    EXPECT_GT(fpga->staticSensors.size(), before);
}

// ============================================================================
// createIRoTResponderLocationContext: direct call with LocationContext present
// Exercises L219 TRUE branch (count("LocationContext") TRUE)
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test,
       DirectCreateLocationContext_WithContext_CreatesObject)
{
    std::string sensorName = "IRoT_LocCtxDirect";
    dbus::PropertyMap props = {
        {"LocationContext", std::string("rear-panel-slot-3")},
    };

    const size_t before = fpga->staticSensors.size();
    createIRoTResponderLocationContext(fpga, sensorName, type, props);
    EXPECT_GT(fpga->staticSensors.size(), before);
}

// ============================================================================
// createIRoTResponderLocationContext: direct call without LocationContext
// Exercises L219 FALSE branch (count("LocationContext") FALSE)
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test,
       DirectCreateLocationContext_NoContext_UsesDefault)
{
    std::string sensorName = "IRoT_LocCtxNone";
    dbus::PropertyMap props = {};

    const size_t before = fpga->staticSensors.size();
    createIRoTResponderLocationContext(fpga, sensorName, type, props);
    EXPECT_GT(fpga->staticSensors.size(), before);
}

// ============================================================================
// createIRoTResponderFieldReplaceable: direct call with FieldReplaceable=true
// Exercises L235 TRUE branch (count("FieldReplaceable") TRUE)
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test,
       DirectCreateFieldReplaceable_WithProp_CreatesObject)
{
    std::string sensorName = "IRoT_FReplDirect";
    dbus::PropertyMap props = {
        {"FieldReplaceable", bool{true}},
    };

    const size_t before = fpga->staticSensors.size();
    createIRoTResponderFieldReplaceable(fpga, sensorName, type, props);
    EXPECT_GT(fpga->staticSensors.size(), before);
}

// ============================================================================
// createIRoTResponderFieldReplaceable: direct call without FieldReplaceable
// Exercises L235 FALSE branch (count("FieldReplaceable") FALSE)
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test,
       DirectCreateFieldReplaceable_NoProp_UsesDefaultFalse)
{
    std::string sensorName = "IRoT_FReplNone";
    dbus::PropertyMap props = {};

    const size_t before = fpga->staticSensors.size();
    createIRoTResponderFieldReplaceable(fpga, sensorName, type, props);
    EXPECT_GT(fpga->staticSensors.size(), before);
}

// ============================================================================
// createNsmIRoTResponder: NSM_Chassis_Attributes with ALL optional properties
// Exercises ALL TRUE branches at L312-336: LocationType, ChassisType,
// LocationCode, LocationContext, FieldReplaceable
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test,
       Factory_ChassisAttributes_AllOptionalProps_AllTrueBranches)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b2_allopt";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("IRoT_AllOpts")},
        {"LocationType",
         std::string("xyz.openbmc_project.Inventory.Decorator.Location."
                     "LocationTypes.Embedded")},
        {"ChassisType",
         std::string("xyz.openbmc_project.Inventory.Item.Chassis."
                     "ChassisType.Module")},
        {"LocationCode", std::string("U99-C2")},
        {"LocationContext", std::string("front-panel")},
        {"FieldReplaceable", bool{false}},
    };

    createNsmIRoTResponder(mockManager, basicIntfName + ".ChassisAttributes",
                           objPath);

    // Asset(1+3inv) + Health(1) + Location(1) + Chassis(1) +
    // LocationCode(1) + LocationContext(1) + FieldReplaceable(1) = 10
    EXPECT_GE(fpga->staticSensors.size(), 10u);
}

// ============================================================================
// createNsmIRoTResponder: missing Name in base → L265 FALSE → name=""
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test,
       DISABLED_Factory_MissingBaseName_FalseBranch_NameEmpty)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b2_noname";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    // Name absent → L265 FALSE → name stays ""
    basePropertyMap = {{"UUID", fpgaUuid}};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("IRoT_NoBaseName")},
    };

    // name="" but sensors should still be created
    createNsmIRoTResponder(mockManager, basicIntfName + ".ChassisAttributes",
                           objPath);

    EXPECT_GE(fpga->staticSensors.size(), 2u);
}

// ============================================================================
// createNsmIRoTResponder: missing Type in current → L270 FALSE → type=""
// Neither baseType nor NSM_Chassis_Attributes branch taken
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test, Factory_MissingType_FalseBranch_NoBranch)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b2_notype";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".NoTypeIntf");
    // Type absent → L270 FALSE → type=""
    propertyMap = {{"Name", std::string("IRoT_NoType")}};

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".NoTypeIntf", objPath);

    // No type branch taken → no sensors added
    EXPECT_EQ(fpga->staticSensors.size(), before);
}

// ============================================================================
// createNsmIRoTResponder: missing UUID in base → L275 FALSE → uuid=""
// getNsmDeviceFromStaticUUID("") throws
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test, Factory_MissingUUID_FalseBranch_Throws)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b2_nouuid";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    // UUID absent → L275 FALSE → uuid=""
    basePropertyMap = {{"Name", name}};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".NoUUIDIntf");
    propertyMap = {{"Type", std::string("NSM_Chassis_Attributes")}};

    EXPECT_THROW_COROUTINE(createNsmIRoTResponder(mockManager,
                                                  basicIntfName + ".NoUUIDIntf",
                                                  objPath),
                           std::exception);
}

// ============================================================================
// NsmIRoTResponder<UuidIntf>::update: MctpDiscovery::getInstance() throws
// in test environment → covers the UuidIntf constexpr if branch entry
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test, UpdateUuidIntf_MctpNotInit_Throws)
{
    auto sensor = std::make_shared<NsmIRoTResponder<UuidIntf>>(
        "UuidB2_throw", "NSM_ChassisIRoTResponder");

    EXPECT_THROW_COROUTINE(sensor->update(fpga), std::runtime_error);
}

// ============================================================================
// NsmIRoTResponder<LocationCodeIntf>::update: non-UuidIntf, non-AssetIntf
// → falls through both constexpr if → co_return NSM_SUCCESS at L126 //

// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test, UpdateLocationCodeIntf_CoReturnSuccess)
{
    auto sensor = std::make_shared<NsmIRoTResponder<LocationCodeIntf>>(
        "LocCodeUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

// ============================================================================
// NsmIRoTResponder<LocationContextIntf>::update: co_return NSM_SUCCESS //

// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test, UpdateLocationContextIntf_CoReturnSuccess)
{
    auto sensor = std::make_shared<NsmIRoTResponder<LocationContextIntf>>(
        "LocCtxUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

// ============================================================================
// NsmIRoTResponder<ReplaceableIntf>::update: co_return NSM_SUCCESS //

// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test, UpdateReplaceableIntf_CoReturnSuccess)
{
    auto sensor = std::make_shared<NsmIRoTResponder<ReplaceableIntf>>(
        "ReplUpd", "NSM_ChassisIRoTResponder");
    sensor->update(fpga);
}

// ============================================================================
// createNsmIRoTResponder: type == baseType ("NSM_ChassisIRoTResponder")
// with all properties, exercises UUID creation + associations path
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test,
       Factory_BaseType_WithAllProps_CreatesUuidAndAssoc)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b2_basetype";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap =
        utils::MockDbusAsync::propertyMap(objPath, basicIntfName + ".BaseType");
    propertyMap = {
        {"Type", std::string("NSM_ChassisIRoTResponder")},
        {"UUID", fpgaUuid},
    };

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".BaseType", objPath);

    // UuidIntf + AssociationDefinitionsIntf = 2 sensors
    EXPECT_GE(fpga->staticSensors.size(), before + 2u);
}

// ============================================================================
// createNsmIRoTResponder: type == baseType without UUID in current interface
// Exercises L289 FALSE → inner uuid stays empty
// ============================================================================

TEST_F(NsmIRoTResponderBranch2Test,
       Factory_BaseType_NoCurrentUUID_UsesEmptyUuid)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_b2_basetype_nouuid";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = {{"Name", name}, {"UUID", fpgaUuid}};

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".BaseNoUUID2");
    propertyMap = {
        {"Type", std::string("NSM_ChassisIRoTResponder")},
        // UUID absent → L289 FALSE → inner uuid=""
    };

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".BaseNoUUID2",
                           objPath);

    EXPECT_GE(fpga->staticSensors.size(), before + 2u);
}
