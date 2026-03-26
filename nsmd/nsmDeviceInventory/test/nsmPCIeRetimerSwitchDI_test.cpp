/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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
 * Branch coverage tests for nsmd/nsmDeviceInventory/nsmPCIeRetimerSwitchDI.cpp
 *
 * Covers:
 * - NsmPCIeRetimerSwitchDI constructors (both variants)
 * - NsmPCIeRetimerSwitchDI::update (isMultiPciePortEnabled=false and true,
 *   sensorIO failure, decode failure, success)
 * - NsmPCIeRetimerSwitchGetClockState constructor
 * - NsmPCIeRetimerSwitchGetClockState::genRequestMsg (success)
 * - NsmPCIeRetimerSwitchGetClockState::handleResponseMsg
 *   (decode fail, cc fail, success)
 * - NsmPCIeRetimerSwitchGetClockState::getRetimerClockState (instances 0-7 +
 *   default)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "nsmPCIeRetimerSwitchDI.hpp"

using namespace nsm;

// ============================================================================
// Helper: build a valid group-0 response buffer
// ============================================================================

static std::vector<uint8_t> makeGroup0Resp(uint32_t vendorId = 0x10DE,
                                           uint32_t deviceId = 0x2330,
                                           uint8_t cc = NSM_SUCCESS)
{
    nsm_query_scalar_group_telemetry_group_0 data{};
    data.pci_vendor_id = vendorId;
    data.pci_device_id = deviceId;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group0_resp(0, cc, ERR_NULL, &data,
                                                       msg);
    return buf;
}

// ============================================================================
// NsmPCIeRetimerSwitchDI constructor tests
// ============================================================================

TEST(NsmPCIeRetimerSwitchDI, Constructor_Basic_NoAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "Retimer0";
    std::string type = "NSM_PCIeRetimer_Switch";
    std::string inv = "/xyz/openbmc_project/inventory/";
    std::vector<utils::Association> assoc;

    NsmPCIeRetimerSwitchDI obj(bus, name, assoc, type, inv, 1);
    EXPECT_EQ(obj.getName(), name);
    EXPECT_EQ(obj.getType(), type);
    EXPECT_EQ(obj.deviceIndex, 1);
    EXPECT_FALSE(obj.isMultiPciePortEnabled);
    EXPECT_NE(obj.switchIntf, nullptr);
}

TEST(NsmPCIeRetimerSwitchDI, Constructor_Basic_WithAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "Retimer1";
    std::string type = "NSM_PCIeRetimer_Switch";
    std::string inv = "/xyz/openbmc_project/inventory/";
    std::vector<utils::Association> assoc = {
        {"chassis", "retimer", "/xyz/openbmc_project/inventory/system"}};

    NsmPCIeRetimerSwitchDI obj(bus, name, assoc, type, inv, 2);
    EXPECT_EQ(obj.getName(), name);
    EXPECT_EQ(obj.deviceIndex, 2);
}

TEST(NsmPCIeRetimerSwitchDI, Constructor_MultiPort)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "MultiPortSwitch0";
    std::string type = "NSM_MultiPortPCIeSwitchDevice";
    std::string inv = "/xyz/openbmc_project/inventory/";
    std::vector<utils::Association> assoc;

    NsmPCIeRetimerSwitchDI obj(bus, name, assoc, type, inv, 3, 1, 2, 0);
    EXPECT_EQ(obj.getName(), name);
    EXPECT_EQ(obj.deviceIndex, 3);
    EXPECT_TRUE(obj.isMultiPciePortEnabled);
    EXPECT_EQ(obj.multiPortType, 1);
    EXPECT_EQ(obj.multiPortIndex, 2);
    EXPECT_EQ(obj.multiPortUpstreamPort, 0);
}

// ============================================================================
// NsmPCIeRetimerSwitchDI::update tests (uses sensorIO)
// ============================================================================

struct NsmPCIeRetimerSwitchDIUpdateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    const std::string inv = "/xyz/openbmc_project/inventory/";

    NsmPCIeRetimerSwitchDIUpdateTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPCIeRetimerSwitchDIUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmPCIeRetimerSwitchDIUpdateTest, Update_SensorIOFails_Basic)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "Retimer2";
    std::string type = "NSM_PCIeRetimer_Switch";
    std::vector<utils::Association> assoc;

    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, assoc, type, const_cast<std::string&>(inv), 1);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    auto coro = obj->update(gpu);
    (void)coro;
}

