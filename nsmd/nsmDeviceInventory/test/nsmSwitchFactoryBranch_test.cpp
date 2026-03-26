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
 * Additional branch coverage tests for createNsmSwitchDI factory.
 *
 * Targets FALSE branches of .count() checks and type-dispatch paths //
 * not yet exercised in nsmSwitch_extended_test.cpp or
 * nsmSwitchBranch_test.cpp.
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"
#include "network-ports.h"

#include "nsmDeviceInventory/nsmSwitch.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine createNsmSwitchDI(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

class NsmSwitchFactoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch";
    const uuid_t switchUuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string switchName = "NVSwitch_FBr";
    const std::string inventoryPath =
        "/xyz/openbmc_project/inventory/system/fabr/nvswitch/";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchFactoryBranchTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchFactoryBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    void setupBaseProperties(const std::string& path,
                             dbus::PropertyMap extra = {})
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
        pm["Name"] = switchName;
        pm["UUID"] = switchUuid;
        pm["InventoryObjPath"] = inventoryPath;
        for (auto& [k, v] : extra)
        {
            pm[k] = v;
        }
    }

    void setupCurrentProperties(const std::string& path,
                                const std::string& intf,
                                dbus::PropertyMap props)
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
        pm = props;
    }
};

// ============================================================================
// Missing "Type" from current properties -> type="" -> no type branch matches
// -> falls through all if/else if -> NSM_SUCCESS returned, no sensors added
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, Factory_MissingType_NoSensorCreated)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch.PortDisableFuture";
    const std::string path = "/test/switchfbr/no_type";

    setupBaseProperties(path);
    setupCurrentProperties(path, intf, {});
    // "Type" omitted -> count("Type") FALSE branch

    const size_t before = nvswitch->deviceSensors.size() +
                          nvswitch->roundRobinSensors.size() +
                          nvswitch->staticSensors.size();
    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_EQ(nvswitch->deviceSensors.size() +
                  nvswitch->roundRobinSensors.size() +
                  nvswitch->staticSensors.size(),
              before);
}

// ============================================================================
// Missing "UUID" from base -> uuid="" -> getNsmDeviceFromStaticUUID returns
// a new device (mock behavior). Sensor creation still proceeds.
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, DISABLED_Factory_MissingUUID_FalseBranch)
{
    const std::string path = "/test/switchfbr/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = switchName;
    // "UUID" omitted -> count("UUID") FALSE branch -> uuid=""
    pm["InventoryObjPath"] = inventoryPath + "nouuid/";
    pm["Type"] = std::string("NSM_Chassis_Attributes");

    createNsmSwitchDI(mockManager, baseIntf, path);
}

// ============================================================================
// NSM_PortDisableFuture with missing Priority -> priority=false (default)
// Covers: L843 count("Priority") FALSE branch
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, PortDisableFuture_MissingPriority)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch.PortDisableFuture";
    const std::string path = "/test/switchfbr/pdf_no_prio";

    setupBaseProperties(path);
    setupCurrentProperties(path, intf,
                           {{"Type", std::string("NSM_PortDisableFuture")}});
    // "Priority" omitted -> count("Priority") FALSE -> priority=false

    const size_t before = nvswitch->roundRobinSensors.size();
    createNsmSwitchDI(mockManager, intf, path);
    // Sensor added with priority=false -> roundRobin
    EXPECT_GT(nvswitch->roundRobinSensors.size(), before);
}

// ============================================================================
// NSM_PortDisableFuture with Priority=true -> priority sensor
// Covers: L843 count("Priority") TRUE branch, priority=true
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, PortDisableFuture_WithPriorityTrue)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch.PortDisableFuture";
    const std::string path = "/test/switchfbr/pdf_prio_true";

    setupBaseProperties(path);
    setupCurrentProperties(path, intf,
                           {{"Type", std::string("NSM_PortDisableFuture")},
                            {"Priority", bool(true)}});

    const size_t before = nvswitch->prioritySensors.size();
    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->prioritySensors.size(), before);
}

// ============================================================================
// NSM_PowerMode with missing Priority -> priority=false
// Covers: L871 count("Priority") FALSE branch
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, PowerMode_MissingPriority)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch.PowerMode";
    const std::string path = "/test/switchfbr/pm_no_prio";

    setupBaseProperties(path);
    setupCurrentProperties(path, intf,
                           {{"Type", std::string("NSM_PowerMode")}});
    // "Priority" omitted -> priority=false

    const size_t before = nvswitch->roundRobinSensors.size();
    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), before);
}

