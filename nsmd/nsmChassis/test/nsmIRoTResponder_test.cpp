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

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmIRoTResponder.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <libnsm/debug-token.h>

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

    /** @brief Run an IRoT feature creator of the device `deviceName` on the
     *  mock NsmDevice; params gets "Name" unless withName is false
     */
    requester::Coroutine runFeature(nsm::features::CreatorFn creator,
                                    dbus::PropertyMap params,
                                    const std::string& deviceName,
                                    bool withName = true)
    {
        nsm::features::DeviceContext dev;
        dev.name = deviceName;
        dev.kind = "irot";
        dev.staticUuid = fpgaUuid;
        dev.platformPrefix = "HGX_";
        dev.device = fpga;
        if (withName)
        {
            params["Name"] = deviceName;
        }
        return creator(mockManager, dev, params);
    }

    /** @brief Last static sensor of the mock device as T, nullptr if none */
    template <typename T>
    std::shared_ptr<T> lastStatic()
    {
        if (fpga->staticSensors.size() == 0)
        {
            return nullptr;
        }
        return std::dynamic_pointer_cast<T>(fpga->staticSensors.back());
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
// IRoT feature creators (nsm::features::createIRoT*Feature)
// =============================================================================

TEST_F(NsmIRoTResponderTest, goodTestSPDMResponderFeature)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTSPDMResponderFeature, {},
                         "HGX_IRoT_SPDM");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    ASSERT_EQ(fpga->staticSensors.size(), before + 1);
    auto uuidObject = lastStatic<NsmIRoTResponder<UuidIntf>>();
    ASSERT_NE(uuidObject, nullptr);
    EXPECT_EQ(uuidObject->invoke(pdiMethod(uuid)), fpgaUuid);
}

TEST_F(NsmIRoTResponderTest, goodTestSPDMResponderFeatureMctpUuidEnabled)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTSPDMResponderFeature,
                         {{"MctpUuidEnabled", true}}, "HGX_IRoT_SPDM_Mctp");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Common.UUID + MCTP.UUID
    EXPECT_EQ(fpga->staticSensors.size(), before + 2);
}

TEST_F(NsmIRoTResponderTest, goodTestSPDMResponderFeatureMctpUuidDisabled)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTSPDMResponderFeature,
                         {{"MctpUuidEnabled", false}}, "HGX_IRoT_SPDM_NoMctp");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(fpga->staticSensors.size(), before + 1);
}

TEST_F(NsmIRoTResponderTest, goodTestAssociationsFeatureAllPaths)
{
    dbus::PropertyMap params{
        {"ParentChassisAssociationPaths",
         std::vector<std::string>{"chassis/HGX_Chassis_0"}},
        {"RoTInventoryAssociationPaths",
         std::vector<std::string>{"chassis/HGX_GPU_0", "chassis/HGX_GPU_1"}},
        {"InventoryChassisAssociationPaths",
         std::vector<std::string>{"chassis/HGX_GPU_0"}}};

    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTAssociationsFeature, params,
                         "HGX_IRoT_Assoc");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(fpga->staticSensors.size(), before + 1);
    auto associationsObject =
        lastStatic<NsmIRoTResponder<AssociationDefinitionsIntf>>();
    ASSERT_NE(associationsObject, nullptr);
    auto associations = associationsObject->invoke(pdiMethod(associations));
    ASSERT_EQ(associations.size(), 4);
    EXPECT_EQ(
        associations[0],
        std::make_tuple(
            std::string("parent_chassis"), std::string("all_chassis"),
            std::string(
                "/xyz/openbmc_project/inventory/system/chassis/HGX_Chassis_0")));
    EXPECT_EQ(
        associations[1],
        std::make_tuple(
            std::string("inventory"), std::string("associated_ROT"),
            std::string(
                "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_0")));
    EXPECT_EQ(
        associations[2],
        std::make_tuple(
            std::string("inventory"), std::string("associated_ROT"),
            std::string(
                "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_1")));
    EXPECT_EQ(
        associations[3],
        std::make_tuple(
            std::string("inventory_chassis"), std::string("associated_chassis"),
            std::string(
                "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_0")));
}

TEST_F(NsmIRoTResponderTest, goodTestAssociationsFeatureEmptyEntriesSkipped)
{
    dbus::PropertyMap params{
        {"ParentChassisAssociationPaths", std::vector<std::string>{""}},
        {"RoTInventoryAssociationPaths",
         std::vector<std::string>{"", "chassis/HGX_GPU_0"}}};

    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTAssociationsFeature, params,
                         "HGX_IRoT_AssocEmpty");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(fpga->staticSensors.size(), before + 1);
    auto associationsObject =
        lastStatic<NsmIRoTResponder<AssociationDefinitionsIntf>>();
    ASSERT_NE(associationsObject, nullptr);
    EXPECT_EQ(associationsObject->invoke(pdiMethod(associations)).size(), 1);
}

TEST_F(NsmIRoTResponderTest, goodTestAssociationsFeatureNoPathsNoObject)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTAssociationsFeature, {},
                         "HGX_IRoT_AssocNone");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(fpga->staticSensors.size(), before);
}

TEST_F(NsmIRoTResponderTest, goodTestAssetFeatureWithAssetName)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTAssetFeature,
                         {{"AssetName", std::string("IRoT_Asset")}},
                         "HGX_IRoT_Asset");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Asset + build date, model and part number inventory properties
    ASSERT_EQ(fpga->staticSensors.size(), before + 4);
    auto assetObject =
        std::dynamic_pointer_cast<NsmIRoTResponder<NsmAssetIntf>>(
            fpga->staticSensors[before]);
    ASSERT_NE(assetObject, nullptr);
    EXPECT_EQ(assetObject->invoke(pdiMethod(name)), "IRoT_Asset");
    EXPECT_EQ(assetObject->invoke(pdiMethod(manufacturer)),
              MANUFACTURER_NVIDIA);
}

