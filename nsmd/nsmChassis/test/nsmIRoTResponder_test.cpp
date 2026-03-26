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