// ============================================================================
// NSM_PowerMode with Priority=true
// Covers: L871 count("Priority") TRUE branch, priority=true
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, PowerMode_WithPriorityTrue)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch.PowerMode";
    const std::string path = "/test/switchfbr/pm_prio_true";

    setupBaseProperties(path);
    setupCurrentProperties(
        path, intf,
        {{"Type", std::string("NSM_PowerMode")}, {"Priority", bool(true)}});

    const size_t before = nvswitch->prioritySensors.size();
    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->prioritySensors.size(), before);
}

// ============================================================================
// NSM_Switch with missing SwitchType -> switchType="" (FALSE branch)
// Covers: L949 count("SwitchType") FALSE
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, DISABLED_Switch_MissingSwitchType)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch.Switch";
    const std::string path = "/test/switchfbr/sw_no_type";

    setupBaseProperties(path);
    setupCurrentProperties(
        path, intf,
        {{"Type", std::string("NSM_Switch")},
         {"SwitchSupportedProtocols",
          std::vector<std::string>{
              "xyz.openbmc_project.Inventory.Item.Switch.SwitchType.OEM"}}});
    // "SwitchType" omitted -> switchType="" -> convertSwitchTypeFromString("")
    // may throw -> caught by the switch constructor or by factory //

    createNsmSwitchDI(mockManager, intf, path);
}

// ============================================================================
// NSM_FabricManager with missing Name (current) -> nameFM="" (FALSE branch)
// Covers: L989 count("Name") FALSE
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, DISABLED_FabricManager_MissingName)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch.FabricManager";
    const std::string path = "/test/switchfbr/fm_no_name";

    setupBaseProperties(path);
    setupCurrentProperties(
        path, intf,
        {{"Type", std::string("NSM_FabricManager")},
         {"InventoryObjPath",
          std::string(
              "/xyz/openbmc_project/inventory/system/fabricmanager_noname/")},
         {"Description", std::string("FM no name")}});
    // "Name" omitted -> count("Name") FALSE -> nameFM=""

    createNsmSwitchDI(mockManager, intf, path);
}

// ============================================================================
// NSM_FabricManager with missing InventoryObjPath -> inventoryObjPathFM=""
// Covers: L995 count("InventoryObjPath") FALSE
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest,
       DISABLED_FabricManager_MissingInventoryObjPath)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch.FabricManager";
    const std::string path = "/test/switchfbr/fm_no_inv";

    setupBaseProperties(path);
    setupCurrentProperties(path, intf,
                           {{"Type", std::string("NSM_FabricManager")},
                            {"Name", std::string("FM_noInv")},
                            {"Description", std::string("FM no invpath")}});
    // "InventoryObjPath" omitted -> count("InventoryObjPath") FALSE

    createNsmSwitchDI(mockManager, intf, path);
}

// ============================================================================
// NSM_FabricManager with missing Description -> description="" (FALSE branch)
// Covers: L1001 count("Description") FALSE
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, FabricManager_MissingDescription)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch.FabricManager";
    const std::string path = "/test/switchfbr/fm_no_desc";

    setupBaseProperties(path);
    setupCurrentProperties(
        path, intf,
        {{"Type", std::string("NSM_FabricManager")},
         {"Name", std::string("FM_noDesc")},
         {"InventoryObjPath",
          std::string(
              "/xyz/openbmc_project/inventory/system/fabricmanager_nodesc/")}});
    // "Description" omitted -> count("Description") FALSE

    const size_t before = nvswitch->roundRobinSensors.size();
    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), before);
}

// ============================================================================
// Unknown type -> none of the if/else if branches match
// -> falls through to co_return NSM_SUCCESS with no sensor added //

// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, Factory_UnknownType_NoSensor)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch.ChassisAttributes";
    const std::string path = "/test/switchfbr/unknown";

    setupBaseProperties(path);
    setupCurrentProperties(path, intf,
                           {{"Type", std::string("NSM_UnknownType_XYZ")}});

    const size_t before = nvswitch->deviceSensors.size() +
                          nvswitch->roundRobinSensors.size() +
                          nvswitch->staticSensors.size();
    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_EQ(nvswitch->deviceSensors.size() +
                  nvswitch->roundRobinSensors.size() +
                  nvswitch->staticSensors.size(),
              before);
}

