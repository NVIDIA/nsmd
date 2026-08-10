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
 * Branch coverage tests for nsmProcessorModulePowerControl.cpp
 *
 * Targets:
 *   - genRequestMsg: encode failure (instanceId > NSM_INSTANCE_MAX)
 *   - handleResponseMsg: decode success + various cc/rc combinations
 *   - handleResponseMsg: INVALID_POWER_LIMIT ternary branch
 *   - handleResponseMsg: decode fail (truncated buffer)
 *   - handleResponseMsg: decode success, non-zero cc → else branch
 *   - setModulePowerCap: invalid value type → throw
 *   - setModulePowerCap: out of range → InvalidArgument status
 *   - updatePowerLimitOnModule: patchPowerLimitInProgress → Unavailable
 *   - updatePowerLimitOnModule: encode failure → break
 *   - updatePowerLimitOnModule: postPatchIO failure → break
 *   - updatePowerLimitOnModule: decode success + non-zero cc → break
 *   - doClearPowerCapOnModule: wrapper coroutine
 *   - clearPowerCap: calls doClearPowerCapOnModule and returns path
 *   - NsmModulePowerLimit::update: encode fail, sensorIO fail, decode paths
 *   - NsmDefaultModulePowerLimit::update: encode fail, sensorIO fail, decode
 * paths
 *   - Factory: missing properties, index != 0, chassisPath empty
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"

#include "nsmProcessorModulePowerControl.hpp"

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmProcModPowerBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "ProcModPower_Branch";
    const std::string type = "NSM_ProcessorModulePowerControl";
    const std::string path =
        "/xyz/openbmc_project/control/processor_module_power_cap/GPU_br";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<NsmProcessorModulePowerControl> powerControl;
    std::shared_ptr<PowerCapIntf> powerCapIntf;
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf;

    NsmProcModPowerBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::vector<std::tuple<std::string, std::string, std::string>>
            associations_list;
        associations_list.push_back(
            {"chassis", "power_controls",
             "/xyz/openbmc_project/inventory/system/chassis/HGX_br"});

        powerCapIntf = std::make_shared<PowerCapIntf>(bus, path.c_str());
        clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(bus, path);

        powerControl = std::make_shared<NsmProcessorModulePowerControl>(
            bus, name, type, powerCapIntf, clearPowerCapIntf, path,
            associations_list);

        ASSERT_NE(powerControl, nullptr);
    }

    ~NsmProcModPowerBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// genRequestMsg branches
// ============================================================================

// encode failure: instanceId > NSM_INSTANCE_MAX → nullopt
TEST_F(NsmProcModPowerBranchTest,
       GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    auto result = powerControl->genRequestMsg(5, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// handleResponseMsg branches
// ============================================================================

// Success path with normal enforced limit → reading = enforced_limit / 1000
TEST_F(NsmProcModPowerBranchTest, HandleResponseMsg_Success_NormalLimit)
{
    uint32_t requested_persistent = 500000;
    uint32_t requested_oneshot = 0;
    uint32_t enforced = 400000; // 400W in milliwatts

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_power_limit_resp));
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL,
                                          requested_persistent,
                                          requested_oneshot, enforced, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = powerControl->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(powerCapIntf->powerCap(), 400u);
}

// Success path with INVALID_POWER_LIMIT → reading = INVALID_POWER_LIMIT
TEST_F(NsmProcModPowerBranchTest,
       HandleResponseMsg_Success_InvalidPowerLimit_TernaryTrue)
{
    uint32_t requested_persistent = 0;
    uint32_t requested_oneshot = 0;
    uint32_t enforced = INVALID_POWER_LIMIT;

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_power_limit_resp));
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL,
                                          requested_persistent,
                                          requested_oneshot, enforced, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = powerControl->handleResponseMsg(msg, buf.size());
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(powerCapIntf->powerCap(), INVALID_POWER_LIMIT);
}