TEST_F(NsmIRoTResponderTest, goodTestAssetFeatureWithoutAssetName)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTAssetFeature, {},
                         "HGX_IRoT_AssetNoName");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(fpga->staticSensors.size(), before + 4);
    auto assetObject =
        std::dynamic_pointer_cast<NsmIRoTResponder<NsmAssetIntf>>(
            fpga->staticSensors[before]);
    ASSERT_NE(assetObject, nullptr);
    EXPECT_EQ(assetObject->invoke(pdiMethod(name)), "");
}

TEST_F(NsmIRoTResponderTest, badTestAssetFeatureNoName)
{
    // No "Name" param -> object path ends with "/" -> sdbusplus throws
    EXPECT_THROW_COROUTINE(runFeature(nsm::features::createIRoTAssetFeature, {},
                                      "HGX_IRoT_NoName", false),
                           std::exception);
}

TEST_F(NsmIRoTResponderTest, goodTestHealthFeature)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTHealthFeature, {},
                         "HGX_IRoT_Health");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(fpga->staticSensors.size(), before + 1);
    auto healthObject = lastStatic<NsmIRoTResponder<HealthIntf>>();
    ASSERT_NE(healthObject, nullptr);
    EXPECT_EQ(healthObject->invoke(pdiMethod(health)),
              HealthIntf::HealthType::OK);
}

TEST_F(NsmIRoTResponderTest, goodTestChassisTypeFeature)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(
        nsm::features::createIRoTChassisTypeFeature,
        {{"ChassisType",
          std::string(
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module")}},
        "HGX_IRoT_ChassisType");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(fpga->staticSensors.size(), before + 1);
    auto chassisObject = lastStatic<NsmIRoTResponder<ChassisIntf>>();
    ASSERT_NE(chassisObject, nullptr);
    EXPECT_EQ(chassisObject->invoke(pdiMethod(type)),
              ChassisIntf::ChassisType::Module);
}

TEST_F(NsmIRoTResponderTest, goodTestLocationTypeFeature)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(
        nsm::features::createIRoTLocationTypeFeature,
        {{"LocationType",
          std::string("xyz.openbmc_project.Inventory.Decorator.Location."
                      "LocationTypes.Embedded")}},
        "HGX_IRoT_LocationType");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(fpga->staticSensors.size(), before + 1);
    auto locationObject = lastStatic<NsmIRoTResponder<LocationIntf>>();
    ASSERT_NE(locationObject, nullptr);
    EXPECT_EQ(locationObject->invoke(pdiMethod(locationType)),
              LocationIntf::LocationTypes::Embedded);
}

TEST_F(NsmIRoTResponderTest, goodTestLocationCodeFeature)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTLocationCodeFeature,
                         {{"LocationCode", std::string("U1.A.B")}},
                         "HGX_IRoT_LocationCode");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(fpga->staticSensors.size(), before + 1);
    auto locationCodeObject = lastStatic<NsmIRoTResponder<LocationCodeIntf>>();
    ASSERT_NE(locationCodeObject, nullptr);
    EXPECT_EQ(locationCodeObject->invoke(pdiMethod(locationCode)), "U1.A.B");
}

TEST_F(NsmIRoTResponderTest, goodTestLocationContextFeature)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTLocationContextFeature,
                         {{"LocationContext", std::string("Slot0")}},
                         "HGX_IRoT_LocationContext");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(fpga->staticSensors.size(), before + 1);
    auto locationContextObject =
        lastStatic<NsmIRoTResponder<LocationContextIntf>>();
    ASSERT_NE(locationContextObject, nullptr);
    EXPECT_EQ(locationContextObject->invoke(pdiMethod(locationContext)),
              "Slot0");
}

TEST_F(NsmIRoTResponderTest, goodTestFieldReplaceableFeature)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTFieldReplaceableFeature,
                         {{"FieldReplaceable", true}},
                         "HGX_IRoT_FieldReplaceable");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(fpga->staticSensors.size(), before + 1);
    auto replaceableObject = lastStatic<NsmIRoTResponder<ReplaceableIntf>>();
    ASSERT_NE(replaceableObject, nullptr);
    EXPECT_TRUE(replaceableObject->invoke(pdiMethod(fieldReplaceable)));
}

// Optional-property features create nothing when their param is not set
TEST_F(NsmIRoTResponderTest, goodTestOptionalFeaturesWithoutParams)
{
    const size_t before = fpga->staticSensors.size();
    const nsm::features::CreatorFn creators[] = {
        nsm::features::createIRoTChassisTypeFeature,
        nsm::features::createIRoTLocationTypeFeature,
        nsm::features::createIRoTLocationCodeFeature,
        nsm::features::createIRoTLocationContextFeature,
        nsm::features::createIRoTFieldReplaceableFeature};
    for (auto creator : creators)
    {
        auto rc = runFeature(creator, {}, "HGX_IRoT_NoOptional");
        EXPECT_EQ(rc.data(), NSM_SUCCESS);
        EXPECT_FALSE(rc.exception());
    }
    EXPECT_EQ(fpga->staticSensors.size(), before);
}

TEST_F(NsmIRoTResponderTest, goodTestSKUFeature)
{
    const size_t before = fpga->staticSensors.size();
    auto rc = runFeature(nsm::features::createIRoTSKUFeature, {},
                         "HGX_IRoT_SKU");

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(fpga->staticSensors.size(), before + 1);
}
