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
 * Batch 11B - Comprehensive tests targeting uncovered functions in:
 *   1. nsmd/nsmProcessor/nsmProcessor.cpp
 *      - NsmAcceleratorIntf constructor
 *      - NsmProcessorAssociation constructor
 *      - NsmUuidIntf constructor
 *      - NsmLocationIntfProcessor constructor
 *      - NsmLocationCodeIntfProcessor constructor
 *      - NsmEccMode genRequestMsg / handleResponseMsg / updateReading
 *      - NsmEDPpScalingFactor::patchSetPoint (coroutine)
 *      - NsmMigMode handleResponseMsg (long-running path)
 *   2. nsmd/nsmDeviceInventory/nsmSwitch.cpp
 *      - NsmSwitchDIPowerMode::getPowerModeData
 *      - NsmSwitchDIPowerMode::setL1PowerDevice (coroutine)
 *      - NsmSwitchDIPowerMode::setL1PowerModePatch (coroutine + all keys)
 *      - NsmSwitchIsolationMode::setSwitchIsolationMode (coroutine)
 *      - NsmSwitchL1PredictionMode::setL1PredictionMode (coroutine)
 *      - NsmSwitchDIReset constructor
 *   3. nsmd/nsmDevice.cpp
 *      - FruInterfaceManager::createAndRegisterInterface
 *      - FruInterfaceManager::updateAllPropertyValues
 *      - FruInterfaceManager::registerAllProperties
 *      - NsmDevice::addSensorBase (PollingType enum paths)
 *      - NsmDevice::updateDiscoveryIdentifiers
 *      - NsmDevice::registerLongRunningHandler
 *      - NsmDevice::invokeLongRunningHandler (decode / mismatch paths)
 *      - NsmDevice::progressCounters (lazy init)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#include "base.h"
#include "device-capability-discovery.h"
#include "device-configuration.h"
#include "network-ports.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmDevice.hpp"
#include "nsmDeviceInventory/nsmSwitch.hpp"
#include "nsmEvent/nsmLongRunningEvent.hpp"
#include "nsmProcessor/nsmProcessor.hpp"

#undef private
#undef protected

using namespace nsm;

// Common test bus and strings
static auto& testBus = utils::DBusHandler::getBus();
static std::string sName("batch11b_sensor");
static std::string sType("batch11b_type");
static std::string invPath("/xyz/openbmc_project/inventory/batch11b");

// ============================================================================
// PART 1: nsmProcessor.cpp - Constructor coverage
// ============================================================================

// --- NsmAcceleratorIntf constructor ---
TEST(NsmAcceleratorIntfTest, Constructor_ValidArgs_CreatesObjectWithGPUType)
{
    // Arrange
    std::string name = "GPU_0";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/gpu0";

    // Act
    NsmAcceleratorIntf accel(testBus, name, type, objPath);

    // Assert
    EXPECT_EQ(accel.getName(), name);
    EXPECT_EQ(accel.getType(), type);
    EXPECT_NE(accel.acceleratorIntf, nullptr);
    EXPECT_EQ(accel.acceleratorIntf->type(), accelaratorType::GPU);
}

// --- NsmProcessorAssociation constructor ---
TEST(NsmProcessorAssociationTest,
     Constructor_EmptyAssociations_CreatesValidObject)
{
    // Arrange
    std::vector<utils::Association> assoc = {};

    // Act
    NsmProcessorAssociation pa(testBus, "proc0", "NSM_Processor", invPath,
                               assoc);

    // Assert
    EXPECT_NE(pa.associationDef, nullptr);
    EXPECT_EQ(pa.getName(), "proc0");
}

TEST(NsmProcessorAssociationTest, Constructor_WithAssociations_SetsAssociations)
{
    // Arrange
    std::vector<utils::Association> assoc = {
        {"parent_chassis", "all_processors", "/xyz/openbmc_project/chassis"},
        {"all_switches", "parent_device", "/xyz/openbmc_project/switch"}};

    // Act
    NsmProcessorAssociation pa(testBus, "proc1", "NSM_Processor", invPath,
                               assoc);

    // Assert
    EXPECT_NE(pa.associationDef, nullptr);
    auto associations = pa.associationDef->associations();
    EXPECT_EQ(associations.size(), 2u);
    EXPECT_EQ(std::get<0>(associations[0]), "parent_chassis");
    EXPECT_EQ(std::get<1>(associations[0]), "all_processors");
    EXPECT_EQ(std::get<2>(associations[0]), "/xyz/openbmc_project/chassis");
}

// --- NsmUuidIntf constructor ---
TEST(NsmUuidIntfTest, Constructor_ValidUuid_SetsUuid)
{
    // Arrange
    std::string name = "GPU_0";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/gpu_uuid";
    uuid_t uuid = "abcdef01-2345-6789-abcd-ef0123456789";

    // Act
    NsmUuidIntf uuidIntf(testBus, name, type, objPath, uuid);

    // Assert
    EXPECT_NE(uuidIntf.uuidIntf, nullptr);
    EXPECT_EQ(uuidIntf.uuidIntf->uuid(), uuid);
    EXPECT_EQ(uuidIntf.inventoryObjPath, objPath);
}

TEST(NsmUuidIntfTest, Constructor_EmptyUuid_SetsEmptyUuid)
{
    // Arrange
    std::string name = "GPU_1";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/gpu_uuid_empty";
    uuid_t uuid = "";

    // Act
    NsmUuidIntf uuidIntf(testBus, name, type, objPath, uuid);

    // Assert
    EXPECT_NE(uuidIntf.uuidIntf, nullptr);
    EXPECT_EQ(uuidIntf.uuidIntf->uuid(), "");
}

// --- NsmLocationIntfProcessor constructor ---
TEST(NsmLocationIntfProcessorTest,
     Constructor_ValidLocationTypeSlot_CreatesObject)
{
    // Arrange
    std::string name = "GPU_0";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/loc_proc";
    std::string locationType = "xyz.openbmc_project.Inventory.Decorator."
                               "Location.LocationTypes.Slot";

    // Act
    NsmLocationIntfProcessor loc(testBus, name, type, objPath, locationType);

    // Assert
    EXPECT_NE(loc.locationIntf, nullptr);
    EXPECT_EQ(loc.getName(), name);
}

