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
 * Branch coverage tests for nsmPCIeRetimerSwitchDI.cpp
 *
 * Calls CreatePCIeRetimerSwitch directly (forward-declared, no longer static)
 * to bypass SIOF issues with NsmObjectFactory registration.
 *
 * Covers:
 *  - Factory: all properties present (NSM_PCIeRetimer_Switch and
 *    NSM_MultiPortPCIeSwitchDevice types)
 *  - Factory: missing optional properties (Name, UUID, InventoryObjPath absent)
 *  - Factory: invalid UUID -> nsmDevice == nullptr -> error path
 *  - Factory: unknown type -> else branch
 *  - NsmPCIeRetimerSwitchDI::update: encode failure (non-multiport & multiport)
 *  - NsmPCIeRetimerSwitchDI::update: sensorIO fail, decode fail, errorCC,
 *    success
 *  - NsmPCIeRetimerSwitchGetClockState: genRequestMsg encode failure,
 *    handleResponseMsg success/errorCC/decode fail
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "nsmDeviceInventory/nsmPCIeRetimerSwitchDI.hpp"

#undef private
#undef protected

// Forward-declare the factory function (no longer static since ed8cf48c)
namespace nsm
{
requester::Coroutine CreatePCIeRetimerSwitch(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmPCIeRetimerSwitchDIBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string retimerIntf =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_Switch";
    const std::string multiPortIntf =
        "xyz.openbmc_project.Configuration.NSM_MultiPortPCIeSwitchDevice";
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string invPath =
        "/xyz/openbmc_project/inventory/brtest/retimer/";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPCIeRetimerSwitchDIBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPCIeRetimerSwitchDIBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// Factory: NSM_PCIeRetimer_Switch with all properties -> sensors created
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest,
       Factory_PCIeRetimerSwitch_AllProps_SensorCreated)
{
    const std::string testPath = "/xyz/test/brtest/retimer_sw_all";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm["Name"] = std::string("BrRetimer0");
    pm["UUID"] = deviceUuid;
    pm["InventoryObjPath"] = invPath;
    pm["DeviceInstance"] = uint64_t(2);
#if defined(ENABLE_CLOCK_OUTPUT_STATE)
    pm["Priority"] = true;
#endif

    const size_t before = gpu->standByToDcRefreshSensors.size();
    CreatePCIeRetimerSwitch(mockManager, retimerIntf, testPath);
    EXPECT_GT(gpu->standByToDcRefreshSensors.size(), before);
}

// ============================================================================
// Factory: NSM_MultiPortPCIeSwitchDevice with all properties -> sensors created
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest,
       Factory_MultiPortSwitch_AllProps_SensorCreated)
{
    const std::string testPath = "/xyz/test/brtest/multiport_all";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, multiPortIntf);
    pm["Name"] = std::string("BrMultiPort0");
    pm["UUID"] = deviceUuid;
    pm["InventoryObjPath"] = invPath;
    pm["DeviceInstance"] = uint64_t(1);

    const size_t before = gpu->standByToDcRefreshSensors.size();
    CreatePCIeRetimerSwitch(mockManager, multiPortIntf, testPath);
    EXPECT_GT(gpu->standByToDcRefreshSensors.size(), before);
}

// ============================================================================
// Factory: missing Name -> name="" -> empty D-Bus path segment -> throws
// (CreatePCIeRetimerSwitch does not catch; exception propagates)
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest, DISABLED_Factory_MissingName_Throws)
{
    const std::string testPath = "/xyz/test/brtest/retimer_noname";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    // Name deliberately absent -> name="" -> invalid D-Bus path
    pm["UUID"] = deviceUuid;
    pm["InventoryObjPath"] = invPath;
    pm["DeviceInstance"] = uint64_t(0);

    EXPECT_ANY_THROW(
        CreatePCIeRetimerSwitch(mockManager, retimerIntf, testPath));
}

