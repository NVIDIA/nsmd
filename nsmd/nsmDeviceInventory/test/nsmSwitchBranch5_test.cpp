/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage batch 5 for nsmSwitch.cpp:
 *
 * Factory:
 * - createNsmSwitchDI with type NSM_NVSwitch (SupportL1PredictionMode
 *   true/false)
 * - createNsmSwitchDI with type NSM_Switch (with/without SwitchType,
 *   SwitchSupportedProtocols)
 * - createNsmSwitchDI with type NSM_Chassis_Attributes
 * - createNsmSwitchDI with type NSM_FabricManager (with/without optional
 *   properties)
 * - createNsmSwitchDI with type NSM_PortDisableFuture (with/without Priority)
 * - createNsmSwitchDI with type NSM_PowerMode (with/without Priority)
 *
 * Sensor:
 * - NsmSwitchIsolationMode::handleResponseMsg with Disabled, Unknown modes
 * - NsmSwitchIsolationMode::setSwitchIsolationMode with
 *   SwitchCommunicationDisabled
 * - NsmSwitchIsolationMode::setSwitchIsolationMode with null value
 * - NsmSwitchDIPowerMode::setL1PowerModePatch with each valid key
 * - NsmSwitchDIPowerMode::setL1PowerDevice success path
 * - NsmSwitchL1PredictionMode::handleResponseMsg DISABLED branch
 * - NsmSwitchL1PredictionMode::setL1PredictionMode false path
 * - NsmSwitchL1PredictionMode::setL1PredictionMode null value
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
#include "test/commonMock.hpp"

#undef private
#undef protected

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmSwitchDI(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

using PatchTupleValue =
    std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
using PatchValueList = std::vector<std::tuple<std::string, PatchTupleValue>>;

static auto& testBus5 = utils::DBusHandler::getBus();

// ============================================================================
// Fixture
// ============================================================================

class NsmSwitchBranch5Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch";
    const uuid_t switchUuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:505";
    const std::string switchName = "NVSwitch_Br5";
    const std::string inventoryPath =
        "/xyz/openbmc_project/inventory/br5test/nvswitch/";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchBranch5Test() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmSwitchBranch5Test()
    {
        cleanupDeviceSensors(devices);
    }

    void setupBase(const std::string& path)
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
        pm["Name"] = switchName;
        pm["UUID"] = switchUuid;
        pm["InventoryObjPath"] = inventoryPath;
    }

    std::shared_ptr<NsmSwitchDIPowerMode> makePowerMode()
    {
        auto pm = std::make_shared<NsmSwitchDIPowerMode>(switchName,
                                                         inventoryPath);
        pm->invoke(pdiMethod(hwModeControl), false);
        pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(0));
        pm->invoke(pdiMethod(fwThrottlingMode), false);
        pm->invoke(pdiMethod(predictionMode), false);
        pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(0));
        pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(0));
        pm->invoke(pdiMethod(hwPredictionInactiveTime),
                   static_cast<uint64_t>(0));
        return pm;
    }

    static std::vector<uint8_t> decodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ============================================================================
// Factory: NSM_NVSwitch with SupportL1PredictionMode = true
// ============================================================================

TEST_F(NsmSwitchBranch5Test, Factory_NVSwitch_L1PredictionTrue)
{
    const std::string path = inventoryPath + "nvs_l1true";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = switchName;
    pm["UUID"] = switchUuid;
    pm["InventoryObjPath"] = inventoryPath;
    pm["Type"] = std::string("NSM_NVSwitch");
    pm["SupportL1PredictionMode"] = true;

    createNsmSwitchDI(mockManager, baseIntf, path);

    EXPECT_GT(nvswitch->deviceSensors.size(), 0u);
    EXPECT_GT(nvswitch->staticSensors.size(), 0u);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), 0u);
}

TEST_F(NsmSwitchBranch5Test, Factory_NVSwitch_L1PredictionFalse)
{
    const std::string path = inventoryPath + "nvs_l1false";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = switchName;
    pm["UUID"] = switchUuid;
    pm["InventoryObjPath"] = inventoryPath;
    pm["Type"] = std::string("NSM_NVSwitch");
    pm["SupportL1PredictionMode"] = false;

    createNsmSwitchDI(mockManager, baseIntf, path);

    EXPECT_GT(nvswitch->deviceSensors.size(), 0u);
}