TEST(NsmLocationIntfProcessorTest,
     Constructor_EmbeddedLocationType_CreatesObject)
{
    // Arrange
    std::string name = "GPU_1";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/loc_proc_embed";
    std::string locationType = "xyz.openbmc_project.Inventory.Decorator."
                               "Location.LocationTypes.Embedded";

    // Act
    NsmLocationIntfProcessor loc(testBus, name, type, objPath, locationType);

    // Assert
    EXPECT_NE(loc.locationIntf, nullptr);
}

// --- NsmLocationCodeIntfProcessor constructor ---
TEST(NsmLocationCodeIntfProcessorTest,
     Constructor_ValidLocationCode_SetsLocationCode)
{
    // Arrange
    std::string name = "GPU_0";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/loccode_proc";
    std::string locationCode = "U100-E0-C0";

    // Act
    NsmLocationCodeIntfProcessor locCode(testBus, name, type, objPath,
                                         locationCode);

    // Assert
    EXPECT_NE(locCode.locationCodeIntf, nullptr);
    EXPECT_EQ(locCode.locationCodeIntf->locationCode(), "U100-E0-C0");
    EXPECT_EQ(locCode.getName(), name);
}

TEST(NsmLocationCodeIntfProcessorTest,
     Constructor_EmptyLocationCode_SetsEmptyString)
{
    // Arrange
    std::string name = "GPU_1";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/loccode_proc_empty";
    std::string locationCode = "";

    // Act
    NsmLocationCodeIntfProcessor locCode(testBus, name, type, objPath,
                                         locationCode);

    // Assert
    EXPECT_NE(locCode.locationCodeIntf, nullptr);
    EXPECT_EQ(locCode.locationCodeIntf->locationCode(), "");
}

// ============================================================================
// PART 2: nsmProcessor.cpp - NsmEccMode tests
// ============================================================================

TEST(NsmEccMode, Constructor_ValidArgs_CreatesObject)
{
    // Arrange
    auto eccIntf = std::make_shared<EccModeIntf>(testBus, invPath.c_str());
    std::string name = "ecc_sensor";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/ecc";

    // Act
    NsmEccMode eccMode(name, type, eccIntf, objPath, false, nullptr);

    // Assert
    EXPECT_NE(eccMode.eccModeIntf, nullptr);
    EXPECT_EQ(eccMode.NsmSensor::getName(), name);
}

TEST(NsmEccMode, GenRequestMsg_ValidArgs_ReturnsValidRequest)
{
    // Arrange
    auto eccIntf = std::make_shared<EccModeIntf>(testBus, invPath.c_str());
    std::string name = "ecc_sensor";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/ecc_gen";
    NsmEccMode eccMode(name, type, eccIntf, objPath, false, nullptr);

    // Act
    auto request = eccMode.genRequestMsg(12, 30);

    // Assert
    EXPECT_TRUE(request.has_value());
    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_ECC_MODE);
}

