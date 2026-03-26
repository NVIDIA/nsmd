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

namespace nsm
{
requester::Coroutine createNsmIRoTResponder(SensorManager& manager,
                                            const std::string& interface,
                                            const std::string& objPath);
} // namespace nsm

using namespace nsm;

struct NsmIRoTResponderTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "IRoTResponder";
    const std::string type = "NSM_IRoTResponder";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmIRoTResponderTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmIRoTResponderTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmIRoTResponderTest, testConstructorWithAssetIntf)
{
    std::string testName = "TestIRoT";
    std::string testType = "NSM_IRoTResponder";

    auto irotResponder =
        std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(testName, testType);

    EXPECT_NE(irotResponder, nullptr);
    EXPECT_EQ(irotResponder->getName(), testName);
    EXPECT_EQ(irotResponder->getType(), testType);
    EXPECT_EQ(irotResponder->name, testName);
}

TEST_F(NsmIRoTResponderTest, testConstructorWithChassisIntf)
{
    std::string testName = "TestIRoT_Chassis";
    std::string testType = "NSM_IRoTResponder";

    auto irotResponder =
        std::make_shared<NsmIRoTResponder<ChassisIntf>>(testName, testType);

    EXPECT_NE(irotResponder, nullptr);
    EXPECT_EQ(irotResponder->getName(), testName);
    EXPECT_EQ(irotResponder->getType(), testType);
}

TEST_F(NsmIRoTResponderTest, testMultipleInstances)
{
    std::string name1 = "IRoT1";
    std::string name2 = "IRoT2";
    std::string testType = "NSM_IRoTResponder";

    auto irot1 = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(name1,
                                                                  testType);
    auto irot2 = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(name2,
                                                                  testType);

    EXPECT_NE(irot1, nullptr);
    EXPECT_NE(irot2, nullptr);
    EXPECT_NE(irot1, irot2);
    EXPECT_EQ(irot1->getName(), name1);
    EXPECT_EQ(irot2->getName(), name2);
}

TEST_F(NsmIRoTResponderTest, goodTestCreateIRoTResponder)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/irot";

    dbus::PropertyMap baseProperties = {
        {"Name", name},
        {"UUID", fpgaUuid},
    };

    dbus::PropertyMap responderProperties = {
        {"Type", std::string("NSM_ChassisIRoTResponder")},
        {"UUID", fpgaUuid},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = baseProperties;

    auto& propertyMap =
        utils::MockDbusAsync::propertyMap(objPath, basicIntfName + ".Chassis");
    propertyMap = responderProperties;

    createNsmIRoTResponder(mockManager, basicIntfName + ".Chassis", objPath);

    EXPECT_GE(fpga->staticSensors.size(), 1);
}

TEST_F(NsmIRoTResponderTest, testCreateIRoTResponderWithAttributes)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/irot";

    dbus::PropertyMap baseProperties = {
        {"Name", name},
        {"UUID", fpgaUuid},
    };

    dbus::PropertyMap attributesProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("IRoT_Attributes")},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"},
        {"ChassisType",
         "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module"},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = baseProperties;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = attributesProperties;

    createNsmIRoTResponder(mockManager, basicIntfName + ".ChassisAttributes",
                           objPath);

    EXPECT_GE(fpga->staticSensors.size(), 1);
}

TEST_F(NsmIRoTResponderTest, testUpdateAssetIntfCoroutine)
{
    auto irotResponder = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(name,
                                                                          type);

    // Mock query device ids response
    std::vector<uint8_t> deviceId = {0x01, 0x02, 0x03, 0x04};
    Response queryDeviceIdsResp(sizeof(nsm_msg_hdr) +
                                    sizeof(nsm_query_device_ids_resp) +
                                    deviceId.size(),
                                0);
    auto queryMsg = reinterpret_cast<nsm_msg*>(queryDeviceIdsResp.data());
    encode_nsm_query_device_ids_resp(0, NSM_SUCCESS, ERR_NULL, deviceId.data(),
                                     deviceId.size(), queryMsg);

    EXPECT_CALL(*fpga, sensorIO)
        .WillOnce(mockSensorIO(queryDeviceIdsResp, Response{}));

    irotResponder->update(fpga);
}