// ============================================================================
// NsmSwitchIsolationMode::handleResponseMsg with
// SWITCH_COMMUNICATION_MODE_DISABLED Covers L429: else if (isolationMode ==
// SWITCH_COMMUNICATION_MODE_DISABLED)
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, IsolationMode_HandleResponse_DisabledMode)
{
    static auto& testBus = utils::DBusHandler::getBus();
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/fabr/iso_disabled_handleresp");
    NsmSwitchIsolationMode sensor("IsoSw_disabled_hr", "NSM_NVSwitch",
                                  isolationIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_switch_isolation_mode_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_switch_isolation_mode_resp(
        0, NSM_SUCCESS, ERR_NULL, SWITCH_COMMUNICATION_MODE_DISABLED, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationDisabled);
}

// ============================================================================
// NsmSwitchIsolationMode::handleResponseMsg with ENABLED mode
// Covers L423: if (isolationMode == SWITCH_COMMUNICATION_MODE_ENABLED)
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, IsolationMode_HandleResponse_EnabledMode)
{
    static auto& testBus = utils::DBusHandler::getBus();
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/fabr/iso_enabled_handleresp");
    NsmSwitchIsolationMode sensor("IsoSw_enabled_hr", "NSM_NVSwitch",
                                  isolationIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_switch_isolation_mode_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_switch_isolation_mode_resp(
        0, NSM_SUCCESS, ERR_NULL, SWITCH_COMMUNICATION_MODE_ENABLED, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationEnabled);
}

// ============================================================================
// NsmSwitchIsolationMode::handleResponseMsg with unknown mode value
// Covers L434: else -> SwitchCommunicationUnknown
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, IsolationMode_HandleResponse_UnknownMode)
{
    static auto& testBus = utils::DBusHandler::getBus();
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/fabr/iso_unknown_handleresp");
    NsmSwitchIsolationMode sensor("IsoSw_unknown_hr", "NSM_NVSwitch",
                                  isolationIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_switch_isolation_mode_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    // Use value 99 which is neither ENABLED nor DISABLED
    auto rc = encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    99, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(isolationIntf->isolationMode(),
              SwitchCommunicationMode::SwitchCommunicationUnknown);
}

// ============================================================================
// NsmSwitchIsolationMode::setSwitchIsolationMode with invalid string
// Covers: L464 else -> invalid isolation mode -> WriteFailure
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest,
       SetSwitchIsolationMode_InvalidMode_WriteFailure)
{
    static auto& testBus = utils::DBusHandler::getBus();
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/fabr/iso_invalid_mode");
    NsmSwitchIsolationMode sensor("IsoSw_invalid", "NSM_NVSwitch",
                                  isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("InvalidModeName");

    sensor.setSwitchIsolationMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmSwitchIsolationMode::genRequestMsg encode fail
// Covers: L397 rc != NSM_SW_SUCCESS -> return nullopt
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, IsolationMode_GenRequestMsg_EncodeFail)
{
    static auto& testBus = utils::DBusHandler::getBus();
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/fabr/iso_genreq_fail");
    NsmSwitchIsolationMode sensor("IsoSw_genreq", "NSM_NVSwitch",
                                  isolationIntf);

    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmSwitchL1PredictionMode::genRequestMsg encode fail
// Covers: L531 rc != NSM_SW_SUCCESS -> return nullopt
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, L1PredictionMode_GenRequestMsg_EncodeFail)
{
    static auto& testBus = utils::DBusHandler::getBus();
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/fabr/pred_genreq_fail");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/fabr/pred_genreq_fail");
    NsmSwitchL1PredictionMode sensor("Pred_genreq", "NSM_NVSwitch", enableIntf,
                                     assocIntf);

    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmSwitchL1PredictionMode::handleResponseMsg success with ENABLED
// Covers: L554 predictionMode == ENABLED -> enabled(true)
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, L1PredictionMode_HandleResponse_Enabled)
{
    static auto& testBus = utils::DBusHandler::getBus();
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/fabr/pred_enabled_hr");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/fabr/pred_enabled_hr");
    NsmSwitchL1PredictionMode sensor("Pred_en_hr", "NSM_NVSwitch", enableIntf,
                                     assocIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_device_mode_setting_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    nsm_l1_prediction_mode_config mode = nsm_l1_prediction_mode_config::ENABLED;
    auto rc = encode_get_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   mode, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(enableIntf->enabled());
}

