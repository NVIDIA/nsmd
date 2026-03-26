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
 * Additional branch coverage for nsmNetworkAdapter.cpp:
 *
 * Targets uncovered code paths:
 * - NsmNetworkAdapterDI constructor: empty associations, multiple associations
 * - NsmDeviceProtectionOptions: genRequestMsg encode failure
 *   (instanceId > NSM_INSTANCE_MAX)
 * - NsmDeviceProtectionOptions: handleResponseMsg with cc != NSM_SUCCESS
 *   (error CC path where decode succeeds but cc indicates failure)
 * - NsmDeviceProtectionOptions: setProtectionOptions encode failure
 * - createNSMNetworkAdapter factory: with associations from D-Bus,
 *   missing InventoryObjPath FALSE branch
 * - convertNsmdToDbusProtectionMode: verify all switch paths
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "libnsm/device-configuration.h"

#include "nsmNetworkAdapter.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine createNSMNetworkAdapter(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmNetworkAdapterBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NetworkAdapter";
    const uuid_t deviceUuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    NsmNetworkAdapterBranch2Test() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(device, nullptr);
    }

    ~NsmNetworkAdapterBranch2Test() override
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NsmNetworkAdapterDI constructor: empty associations list
// Exercises the for-loop body NOT being entered (empty vector)
// ============================================================================

TEST_F(NsmNetworkAdapterBranch2Test, Constructor_EmptyAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> emptyAssociations;

    auto adapter = std::make_shared<NsmNetworkAdapterDI>(
        bus, "NIC_empty_assoc", emptyAssociations, "NSM_NetworkAdapter",
        "/xyz/openbmc_project/inventory/br2test/");
    EXPECT_NE(adapter, nullptr);
    EXPECT_NE(adapter->associationDefIntf, nullptr);
    EXPECT_NE(adapter->pcieDeviceIntf, nullptr);
    EXPECT_NE(adapter->networkInterfaceIntf, nullptr);
}

// ============================================================================
// NsmNetworkAdapterDI constructor: multiple associations
// Exercises the for-loop body multiple times
// ============================================================================

TEST_F(NsmNetworkAdapterBranch2Test, Constructor_MultipleAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    associations.push_back(
        {"chassis", "adapters", "/xyz/openbmc_project/test/chassis0"});
    associations.push_back(
        {"parent", "child", "/xyz/openbmc_project/test/parent0"});
    associations.push_back(
        {"fabric", "endpoint", "/xyz/openbmc_project/test/fabric0"});

    auto adapter = std::make_shared<NsmNetworkAdapterDI>(
        bus, "NIC_multi_assoc", associations, "NSM_NetworkAdapter",
        "/xyz/openbmc_project/inventory/br2test/");
    EXPECT_NE(adapter, nullptr);
}

// ============================================================================
// NsmDeviceProtectionOptions: genRequestMsg encode failure
// instanceId > NSM_INSTANCE_MAX triggers encode failure -> nullopt
// ============================================================================

TEST_F(NsmNetworkAdapterBranch2Test, DevProtection_GenReq_EncodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/br2test/prot_genfail", "prot_genfail",
        "NSM_NetworkAdapter");

    auto result = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// NsmDeviceProtectionOptions: handleResponseMsg with non-zero CC
// Exercises the path where decode succeeds but cc indicates an error,
// causing the if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS) to be FALSE.
// ============================================================================

TEST_F(NsmNetworkAdapterBranch2Test, DevProtection_HandleResp_NonZeroCC)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/br2test/prot_ccerr", "prot_ccerr",
        "NSM_NetworkAdapter");

    // Build a response with NSM_ERROR completion code
    Response response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_protection_options_resp(0, NSM_ERROR, 0xABCD, PROTECTION_NONE,
                                       msg);

    auto rc = sensor->handleResponseMsg(msg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
    // Protection level should remain Unknown (initial value, not updated)
    EXPECT_EQ(sensor->protectionIntf->protectionLevel(),
              ProtectionOption::Unknown);
}

// ============================================================================
// Factory: with associations from D-Bus (exercises coGetAssociations path)
// ============================================================================

TEST_F(NsmNetworkAdapterBranch2Test, Factory_WithAssociations_SensorCreated)
{
    const std::string testPath = "/xyz/test/netadapter_br2/with_assoc";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("NetAdapter_BR2_Assoc");
    pm["UUID"] = deviceUuid;
    pm["Type"] = std::string("NSM_NetworkAdapter");
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/br2test/");

    // Set up association service map
    auto& serviceMap = utils::MockDbusAsync::serviceMap();
    MapperServiceMap assocServiceMap = {
        {{
            "xyz.openbmc_project.NSM",
            {intf + ".Associations"},
        }},
    };
    serviceMap = assocServiceMap;

    dbus::PropertyMap assocProperties = {
        {"Forward", "chassis"},
        {"Backward", "network_adapter"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system/chassis"},
    };
    auto& assocPm = utils::MockDbusAsync::propertyMap(testPath,
                                                      intf + ".Associations");
    assocPm = assocProperties;

    const size_t before = device->deviceSensors.size();
    createNSMNetworkAdapter(mockManager, intf, testPath);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// Factory: missing Type property (FALSE branch for count("Type"))
// type="" -> sensor created with empty type
// ============================================================================

TEST_F(NsmNetworkAdapterBranch2Test, Factory_MissingType_EmptyType)
{
    const std::string testPath = "/xyz/test/netadapter_br2/no_type";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("NetAdapter_BR2_NoType");
    pm["UUID"] = deviceUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/br2test/");
    // Type intentionally omitted

    const size_t before = device->deviceSensors.size();
    createNSMNetworkAdapter(mockManager, intf, testPath);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// convertNsmdToDbusProtectionMode: verify each switch case explicitly
// ============================================================================

TEST_F(NsmNetworkAdapterBranch2Test, ConvertProtectionMode_AllCases)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmDeviceProtectionOptions sensor(bus,
                                      "/xyz/openbmc_project/br2test/prot_conv",
                                      "prot_conv", "NSM_NetworkAdapter");

    EXPECT_EQ(sensor.convertNsmdToDbusProtectionMode(PROTECTION_NONE),
              ProtectionOption::NoProtection);
    EXPECT_EQ(sensor.convertNsmdToDbusProtectionMode(
                  PROTECTION_PREVENT_FW_UPDATE_AND_CONFIG),
              ProtectionOption::PreventAll);
    EXPECT_EQ(
        sensor.convertNsmdToDbusProtectionMode(PROTECTION_PREVENT_FW_UPDATE),
        ProtectionOption::PreventHostFirmwareUpdates);
    EXPECT_EQ(sensor.convertNsmdToDbusProtectionMode(PROTECTION_PREVENT_CONFIG),
              ProtectionOption::PreventHostConfigurations);
    // Default/unknown value
    EXPECT_EQ(sensor.convertNsmdToDbusProtectionMode(0xFE),
              ProtectionOption::Unknown);
    EXPECT_EQ(sensor.convertNsmdToDbusProtectionMode(0x55),
              ProtectionOption::Unknown);
}

// ============================================================================
// NsmDeviceProtectionOptions: setProtectionOptions with Unknown protection
// option (exercises default case in switch)
// ============================================================================

TEST_F(NsmNetworkAdapterBranch2Test,
       DevProtection_SetProtOpts_UnknownProtection)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/br2test/prot_unk_set", "prot_unk_set",
        "NSM_NetworkAdapter");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("com.nvidia.DeviceProtection.ProtectionOption.Unknown");

    sensor->setProtectionOptions(value, &status, device);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}