// ============================================================================
// Factory: NSM_Switch with/without SwitchType and Protocols
// ============================================================================

TEST_F(NsmSwitchBranch5Test, Factory_Switch_WithTypeAndProtocols)
{
    const std::string path = inventoryPath + "sw_full";
    setupBase(path);
    const std::string intf = std::string(baseIntf) + ".Switch";

    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_Switch");
    cur["SwitchType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Switch.SwitchType.NVLink");
    cur["SwitchSupportedProtocols"] = std::vector<std::string>{
        "xyz.openbmc_project.Inventory.Item.Switch.SwitchType.NVLink"};

    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), 0u);
}

TEST_F(NsmSwitchBranch5Test, Factory_Switch_WithTypeNoProtocols)
{
    const std::string path = inventoryPath + "sw_noproto";
    setupBase(path);
    const std::string intf = std::string(baseIntf) + ".Switch2";

    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_Switch");
    cur["SwitchType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Switch.SwitchType.NVLink");
    // No SwitchSupportedProtocols

    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), 0u);
}

// ============================================================================
// Factory: NSM_Chassis_Attributes
// ============================================================================

TEST_F(NsmSwitchBranch5Test, Factory_ChassisAttributes)
{
    const std::string path = inventoryPath + "chassis_attr";
    setupBase(path);
    const std::string intf = std::string(baseIntf) + ".ChassisAttributes";

    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_Chassis_Attributes");

    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->staticSensors.size(), 0u);
}

// ============================================================================
// Factory: NSM_FabricManager with all/no optional properties
// ============================================================================

TEST_F(NsmSwitchBranch5Test, Factory_FabricManager_AllProps)
{
    const std::string path = inventoryPath + "fm_full";
    setupBase(path);
    const std::string intf = std::string(baseIntf) + ".FabricManager";

    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_FabricManager");
    cur["Name"] = std::string("FM0");
    cur["InventoryObjPath"] = std::string("/xyz/openbmc_project/fm/");
    cur["Description"] = std::string("Fabric Manager Instance 0");

    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), 0u);
}

TEST_F(NsmSwitchBranch5Test, Factory_FabricManager_NoDescription)
{
    const std::string path = inventoryPath + "fm_nodesc";
    // Use unique base inventory path to avoid D-Bus path collision
    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = std::string("NVSwitch_Br5_nd");
    base["UUID"] = switchUuid;
    base["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/br5test/nvswitch_nd/");

    const std::string intf = std::string(baseIntf) + ".FabricManager2";

    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_FabricManager");
    cur["Name"] = std::string("FM_br5_nodesc");
    cur["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/br5test/fm_nodesc/");
    // No Description -> defaults to ""

    createNsmSwitchDI(mockManager, intf, path);
}

// ============================================================================
// Factory: NSM_PortDisableFuture with/without Priority
// ============================================================================

TEST_F(NsmSwitchBranch5Test, Factory_PortDisableFuture_WithPriority)
{
    const std::string path = inventoryPath + "pdf_prio";
    setupBase(path);
    const std::string intf = std::string(baseIntf) + ".PortDisableFuture";

    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_PortDisableFuture");
    cur["Priority"] = true;

    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), 0u);
}

TEST_F(NsmSwitchBranch5Test, Factory_PortDisableFuture_NoPriority)
{
    const std::string path = inventoryPath + "pdf_noprio";
    setupBase(path);
    const std::string intf = std::string(baseIntf) + ".PortDisableFuture2";

    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_PortDisableFuture");
    // No Priority -> defaults to false

    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), 0u);
}

// ============================================================================
// Factory: NSM_PowerMode with/without Priority
// ============================================================================

TEST_F(NsmSwitchBranch5Test, Factory_PowerMode_WithPriority)
{
    const std::string path = inventoryPath + "pm_prio";
    setupBase(path);
    const std::string intf = std::string(baseIntf) + ".PowerMode";

    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_PowerMode");
    cur["Priority"] = true;

    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), 0u);
}

TEST_F(NsmSwitchBranch5Test, Factory_PowerMode_NoPriority)
{
    const std::string path = inventoryPath + "pm_noprio";
    setupBase(path);
    const std::string intf = std::string(baseIntf) + ".PowerMode2";

    auto& cur = utils::MockDbusAsync::propertyMap(path, intf);
    cur["Type"] = std::string("NSM_PowerMode");
    // No Priority -> defaults to false

    createNsmSwitchDI(mockManager, intf, path);
    EXPECT_GT(nvswitch->roundRobinSensors.size(), 0u);
}

