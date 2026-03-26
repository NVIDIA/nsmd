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
 * Branch coverage for NsmBuildTypeObject in nsmd/nsmChassis/nsmErot.cpp
 *
 * Covers:
 * - genRequestMsg encode fail (instanceId > NSM_INSTANCE_MAX)
 * - handleResponseMsg: success with matching slot count
 * - handleResponseMsg: errorCC (cc != NSM_SUCCESS)
 * - handleResponseMsg: decode fail (short buffer)
 * - handleResponseMsg: slot count mismatch with count>0 (free path)
 * - handleResponseMsg: slot count mismatch with count==0 (no free path)
 * - handleResponseMsg: success with 0 slots (no free needed)
 * - Factory: device not found (UUID returns nullptr)
 * - Factory: NSM_Chassis type (vs NSM_ChassisRoT)
 * - Factory: only AP slots (no EC → rotProgressIntf created fresh)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "libnsm/firmware-utils.h"

#include "nsmChassis/nsmErot.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine nsmErotCreateSensors(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath);
} // namespace nsm

// ============================================================================
// NsmBuildTypeObject: genRequestMsg / handleResponseMsg branch tests
// ============================================================================

struct NsmErotBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_RoT_BRANCH2";
    const std::string type = "NSM_ChassisRoT";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const int classification = 1;
    const int identifier = 2;

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;
    std::shared_ptr<NsmBuildTypeObject> sensor;

    NsmErotBranch2Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    void SetUp() override
    {
        sensor = std::make_shared<NsmBuildTypeObject>(
            chassisName, type, fpgaUuid, classification, identifier);
        EXPECT_NE(sensor, nullptr);
    }

    void TearDown() override
    {
        cleanupDeviceSensors(devices);
    }

    // Helper: add N slot objects to the sensor
    void addSlots(size_t count)
    {
        auto& bus = utils::DBusHandler::getBus();
        std::vector<utils::Association> associations;
        for (size_t i = 0; i < count; i++)
        {
            std::string path = "/test/slot/branch2_" + std::to_string(i);
            auto slot = std::make_shared<NsmFirmwareSlot>(
                bus, path, associations, static_cast<int>(i),
                SlotIntf::FirmwareType::AP, chassisName);
            sensor->addSlotObject(slot);
        }
    }

    // Helper: build a valid erot response with given slot count and cc
    std::vector<uint8_t> buildResponse(uint8_t cc, uint8_t slotCount)
    {
        std::vector<nsm_firmware_slot_info> slotInfoArray(slotCount);
        for (auto& si : slotInfoArray)
        {
            si = {};
        }

        size_t responseSize = sizeof(nsm_msg_hdr) +
                              sizeof(nsm_firmware_erot_state_info_hdr_resp) +
                              slotCount * sizeof(nsm_firmware_slot_info);
        std::vector<uint8_t> response(responseSize, 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        nsm_firmware_erot_state_info_resp erotInfo = {};
        erotInfo.fq_resp_hdr.firmware_slot_count = slotCount;
        erotInfo.slot_info = slotCount > 0 ? slotInfoArray.data() : nullptr;

        auto rc = encode_nsm_query_get_erot_state_parameters_resp(
            0, cc, ERR_NULL, &erotInfo, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);

        return response;
    }

    // Helper: build an error-CC response (cc != NSM_SUCCESS)
    std::vector<uint8_t> buildErrorCCResponse()
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

        nsm_firmware_erot_state_info_resp erotInfo = {};
        erotInfo.fq_resp_hdr.firmware_slot_count = 0;
        erotInfo.slot_info = nullptr;

        auto rc = encode_nsm_query_get_erot_state_parameters_resp(
            0, NSM_ERROR, ERR_NULL, &erotInfo, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);

        // Set completion code to NSM_ERROR in the payload
        auto resp = reinterpret_cast<nsm_firmware_get_erot_state_info_resp*>(
            responseMsg->payload);
        resp->hdr.completion_code = NSM_ERROR;
        response.resize(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_common_non_success_resp));

        return response;
    }

    // Helper: valgrind-safe decode-fail buffer (7 bytes minimum)
    std::vector<uint8_t> buildDecodeFailResponse()
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        responseMsg->payload[1] = NSM_ERROR;
        return response;
    }
};

// genRequestMsg: encode fail when instanceId > NSM_INSTANCE_MAX
TEST_F(NsmErotBranch2Test, GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    auto result = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// genRequestMsg: success path
TEST_F(NsmErotBranch2Test, GenRequestMsg_Success_ReturnsRequest)
{
    auto result = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_firmware_get_erot_state_info_req));
}