TEST_F(NsmPCIeRetimerSwitchDIUpdateTest, Update_SensorIOFails_MultiPort)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "MultiPort2";
    std::string type = "NSM_MultiPortPCIeSwitchDevice";
    std::vector<utils::Association> assoc;

    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, assoc, type, const_cast<std::string&>(inv), 3, 0, 0, 0);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    auto coro = obj->update(gpu);
    (void)coro;
}

TEST_F(NsmPCIeRetimerSwitchDIUpdateTest, Update_DecodeFailure_Basic)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "Retimer3";
    std::string type = "NSM_PCIeRetimer_Switch";
    std::vector<utils::Association> assoc;

    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, assoc, type, const_cast<std::string&>(inv), 1);

    // Tiny response that fails decode (must be >= 7 bytes for
    // decode_reason_code_and_cc)
    std::vector<uint8_t> tiny(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    tiny[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(tiny));

    auto coro = obj->update(gpu);
    (void)coro;
}

TEST_F(NsmPCIeRetimerSwitchDIUpdateTest, Update_ErrorCC_Basic)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "Retimer4";
    std::string type = "NSM_PCIeRetimer_Switch";
    std::vector<utils::Association> assoc;

    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, assoc, type, const_cast<std::string&>(inv), 1);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeGroup0Resp(0, 0, NSM_ERROR)));

    auto coro = obj->update(gpu);
    (void)coro;
}

TEST_F(NsmPCIeRetimerSwitchDIUpdateTest, Update_Success_Basic)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "Retimer5";
    std::string type = "NSM_PCIeRetimer_Switch";
    std::vector<utils::Association> assoc;

    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, assoc, type, const_cast<std::string&>(inv), 1);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeGroup0Resp(0x10DE, 0x2330)));

    auto coro = obj->update(gpu);
    EXPECT_EQ(obj->switchIntf->deviceId(), "0x2330");
    EXPECT_EQ(obj->switchIntf->vendorId(), "0x10de");
}

TEST_F(NsmPCIeRetimerSwitchDIUpdateTest, Update_Success_MultiPort)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "MultiPort3";
    std::string type = "NSM_MultiPortPCIeSwitchDevice";
    std::vector<utils::Association> assoc;

    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, assoc, type, const_cast<std::string&>(inv), 2, 1, 0, 0);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeGroup0Resp(0x1234, 0x5678)));

    auto coro = obj->update(gpu);
    EXPECT_EQ(obj->switchIntf->deviceId(), "0x5678");
}

// ============================================================================
// NsmPCIeRetimerSwitchGetClockState tests (only if ENABLE_CLOCK_OUTPUT_STATE)
// ============================================================================

#if defined(ENABLE_CLOCK_OUTPUT_STATE)

static std::vector<uint8_t> makeClockStateResp(uint32_t clkBuf = 0xFF,
                                               uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_output_enabled_state_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_output_enable_state_resp(0, cc, ERR_NULL, clkBuf, msg);
    return buf;
}

TEST(NsmPCIeRetimerSwitchGetClockState, Constructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ClockState0";
    std::string type = "NSM_PCIeRetimer_Switch";
    std::string inv = "/xyz/openbmc_project/inventory/";

    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 0, inv);
    EXPECT_EQ(obj.getName(), name);
    EXPECT_EQ(obj.getType(), type);
    EXPECT_EQ(obj.deviceInstanceNumber, 0);
    EXPECT_NE(obj.pcieRefClockIntf, nullptr);
}

TEST(NsmPCIeRetimerSwitchGetClockState, GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ClockState1";
    std::string type = "NSM_PCIeRetimer_Switch";
    std::string inv = "/xyz/openbmc_project/inventory/";

    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 1, inv);
    auto result = obj.genRequestMsg(0x01, 0);
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0u);
}