// ============================================================================
// NsmSwitchIsolationMode::handleResponseMsg - Disabled mode
// ============================================================================

TEST_F(NsmSwitchBranch5Test, IsolationMode_HandleResponseMsg_Disabled)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/iso_disabled");
    NsmSwitchIsolationMode sensor("Iso_dis", "NSM_NVSwitch", isolationIntf);

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                              sizeof(uint8_t));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    uint8_t mode = SWITCH_COMMUNICATION_MODE_DISABLED;
    encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL, mode, msg);

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
}

// ============================================================================
// NsmSwitchIsolationMode::handleResponseMsg - Unknown mode value
// ============================================================================

TEST_F(NsmSwitchBranch5Test, IsolationMode_HandleResponseMsg_Unknown)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/iso_unknown");
    NsmSwitchIsolationMode sensor("Iso_unk", "NSM_NVSwitch", isolationIntf);

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                              sizeof(uint8_t));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    // Use an invalid mode value (not ENABLED=0 or DISABLED=1)
    uint8_t mode = 0xFF;
    encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL, mode, msg);

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
}

// ============================================================================
// NsmSwitchIsolationMode::setSwitchIsolationMode - Disabled mode
// ============================================================================

TEST_F(NsmSwitchBranch5Test, SetSwitchIsolationMode_Disabled_Success)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/iso_dis_set");
    NsmSwitchIsolationMode sensor("Iso_dis_set", "NSM_NVSwitch", isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationDisabled");

    // Build success response
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    sensor.setSwitchIsolationMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// NsmSwitchIsolationMode::setSwitchIsolationMode - null value type
// ============================================================================

TEST_F(NsmSwitchBranch5Test, SetSwitchIsolationMode_NullValue)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/iso_null");
    NsmSwitchIsolationMode sensor("Iso_null", "NSM_NVSwitch", isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass a bool instead of string - std::get_if<std::string> returns nullptr
    AsyncSetOperationValueType value = bool{true};

    EXPECT_THROW_COROUTINE(
        sensor.setSwitchIsolationMode(value, &status, nvswitch),
        std::exception);
}

// ============================================================================
// NsmSwitchIsolationMode::setSwitchIsolationMode - Enabled mode success
// ============================================================================

TEST_F(NsmSwitchBranch5Test, SetSwitchIsolationMode_Enabled_Success)
{
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/iso_en_set");
    NsmSwitchIsolationMode sensor("Iso_en_set", "NSM_NVSwitch", isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    sensor.setSwitchIsolationMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - each valid key with correct type
// ============================================================================

TEST_F(NsmSwitchBranch5Test, SetL1PowerModePatch_HWModeControl_Valid)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWModeControl", bool{true}}};

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_power_mode_resp(0, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    pm->setL1PowerModePatch(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSwitchBranch5Test, SetL1PowerModePatch_FWThrottlingMode_Valid)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"FWThrottlingMode", bool{false}}};

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_power_mode_resp(0, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    pm->setL1PowerModePatch(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSwitchBranch5Test, SetL1PowerModePatch_PredictionMode_Valid)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"PredictionMode", bool{true}}};

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_power_mode_resp(0, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    pm->setL1PowerModePatch(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSwitchBranch5Test, SetL1PowerModePatch_HWThreshold_Valid)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWThreshold", uint32_t{500}}};

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_power_mode_resp(0, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    pm->setL1PowerModePatch(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSwitchBranch5Test, SetL1PowerModePatch_HWActiveTime_Valid)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWActiveTime", uint32_t{100}}};

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_power_mode_resp(0, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    pm->setL1PowerModePatch(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSwitchBranch5Test, SetL1PowerModePatch_HWInactiveTime_Valid)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWInactiveTime", uint32_t{200}}};

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_power_mode_resp(0, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    pm->setL1PowerModePatch(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmSwitchBranch5Test, SetL1PowerModePatch_HWPredictionInactiveTime_Valid)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        PatchValueList{{"HWPredictionInactiveTime", uint32_t{300}}};

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_power_mode_resp(0, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    pm->setL1PowerModePatch(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerModePatch - multiple keys at once
// ============================================================================

TEST_F(NsmSwitchBranch5Test, SetL1PowerModePatch_MultipleKeys)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = PatchValueList{
        {"HWModeControl", bool{true}},
        {"FWThrottlingMode", bool{false}},
        {"PredictionMode", bool{true}},
        {"HWThreshold", uint32_t{1000}},
        {"HWActiveTime", uint32_t{50}},
        {"HWInactiveTime", uint32_t{150}},
        {"HWPredictionInactiveTime", uint32_t{250}},
    };

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_power_mode_resp(0, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    pm->setL1PowerModePatch(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// NsmSwitchDIPowerMode::setL1PowerDevice - success path
// ============================================================================

TEST_F(NsmSwitchBranch5Test, SetL1PowerDevice_Success)
{
    auto pm = makePowerMode();
    nsm_power_mode_data data = {};
    data.l1_hw_mode_control = 1;

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_power_mode_resp(0, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    pm->setL1PowerDevice(data, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// NsmSwitchL1PredictionMode::handleResponseMsg - DISABLED branch
// ============================================================================

TEST_F(NsmSwitchBranch5Test, PredictionMode_HandleResponseMsg_Disabled)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/pred_disabled");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/pred_disabled");
    NsmSwitchL1PredictionMode sensor("Pred_dis", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_device_mode_setting_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto* payload =
        reinterpret_cast<nsm_get_device_mode_setting_resp*>(msg->payload);
    payload->hdr.completion_code = NSM_SUCCESS;
    payload->hdr.data_size = htole16(sizeof(uint8_t));
    payload->device_mode = DISABLED;

    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_FALSE(enableIntf->enabled());
}

// ============================================================================
// NsmSwitchL1PredictionMode::setL1PredictionMode - false path
// ============================================================================

TEST_F(NsmSwitchBranch5Test, SetL1PredictionMode_False_Success)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/pred_false_ok");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/pred_false_ok");
    NsmSwitchL1PredictionMode sensor("Pred_false", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{false};

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    sensor.setL1PredictionMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// NsmSwitchL1PredictionMode::setL1PredictionMode - true success path
// ============================================================================

TEST_F(NsmSwitchBranch5Test, SetL1PredictionMode_True_Success)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/pred_true_ok");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/pred_true_ok");
    NsmSwitchL1PredictionMode sensor("Pred_true", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL, msg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp));

    sensor.setL1PredictionMode(value, &status, nvswitch);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// NsmSwitchL1PredictionMode::setL1PredictionMode - null value
// ============================================================================

TEST_F(NsmSwitchBranch5Test, SetL1PredictionMode_NullValue)
{
    auto enableIntf = std::make_shared<EnableIntf>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/pred_null");
    auto assocDefIntf = std::make_shared<AssociationDefinitionsInft>(
        testBus5, "/xyz/openbmc_project/inventory/br5test/pred_null");
    NsmSwitchL1PredictionMode sensor("Pred_null", "NSM_NVSwitch", enableIntf,
                                     assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass string instead of bool
    AsyncSetOperationValueType value = std::string("not a bool");

    EXPECT_THROW_COROUTINE(sensor.setL1PredictionMode(value, &status, nvswitch),
                           std::exception);
}

// ============================================================================
// NsmSwitchDIPowerMode::update - success with mixed ternary
// (hwModeControl=1, fwThrottling=0, prediction=1)
// ============================================================================

TEST_F(NsmSwitchBranch5Test, PowerMode_Update_MixedTernary)
{
    auto pm = makePowerMode();

    nsm_power_mode_data data = {};
    data.l1_hw_mode_control = 1;
    data.l1_hw_mode_threshold = 42;
    data.l1_fw_throttling_mode = 0;
    data.l1_prediction_mode = 1;
    data.l1_hw_active_time = 5;
    data.l1_hw_inactive_time = 10;
    data.l1_prediction_inactive_time = 15;

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_power_mode_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &data, msg);

    EXPECT_CALL(*nvswitch, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp));

    pm->update(nvswitch);

    EXPECT_TRUE(pm->invoke(pdiMethod(hwModeControl)));
    EXPECT_FALSE(pm->invoke(pdiMethod(fwThrottlingMode)));
    EXPECT_TRUE(pm->invoke(pdiMethod(predictionMode)));
}