// handleResponseMsg: success with 0 slots, sensor has 0 slots
TEST_F(NsmErotBranch2Test, HandleResponseMsg_Success_ZeroSlots)
{
    auto response = buildResponse(NSM_SUCCESS, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// handleResponseMsg: success with matching slot count (2 slots)
TEST_F(NsmErotBranch2Test, HandleResponseMsg_Success_MatchingSlotCount)
{
    addSlots(2);
    EXPECT_EQ(sensor->fwSlotObjects.size(), 2u);

    auto response = buildResponse(NSM_SUCCESS, 2);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// handleResponseMsg: cc != NSM_SUCCESS → return NSM_SW_ERROR_COMMAND_FAIL
TEST_F(NsmErotBranch2Test, HandleResponseMsg_ErrorCC_ReturnsCommandFail)
{
    auto response = buildErrorCCResponse();
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

// handleResponseMsg: decode fail (short buffer)
TEST_F(NsmErotBranch2Test, HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto response = buildDecodeFailResponse();
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// handleResponseMsg: slot count mismatch (sensor has 1, response has 0)
// → count>0 is FALSE → no free() call
TEST_F(NsmErotBranch2Test, HandleResponseMsg_MismatchZeroCount_NoFree)
{
    addSlots(1);

    auto response = buildResponse(NSM_SUCCESS, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

// handleResponseMsg: slot count mismatch (sensor has 1, response has 3)
// → count>0 is TRUE → free() called
TEST_F(NsmErotBranch2Test, HandleResponseMsg_MismatchNonZeroCount_Free)
{
    addSlots(1);

    auto response = buildResponse(NSM_SUCCESS, 3);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

// ============================================================================
// Factory function branches: device not found
// ============================================================================

struct NsmErotFactoryBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_RoT";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmErotFactoryBranch2Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    void TearDown() override
    {
        cleanupDeviceSensors(devices);
    }

    const MapperServiceMap slotServiceMap = {{
        {
            "xyz.openbmc_project.NSM",
            {"xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations"},
        },
    }};
};

// Device not found (invalid UUID) → co_return NSM_SW_ERROR
TEST_F(NsmErotFactoryBranch2Test, DeviceNotFound_ReturnsError)
{
    const uuid_t badUuid = "INVALID:UUID:DEVICE:NOT:FOUND";
    const std::string uniquePath = "/xyz/test/erot_branch2/no_device";

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = std::string("ErotNoDevice");
    pm["UUID"] = badUuid;
    pm["SlotCount"] = uint64_t(0);

    EXPECT_THROW_COROUTINE(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath),
        std::runtime_error);
}

// NSM_Chassis type (not NSM_ChassisRoT) with SlotCount → still processes
TEST_F(NsmErotFactoryBranch2Test, NSMChassisType_WithSlots)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_Chassis_Branch2";
    const std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                                   uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_Chassis");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(1);

    std::string slot1Path = uniquePath + "/Slot1";
    auto& slotProps = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slotProps["Name"] = std::string("Slot1");
    slotProps["FirmwareType"] = std::string("EC");
    slotProps["ChassisName"] = uniqueName;
    slotProps["ComponentClassification"] = uint64_t(1);
    slotProps["ComponentIdentifier"] = uint64_t(1);
    slotProps["ComponentIndex"] = uint64_t(0);

    auto& slotAssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slotAssocProps["Forward"] = std::string("chassis");
    slotAssocProps["Backward"] = std::string("firmware_slot");
    slotAssocProps["AbsolutePath"] = uniquePath;

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// Only AP slots (no EC) → rotProgressIntf == nullptr && ecProgressIntf ==
// nullptr → else branch at L548: fresh ProgressIntf created
TEST_F(NsmErotFactoryBranch2Test, OnlyAPSlots_FreshProgressIntfCreated)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_RoT_OnlyAP_Branch2";
    const std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                                   uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(1);

    std::string slot1Path = uniquePath + "/Slot1";
    auto& slotProps = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slotProps["Name"] = std::string("Slot1");
    slotProps["FirmwareType"] = std::string("AP");
    slotProps["ChassisName"] = uniqueName + "_AP";
    slotProps["ComponentClassification"] = uint64_t(1);
    slotProps["ComponentIdentifier"] = uint64_t(1);
    slotProps["ComponentIndex"] = uint64_t(0);

    auto& slotAssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slotAssocProps["Forward"] = std::string("chassis");
    slotAssocProps["Backward"] = std::string("firmware_slot");
    slotAssocProps["AbsolutePath"] = uniquePath;

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    // At minimum apFirmwareType + apMinSecVersion + securityCfg
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// AP slot with IsRoT=true → slotKey = name (not chassisName) → different
// map key for apFirmwareTypeMap
TEST_F(NsmErotFactoryBranch2Test, APSlotIsRoT_SlotKeyIsName)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_RoT_APIsRoT_Branch2";
    const std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                                   uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(1);

    std::string slot1Path = uniquePath + "/Slot1";
    auto& slotProps = utils::MockDbusAsync::propertyMap(
        slot1Path, "xyz.openbmc_project.Configuration.NSM_RoT_Slot");
    slotProps["Name"] = std::string("Slot1");
    slotProps["FirmwareType"] = std::string("AP");
    slotProps["ChassisName"] = uniqueName + "_AP";
    slotProps["ComponentClassification"] = uint64_t(1);
    slotProps["ComponentIdentifier"] = uint64_t(1);
    slotProps["ComponentIndex"] = uint64_t(0);
    slotProps["IsRoT"] = bool(true); // slotKey = name instead of chassisName

    auto& slotAssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path,
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations");
    slotAssocProps["Forward"] = std::string("chassis");
    slotAssocProps["Backward"] = std::string("firmware_slot");
    slotAssocProps["AbsolutePath"] = uniquePath;

    const size_t before = fpga->deviceSensors.size();
    nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}