TEST_F(NsmIRoTResponderTest, badTestMissingUUID)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/irot";

    dbus::PropertyMap baseProperties = {
        {"Name", name},
        // Missing UUID
    };

    dbus::PropertyMap chassisProperties = {
        {"Type", std::string("NSM_ChassisIRoTResponder")},
        // Missing UUID
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = baseProperties;

    auto& propertyMap =
        utils::MockDbusAsync::propertyMap(objPath, basicIntfName + ".Chassis");
    propertyMap = chassisProperties;

    EXPECT_THROW_COROUTINE(createNsmIRoTResponder(mockManager,
                                                  basicIntfName + ".Chassis",
                                                  objPath),
                           std::runtime_error);
}

TEST_F(NsmIRoTResponderTest, testUpdateAssetIntfDecodeFailure)
{
    auto irotResponder = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(name,
                                                                          type);

    // Mock invalid response with minimal valid size but invalid data
    Response invalidResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + sizeof(uint16_t), 0);
    auto msg = reinterpret_cast<nsm_msg*>(invalidResp.data());
    encode_reason_code(NSM_ERROR, 0x9999, NSM_QUERY_DEVICE_IDS, msg);

    EXPECT_CALL(*fpga, sensorIO)
        .WillOnce(mockSensorIO(invalidResp, Response{}));

    auto rc = irotResponder->update(fpga);
    // Should handle decode failure gracefully
}

TEST_F(NsmIRoTResponderTest, testUpdateAssetIntfCompletionCodeError)
{
    auto irotResponder = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(name,
                                                                          type);

    // Mock error response
    std::vector<uint8_t> deviceId = {0x01, 0x02};
    Response errorResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_nsm_query_device_ids_resp(0, NSM_ERROR, 0x1234, deviceId.data(),
                                     deviceId.size(), msg);

    EXPECT_CALL(*fpga, sensorIO).WillOnce(mockSensorIO(errorResp, Response{}));

    auto rc = irotResponder->update(fpga);
}

TEST_F(NsmIRoTResponderTest, testUpdateUuidIntfSuccess)
{
    auto irotResponder = std::make_shared<NsmIRoTResponder<UuidIntf>>(name,
                                                                      type);

    mctp::mctpDiscoveryInstance.reset(
        reinterpret_cast<mctp::MctpDiscovery*>(&mockManager.mockMctpDiscovery));
    irotResponder->update(fpga);
    mctp::mctpDiscoveryInstance.release();
}

TEST_F(NsmIRoTResponderTest, testCreateIRoTResponderWithAssociations)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/irot";

    dbus::PropertyMap baseProperties = {
        {"Name", name},
        {"UUID", fpgaUuid},
    };

    dbus::PropertyMap responderProperties = {
        {"Type", std::string("NSM_ChassisIRoTResponder")},
        {"UUID", fpgaUuid},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = baseProperties;

    auto& propertyMap =
        utils::MockDbusAsync::propertyMap(objPath, basicIntfName + ".Chassis");
    propertyMap = responderProperties;

    // Setup associations
    auto& serviceMap = utils::MockDbusAsync::serviceMap();
    MapperServiceMap assocServiceMap = {
        {
            {
                "xyz.openbmc_project.NSM",
                {
                    basicIntfName + ".Chassis.Associations",
                },
            },
        },
    };
    serviceMap = assocServiceMap;

    dbus::PropertyMap assocProperties = {
        {"Forward", "parent_chassis"},
        {"Backward", "irot_responder"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system/chassis"},
    };

    auto& assocPropertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".Chassis.Associations");
    assocPropertyMap = assocProperties;

    createNsmIRoTResponder(mockManager, basicIntfName + ".Chassis", objPath);

    EXPECT_GE(fpga->staticSensors.size(), 2); // UUID + Associations
}

TEST_F(NsmIRoTResponderTest, testCreateIRoTResponderInventoryProperties)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/irot";

    dbus::PropertyMap baseProperties = {
        {"Name", name},
        {"UUID", fpgaUuid},
    };

    dbus::PropertyMap attributesProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("IRoT_Asset")},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"},
        {"ChassisType",
         "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module"},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = baseProperties;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = attributesProperties;

    createNsmIRoTResponder(mockManager, basicIntfName + ".ChassisAttributes",
                           objPath);

    // Should create Asset, Health, Location, Chassis, and inventory property
    // sensors
    EXPECT_GE(fpga->staticSensors.size(), 4);
}