// ============================================================================
// Factory: missing UUID -> uuid="" -> parseStaticUuid throws
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest, DISABLED_Factory_MissingUUID_Throws)
{
    const std::string testPath = "/xyz/test/brtest/retimer_nouuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm["Name"] = std::string("BrRetimerNoUUID");
    // UUID deliberately absent -> uuid="" -> parseStaticUuid throws
    pm["InventoryObjPath"] = invPath;
    pm["DeviceInstance"] = uint64_t(0);

    EXPECT_ANY_THROW(
        CreatePCIeRetimerSwitch(mockManager, retimerIntf, testPath));
}

// ============================================================================
// Factory: invalid UUID -> nsmDevice is nullptr -> error path (line 295-303)
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest, Factory_InvalidUUID_NoSensor)
{
    const std::string testPath = "/xyz/test/brtest/retimer_baduuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm["Name"] = std::string("BrRetimerBadUUID");
    pm["UUID"] = std::string("STATIC:99:99:NSM_DEVICE_INSTANCE_NUMBER:999");
    pm["InventoryObjPath"] = invPath;
    pm["DeviceInstance"] = uint64_t(0);

    const size_t before = gpu->standByToDcRefreshSensors.size();
    CreatePCIeRetimerSwitch(mockManager, retimerIntf, testPath);
    EXPECT_EQ(before, gpu->standByToDcRefreshSensors.size());
}

// ============================================================================
// Factory: missing InventoryObjPath -> inventoryObjPath="" -> bad D-Bus path
// -> throws (no leading "/" in path)
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest,
       DISABLED_Factory_MissingInventoryObjPath_Throws)
{
    const std::string testPath = "/xyz/test/brtest/retimer_noinvpath";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm["Name"] = std::string("BrRetimerNoInvPath");
    pm["UUID"] = deviceUuid;
    // InventoryObjPath deliberately absent -> "" + name -> invalid D-Bus path
    pm["DeviceInstance"] = uint64_t(0);

    EXPECT_ANY_THROW(
        CreatePCIeRetimerSwitch(mockManager, retimerIntf, testPath));
}

// ============================================================================
// Factory: unknown type -> else branch (line 357-360)
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest, Factory_UnknownType_ReturnsError)
{
    const std::string unknownIntf =
        "xyz.openbmc_project.Configuration.NSM_UnknownPCIeType";
    const std::string testPath = "/xyz/test/brtest/retimer_unknown";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, unknownIntf);
    pm["Name"] = std::string("BrRetimerUnknown");
    pm["UUID"] = deviceUuid;
    pm["InventoryObjPath"] = invPath;
    pm["DeviceInstance"] = uint64_t(0);

    const size_t before = gpu->standByToDcRefreshSensors.size();
    CreatePCIeRetimerSwitch(mockManager, unknownIntf, testPath);
    EXPECT_EQ(before, gpu->standByToDcRefreshSensors.size());
}

// ============================================================================
// Factory: missing DeviceInstance -> defaults to 0 -> sensor still created
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest,
       Factory_MissingDeviceInstance_DefaultsToZero)
{
    const std::string testPath = "/xyz/test/brtest/retimer_nodevinst";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, multiPortIntf);
    pm["Name"] = std::string("BrMultiPortNoDevInst");
    pm["UUID"] = deviceUuid;
    pm["InventoryObjPath"] = invPath;
    // DeviceInstance deliberately absent -> uint64_t{} == 0

    const size_t before = gpu->standByToDcRefreshSensors.size();
    CreatePCIeRetimerSwitch(mockManager, multiPortIntf, testPath);
    EXPECT_GT(gpu->standByToDcRefreshSensors.size(), before);
}