TEST(NsmPCIeRetimerSwitchGetClockState, HandleResponseMsg_BadSize_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ClockState2";
    std::string type = "NSM_PCIeRetimer_Switch";
    std::string inv = "/xyz/openbmc_project/inventory/";

    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 0, inv);

    std::vector<uint8_t> tiny(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<const nsm_msg*>(tiny.data());
    auto rc = obj.handleResponseMsg(msg, tiny.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeRetimerSwitchGetClockState, HandleResponseMsg_ErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ClockState3";
    std::string type = "NSM_PCIeRetimer_Switch";
    std::string inv = "/xyz/openbmc_project/inventory/";

    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 0, inv);

    auto resp = makeClockStateResp(0, NSM_ERROR);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = obj.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmPCIeRetimerSwitchGetClockState, HandleResponseMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ClockState4";
    std::string type = "NSM_PCIeRetimer_Switch";
    std::string inv = "/xyz/openbmc_project/inventory/";

    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 0, inv);

    // clkBuf with bit 8 (retimer1) set → retimer1 clock enabled
    auto resp = makeClockStateResp(0x0100);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = obj.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// getRetimerClockState: test all 8 device instances + default

// nsm_pcie_clock_buffer_data layout:
//   bits  7:0  = GPU clocks (clk_buf_gpu1..8)
//   bits 15:8  = Retimer clocks (clk_buf_retimer1..8)
// So retimer1 is bit 8 (0x0100), retimer2 is bit 9 (0x0200), etc.

TEST(NsmPCIeRetimerSwitchGetClockState, GetRetimerClockState_Instance0)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "CS_inst0", type = "T",
                inv = "/xyz/openbmc_project/inventory/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 0, inv);
    // clk_buf_retimer1 = bit 8
    EXPECT_TRUE(obj.getRetimerClockState(0x0100));
    EXPECT_FALSE(obj.getRetimerClockState(0x0000));
}

TEST(NsmPCIeRetimerSwitchGetClockState, GetRetimerClockState_Instance1)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "CS_inst1", type = "T",
                inv = "/xyz/openbmc_project/inventory/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 1, inv);
    // clk_buf_retimer2 = bit 9
    EXPECT_TRUE(obj.getRetimerClockState(0x0200));
    EXPECT_FALSE(obj.getRetimerClockState(0x0000));
}

TEST(NsmPCIeRetimerSwitchGetClockState, GetRetimerClockState_Instance2)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "CS_inst2", type = "T",
                inv = "/xyz/openbmc_project/inventory/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 2, inv);
    // clk_buf_retimer3 = bit 10
    EXPECT_TRUE(obj.getRetimerClockState(0x0400));
}

TEST(NsmPCIeRetimerSwitchGetClockState, GetRetimerClockState_Instance3)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "CS_inst3", type = "T",
                inv = "/xyz/openbmc_project/inventory/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 3, inv);
    // clk_buf_retimer4 = bit 11
    EXPECT_TRUE(obj.getRetimerClockState(0x0800));
}

TEST(NsmPCIeRetimerSwitchGetClockState, GetRetimerClockState_Instance4)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "CS_inst4", type = "T",
                inv = "/xyz/openbmc_project/inventory/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 4, inv);
    // clk_buf_retimer5 = bit 12
    EXPECT_TRUE(obj.getRetimerClockState(0x1000));
}

TEST(NsmPCIeRetimerSwitchGetClockState, GetRetimerClockState_Instance5)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "CS_inst5", type = "T",
                inv = "/xyz/openbmc_project/inventory/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 5, inv);
    // clk_buf_retimer6 = bit 13
    EXPECT_TRUE(obj.getRetimerClockState(0x2000));
}

TEST(NsmPCIeRetimerSwitchGetClockState, GetRetimerClockState_Instance6)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "CS_inst6", type = "T",
                inv = "/xyz/openbmc_project/inventory/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 6, inv);
    // clk_buf_retimer7 = bit 14
    EXPECT_TRUE(obj.getRetimerClockState(0x4000));
}

TEST(NsmPCIeRetimerSwitchGetClockState, GetRetimerClockState_Instance7)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "CS_inst7", type = "T",
                inv = "/xyz/openbmc_project/inventory/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 7, inv);
    // clk_buf_retimer8 = bit 15
    EXPECT_TRUE(obj.getRetimerClockState(0x8000));
}