TEST_F(NsmIRoTResponderTest, testUpdateAssetIntfWithEmptyDeviceId)
{
    auto irotResponder = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(name,
                                                                          type);

    // Mock query device ids response with empty device ID
    std::vector<uint8_t> deviceId = {};
    Response queryDeviceIdsResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_ids_resp), 0);
    auto queryMsg = reinterpret_cast<nsm_msg*>(queryDeviceIdsResp.data());
    encode_nsm_query_device_ids_resp(0, NSM_SUCCESS, ERR_NULL, deviceId.data(),
                                     0, queryMsg);

    EXPECT_CALL(*fpga, sensorIO)
        .WillOnce(mockSensorIO(queryDeviceIdsResp, Response{}));

    irotResponder->update(fpga);

    // Serial number should be set to "0x" only
    EXPECT_EQ(irotResponder->invoke(pdiMethod(serialNumber)), "0x");
}

TEST_F(NsmIRoTResponderTest, testUpdateAssetIntfWithLargeDeviceId)
{
    auto irotResponder = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(name,
                                                                          type);

    // Mock query device ids response with large device ID
    std::vector<uint8_t> deviceId(128);
    for (size_t i = 0; i < deviceId.size(); i++)
    {
        deviceId[i] = static_cast<uint8_t>(i & 0xFF);
    }

    Response queryDeviceIdsResp(sizeof(nsm_msg_hdr) +
                                    sizeof(nsm_query_device_ids_resp) +
                                    deviceId.size(),
                                0);
    auto queryMsg = reinterpret_cast<nsm_msg*>(queryDeviceIdsResp.data());
    encode_nsm_query_device_ids_resp(0, NSM_SUCCESS, ERR_NULL, deviceId.data(),
                                     deviceId.size(), queryMsg);

    EXPECT_CALL(*fpga, sensorIO)
        .WillOnce(mockSensorIO(queryDeviceIdsResp, Response{}));

    irotResponder->update(fpga);
}

TEST_F(NsmIRoTResponderTest, testCreateIRoTHelperFunctions)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/irot_helpers";

    dbus::PropertyMap baseProperties = {
        {"Name", std::string("IRoT_Helpers")},
        {"UUID", fpgaUuid},
    };

    dbus::PropertyMap attributesProps = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("IRoT_Asset_Full")},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"},
        {"ChassisType",
         "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module"},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = baseProperties;

    auto& attrPropertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    attrPropertyMap = attributesProps;

    createNsmIRoTResponder(mockManager, basicIntfName + ".ChassisAttributes",
                           objPath);

    // Should create: Asset (with 3 inventory properties), Health, Location,
    // Chassis Total: at least 7 sensors
    EXPECT_GE(fpga->staticSensors.size(), 7);
}

TEST_F(NsmIRoTResponderTest, testUpdateAssetIntfSendError)
{
    auto irotResponder =
        std::make_shared<NsmIRoTResponder<NsmAssetIntf>>("SendErrTest", type);

    // Mock sensorIO failure (timeout) - update returns Coroutine, don't check
    // return
    Response errorResp{};
    EXPECT_CALL(*fpga, sensorIO).WillOnce(mockSensorIO(errorResp, Response{}));

    irotResponder->update(fpga);
}

TEST_F(NsmIRoTResponderTest, testUpdateAssetIntfSecondDecodeFailure)
{
    // Test to cover line 91 - second decode with error completion code
    auto irotResponder = std::make_shared<NsmIRoTResponder<NsmAssetIntf>>(name,
                                                                          type);

    // Create a malformed response: header indicates large deviceId but data is
    // corrupted This will pass first decode (gets length) but fail second
    // decode
    std::vector<uint8_t> deviceId = {0x01, 0x02, 0x03, 0x04};

    // Create response with corrupted data - set completion code to error after
    // encoding
    Response corruptResp(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_device_ids_resp) +
                             deviceId.size(),
                         0);
    auto queryMsg = reinterpret_cast<nsm_msg*>(corruptResp.data());

    // First encode correctly to get structure
    encode_nsm_query_device_ids_resp(0, NSM_SUCCESS, ERR_NULL, deviceId.data(),
                                     deviceId.size(), queryMsg);

    // Then corrupt the response by making the buffer shorter than claimed
    // This simulates a truncated response
    corruptResp.resize(sizeof(nsm_msg_hdr) +
                       sizeof(nsm_common_resp)); // Too short for deviceId data

    EXPECT_CALL(*fpga, sensorIO)
        .WillOnce(mockSensorIO(corruptResp, Response{}));

    irotResponder->update(fpga);
}