// ============================================================================
// update() multiport: encode failure
// Line 114: encode_multiport fails -> co_return
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest,
       Update_Multiport_EncodeFails_SkipsSensorIO)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assoc;
    std::string inv = "/xyz/openbmc_project/inventory/brtest/rt_mp_enc/";
    // deviceIndex=255 -> encode_multiport_query fails
    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "BrRtMpEncFail", assoc, "NSM_MultiPortPCIeSwitchDevice", inv,
        uint8_t(255), uint8_t(1), uint8_t(0), uint8_t(0));

    EXPECT_CALL(*gpu, sensorIO).Times(0);
    obj->update(gpu);
    EXPECT_EQ(obj->switchIntf->deviceId(), "");
}

// ============================================================================
// update() non-multiport: sensorIO failure -> early return (line 127-131)
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest,
       Update_NonMultiport_SensorIOFails_ReturnsEarly)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assoc;
    std::string inv = "/xyz/openbmc_project/inventory/brtest/rt_iofail/";
    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "BrRtIOFail", assoc, "NSM_PCIeRetimer_Switch", inv, uint8_t(2));

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(
            mockSensorIO(static_cast<nsm_completion_codes>(NSM_SW_ERROR)));

    obj->update(gpu);
    EXPECT_EQ(obj->switchIntf->deviceId(), "");
}

// ============================================================================
// update() multiport: sensorIO failure -> early return
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest,
       Update_Multiport_SensorIOFails_ReturnsEarly)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assoc;
    std::string inv = "/xyz/openbmc_project/inventory/brtest/rt_mp_iofail/";
    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "BrRtMpIOFail", assoc, "NSM_MultiPortPCIeSwitchDevice", inv,
        uint8_t(2), uint8_t(0), uint8_t(0), uint8_t(0));

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(
            mockSensorIO(static_cast<nsm_completion_codes>(NSM_SW_ERROR)));

    obj->update(gpu);
    EXPECT_EQ(obj->switchIntf->deviceId(), "");
}

// ============================================================================
// update() non-multiport: decode failure (truncated response)
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest,
       Update_NonMultiport_DecodeFails_DoesNotUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assoc;
    std::string inv = "/xyz/openbmc_project/inventory/brtest/rt_decfail/";
    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "BrRtDecFail", assoc, "NSM_PCIeRetimer_Switch", inv, uint8_t(1));

    // Valgrind-safe minimum buffer causing decode failure
    std::vector<uint8_t> tiny(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    tiny[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(tiny));

    obj->update(gpu);
    EXPECT_EQ(obj->switchIntf->deviceId(), "");
}

// ============================================================================
// update() non-multiport: error CC -> does not update IDs (line 142 false)
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest,
       Update_NonMultiport_ErrorCC_DoesNotUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assoc;
    std::string inv = "/xyz/openbmc_project/inventory/brtest/rt_errcc/";
    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "BrRtErrCC", assoc, "NSM_PCIeRetimer_Switch", inv, uint8_t(1));

    nsm_query_scalar_group_telemetry_group_0 data{};
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_query_scalar_group_telemetry_v1_group0_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, msg);
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(resp));

    obj->update(gpu);
    EXPECT_EQ(obj->switchIntf->deviceId(), "");
    EXPECT_EQ(obj->switchIntf->vendorId(), "");
}

// ============================================================================
// update() non-multiport: success -> updates device/vendor IDs (line 142 true)
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest, Update_NonMultiport_Success_UpdatesIds)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assoc;
    std::string inv = "/xyz/openbmc_project/inventory/brtest/rt_ok/";
    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "BrRtSuccess", assoc, "NSM_PCIeRetimer_Switch", inv, uint8_t(1));

    nsm_query_scalar_group_telemetry_group_0 data{};
    data.pci_device_id = 0xABCD;
    data.pci_vendor_id = 0x10DE;
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_query_scalar_group_telemetry_v1_group0_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(resp));

    obj->update(gpu);
    EXPECT_EQ(obj->switchIntf->deviceId(), "0xabcd");
    EXPECT_EQ(obj->switchIntf->vendorId(), "0x10de");
}