// ============================================================================
// NsmSwitchL1PredictionMode::handleResponseMsg success with DISABLED
// Covers: L558 else -> enabled(false)
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, L1PredictionMode_HandleResponse_Disabled)
{
    static auto& testBus = utils::DBusHandler::getBus();
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/fabr/pred_disabled_hr");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/fabr/pred_disabled_hr");
    NsmSwitchL1PredictionMode sensor("Pred_dis_hr", "NSM_NVSwitch", enableIntf,
                                     assocIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_device_mode_setting_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    nsm_l1_prediction_mode_config mode =
        nsm_l1_prediction_mode_config::DISABLED;
    auto rc = encode_get_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   mode, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(enableIntf->enabled());
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerDevice -- decode fail
// Covers L219-231: else branch (rc!=SUCCESS || cc!=SUCCESS)
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, SetL1PowerDevice_DecodeFail_WriteFailure)
{
    auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName, inventoryPath);
    pm->invoke(pdiMethod(hwModeControl), false);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(fwThrottlingMode), false);
    pm->invoke(pdiMethod(predictionMode), false);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(0));

    nsm_power_mode_data data = {};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    responseData[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    pm->setL1PowerDevice(data, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch -- wrong value type
// Covers L254: if (!patchRequestedValues) -> throws InvalidArgument
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, SetL1PowerModePatch_WrongType_Throws)
{
    auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName, inventoryPath);
    pm->invoke(pdiMethod(hwModeControl), false);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(fwThrottlingMode), false);
    pm->invoke(pdiMethod(predictionMode), false);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(0));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass a string instead of PatchValueList
    AsyncSetOperationValueType value = std::string("wrong type");

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch -- empty list
// Covers L261: if (patchRequestedValues->empty()) -> throws InvalidArgument
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, SetL1PowerModePatch_EmptyList_Throws)
{
    using PatchTupleValue =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValueList =
        std::vector<std::tuple<std::string, PatchTupleValue>>;

    auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName, inventoryPath);
    pm->invoke(pdiMethod(hwModeControl), false);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(fwThrottlingMode), false);
    pm->invoke(pdiMethod(predictionMode), false);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(0));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = PatchValueList{};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch -- unrecognized property key
// Covers L358: else -> Unrecognized property -> throws InvalidArgument
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, SetL1PowerModePatch_UnrecognizedKey_Throws)
{
    using PatchTupleValue =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValueList =
        std::vector<std::tuple<std::string, PatchTupleValue>>;

    auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName, inventoryPath);
    pm->invoke(pdiMethod(hwModeControl), false);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(fwThrottlingMode), false);
    pm->invoke(pdiMethod(predictionMode), false);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(0));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"BogusProperty", bool{true}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch -- wrong type for HWModeControl