// =============================================================================
// FALSE-branch coverage: count() checks in createNsmIRoTResponder
// =============================================================================

// Type absent → type="" → neither "NSM_ChassisIRoTResponder" nor
// "NSM_Chassis_Attributes" block executes → no sensors created
TEST_F(NsmIRoTResponderTest, CreateIRoTResponder_MissingType_NoSensors)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string uniquePath = "/xyz/test/irot/no_type_false_branch";

    // Register base interface so coGetCachedBaseProperties succeeds
    auto& baseProps = utils::MockDbusAsync::propertyMap(uniquePath,
                                                        basicIntfName);
    baseProps["Name"] = std::string("IRoT_NoType");
    baseProps["UUID"] = fpgaUuid;

    // Register current sub-interface WITHOUT "Type" → type="" → no block runs
    auto& currProps = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".NoTypeIface");
    (void)currProps; // intentionally empty

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".NoTypeIface",
                           uniquePath);
    // Neither if/else-if block ran → no new sensors
    EXPECT_EQ(before, fpga->staticSensors.size());
}

// Name absent from base → name="" → createIRoTResponderAsset constructs
// NsmIRoTResponder<NsmAssetIntf> at path ending with "/" → sdbusplus throws
TEST_F(NsmIRoTResponderTest, CreateIRoTResponder_MissingName_InBase_Throws)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string uniquePath = "/xyz/test/irot/no_base_name";

    // Base interface has UUID but NOT "Name" → name=""
    auto& baseProps = utils::MockDbusAsync::propertyMap(uniquePath,
                                                        basicIntfName);
    baseProps["UUID"] = fpgaUuid;
    // "Name" intentionally omitted → FALSE branch → name=""

    // Current interface triggers NSM_Chassis_Attributes block (needs Name)
    auto& currProps = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".NoNameAttrs");
    currProps["Type"] = std::string("NSM_Chassis_Attributes");
    currProps["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location"
        ".LocationTypes.Embedded");
    currProps["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis"
        ".ChassisType.Module");

    // name="" → NsmIRoTResponder<NsmAssetIntf>("", ...) → path ends with "/"
    // → sdbusplus SdBusError (InvalidArgs)
    EXPECT_THROW_COROUTINE(
        createNsmIRoTResponder(mockManager, basicIntfName + ".NoNameAttrs",
                               uniquePath),
        std::exception);
}

// createNsmIRoTResponder: base interface NOT registered at path →
// coGetCachedBaseProperties returns non-NSM_SUCCESS → co_return rc (line 259).
//
TEST_F(NsmIRoTResponderTest, CreateIRoTResponder_BasePropertiesFail_EarlyReturn)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string uniquePath = "/xyz/test/irot/base_fail_unique";

    // Register a different sub-interface so the path exists but basicIntfName
    // (the hardcoded baseInterface inside createNsmIRoTResponder) is absent.
    auto& other = utils::MockDbusAsync::propertyMap(uniquePath,
                                                    basicIntfName + ".Sub");
    other["Type"] = std::string("NSM_ChassisIRoTResponder");

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, basicIntfName + ".Chassis", uniquePath);
    EXPECT_EQ(before, fpga->staticSensors.size());
}