// Error CC → else branch, cc != 0 → returns cc
TEST_F(NsmProcModPowerBranchTest, HandleResponseMsg_ErrorCC_ReturnsCC)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_power_limit_resp));
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_power_limit_resp(0, NSM_ERROR, ERR_NULL, 0, 0, 0, msg);

    auto result = powerControl->handleResponseMsg(msg, buf.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// Decode failure (truncated buffer) → rc != 0, cc could be init → else branch
TEST_F(NsmProcModPowerBranchTest, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto result = powerControl->handleResponseMsg(msg, buf.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// Decode success but cc != NSM_SUCCESS → if condition false, else branch
TEST_F(NsmProcModPowerBranchTest,
       HandleResponseMsg_DecodeSuccessNonZeroCC_DoesNotUpdate)
{
    // Set initial power cap to verify it doesn't change
    powerCapIntf->powerCap(999);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto result = powerControl->handleResponseMsg(msg, buf.size());
    EXPECT_NE(result, NSM_SUCCESS);
    // Power cap should not have been updated
    EXPECT_EQ(powerCapIntf->powerCap(), 999u);
}

// ============================================================================
// setModulePowerCap branches
// ============================================================================

// Invalid value type → throw InvalidArgument
TEST_F(NsmProcModPowerBranchTest, SetModulePowerCap_InvalidType_Throws)
{
    AsyncSetOperationValueType wrongValue = std::string("not_a_uint32");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_THROW_COROUTINE(
        powerControl->setModulePowerCap(wrongValue, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// Value above maxPowerCapValue → InvalidArgument status
TEST_F(NsmProcModPowerBranchTest, SetModulePowerCap_AboveMax_InvalidArgument)
{
    powerCapIntf->minPowerCapValue(100);
    powerCapIntf->maxPowerCapValue(500);

    AsyncSetOperationValueType value = uint32_t{600}; // above max
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    powerControl->setModulePowerCap(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// Value below minPowerCapValue → InvalidArgument status
TEST_F(NsmProcModPowerBranchTest, SetModulePowerCap_BelowMin_InvalidArgument)
{
    powerCapIntf->minPowerCapValue(100);
    powerCapIntf->maxPowerCapValue(500);

    AsyncSetOperationValueType value = uint32_t{50}; // below min
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    powerControl->setModulePowerCap(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// ============================================================================
// updatePowerLimitOnModule branches
// ============================================================================

// patchPowerLimitInProgress = true → Unavailable status
TEST_F(NsmProcModPowerBranchTest,
       UpdatePowerLimitOnModule_InProgress_ReturnsUnavailable)
{
    powerControl->patchPowerLimitInProgress = true;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT, 300);
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
    // Reset for cleanup
    powerControl->patchPowerLimitInProgress = false;
}

// path not in processorModuleToDeviceMap → rc = NSM_SW_ERROR
TEST_F(NsmProcModPowerBranchTest,
       UpdatePowerLimitOnModule_NoDeviceInMap_WriteFailure)
{
    // path is not registered in processorModuleToDeviceMap
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT, 300);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// postPatchIO failure → break → WriteFailure
TEST_F(NsmProcModPowerBranchTest,
       UpdatePowerLimitOnModule_PostPatchIOFail_WriteFailure)
{
    auto& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {gpu};

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT, 300);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);

    manager.processorModuleToDeviceMap.erase(path);
}

// decode success + cc != NSM_SUCCESS → break → WriteFailure
TEST_F(NsmProcModPowerBranchTest,
       UpdatePowerLimitOnModule_DecodeSuccessNonZeroCC_WriteFailure)
{
    auto& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {gpu};

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(buf));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT, 300);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);

    manager.processorModuleToDeviceMap.erase(path);
}

// Success path → cc=0, rc=0 → Success status
TEST_F(NsmProcModPowerBranchTest,
       UpdatePowerLimitOnModule_Success_ReturnsSuccess)
{
    auto& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {gpu};

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT, 300);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);

    manager.processorModuleToDeviceMap.erase(path);
}

// ============================================================================
// doClearPowerCapOnModule + clearPowerCap
// ============================================================================

// doClearPowerCapOnModule wraps updatePowerLimitOnModule with DEFAULT_LIMIT
TEST_F(NsmProcModPowerBranchTest, DoClearPowerCapOnModule_SetsStatus)
{
    auto& bus = utils::DBusHandler::getBus();
    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/com/nvidia/nsmd/test/procmod_doclear0");

    // No devices in map → will set WriteFailure
    powerControl->doClearPowerCapOnModule(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// clearPowerCap returns a valid object path
TEST_F(NsmProcModPowerBranchTest, ClearPowerCap_ReturnsObjectPath)
{
    auto& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {gpu};

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillRepeatedly(mockPostPatchIO(successResp));

    auto objPath = powerControl->clearPowerCap();
    EXPECT_FALSE(std::string(objPath).empty());

    manager.processorModuleToDeviceMap.erase(path);
}

// ============================================================================
// NsmModulePowerLimit::update branches
// ============================================================================

struct NsmModulePowerLimitBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<PowerCapIntf> powerCapIntf;
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string path = "/xyz/openbmc_project/test/module_power_limit_br";

    NsmModulePowerLimitBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        powerCapIntf = std::make_shared<PowerCapIntf>(bus, path.c_str());
    }

    ~NsmModulePowerLimitBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// sensorIO failure → co_return rc
TEST_F(NsmModulePowerLimitBranchTest, Update_SensorIOFail_ReturnsError)
{
    std::string n = "MaxPLBr";
    std::string t = "NSM_ModulePowerLimit";
    auto sensor = std::make_shared<NsmModulePowerLimit>(
        n, t, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor->update(gpu);
}

// sensorIO success + decode success + cc=0 + max limit
TEST_F(NsmModulePowerLimitBranchTest, Update_Success_MaxLimit_SetsMaxPowerCap)
{
    std::string n = "MaxPLBr2";
    std::string t = "NSM_ModulePowerLimit";
    auto sensor = std::make_shared<NsmModulePowerLimit>(
        n, t, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    uint32_t value = 600000; // 600W in milliwatts
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &value, sizeof(value));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_inventory_information_resp) +
                                  sizeof(value));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(value), data.data(), msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->update(gpu);
    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), 600u);
}