// Covers L272: if (!l1HWModeControl) -> throws InvalidArgument
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest,
       SetL1PowerModePatch_HWModeControl_WrongType_Throws)
{
    using PatchTupleValue =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValueList =
        std::vector<std::tuple<std::string, PatchTupleValue>>;

    auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName, inventoryPath);
    pm->invoke(pdiMethod(hwModeControl), false);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(fwThrottlingMode), false);
    pm->invoke(pdiMethod(predictionMode), false);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(0));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // HWModeControl expects bool, pass uint32_t
    AsyncSetOperationValueType value =
        PatchValueList{{"HWModeControl", uint32_t{1}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// Wrong types for FWThrottlingMode, PredictionMode, HWThreshold, etc.
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest,
       SetL1PowerModePatch_FWThrottlingMode_WrongType_Throws)
{
    using PatchTupleValue =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValueList =
        std::vector<std::tuple<std::string, PatchTupleValue>>;

    auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName, inventoryPath);
    pm->invoke(pdiMethod(hwModeControl), false);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(fwThrottlingMode), false);
    pm->invoke(pdiMethod(predictionMode), false);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(0));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"FWThrottlingMode", uint32_t{1}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchFactoryBranchTest,
       SetL1PowerModePatch_PredictionMode_WrongType_Throws)
{
    using PatchTupleValue =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValueList =
        std::vector<std::tuple<std::string, PatchTupleValue>>;

    auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName, inventoryPath);
    pm->invoke(pdiMethod(hwModeControl), false);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(fwThrottlingMode), false);
    pm->invoke(pdiMethod(predictionMode), false);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(0));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"PredictionMode", uint32_t{1}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchFactoryBranchTest,
       SetL1PowerModePatch_HWThreshold_WrongType_Throws)
{
    using PatchTupleValue =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValueList =
        std::vector<std::tuple<std::string, PatchTupleValue>>;

    auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName, inventoryPath);
    pm->invoke(pdiMethod(hwModeControl), false);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(fwThrottlingMode), false);
    pm->invoke(pdiMethod(predictionMode), false);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(0));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWThreshold", bool{true}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchFactoryBranchTest,
       SetL1PowerModePatch_HWActiveTime_WrongType_Throws)
{
    using PatchTupleValue =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValueList =
        std::vector<std::tuple<std::string, PatchTupleValue>>;

    auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName, inventoryPath);
    pm->invoke(pdiMethod(hwModeControl), false);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(fwThrottlingMode), false);
    pm->invoke(pdiMethod(predictionMode), false);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(0));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWActiveTime", bool{false}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchFactoryBranchTest,
       SetL1PowerModePatch_HWInactiveTime_WrongType_Throws)
{
    using PatchTupleValue =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValueList =
        std::vector<std::tuple<std::string, PatchTupleValue>>;

    auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName, inventoryPath);
    pm->invoke(pdiMethod(hwModeControl), false);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(fwThrottlingMode), false);
    pm->invoke(pdiMethod(predictionMode), false);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(0));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWInactiveTime", bool{false}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

TEST_F(NsmSwitchFactoryBranchTest,
       SetL1PowerModePatch_HWPredictionInactiveTime_WrongType_Throws)
{
    using PatchTupleValue =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValueList =
        std::vector<std::tuple<std::string, PatchTupleValue>>;

    auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName, inventoryPath);
    pm->invoke(pdiMethod(hwModeControl), false);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(fwThrottlingMode), false);
    pm->invoke(pdiMethod(predictionMode), false);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(0));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWPredictionInactiveTime", bool{false}}};

    EXPECT_THROW_COROUTINE(pm->setL1PowerModePatch(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch -- asyncPatchInProgress
// Covers L241: if (asyncPatchInProgress) -> Unavailable
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest,
       SetL1PowerModePatch_AlreadyInProgress_Unavailable)
{
    using PatchTupleValue =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValueList =
        std::vector<std::tuple<std::string, PatchTupleValue>>;

    auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName, inventoryPath);
    pm->invoke(pdiMethod(hwModeControl), false);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(fwThrottlingMode), false);
    pm->invoke(pdiMethod(predictionMode), false);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(0));

    pm->asyncPatchInProgress = true;

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWModeControl", bool{true}}};

    pm->setL1PowerModePatch(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

// ============================================================================
// NsmSwitchIsolationMode::setSwitchIsolationMode -- decode fail
// Covers L513-520: else branch (rc!=SUCCESS || cc!=SUCCESS) -> WriteFailure
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest,
       SetSwitchIsolationMode_DecodeFail_WriteFailure)
{
    static auto& testBus = utils::DBusHandler::getBus();
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus, "/xyz/openbmc_project/inventory/fabr/iso_decode_fail");
    NsmSwitchIsolationMode sensor("IsoSw_decfail", "NSM_NVSwitch",
                                  isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    responseData[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    sensor.setSwitchIsolationMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmSwitchL1PredictionMode::setL1PredictionMode -- decode fail
// Covers L633-638: else branch (rc!=SUCCESS || cc!=SUCCESS) -> WriteFailure
// ============================================================================
TEST_F(NsmSwitchFactoryBranchTest, SetL1PredictionMode_DecodeFail_WriteFailure)
{
    static auto& testBus = utils::DBusHandler::getBus();
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus, "/xyz/openbmc_project/inventory/fabr/pred_decode_fail");
    auto assocIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus, "/xyz/openbmc_project/inventory/fabr/pred_decode_fail");
    NsmSwitchL1PredictionMode sensor("Pred_decfail", "NSM_NVSwitch", enableIntf,
                                     assocIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    responseData[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    sensor.setL1PredictionMode(value, &status, nvswitch);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}