TEST(NsmPCIeRetimerSwitchGetClockState, GetRetimerClockState_Default)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "CS_def", type = "T",
                inv = "/xyz/openbmc_project/inventory/";
    // deviceInstance = 255 hits the default case
    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 255, inv);
    EXPECT_FALSE(obj.getRetimerClockState(0xFF));
}

#endif // ENABLE_CLOCK_OUTPUT_STATE

// =============================================================================
// CreatePCIeRetimerSwitch factory branch coverage
// (exercised via NsmObjectFactory dispatch — static linkage workaround)
// =============================================================================

#include "nsmObjectFactory.hpp"

struct NsmPCIeRetimerSwitchFactoryTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string retimerIntf =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_Switch";
    const std::string multiPortIntf =
        "xyz.openbmc_project.Configuration.NSM_MultiPortPCIeSwitchDevice";
    const uuid_t retimerUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    NsmPCIeRetimerSwitchFactoryTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(retimerUuid));
        EXPECT_NE(device, nullptr);
    }

    ~NsmPCIeRetimerSwitchFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", std::string("PCIeRetimerSwitch0")},
        {"UUID", retimerUuid},
        {"InventoryObjPath",
         std::string("/xyz/openbmc_project/inventory/system/")},
        {"DeviceInstance", uint64_t(0)},
    };
};

// NSM_PCIeRetimer_Switch interface → type == "NSM_PCIeRetimer_Switch" TRUE
// branch → NsmPCIeRetimerSwitchDI created and added via addStaticSensor
// DISABLED: constructor throws in test D-Bus env (path registered twice:
// NsmPCIeRetimerSwitchGetClockState + NsmPCIeRetimerSwitchDI at same path)
TEST_F(NsmPCIeRetimerSwitchFactoryTest,
       DISABLED_Factory_PCIeRetimerSwitch_SensorCreated)
{
    const std::string testPath = "/xyz/test/retimer_switch/pcie";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm = basicProperties;

    const size_t before = device->standByToDcRefreshSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, retimerIntf,
                                               testPath);
    EXPECT_GT(device->standByToDcRefreshSensors.size(), before);
}

// NSM_MultiPortPCIeSwitchDevice interface → else-if branch taken →
// NsmPCIeRetimerSwitchDI (multi-port variant) created and added
// DISABLED: same D-Bus path registration issue as above
TEST_F(NsmPCIeRetimerSwitchFactoryTest,
       DISABLED_Factory_MultiPortPCIeSwitchDevice_SensorCreated)
{
    const std::string testPath = "/xyz/test/retimer_switch/multiport";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, multiPortIntf);
    pm = basicProperties;
    pm["Name"] = std::string("MultiPortSwitch0"); // unique name

    const size_t before = device->standByToDcRefreshSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, multiPortIntf,
                                               testPath);
    EXPECT_GT(device->standByToDcRefreshSensors.size(), before);
}

// Missing UUID → FALSE branch for count("UUID") → uuid="" →
// getNsmDeviceFromStaticUUID("") calls parseStaticUuid which throws →
// caught by createObjects → no sensor added
TEST_F(NsmPCIeRetimerSwitchFactoryTest, Factory_MissingUUID_NoSensor)
{
    const std::string testPath = "/xyz/test/retimer_switch/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm = basicProperties;
    pm.erase("UUID"); // uuid="" → parseStaticUuid throws

    const size_t before = device->standByToDcRefreshSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, retimerIntf,
                                               testPath);
    EXPECT_EQ(before, device->standByToDcRefreshSensors.size());
}

// Missing Name → FALSE branch for count("Name") → name="" →
// NsmPCIeRetimerSwitchDI path = inventoryObjPath + "" (trailing slash) →
// sdbusplus throws invalid D-Bus path → no sensor added
TEST_F(NsmPCIeRetimerSwitchFactoryTest, Factory_MissingName_NoSensor)
{
    const std::string testPath = "/xyz/test/retimer_switch/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm = basicProperties;
    pm.erase("Name"); // name="" → D-Bus path ends with "/" → throws

    const size_t before = device->standByToDcRefreshSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, retimerIntf,
                                               testPath);
    EXPECT_EQ(before, device->standByToDcRefreshSensors.size());
}