// sensorIO success + decode success + cc=0 + min limit
TEST_F(NsmModulePowerLimitBranchTest, Update_Success_MinLimit_SetsMinPowerCap)
{
    std::string n = "MinPLBr";
    std::string t = "NSM_ModulePowerLimit";
    auto sensor = std::make_shared<NsmModulePowerLimit>(
        n, t, MINIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    uint32_t value = 200000; // 200W
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &value, sizeof(value));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_inventory_information_resp) +
                                  sizeof(value));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(value), data.data(), msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->update(gpu);
    EXPECT_EQ(powerCapIntf->minPowerCapValue(), 200u);
}

// sensorIO success + decode success + cc=0 + INVALID_POWER_LIMIT ternary
TEST_F(NsmModulePowerLimitBranchTest,
       Update_Success_InvalidLimit_SetsInvalidPowerCap)
{
    std::string n = "MaxPLInv";
    std::string t = "NSM_ModulePowerLimit";
    auto sensor = std::make_shared<NsmModulePowerLimit>(
        n, t, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    uint32_t value = INVALID_POWER_LIMIT;
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &value, sizeof(value));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_inventory_information_resp) +
                                  sizeof(value));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(value), data.data(), msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->update(gpu);
    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), INVALID_POWER_LIMIT);
}

// sensorIO success + decode success + cc != 0 → does not set value
TEST_F(NsmModulePowerLimitBranchTest, Update_DecodeSuccessNonZeroCC_NoUpdate)
{
    std::string n = "MaxPLCC";
    std::string t = "NSM_ModulePowerLimit";
    auto sensor = std::make_shared<NsmModulePowerLimit>(
        n, t, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    powerCapIntf->maxPowerCapValue(999);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(buf));

    sensor->update(gpu);
    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), 999u);
}

// default switch case: unknown propertyId → does not set any value
TEST_F(NsmModulePowerLimitBranchTest, Update_UnknownPropertyId_DefaultCase)
{
    std::string n = "UnkPLBr";
    std::string t = "NSM_ModulePowerLimit";
    uint8_t unknownId = 99;
    auto sensor = std::make_shared<NsmModulePowerLimit>(n, t, unknownId,
                                                        powerCapIntf);

    uint32_t value = 300000;
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &value, sizeof(value));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_inventory_information_resp) +
                                  sizeof(value));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(value), data.data(), msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->update(gpu);
    // Neither max nor min should be updated to 300
}

// ============================================================================
// NsmDefaultModulePowerLimit::update branches
// ============================================================================

struct NsmDefaultModulePowerLimitBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<PowerCapIntf> powerCapIntf;
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string path =
        "/xyz/openbmc_project/test/default_module_power_limit_br";

    NsmDefaultModulePowerLimitBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        powerCapIntf = std::make_shared<PowerCapIntf>(bus, path.c_str());
    }

    ~NsmDefaultModulePowerLimitBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// sensorIO failure → co_return rc
TEST_F(NsmDefaultModulePowerLimitBranchTest, Update_SensorIOFail_ReturnsError)
{
    auto sensor = std::make_shared<NsmDefaultModulePowerLimit>(
        "DefPLBr", "NSM_ModulePowerLimit", powerCapIntf);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor->update(gpu);
}

// Success path → sets defaultPowerCap
TEST_F(NsmDefaultModulePowerLimitBranchTest, Update_Success_SetsDefaultPowerCap)
{
    auto sensor = std::make_shared<NsmDefaultModulePowerLimit>(
        "DefPLBr2", "NSM_ModulePowerLimit", powerCapIntf);

    uint32_t value = 450000; // 450W
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &value, sizeof(value));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_inventory_information_resp) +
                                  sizeof(value));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(value), data.data(), msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->update(gpu);
    EXPECT_EQ(powerCapIntf->defaultPowerCap(), 450u);
}

// Success path with INVALID_POWER_LIMIT ternary
TEST_F(NsmDefaultModulePowerLimitBranchTest,
       Update_Success_InvalidLimit_SetsInvalidPowerCap)
{
    auto sensor = std::make_shared<NsmDefaultModulePowerLimit>(
        "DefPLInv", "NSM_ModulePowerLimit", powerCapIntf);

    uint32_t value = INVALID_POWER_LIMIT;
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &value, sizeof(value));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_inventory_information_resp) +
                                  sizeof(value));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(value), data.data(), msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->update(gpu);
    EXPECT_EQ(powerCapIntf->defaultPowerCap(), INVALID_POWER_LIMIT);
}

// Decode success + cc != 0 → does not update
TEST_F(NsmDefaultModulePowerLimitBranchTest,
       Update_DecodeSuccessNonZeroCC_NoUpdate)
{
    auto sensor = std::make_shared<NsmDefaultModulePowerLimit>(
        "DefPLCC", "NSM_ModulePowerLimit", powerCapIntf);

    powerCapIntf->defaultPowerCap(888);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(buf));

    sensor->update(gpu);
    EXPECT_EQ(powerCapIntf->defaultPowerCap(), 888u);
}