// NSM_Chassis_Attributes: LocationCode/LocationContext/FieldReplaceable present
// but NO LocationType and NO ChassisType → covers:
//   - line 136 count("Name") FALSE (Name absent from current props)
//   - line 312 count("LocationType") FALSE
//   - line 317 count("ChassisType") FALSE
//   - lines 322/327/332 TRUE (creates LocationCode, LocationContext,
//   Replaceable)
//   - lines 201/218/235 TRUE inside static helper functions
TEST_F(
    NsmIRoTResponderTest,
    CreateIRoTResponder_ChassisAttributes_OptionalPropsNoLocTypeOrChassisType)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string subIntfName = basicIntfName + ".AttrOptProps";
    const std::string uniquePath = "/xyz/test/irot/chassis_attr_opt_props";

    // Base interface: Name and UUID
    auto& baseProps = utils::MockDbusAsync::propertyMap(uniquePath,
                                                        basicIntfName);
    baseProps["Name"] = std::string("IRoT_OptProps");
    baseProps["UUID"] = fpgaUuid;

    // Current interface: Type + optional location props, but NO Name,
    // NO LocationType, NO ChassisType
    auto& currProps = utils::MockDbusAsync::propertyMap(uniquePath,
                                                        subIntfName);
    currProps["Type"] = std::string("NSM_Chassis_Attributes");
    // "Name" intentionally omitted → line 136 FALSE → assetName=""
    // "LocationType" omitted → line 312 FALSE → skip
    // createIRoTResponderLocation "ChassisType" omitted → line 317 FALSE → skip
    // createIRoTResponderChassis
    currProps["LocationCode"] = std::string("U1.A.B");   // line 322 TRUE
    currProps["LocationContext"] = std::string("Slot0"); // line 327 TRUE
    currProps["FieldReplaceable"] = bool(false);         // line 332 TRUE

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, subIntfName, uniquePath);
    // asset + health + locationCode + locationContext + replaceable sensors
    // added
    EXPECT_GT(fpga->staticSensors.size(), before);
}

// type=="NSM_ChassisIRoTResponder" (baseType) but current props have no "UUID"
// → line 289 count("UUID") FALSE → local uuid stays empty, still adds sensors
TEST_F(NsmIRoTResponderTest, CreateIRoTResponder_BaseType_NoCurrentUUID)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string subIntfName = basicIntfName + ".BaseNoUUID";
    const std::string uniquePath = "/xyz/test/irot/base_type_no_curr_uuid";

    // Base interface: Name and UUID (so device lookup succeeds)
    auto& baseProps = utils::MockDbusAsync::propertyMap(uniquePath,
                                                        basicIntfName);
    baseProps["Name"] = std::string("IRoT_BaseNoUUID");
    baseProps["UUID"] = fpgaUuid;

    // Current interface: type=baseType but NO "UUID"
    // → count("UUID") at line 289 is FALSE → local uuid{} stays empty
    auto& currProps = utils::MockDbusAsync::propertyMap(uniquePath,
                                                        subIntfName);
    currProps["Type"] = std::string("NSM_ChassisIRoTResponder");
    // "UUID" intentionally omitted → FALSE branch at line 289

    const size_t before = fpga->staticSensors.size();
    createNsmIRoTResponder(mockManager, subIntfName, uniquePath);
    // uuidObject and associationsObject were created and added
    EXPECT_GT(fpga->staticSensors.size(), before);
}

// count("UUID") FALSE in base interface (line 275): base "UUID" absent →
// uuid="" → getNsmDeviceFromStaticUUID("") throws std::runtime_error.
TEST_F(NsmIRoTResponderTest, CreateIRoTResponder_MissingBaseUUID_Throws)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisIRoTResponder";
    const std::string subIntfName = basicIntfName + ".NoBaseUUID";
    const std::string uniquePath = "/xyz/test/irot/no_base_uuid";

    // Base interface: has Name but NO "UUID" → uuid="" at line 275
    auto& baseProps = utils::MockDbusAsync::propertyMap(uniquePath,
                                                        basicIntfName);
    baseProps["Name"] = std::string("IRoT_NoBaseUUID");
    // "UUID" intentionally omitted → FALSE branch → uuid="" → throws

    // Current interface has valid type (so the device lookup is reached)
    auto& currProps = utils::MockDbusAsync::propertyMap(uniquePath,
                                                        subIntfName);
    currProps["Type"] = std::string("NSM_ChassisIRoTResponder");

    // uuid="" → getNsmDeviceFromStaticUUID("") throws std::runtime_error
    EXPECT_THROW_COROUTINE(
        createNsmIRoTResponder(mockManager, subIntfName, uniquePath),
        std::runtime_error);
}