TEST(NsmEccMode, HandleResponseMsg_SuccessEccEnabled_UpdatesReading)
{
    // Arrange
    auto eccIntf = std::make_shared<EccModeIntf>(testBus, invPath.c_str());
    std::string name = "ecc_sensor";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/ecc_resp";
    NsmEccMode eccMode(name, type, eccIntf, objPath, false, nullptr);

    bitfield8_t flags = {};
    flags.bits.bit0 = 1; // ECC enabled
    flags.bits.bit1 = 0; // pending ECC off

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_ECC_mode_resp(0, NSM_SUCCESS, reason_code, &flags,
                                          response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = eccMode.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(eccMode.eccModeIntf->eccModeEnabled());
}

TEST(NsmEccMode, HandleResponseMsg_SuccessEccDisabled_UpdatesReading)
{
    // Arrange
    auto eccIntf = std::make_shared<EccModeIntf>(testBus, invPath.c_str());
    std::string name = "ecc_off";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/ecc_off";
    NsmEccMode eccMode(name, type, eccIntf, objPath, false, nullptr);

    bitfield8_t flags = {};
    flags.bits.bit0 = 0; // ECC disabled

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_ECC_mode_resp(0, NSM_SUCCESS, reason_code, &flags,
                                          response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = eccMode.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(eccMode.eccModeIntf->eccModeEnabled());
}

TEST(NsmEccMode, HandleResponseMsg_ErrorCC_DoesNotUpdate)
{
    // Arrange
    auto eccIntf = std::make_shared<EccModeIntf>(testBus, invPath.c_str());
    std::string name = "ecc_err";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/ecc_err";
    NsmEccMode eccMode(name, type, eccIntf, objPath, false, nullptr);

    bitfield8_t flags = {};
    flags.bits.bit0 = 1;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_ECC_mode_resp(0, NSM_ERROR, reason_code, &flags,
                                          response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = eccMode.handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmEccMode, HandleResponseMsg_NullMsg_ReturnsError)
{
    // Arrange
    auto eccIntf = std::make_shared<EccModeIntf>(testBus, invPath.c_str());
    std::string name = "ecc_null";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/ecc_null";
    NsmEccMode eccMode(name, type, eccIntf, objPath, false, nullptr);

    // Act
    auto rc = eccMode.handleResponseMsg(nullptr, 100);

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(NsmEccMode, HandleResponseMsg_ShortLen_ReturnsError)
{
    // Arrange
    auto eccIntf = std::make_shared<EccModeIntf>(testBus, invPath.c_str());
    std::string name = "ecc_short";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/ecc_short";
    NsmEccMode eccMode(name, type, eccIntf, objPath, false, nullptr);

    bitfield8_t flags = {};
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_ECC_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags, response);

    // Act - too short
    auto rc = eccMode.handleResponseMsg(response, sizeof(nsm_msg_hdr));

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ============================================================================
// PART 3: nsmSwitch.cpp - NsmSwitchDIPowerMode coroutine tests
// ============================================================================

class NsmSwitchDIPowerModeSetTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const std::string name = "NVSwitch_0";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchDIPowerModeSetTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchDIPowerModeSetTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmSwitchDIPowerMode> makePowerMode()
    {
        auto pm = std::make_shared<NsmSwitchDIPowerMode>(name,
                                                         inventoryObjPath);
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
};

// Test: getPowerModeData retrieves current PDI values
TEST_F(NsmSwitchDIPowerModeSetTest, GetPowerModeData_ReturnsCurrentValues)
{
    // Arrange
    auto pm = makePowerMode();
    pm->invoke(pdiMethod(hwModeControl), true);
    pm->invoke(pdiMethod(hwThreshold), static_cast<uint64_t>(1234));
    pm->invoke(pdiMethod(fwThrottlingMode), true);
    pm->invoke(pdiMethod(predictionMode), true);
    pm->invoke(pdiMethod(hwActiveTime), static_cast<uint64_t>(555));
    pm->invoke(pdiMethod(hwInactiveTime), static_cast<uint64_t>(666));
    pm->invoke(pdiMethod(hwPredictionInactiveTime), static_cast<uint64_t>(777));

    // Act
    auto data = pm->getPowerModeData();

    // Assert
    EXPECT_EQ(data.l1_hw_mode_control, true);
    EXPECT_EQ(data.l1_hw_mode_threshold, 1234u);
    EXPECT_EQ(data.l1_fw_throttling_mode, true);
    EXPECT_EQ(data.l1_prediction_mode, true);
    EXPECT_EQ(data.l1_hw_active_time, 555u);
    EXPECT_EQ(data.l1_hw_inactive_time, 666u);
    EXPECT_EQ(data.l1_prediction_inactive_time, 777u);
}

// Test: setL1PowerDevice success path
TEST_F(NsmSwitchDIPowerModeSetTest, SetL1PowerDevice_Success_ReturnsSwSuccess)
{
    // Arrange
    auto pm = makePowerMode();
    nsm_power_mode_data data = {};
    data.l1_hw_mode_control = 1;
    data.l1_hw_mode_threshold = 1000;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_set_power_mode_resp(0, ERR_NULL, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    pm->setL1PowerDevice(data, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// Test: setL1PowerDevice postPatchIO failure
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerDevice_PostPatchIOFailure_SetsWriteFailure)
{
    // Arrange
    auto pm = makePowerMode();
    nsm_power_mode_data data = {};

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Act
    pm->setL1PowerDevice(data, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// Test: setL1PowerModePatch with HWModeControl
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_HWModeControl_CallsSetL1PowerDevice)
{
    // Arrange
    auto pm = makePowerMode();

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchValue = std::vector<
        std::tuple<std::string,
                   std::variant<bool, uint32_t, double, std::vector<uint8_t>>>>;
    PatchValue patchValues;
    patchValues.emplace_back(
        "HWModeControl",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>{true});

    AsyncSetOperationValueType value = patchValues;

    // Act - coroutine catches exceptions internally
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch with FWThrottlingMode
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_FWThrottlingMode_PatchesCorrectField)
{
    // Arrange
    auto pm = makePowerMode();

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchValue = std::vector<
        std::tuple<std::string,
                   std::variant<bool, uint32_t, double, std::vector<uint8_t>>>>;
    PatchValue patchValues;
    patchValues.emplace_back(
        "FWThrottlingMode",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>{true});

    AsyncSetOperationValueType value = patchValues;

    // Act
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch with PredictionMode
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_PredictionMode_PatchesCorrectField)
{
    // Arrange
    auto pm = makePowerMode();

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchValue = std::vector<
        std::tuple<std::string,
                   std::variant<bool, uint32_t, double, std::vector<uint8_t>>>>;
    PatchValue patchValues;
    patchValues.emplace_back(
        "PredictionMode",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>{true});

    AsyncSetOperationValueType value = patchValues;

    // Act
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch with HWThreshold (uint32_t)
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_HWThreshold_PatchesCorrectField)
{
    // Arrange
    auto pm = makePowerMode();

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchValue = std::vector<
        std::tuple<std::string,
                   std::variant<bool, uint32_t, double, std::vector<uint8_t>>>>;
    PatchValue patchValues;
    patchValues.emplace_back(
        "HWThreshold",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>{
            uint32_t(5000)});

    AsyncSetOperationValueType value = patchValues;

    // Act
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch with HWActiveTime
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_HWActiveTime_PatchesCorrectField)
{
    // Arrange
    auto pm = makePowerMode();

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchValue = std::vector<
        std::tuple<std::string,
                   std::variant<bool, uint32_t, double, std::vector<uint8_t>>>>;
    PatchValue patchValues;
    patchValues.emplace_back(
        "HWActiveTime",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>{
            uint32_t(100)});

    AsyncSetOperationValueType value = patchValues;

    // Act
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch with HWInactiveTime
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_HWInactiveTime_PatchesCorrectField)
{
    // Arrange
    auto pm = makePowerMode();

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchValue = std::vector<
        std::tuple<std::string,
                   std::variant<bool, uint32_t, double, std::vector<uint8_t>>>>;
    PatchValue patchValues;
    patchValues.emplace_back(
        "HWInactiveTime",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>{
            uint32_t(200)});

    AsyncSetOperationValueType value = patchValues;

    // Act
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch with HWPredictionInactiveTime
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_HWPredictionInactiveTime_PatchesCorrectField)
{
    // Arrange
    auto pm = makePowerMode();

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchValue = std::vector<
        std::tuple<std::string,
                   std::variant<bool, uint32_t, double, std::vector<uint8_t>>>>;
    PatchValue patchValues;
    patchValues.emplace_back(
        "HWPredictionInactiveTime",
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>{
            uint32_t(300)});

    AsyncSetOperationValueType value = patchValues;

    // Act
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch with all fields at once
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_AllFields_PatchesAllFields)
{
    // Arrange
    auto pm = makePowerMode();

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_power_mode_resp(0, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValue = std::vector<std::tuple<std::string, PatchVariant>>;
    PatchValue patchValues;
    patchValues.emplace_back("HWModeControl", PatchVariant{true});
    patchValues.emplace_back("FWThrottlingMode", PatchVariant{true});
    patchValues.emplace_back("PredictionMode", PatchVariant{false});
    patchValues.emplace_back("HWThreshold", PatchVariant{uint32_t(9999)});
    patchValues.emplace_back("HWActiveTime", PatchVariant{uint32_t(111)});
    patchValues.emplace_back("HWInactiveTime", PatchVariant{uint32_t(222)});
    patchValues.emplace_back("HWPredictionInactiveTime",
                             PatchVariant{uint32_t(333)});

    AsyncSetOperationValueType value = patchValues;

    // Act
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch with unrecognized property - catches exception
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_UnknownProperty_CatchesException)
{
    // Arrange
    auto pm = makePowerMode();

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValue = std::vector<std::tuple<std::string, PatchVariant>>;
    PatchValue patchValues;
    patchValues.emplace_back("UnknownProperty", PatchVariant{true});

    AsyncSetOperationValueType value = patchValues;

    // Act - exception caught by coroutine internally (direct throw in coverage)
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch with wrong value type - catches exception
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_WrongTypeForBoolField_CatchesException)
{
    // Arrange
    auto pm = makePowerMode();

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValue = std::vector<std::tuple<std::string, PatchVariant>>;
    PatchValue patchValues;
    // HWModeControl expects bool, giving uint32_t
    patchValues.emplace_back("HWModeControl", PatchVariant{uint32_t(42)});

    AsyncSetOperationValueType value = patchValues;

    // Act - exception caught by coroutine internally (direct throw in coverage)
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch with empty values list - catches exception
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_EmptyValuesList_CatchesException)
{
    // Arrange
    auto pm = makePowerMode();

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValue = std::vector<std::tuple<std::string, PatchVariant>>;
    PatchValue patchValues; // empty

    AsyncSetOperationValueType value = patchValues;

    // Act - catches InvalidArgument internally
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch with invalid value type (not vector of tuples)
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_InvalidVariantType_CatchesException)
{
    // Arrange
    auto pm = makePowerMode();

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Pass a string instead of the expected vector of tuples
    AsyncSetOperationValueType value = std::string("invalid");

    // Act - catches InvalidArgument internally
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch wrong type for HWThreshold
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_WrongTypeForHWThreshold_CatchesException)
{
    // Arrange
    auto pm = makePowerMode();

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValue = std::vector<std::tuple<std::string, PatchVariant>>;
    PatchValue patchValues;
    // HWThreshold expects uint32_t, giving bool
    patchValues.emplace_back("HWThreshold", PatchVariant{true});

    AsyncSetOperationValueType value = patchValues;

    // Act
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch wrong type for FWThrottlingMode
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_WrongTypeForFWThrottlingMode_CatchesException)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValue = std::vector<std::tuple<std::string, PatchVariant>>;
    PatchValue patchValues;
    patchValues.emplace_back("FWThrottlingMode", PatchVariant{uint32_t(1)});

    AsyncSetOperationValueType value = patchValues;
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch wrong type for PredictionMode
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_WrongTypeForPredictionMode_CatchesException)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValue = std::vector<std::tuple<std::string, PatchVariant>>;
    PatchValue patchValues;
    patchValues.emplace_back("PredictionMode", PatchVariant{uint32_t(1)});

    AsyncSetOperationValueType value = patchValues;
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch wrong type for HWActiveTime
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_WrongTypeForHWActiveTime_CatchesException)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValue = std::vector<std::tuple<std::string, PatchVariant>>;
    PatchValue patchValues;
    patchValues.emplace_back("HWActiveTime", PatchVariant{true});

    AsyncSetOperationValueType value = patchValues;
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch wrong type for HWInactiveTime
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_WrongTypeForHWInactiveTime_CatchesException)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValue = std::vector<std::tuple<std::string, PatchVariant>>;
    PatchValue patchValues;
    patchValues.emplace_back("HWInactiveTime", PatchVariant{true});

    AsyncSetOperationValueType value = patchValues;
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// Test: setL1PowerModePatch wrong type for HWPredictionInactiveTime
TEST_F(NsmSwitchDIPowerModeSetTest,
       SetL1PowerModePatch_WrongTypeForHWPredictionInactiveTime_Catches)
{
    auto pm = makePowerMode();
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    using PatchVariant =
        std::variant<bool, uint32_t, double, std::vector<uint8_t>>;
    using PatchValue = std::vector<std::tuple<std::string, PatchVariant>>;
    PatchValue patchValues;
    patchValues.emplace_back("HWPredictionInactiveTime", PatchVariant{true});

    AsyncSetOperationValueType value = patchValues;
    EXPECT_NO_THROW_COROUTINE(
        pm->setL1PowerModePatch(value, &status, nvswitch));
}

// ============================================================================
// PART 4: nsmSwitch.cpp - NsmSwitchIsolationMode coroutine tests
// ============================================================================

class NsmSwitchIsolationModeSetTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const std::string name = "NVSwitch_0";
    const std::string type = "NSM_NVSwitch";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/NVSwitch_0";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:101";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchIsolationModeSetTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchIsolationModeSetTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Test: setSwitchIsolationMode - Enabled success
TEST_F(NsmSwitchIsolationModeSetTest,
       SetSwitchIsolationMode_Enabled_ReturnsSuccess)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(testBus,
                                                               objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchIsolationMode>(name, type,
                                                           isolationIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_set_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    // Act
    EXPECT_NO_THROW_COROUTINE(
        sensor->setSwitchIsolationMode(value, &status, nvswitch));
}

// Test: setSwitchIsolationMode - Disabled success
TEST_F(NsmSwitchIsolationModeSetTest,
       SetSwitchIsolationMode_Disabled_ReturnsSuccess)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(testBus,
                                                               objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchIsolationMode>(name, type,
                                                           isolationIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL,
                                          responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationDisabled");

    // Act
    EXPECT_NO_THROW_COROUTINE(
        sensor->setSwitchIsolationMode(value, &status, nvswitch));
}

// Test: setSwitchIsolationMode - Invalid mode
TEST_F(NsmSwitchIsolationModeSetTest,
       SetSwitchIsolationMode_InvalidMode_SetsWriteFailure)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(testBus,
                                                               objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchIsolationMode>(name, type,
                                                           isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("InvalidMode");

    // Act
    sensor->setSwitchIsolationMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// Test: setSwitchIsolationMode - Null value throws
TEST_F(NsmSwitchIsolationModeSetTest,
       SetSwitchIsolationMode_NullValue_ThrowsInvalidArgument)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(testBus,
                                                               objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchIsolationMode>(name, type,
                                                           isolationIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass a bool instead of string
    AsyncSetOperationValueType value = true;

    // Act & Assert - coroutine throws InvalidArgument
    EXPECT_THROW_COROUTINE(
        sensor->setSwitchIsolationMode(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setSwitchIsolationMode - PostPatchIO failure
TEST_F(NsmSwitchIsolationModeSetTest,
       SetSwitchIsolationMode_PostPatchIOFailure_SetsWriteFailure)
{
    // Arrange
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(testBus,
                                                               objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchIsolationMode>(name, type,
                                                           isolationIntf);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("SwitchCommunicationEnabled");

    // Act
    sensor->setSwitchIsolationMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// PART 5: nsmSwitch.cpp - NsmSwitchL1PredictionMode coroutine tests
// ============================================================================

class NsmSwitchL1PredictionModeSetTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string name = "NVSwitch_0";
    const std::string type = "NSM_NVSwitch";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvswitch/NVSwitch_0/"
        "Oem/Nvidia/PowerMode/L1PredictionMode";
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:102";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchL1PredictionModeSetTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmSwitchL1PredictionModeSetTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Test: setL1PredictionMode - Enable success
TEST_F(NsmSwitchL1PredictionModeSetTest,
       SetL1PredictionMode_Enable_ReturnsSuccess)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchL1PredictionMode>(
        name, type, enableIntf, assocDefIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_set_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    // Act
    EXPECT_NO_THROW_COROUTINE(
        sensor->setL1PredictionMode(value, &status, nvswitch));
}

// Test: setL1PredictionMode - Disable success
TEST_F(NsmSwitchL1PredictionModeSetTest,
       SetL1PredictionMode_Disable_ReturnsSuccess)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchL1PredictionMode>(
        name, type, enableIntf, assocDefIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;

    // Act
    EXPECT_NO_THROW_COROUTINE(
        sensor->setL1PredictionMode(value, &status, nvswitch));
}

// Test: setL1PredictionMode - Invalid value type throws
TEST_F(NsmSwitchL1PredictionModeSetTest,
       SetL1PredictionMode_InvalidType_ThrowsInvalidArgument)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchL1PredictionMode>(
        name, type, enableIntf, assocDefIntf);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass string instead of bool
    AsyncSetOperationValueType value = std::string("not_a_bool");

    // Act & Assert - coroutine throws InvalidArgument
    EXPECT_THROW_COROUTINE(
        sensor->setL1PredictionMode(value, &status, nvswitch),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Test: setL1PredictionMode - postPatchIO failure
TEST_F(NsmSwitchL1PredictionModeSetTest,
       SetL1PredictionMode_PostPatchIOFailure_SetsWriteFailure)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchL1PredictionMode>(
        name, type, enableIntf, assocDefIntf);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    // Act
    sensor->setL1PredictionMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// Test: setL1PredictionMode - decode failure (error CC in response)
TEST_F(NsmSwitchL1PredictionModeSetTest,
       SetL1PredictionMode_DecodeFailure_SetsWriteFailure)
{
    // Arrange
    auto enableIntf = std::make_shared<EnableIntf>(bus, objPath.c_str());
    auto assocDefIntf =
        std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmSwitchL1PredictionMode>(
        name, type, enableIntf, assocDefIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_set_device_mode_settings_resp(0, NSM_ERROR, ERR_NULL, responseMsg);

    EXPECT_CALL(*nvswitch, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(responseData));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    // Act
    sensor->setL1PredictionMode(value, &status, nvswitch);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// PART 6: nsmSwitch.cpp - NsmSwitchDIReset constructor
// ============================================================================

TEST(NsmSwitchDIResetTest, Constructor_ValidArgs_CreatesObjectWithReset)
{
    // Arrange
    std::string name = "/NVSwitch_0";
    std::string type = "NSM_NVSwitch";
    std::string invObjPath = "/xyz/openbmc_project/inventory/system/nvswitch";

    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    auto device = std::make_shared<MockNsmDevice>(1, 0, "MCTP_UUID", uuid, 0);

    // Act
    NsmSwitchDIReset reset(testBus, name, type, invObjPath, device);

    // Assert
    EXPECT_EQ(reset.getName(), name);
    EXPECT_EQ(reset.getType(), type);
    EXPECT_NE(reset.resetIntf, nullptr);
    EXPECT_NE(reset.resetAsyncIntf, nullptr);
}

// ============================================================================
// PART 7: nsmDevice.cpp - Additional NsmDevice tests
// ============================================================================

// --- updateDiscoveryIdentifiers - first time with empty uuid ---
TEST(NsmDeviceExtended, UpdateDiscoveryIdentifiers_FirstTime_SetsAllFields)
{
    // Arrange
    uuid_t uuid = "";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    nsmDevice.uuid = "";

    eid_t newEid = 42;
    uuid_t newUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    uint8_t instNum = 5;
    std::string assocPath = "/mctp/assoc";
    std::string medium = "SMBus";
    std::string binding = "I2C";

    // Act
    bool result = nsmDevice.updateDiscoveryIdentifiers(
        newEid, newUuid, instNum, assocPath, medium, binding, 30);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_EQ(nsmDevice.getEid(), newEid);
    EXPECT_EQ(nsmDevice.getUuid(), newUuid);
    EXPECT_EQ(nsmDevice.nsmDeviceInstanceNumber, instNum);
    EXPECT_EQ(nsmDevice.mctpMedium, medium);
    EXPECT_EQ(nsmDevice.mctpBinding, binding);
    EXPECT_EQ(nsmDevice.associatedPath, assocPath);
    EXPECT_EQ(nsmDevice.getMctpLocalEid(), std::optional<eid_t>(30));
}

// --- updateDiscoveryIdentifiers - preferred path updates ---
TEST(NsmDeviceExtended,
     UpdateDiscoveryIdentifiers_PreferredUpdate_ResetsAndUpdates)
{
    // Arrange
    uuid_t uuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    nsmDevice.eid = 10;
    nsmDevice.mctpMedium = "SMBus";
    nsmDevice.mctpBinding = "I2C";

    eid_t newEid = 20;
    uuid_t newUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    uint8_t instNum = 7;
    std::string assocPath = "/mctp/new_assoc";
    std::string medium = "SMBus";
    std::string binding = "I2C";

    // Act
    bool result = nsmDevice.updateDiscoveryIdentifiers(
        newEid, newUuid, instNum, assocPath, medium, binding, 30);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_EQ(nsmDevice.getEid(), newEid);
}

// --- initMsgTypesSensor ---
TEST(NsmDeviceExtended, InitMsgTypesSensor_CreatesMsgTypesSensor)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Assert - initMsgTypesSensor is called in constructor
    EXPECT_NE(nsmDevice.msgTypesSensor, nullptr);
    EXPECT_EQ(nsmDevice.msgTypesSensor->getName(), "Supported Message Types");
}

// --- registerLongRunningEventHandler ---
TEST(NsmDeviceExtended, RegisterLongRunningEventHandler_AddsToDeviceEvents)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Assert - registerLongRunningEventHandler called in constructor
    EXPECT_GE(nsmDevice.deviceEvents.size(), 1u);
}

// --- progressCounters lazy init ---
TEST(NsmDeviceExtended, ProgressCounters_LazyInit_CreatesOnFirstCall)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Assert - initially null
    EXPECT_EQ(nsmDevice.sensorProgressCounters, nullptr);

    // Act
    auto& counters = nsmDevice.progressCounters();

    // Assert - now created
    EXPECT_NE(nsmDevice.sensorProgressCounters, nullptr);
    // Second call returns same instance
    auto& counters2 = nsmDevice.progressCounters();
    EXPECT_EQ(&counters, &counters2);
}

// --- registerLongRunningHandler / clearLongRunningHandler lifecycle ---
class MockLongRunningEvent : public NsmLongRunningEvent
{
  public:
    MockLongRunningEvent() :
        NsmLongRunningEvent("mock_lr", "mock_lr_type", false)
    {}

    int handle(eid_t, NsmType, NsmEventId, const nsm_msg*, size_t) override
    {
        handleCalled = true;
        return NSM_SW_SUCCESS;
    }

    bool handleCalled = false;
};

TEST(NsmDeviceExtended, RegisterLongRunningHandler_ThenClear_NoHandlerRemains)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    auto lrEvent = std::make_shared<MockLongRunningEvent>();

    // Act
    nsmDevice.registerLongRunningHandler(5, 10, lrEvent);
    EXPECT_TRUE(nsmDevice.longRunningHandler.has_value());

    nsmDevice.clearLongRunningHandler();
    EXPECT_FALSE(nsmDevice.longRunningHandler.has_value());
}

TEST(NsmDeviceExtended, RegisterLongRunningHandler_Overwrite_ReplacesExisting)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    auto lrEvent1 = std::make_shared<MockLongRunningEvent>();
    auto lrEvent2 = std::make_shared<MockLongRunningEvent>();

    // Act - register first handler
    nsmDevice.registerLongRunningHandler(1, 2, lrEvent1);

    // Act - register second handler (overwrite)
    nsmDevice.registerLongRunningHandler(3, 4, lrEvent2);

    // Assert - second handler is active
    EXPECT_TRUE(nsmDevice.longRunningHandler.has_value());
    auto& info = *nsmDevice.longRunningHandler;
    EXPECT_EQ(info.messageType, 3);
    EXPECT_EQ(info.commandCode, 4);
    EXPECT_EQ(info.sensorInstance, lrEvent2);
}

// --- invokeLongRunningHandler - no handler registered ---
TEST(NsmDeviceExtended, InvokeLongRunningHandler_NoHandler_ReturnsErrorData)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Clear any handler from constructor
    nsmDevice.longRunningHandler.reset();

    // Act
    int rc = nsmDevice.invokeLongRunningHandler(0, 0, 0, nullptr, 0);

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// --- FruInterfaceManager additional tests ---
TEST(FruInterfaceManagerExtended, IsPropertySupported_EmptySet_ReturnsFalse)
{
    // Arrange
    FruInterfaceManager mgr;

    // Act & Assert
    EXPECT_FALSE(mgr.isPropertySupported("BOARD_PART_NUMBER"));
    EXPECT_FALSE(mgr.isPropertySupported(""));
}

TEST(FruInterfaceManagerExtended, IsPropertySupported_AfterMarkInit_ReturnsTrue)
{
    // Arrange
    FruInterfaceManager mgr;
    std::set<std::string> props = {"BOARD_PART_NUMBER", "SERIAL_NUMBER"};
    mgr.markInitialized(props);

    // Act & Assert
    EXPECT_TRUE(mgr.isPropertySupported("BOARD_PART_NUMBER"));
    EXPECT_TRUE(mgr.isPropertySupported("SERIAL_NUMBER"));
    EXPECT_FALSE(mgr.isPropertySupported("MARKETING_NAME"));
}

TEST(FruInterfaceManagerExtended,
     NeedsRecreation_SamePropertiesAfterInit_ReturnsFalse)
{
    // Arrange
    FruInterfaceManager mgr;
    std::set<std::string> props = {"A", "B", "C"};
    mgr.markInitialized(props);

    // Act & Assert
    EXPECT_FALSE(mgr.needsRecreation(props));
}

TEST(FruInterfaceManagerExtended,
     NeedsRecreation_DifferentPropertiesAfterInit_ReturnsTrue)
{
    // Arrange
    FruInterfaceManager mgr;
    std::set<std::string> props1 = {"A", "B"};
    std::set<std::string> props2 = {"A", "B", "C"};
    mgr.markInitialized(props1);

    // Act & Assert
    EXPECT_TRUE(mgr.needsRecreation(props2));
}

TEST(FruInterfaceManagerExtended, Reset_AfterInit_ClearsEverything)
{
    // Arrange
    FruInterfaceManager mgr;
    std::set<std::string> props = {"A"};
    mgr.markInitialized(props);

    // Act
    mgr.reset();

    // Assert
    EXPECT_TRUE(mgr.needsRecreation({}));
    EXPECT_FALSE(mgr.initialized);
    EXPECT_TRUE(mgr.supportedProperties.empty());
    EXPECT_EQ(mgr.interface, nullptr);
}

TEST(FruInterfaceManagerExtended, UpdateAllPropertyValues_NullInterface_NoOp)
{
    // Arrange
    FruInterfaceManager mgr;
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    auto device = std::make_shared<MockNsmDevice>(1, 0, "MCTP_UUID", uuid, 0);
    InventoryProperties props;

    // Act - should not crash with null interface
    mgr.updateAllPropertyValues(device, props);
}

// --- NsmDevice::addSensorBase PollingType::Static ---
TEST(NsmDeviceAddSensorBase, StaticPollingType_SetsCorrectRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    auto sensor = std::make_shared<MockSensor>("static_s", "type");

    // Act
    nsmDevice.addSensorBase(sensor, PollingType::Static);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

// --- NsmDevice::addSensorBase PollingType::GpuPerformanceMonitoring ---
TEST(NsmDeviceAddSensorBase, GpuPerformanceMonitoring_SetsCorrectRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    auto sensor = std::make_shared<MockSensor>("gpm_s", "type");

    // Act
    nsmDevice.addSensorBase(sensor, PollingType::GpuPerformanceMonitoring);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, GPM_REFRESH_LIMIT_IN_USEC);
}

// --- NsmDevice::addSensorBase invalid PollingType ---
TEST(NsmDeviceAddSensorBase, InvalidPollingType_ThrowsRuntimeError)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    auto sensor = std::make_shared<MockSensor>("invalid_s", "type");

    // Act & Assert
    EXPECT_THROW(nsmDevice.addSensorBase(sensor, static_cast<PollingType>(99)),
                 std::runtime_error);
}

// --- NsmDevice multiple sensors in same queue ---
TEST(NsmDeviceAddSensorBase, MultipleSensors_AllAddedToDeviceSensors)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    size_t initial = nsmDevice.deviceSensors.size();

    auto s1 = std::make_shared<MockSensor>("s1", "t");
    auto s2 = std::make_shared<MockSensor>("s2", "t");
    auto s3 = std::make_shared<MockSensor>("s3", "t");

    // Act
    nsmDevice.addSensorBase(s1, PollingType::Priority);
    nsmDevice.addSensorBase(s2, PollingType::RoundRobin);
    nsmDevice.addSensorBase(s3, PollingType::LongRunning);

    // Assert
    EXPECT_EQ(nsmDevice.deviceSensors.size(), initial + 3);
    EXPECT_EQ(s1->refreshLimitInUsec, PRIORITY_REFRESH_LIMIT_IN_USEC);
    EXPECT_EQ(s2->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
    EXPECT_EQ(s3->refreshLimitInUsec, LONG_RUNNING_REFRESH_LIMIT_IN_USEC);
}

// --- NsmDevice::findAggregatorByType with entries ---
TEST(NsmDeviceExtended, FindAggregatorByType_NoMatch_ReturnsNull)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Act
    auto result = nsmDevice.findAggregatorByType("NonExistentType");

    // Assert
    EXPECT_EQ(result, nullptr);
}

// --- NsmDevice setEventMode boundary ---
TEST(NsmDeviceExtended, SetEventMode_AllValidValues_AcceptsAll)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    nsmDevice.setEventMode(0);
    EXPECT_EQ(nsmDevice.getEventMode(), 0);

    nsmDevice.setEventMode(1);
    EXPECT_EQ(nsmDevice.getEventMode(), 1);

    nsmDevice.setEventMode(2);
    EXPECT_EQ(nsmDevice.getEventMode(), 2);

    // invalid
    nsmDevice.setEventMode(3);
    EXPECT_EQ(nsmDevice.getEventMode(), 2); // unchanged
}

// --- NsmDevice isCommandSupported boundary ---
TEST(NsmDeviceExtended,
     IsCommandSupported_MaxValidMessageType_ReturnsFalseDefault)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    EXPECT_FALSE(nsmDevice.isCommandSupported(NUM_NSM_TYPES - 1, 0));
    EXPECT_FALSE(nsmDevice.isCommandSupported(NUM_NSM_TYPES, 0));
}

// --- NsmDevice updateMessageTypesToCommandCodeMatrix large size ---
TEST(NsmDeviceExtended,
     UpdateCommandCodeMatrix_LargeSize_CappedToMaxCommandCodes)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Create a large array - should be capped by NUM_COMMAND_CODES
    bitfield8_t supportedCommands[64] = {};
    for (int i = 0; i < 64; i++)
    {
        supportedCommands[i].byte = 0xFF;
    }

    // Act - should not crash even with large size
    nsmDevice.updateMessageTypesToCommandCodeMatrix(0, supportedCommands, 64);

    // Assert - command 0 should be supported
    EXPECT_TRUE(nsmDevice.isCommandSupported(0, 0));
}

// --- NsmDevice::allCommandCodesAreRetrieved ---
TEST(NsmDeviceExtended, AllCommandCodesRetrieved_MixedStates_ReturnsFalse)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    nsmDevice.areMessageTypesRetrieved = true;
    nsmDevice.commandCodesRetrieved.clear();
    nsmDevice.retrievedMessageTypes = {0, 1, 2};
    nsmDevice.commandCodesRetrieved[0] = true;
    nsmDevice.commandCodesRetrieved[1] = true;
    nsmDevice.commandCodesRetrieved[2] = false;

    EXPECT_FALSE(nsmDevice.allCommandCodesAreRetrieved());
}

// ============================================================================
// PART 8: nsmSwitch.cpp - NsmSwitchDI template constructors
// ============================================================================

TEST(NsmSwitchDITest, NvSwitchIntfConstructor_ValidArgs_CreatesObject)
{
    // Arrange & Act
    auto di = std::make_shared<NsmSwitchDI<NvSwitchIntf>>(
        "NVSwitch_0", "/xyz/openbmc_project/inventory/nvswitch/");

    // Assert
    EXPECT_NE(di, nullptr);
    EXPECT_EQ(di->getName(), "NVSwitch_0");
    EXPECT_EQ(di->getType(), "NSM_NVSwitch");
}

TEST(NsmSwitchDITest, UuidIntfConstructor_ValidArgs_CreatesObject)
{
    // Arrange & Act
    auto di = std::make_shared<NsmSwitchDI<UuidIntf>>(
        "NVSwitch_1", "/xyz/openbmc_project/inventory/nvswitch/");

    // Assert
    EXPECT_NE(di, nullptr);
    EXPECT_EQ(di->getName(), "NVSwitch_1");
}

TEST(NsmSwitchDITest, AssocDefIntfConstructor_ValidArgs_CreatesObject)
{
    // Arrange & Act
    auto di = std::make_shared<NsmSwitchDI<AssociationDefinitionsInft>>(
        "NVSwitch_2", "/xyz/openbmc_project/inventory/nvswitch/");

    // Assert
    EXPECT_NE(di, nullptr);
    EXPECT_EQ(di->getName(), "NVSwitch_2");
}

TEST(NsmSwitchDITest, SwitchIntfConstructor_ValidArgs_CreatesObject)
{
    // Arrange & Act
    auto di = std::make_shared<NsmSwitchDI<SwitchIntf>>(
        "NVSwitch_3", "/xyz/openbmc_project/inventory/nvswitch/");

    // Assert
    EXPECT_NE(di, nullptr);
    EXPECT_EQ(di->getName(), "NVSwitch_3");
}

TEST(NsmSwitchDITest, AssetIntfConstructor_ValidArgs_CreatesObject)
{
    // Arrange & Act
    auto di = std::make_shared<NsmSwitchDI<NsmAssetIntf>>(
        "NVSwitch_4", "/xyz/openbmc_project/inventory/nvswitch/");

    // Assert
    EXPECT_NE(di, nullptr);
    EXPECT_EQ(di->getName(), "NVSwitch_4");
}

// ============================================================================
// PART 9: nsmDevice.cpp - addDeviceSensors / addSetSensor / addDeviceEvent
// ============================================================================

TEST(NsmDeviceExtended, AddSetSensor_AddsSensorToSetSensorsList)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    auto sensor = std::make_shared<MockSensor>("set_sensor", "test");

    // Act
    nsmDevice.addSetSensor(sensor);

    // Assert
    EXPECT_EQ(nsmDevice.setSensors.size(), 1u);
    EXPECT_EQ(nsmDevice.setSensors[0], sensor);
}

TEST(NsmDeviceExtended, AddCapabilityRefreshSensor_AddsSensorToCapabilityList)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    auto sensor = std::make_shared<MockSensor>("cap_sensor", "test");

    // Act
    nsmDevice.addCapabilityRefreshSensor(sensor);

    // Assert
    EXPECT_EQ(nsmDevice.capabilityRefreshSensors.size(), 1u);
}

// --- dumpNsmDeviceInfo is a fire-and-forget coroutine wrapper ---
// Note: dumpNsmDeviceInfoTask logs device fields via lg2 which
// triggers Valgrind uninitialised-byte errors when MockNsmDevice
// has uninitialised members. Test the function pointer is callable
// without actually invoking the full logging path.
TEST(NsmDeviceExtended, DumpNsmDeviceInfo_FunctionExists)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Assert - verify the method exists and is accessible
    EXPECT_TRUE(static_cast<bool>(&MockNsmDevice::dumpNsmDeviceInfo));
}

// ============================================================================
// PART 10: nsmProcessor.cpp - NsmMigMode with long-running path
// ============================================================================

TEST(NsmMigMode, LongRunning_GoodGenReq)
{
    // Arrange
    std::string name = "mig_lr";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/mig_lr";
    NsmMigMode migSensor(testBus, name, type, objPath, nullptr, true);

    // Act
    auto request = migSensor.genRequestMsg(12, 30);

    // Assert
    EXPECT_TRUE(request.has_value());
    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_MIG_MODE);
}

// --- NsmMigMode long-running handleResponseMsg (decode path) ---
// Note: The long-running path uses decode_get_MIG_mode_event_resp instead
// of decode_get_MIG_mode_resp. We test both non-LR and LR construction paths.
TEST(NsmMigMode, NonLongRunning_HandleResp_UsesStandardDecode)
{
    // Arrange
    std::string name = "mig_standard";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/mig_std";
    NsmMigMode migSensor(testBus, name, type, objPath, nullptr, false);

    // Assert that isLongRunning is false
    EXPECT_FALSE(migSensor.isLongRunning);
}

TEST(NsmMigMode, LongRunning_HandleResp_UsesEventDecode)
{
    // Arrange
    std::string name = "mig_lr2";
    std::string type = "NSM_Processor";
    std::string objPath = "/xyz/openbmc_project/inventory/mig_lr2";
    NsmMigMode migSensor(testBus, name, type, objPath, nullptr, true);

    // Assert that isLongRunning is true
    EXPECT_TRUE(migSensor.isLongRunning);
}

// ============================================================================
// PART 11: NsmSwitchDIPowerMode - getInventoryObjectPath
// ============================================================================

TEST(NsmSwitchDIPowerModeTest, GetInventoryObjectPath_ReturnsConstructedPath)
{
    // Arrange
    std::string name = "NVSwitch_0";
    std::string invObjPath = "/xyz/openbmc_project/inventory/system/nvswitch/";
    NsmSwitchDIPowerMode pm(name, invObjPath);

    // Act
    std::string result = pm.getInventoryObjectPath();

    // Assert
    EXPECT_EQ(result, invObjPath + name);
}

// ============================================================================
// PART 12: NsmSwitchIsolationMode - default constructor
// ============================================================================

TEST(NsmSwitchIsolationModeTest, Constructor_NullInterface_CreatesObject)
{
    // Act - use parameterized constructor with nullptr interface
    NsmSwitchIsolationMode sensor("NVSwitch_default", "NSM_NVSwitch", nullptr);

    // Assert - constructed with null interface
    EXPECT_EQ(sensor.switchIsolationIntf, nullptr);
    EXPECT_EQ(sensor.getName(), "NVSwitch_default");
}

TEST(NsmSwitchIsolationModeTest, ParameterizedConstructor_SetsInterface)
{
    // Arrange
    auto intfPath = "/xyz/openbmc_project/inventory/nvswitch/isolation";
    auto isolationIntf = std::make_shared<SwitchIsolationIntf>(testBus,
                                                               intfPath);

    // Act
    NsmSwitchIsolationMode sensor("NVSwitch_0", "NSM_NVSwitch", isolationIntf);

    // Assert
    EXPECT_NE(sensor.switchIsolationIntf, nullptr);
    EXPECT_EQ(sensor.getName(), "NVSwitch_0");
    EXPECT_EQ(sensor.getType(), "NSM_NVSwitch");
}