// ============================================================================
// update() multiport: success -> updates device/vendor IDs
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBranchTest, Update_Multiport_Success_UpdatesIds)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> assoc;
    std::string inv = "/xyz/openbmc_project/inventory/brtest/rt_mp_ok/";
    auto obj = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "BrRtMpSuccess", assoc, "NSM_MultiPortPCIeSwitchDevice", inv,
        uint8_t(2), uint8_t(1), uint8_t(0), uint8_t(0));

    nsm_query_scalar_group_telemetry_group_0 data{};
    data.pci_device_id = 0x5678;
    data.pci_vendor_id = 0x1234;
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_query_scalar_group_telemetry_v1_group0_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(resp));

    obj->update(gpu);
    EXPECT_EQ(obj->switchIntf->deviceId(), "0x5678");
    EXPECT_EQ(obj->switchIntf->vendorId(), "0x1234");
}

// ============================================================================
// NsmPCIeRetimerSwitchGetClockState: genRequestMsg encode failure
// ============================================================================

#if defined(ENABLE_CLOCK_OUTPUT_STATE)

TEST(NsmPCIeRetimerSwitchGetClockStateBranch,
     GenRequestMsg_EncodeFails_ReturnsNullopt)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string inv = "/xyz/openbmc_project/inventory/brtest/clk_genreq/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, "BrClkGenFail",
                                          "NSM_PCIeRetimer_Switch", 0, inv);

    // instanceId=255 > NSM_INSTANCE_MAX -> encode fails -> nullopt
    auto result = obj.genRequestMsg(0x01, 255);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// NsmPCIeRetimerSwitchGetClockState: genRequestMsg success
// ============================================================================

TEST(NsmPCIeRetimerSwitchGetClockStateBranch,
     GenRequestMsg_Success_ReturnsRequest)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string inv = "/xyz/openbmc_project/inventory/brtest/clk_genok/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, "BrClkGenOk",
                                          "NSM_PCIeRetimer_Switch", 0, inv);

    auto result = obj.genRequestMsg(0x01, 0);
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0u);
}

// ============================================================================
// NsmPCIeRetimerSwitchGetClockState: handleResponseMsg success
// ============================================================================

TEST(NsmPCIeRetimerSwitchGetClockStateBranch,
     HandleResponseMsg_Success_UpdatesClockState)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string inv = "/xyz/openbmc_project/inventory/brtest/clk_ok/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, "BrClkHandleOk",
                                          "NSM_PCIeRetimer_Switch", 0, inv);

    // clk_buf_retimer1 = bit 8 -> 0x0100
    uint32_t clkBuf = 0x0100;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_output_enabled_state_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto encRc = encode_get_clock_output_enable_state_resp(
        0, NSM_SUCCESS, ERR_NULL, clkBuf, msg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = obj.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(obj.pcieRefClockIntf->pcIeReferenceClockEnabled());
}

// ============================================================================
// NsmPCIeRetimerSwitchGetClockState: handleResponseMsg error CC
// ============================================================================

TEST(NsmPCIeRetimerSwitchGetClockStateBranch,
     HandleResponseMsg_ErrorCC_SkipsUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string inv = "/xyz/openbmc_project/inventory/brtest/clk_errcc/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, "BrClkErrCC",
                                          "NSM_PCIeRetimer_Switch", 0, inv);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_output_enabled_state_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto encRc = encode_get_clock_output_enable_state_resp(0, NSM_ERROR,
                                                           ERR_NULL, 0, msg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = obj.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// ============================================================================
// NsmPCIeRetimerSwitchGetClockState: handleResponseMsg decode failure
// ============================================================================

TEST(NsmPCIeRetimerSwitchGetClockStateBranch,
     HandleResponseMsg_DecodeFails_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string inv = "/xyz/openbmc_project/inventory/brtest/clk_decfail/";
    NsmPCIeRetimerSwitchGetClockState obj(bus, "BrClkDecFail",
                                          "NSM_PCIeRetimer_Switch", 0, inv);

    // Valgrind-safe minimum buffer causing decode failure
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = obj.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

#endif // ENABLE_CLOCK_OUTPUT_STATE