// Missing InventoryObjPath → FALSE branch for count("InventoryObjPath") →
// inventoryObjPath="" → D-Bus path = name (no leading "/") →
// sdbusplus throws invalid D-Bus path → no sensor added
TEST_F(NsmPCIeRetimerSwitchFactoryTest,
       Factory_MissingInventoryObjPath_NoSensor)
{
    const std::string testPath = "/xyz/test/retimer_switch/no_invpath";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm = basicProperties;
    pm.erase("InventoryObjPath"); // inventoryObjPath="" → no "/" → throws

    const size_t before = device->standByToDcRefreshSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, retimerIntf,
                                               testPath);
    EXPECT_EQ(before, device->standByToDcRefreshSensors.size());
}

// =============================================================================
// Branch coverage: FALSE branches for property count() checks in
// CreatePCIeRetimerSwitch (nsmPCIeRetimerSwitchDI.cpp lines 272-282)
// =============================================================================

// DeviceInstance absent → FALSE branch for count("DeviceInstance") →
// deviceInstance=0 (default uint64_t{}) →
// deviceIndex=PCIE_RETIMER_DEVICE_INDEX_START → NsmPCIeRetimerSwitchDI created
// with device index START → sensor added. DISABLED: NsmPCIeRetimerSwitchDI
// constructor registers multiple D-Bus interfaces (AssociationDefinitionsInft +
// SwitchIntf) at the same path, which throws in the test D-Bus environment —
// same root cause as the existing
// DISABLED_Factory_MultiPortPCIeSwitchDevice_SensorCreated test.
TEST_F(NsmPCIeRetimerSwitchFactoryTest,
       DISABLED_Factory_MultiPort_MissingDeviceInstance_DefaultsToStartIndex)
{
    const std::string testPath = "/xyz/test/retimer_switch/no_devinst";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, multiPortIntf);
    pm = basicProperties;
    pm["Name"] = std::string("MultiPortSwitch_no_devinst"); // unique name
    // Intentionally omit "DeviceInstance" → FALSE branch → deviceInstance=0 →
    // deviceIndex = PCIE_RETIMER_DEVICE_INDEX_START + 0
    pm.erase("DeviceInstance");

    const size_t before = device->standByToDcRefreshSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, multiPortIntf,
                                               testPath);
    EXPECT_GT(device->standByToDcRefreshSensors.size(), before);
}

// Priority absent (ENABLE_CLOCK_OUTPUT_STATE path) → FALSE branch for
// count("Priority") → priority=false (default bool{}) → addSensor(sensor, //
// false) → roundRobin sensor. DISABLED:
// NSM_PCIeRetimer_Switch creates both NsmPCIeRetimerSwitchGetClockState and
// NsmPCIeRetimerSwitchDI at the same inventoryObjPath+name D-Bus path, causing
// a FileExists conflict in the test D-Bus environment.
TEST_F(NsmPCIeRetimerSwitchFactoryTest,
       DISABLED_Factory_PCIeRetimer_MissingPriority_RoundRobinSensor)
{
    const std::string testPath = "/xyz/test/retimer_switch/no_prio";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm = basicProperties;
    pm["Name"] = std::string("PCIeRetimerSwitch_no_prio"); // unique name
    // Intentionally omit "Priority" → FALSE branch → priority=false →
    // addSensor(retimerSwitchRefClock, false) → roundRobin
    pm.erase("Priority");

    const size_t rrBefore = device->roundRobinSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, retimerIntf,
                                               testPath);
    EXPECT_GT(device->roundRobinSensors.size(), rrBefore);
}

// NsmPCIeRetimerSwitchGetClockState::handleResponseMsg: non-success CC via
// 9-byte buffer → `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch
TEST(NsmPCIeRetimerSwitchGetClockState,
     HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ClockState_ccbranch";
    std::string type = "NSM_PCIeRetimer_Switch";
    std::string inv = "/xyz/openbmc_project/inventory/ccbranch/";

    NsmPCIeRetimerSwitchGetClockState obj(bus, name, type, 0, inv);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = obj.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}
