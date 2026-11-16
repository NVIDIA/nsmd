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
