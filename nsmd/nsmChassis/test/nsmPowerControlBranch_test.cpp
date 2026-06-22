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

// =============================================================================
// Coverage tests for:
//   1. nsmPowerControl.cpp - 6 uncovered functions
//   2. nsmProcessorModulePowerControl.cpp - 6 uncovered functions
// =============================================================================

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "diagnostics.h"
#include "pci-links.h"
#include "platform-environmental.h"

#include "nsmChassis/nsmPowerControl.hpp"
#include "nsmChassis/nsmProcessorModulePowerControl.hpp"
#include "nsmDbusIfaceOverride/nsmPowerCapIface.hpp"
#include "nsmDbusIfaceOverride/nsmPowerSmoothingPowerProfileIntf.hpp"
#include "nsmDbusIfaceOverride/nsmResetIface.hpp"

using namespace nsm;

// =============================================================================
// Test Fixture: NsmPowerControl additional tests
// =============================================================================

struct NsmPowerControlExtTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "PowerControlExt";
    const std::string type = "NSM_PowerControl";
    const std::string path = "/xyz/openbmc_project/control/power_cap/HGX_ext";
    const std::string physicalContext = "SystemBoard";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<NsmPowerControl> powerControl;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPowerControlExtTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        EXPECT_CALL(mockManager, getEid(testing::_))
            .WillRepeatedly(testing::Return(0x10));
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::vector<utils::Association> associations;
        associations.push_back(
            {"chassis", "power_control",
             "/xyz/openbmc_project/inventory/system/chassis/HGX_ext"});
        std::string typeCopy = type;
        powerControl = std::make_shared<NsmPowerControl>(
            bus, name, associations, typeCopy, path, physicalContext);
    }

    ~NsmPowerControlExtTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// =============================================================================
// NsmPowerControl -- updatePowerCapValue
// =============================================================================

TEST_F(NsmPowerControlExtTest,
       UpdatePowerCapValue_SingleChild_UpdatesTotalCorrectly)
{
    // Arrange: add a child value
    powerControl->updatePowerCapValue("child1", 300);

    // Assert
    EXPECT_EQ(powerControl->PowerCapIntf::powerCap(), 300u);
    EXPECT_TRUE(powerControl->PowerCapIntf::powerCapEnable());
}

TEST_F(NsmPowerControlExtTest,
       UpdatePowerCapValue_MultipleChildren_SumsCorrectly)
{
    // Arrange: add two child values
    powerControl->updatePowerCapValue("child1", 200);
    powerControl->updatePowerCapValue("child2", 300);

    // Assert: total should be 500
    EXPECT_EQ(powerControl->PowerCapIntf::powerCap(), 500u);
}

TEST_F(NsmPowerControlExtTest,
       UpdatePowerCapValue_UpdateExistingChild_CorrectTotal)
{
    // Arrange: add, then update
    powerControl->updatePowerCapValue("child1", 200);
    powerControl->updatePowerCapValue("child2", 300);
    powerControl->updatePowerCapValue("child1", 400);

    // Assert: total should be 400 + 300 = 700
    EXPECT_EQ(powerControl->PowerCapIntf::powerCap(), 700u);
}

// =============================================================================
// NsmPowerControl -- clearPowerCap
// =============================================================================

TEST_F(NsmPowerControlExtTest, ClearPowerCap_ReturnsZero)
{
    EXPECT_EQ(powerControl->clearPowerCap(), 0);
}

// =============================================================================
// NsmPowerControl -- maxPowerCapValue / minPowerCapValue / defaultPowerCap
// These are custom getters that sum from the SensorManager lists
// =============================================================================

TEST_F(NsmPowerControlExtTest, MaxPowerCapValue_EmptyList_ReturnsZero)
{
    // maxPowerCapList is empty by default
    EXPECT_EQ(powerControl->maxPowerCapValue(), 0u);
}

TEST_F(NsmPowerControlExtTest, MinPowerCapValue_EmptyList_ReturnsZero)
{
    // minPowerCapList is empty by default
    EXPECT_EQ(powerControl->minPowerCapValue(), 0u);
}

TEST_F(NsmPowerControlExtTest, DefaultPowerCap_EmptyList_ReturnsZero)
{
    // defaultPowerCapList is empty by default
    EXPECT_EQ(powerControl->defaultPowerCap(), 0u);
}

// =============================================================================
// NsmPowerControl -- setPowerCap coroutine
// =============================================================================

TEST_F(NsmPowerControlExtTest, SetPowerCap_InvalidVariant_ThrowsInvalidArgument)
{
    AsyncSetOperationValueType value = std::string("invalid");
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    EXPECT_THROW_COROUTINE(
        powerControl->setPowerCap(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmPowerControlExtTest, SetPowerCap_AboveMax_ReturnsInvalidArgument)
{
    // The max/min come from SensorManager lists which are empty,
    // so max=0, min=0. Any value > 0 would be above max.
    uint32_t aboveMax = 100;
    AsyncSetOperationValueType value = aboveMax;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    auto coroutine = powerControl->setPowerCap(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmPowerControlExtTest, SetPowerCap_ValidValue_EmptyPowerCapList_Success)
{
    // With empty powerCapList, the for loop does nothing so it succeeds
    uint32_t validValue = 0;
    AsyncSetOperationValueType value = validValue;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    auto coroutine = powerControl->setPowerCap(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// Test Fixture: NsmProcessorModulePowerControl -- additional coverage
// =============================================================================

struct NsmProcessorModulePowerControlExtTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "ProcessorModulePowerControlExt";
    const std::string type = "NSM_ProcessorModulePowerControl";
    const std::string path =
        "/xyz/openbmc_project/control/proc_mod_power_ext/GPU_ext0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<NsmProcessorModulePowerControl> powerControl;
    std::shared_ptr<PowerCapIntf> powerCapIntf;
    std::shared_ptr<nsm::NsmClearPowerCapIntf> clearPowerCapIntf;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProcessorModulePowerControlExtTest() : SensorManagerTest(devices)
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
            {"chassis", "power_control",
             "/xyz/openbmc_project/inventory/system/chassis/HGX_ext"});

        powerCapIntf = std::make_shared<PowerCapIntf>(bus, path.c_str());
        clearPowerCapIntf = std::make_shared<nsm::NsmClearPowerCapIntf>(bus,
                                                                        path);

        powerControl = std::make_shared<NsmProcessorModulePowerControl>(
            bus, name, type, powerCapIntf, clearPowerCapIntf, path,
            associations_list);
    }

    ~NsmProcessorModulePowerControlExtTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// =============================================================================
// NsmDefaultModulePowerLimit -- constructor and update
// =============================================================================

TEST_F(NsmProcessorModulePowerControlExtTest,
       DefaultModulePowerLimit_Constructor_CreatesCorrectly)
{
    std::string dlName = "DefaultModulePowerLimit";
    std::string dlType = "NSM_DefaultModulePowerLimit";

    auto defaultLimit = std::make_shared<NsmDefaultModulePowerLimit>(
        dlName, dlType, clearPowerCapIntf);

    EXPECT_NE(defaultLimit, nullptr);
    EXPECT_EQ(defaultLimit->getName(), dlName);
}

TEST_F(NsmProcessorModulePowerControlExtTest,
       DefaultModulePowerLimit_Update_Success_SetsDefaultPowerCap)
{
    std::string dlName = "DefaultModulePowerLimit";
    std::string dlType = "NSM_DefaultModulePowerLimit";

    auto defaultLimit = std::make_shared<NsmDefaultModulePowerLimit>(
        dlName, dlType, clearPowerCapIntf);

    // Build a valid inventory information response with 4-byte value
    uint32_t ratedPower = 400000; // 400 W in mW
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &ratedPower, sizeof(ratedPower));

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 3,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(ratedPower), data.data(), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(testing::_, testing::_, testing::_, testing::_,
                               testing::_))
        .WillOnce(mockSensorIO(responseData));

    // Act
    EXPECT_NO_THROW(defaultLimit->update(gpu));

    // Assert: 400000/1000 = 400
    EXPECT_EQ(clearPowerCapIntf->defaultPowerCap(), 400u);
}

TEST_F(NsmProcessorModulePowerControlExtTest,
       DefaultModulePowerLimit_Update_SensorIOFailure_ReturnsError)
{
    std::string dlName = "DefaultModulePowerLimit2";
    std::string dlType = "NSM_DefaultModulePowerLimit";

    auto defaultLimit = std::make_shared<NsmDefaultModulePowerLimit>(
        dlName, dlType, clearPowerCapIntf);

    EXPECT_CALL(*gpu, sensorIO(testing::_, testing::_, testing::_, testing::_,
                               testing::_))
        .WillOnce(mockSensorIO(NSM_ERROR));

    // Act
    EXPECT_NO_THROW(defaultLimit->update(gpu));
}

TEST_F(NsmProcessorModulePowerControlExtTest,
       DefaultModulePowerLimit_Update_InvalidPowerLimit_SetsInvalidMarker)
{
    std::string dlName = "DefaultModulePowerLimit3";
    std::string dlType = "NSM_DefaultModulePowerLimit";

    auto defaultLimit = std::make_shared<NsmDefaultModulePowerLimit>(
        dlName, dlType, clearPowerCapIntf);

    uint32_t ratedPower = INVALID_POWER_LIMIT;
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &ratedPower, sizeof(ratedPower));

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 3,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(ratedPower), data.data(), response);

    EXPECT_CALL(*gpu, sensorIO(testing::_, testing::_, testing::_, testing::_,
                               testing::_))
        .WillOnce(mockSensorIO(responseData));

    // Act
    EXPECT_NO_THROW(defaultLimit->update(gpu));

    EXPECT_EQ(clearPowerCapIntf->defaultPowerCap(), INVALID_POWER_LIMIT);
}

// =============================================================================
// NsmModulePowerLimit -- update coroutine
// =============================================================================

TEST_F(NsmProcessorModulePowerControlExtTest,
       ModulePowerLimit_Update_MaxPowerLimit_Success)
{
    std::string mlName = "MaxModulePowerLimit";
    std::string mlType = "NSM_ModulePowerLimit";

    auto maxLimit = std::make_shared<NsmModulePowerLimit>(
        mlName, mlType, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    uint32_t maxPower = 600000; // 600 W in mW
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &maxPower, sizeof(maxPower));

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 3,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(maxPower), data.data(), response);

    EXPECT_CALL(*gpu, sensorIO(testing::_, testing::_, testing::_, testing::_,
                               testing::_))
        .WillOnce(mockSensorIO(responseData));

    // Act
    EXPECT_NO_THROW(maxLimit->update(gpu));

    // Assert: 600000/1000 = 600
    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), 600u);
}

TEST_F(NsmProcessorModulePowerControlExtTest,
       ModulePowerLimit_Update_MinPowerLimit_Success)
{
    std::string mlName = "MinModulePowerLimit";
    std::string mlType = "NSM_ModulePowerLimit";

    auto minLimit = std::make_shared<NsmModulePowerLimit>(
        mlName, mlType, MINIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    uint32_t minPower = 200000; // 200 W in mW
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &minPower, sizeof(minPower));

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 3,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(minPower), data.data(), response);

    EXPECT_CALL(*gpu, sensorIO(testing::_, testing::_, testing::_, testing::_,
                               testing::_))
        .WillOnce(mockSensorIO(responseData));

    // Act
    EXPECT_NO_THROW(minLimit->update(gpu));

    // Assert: 200000/1000 = 200
    EXPECT_EQ(powerCapIntf->minPowerCapValue(), 200u);
}

TEST_F(NsmProcessorModulePowerControlExtTest,
       ModulePowerLimit_Update_SensorIOFailure_ReturnsError)
{
    std::string mlName = "MaxModulePowerLimit2";
    std::string mlType = "NSM_ModulePowerLimit";

    auto maxLimit = std::make_shared<NsmModulePowerLimit>(
        mlName, mlType, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    EXPECT_CALL(*gpu, sensorIO(testing::_, testing::_, testing::_, testing::_,
                               testing::_))
        .WillOnce(mockSensorIO(NSM_ERROR));

    // Act
    EXPECT_NO_THROW(maxLimit->update(gpu));
}

TEST_F(NsmProcessorModulePowerControlExtTest,
       ModulePowerLimit_Update_ErrorCC_DoesNotSetValue)
{
    std::string mlName = "MaxModulePowerLimit3";
    std::string mlType = "NSM_ModulePowerLimit";

    auto maxLimit = std::make_shared<NsmModulePowerLimit>(
        mlName, mlType, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    // Set a known value first
    powerCapIntf->maxPowerCapValue(999);

    uint32_t maxPower = 600000;
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &maxPower, sizeof(maxPower));

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 3,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_inventory_information_resp(
        0, NSM_ERROR, ERR_NULL, sizeof(maxPower), data.data(), response);

    EXPECT_CALL(*gpu, sensorIO(testing::_, testing::_, testing::_, testing::_,
                               testing::_))
        .WillOnce(mockSensorIO(responseData));

    // Act
    EXPECT_NO_THROW(maxLimit->update(gpu));

    // Assert: value should remain 999 since CC was error
    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), 999u);
}

TEST_F(NsmProcessorModulePowerControlExtTest,
       ModulePowerLimit_Update_InvalidPowerLimit_SetsInvalidMarker)
{
    std::string mlName = "MaxModulePowerLimit4";
    std::string mlType = "NSM_ModulePowerLimit";

    auto maxLimit = std::make_shared<NsmModulePowerLimit>(
        mlName, mlType, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    uint32_t maxPower = INVALID_POWER_LIMIT;
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &maxPower, sizeof(maxPower));

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 3,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(maxPower), data.data(), response);

    EXPECT_CALL(*gpu, sensorIO(testing::_, testing::_, testing::_, testing::_,
                               testing::_))
        .WillOnce(mockSensorIO(responseData));

    // Act
    EXPECT_NO_THROW(maxLimit->update(gpu));

    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), INVALID_POWER_LIMIT);
}

// =============================================================================
// NsmProcessorModulePowerControl -- doClearPowerCapOnModule coroutine
// =============================================================================

TEST_F(NsmProcessorModulePowerControlExtTest,
       DoClearPowerCapOnModule_Success_StatusSuccess)
{
    auto& bus = utils::DBusHandler::getBus();

    // Register a mock device in processorModuleToDeviceMap
    SensorManager& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {gpu};

    // Set a default power cap value
    clearPowerCapIntf->defaultPowerCap(400);

    // Build a valid set_power_limit response
    std::vector<uint8_t> setPowerLimitResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setPowerLimitRespMsg =
        reinterpret_cast<nsm_msg*>(setPowerLimitResp.data());
    encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, setPowerLimitRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(setPowerLimitResp));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/test_clear_mod1");

    // Act
    EXPECT_NO_THROW(powerControl->doClearPowerCapOnModule(statusIntf));

    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmProcessorModulePowerControlExtTest,
       DoClearPowerCapOnModule_Failure_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();

    SensorManager& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {gpu};

    clearPowerCapIntf->defaultPowerCap(400);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/test_clear_mod2");

    // Act
    EXPECT_NO_THROW(powerControl->doClearPowerCapOnModule(statusIntf));

    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// NsmModulePowerLimit -- update with unknown propertyId (default branch)
// =============================================================================

TEST_F(NsmProcessorModulePowerControlExtTest,
       ModulePowerLimit_Update_UnknownPropertyId_DoesNotSetValues)
{
    std::string mlName = "UnknownPropLimit";
    std::string mlType = "NSM_ModulePowerLimit";
    uint8_t unknownPropId = 99;

    auto unknownLimit = std::make_shared<NsmModulePowerLimit>(
        mlName, mlType, unknownPropId, powerCapIntf);

    // Even though decode succeeds with correct size, the switch falls through
    // to default and does nothing
    uint32_t someValue = 500000;
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &someValue, sizeof(someValue));

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 3,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(someValue), data.data(), response);

    EXPECT_CALL(*gpu, sensorIO(testing::_, testing::_, testing::_, testing::_,
                               testing::_))
        .WillOnce(mockSensorIO(responseData));

    // Set known values
    powerCapIntf->maxPowerCapValue(111);
    powerCapIntf->minPowerCapValue(222);

    // Act
    EXPECT_NO_THROW(unknownLimit->update(gpu));

    // Assert: values unchanged since unknown property ID hits default case
    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), 111u);
    EXPECT_EQ(powerCapIntf->minPowerCapValue(), 222u);
}

// =============================================================================
// NsmPowerCapIntf – coroutine branch coverage
// =============================================================================

static std::vector<uint8_t>
    makeGetPowerLimitSuccessResp(uint32_t enforcedLimitMw)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0, enforcedLimitMw,
                                msg);
    return buf;
}

static std::vector<uint8_t> makeSetPowerLimitSuccessResp()
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    return buf;
}

struct NsmPowerCapIfaceTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::string pcName = "TestPowerCap";
    std::vector<std::string> parents;

    NsmPowerCapIfaceTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPowerCapIfaceTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmPowerCapIntf> makePowerCapIntf(const char* path)
    {
        return std::make_shared<NsmPowerCapIntf>(utils::DBusHandler::getBus(),
                                                 path, pcName, parents, gpu);
    }
};

// getPowerCapFromDevice: postPatchIO fails → returns error, doesn't set
// powerCap
TEST_F(NsmPowerCapIfaceTest, GetPowerCapFromDevice_PostPatchIOFails)
{
    auto iface =
        makePowerCapIntf("/xyz/openbmc_project/control/power_cap/pc_piof");
    iface->powerCap(0xDEAD);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeGetPowerLimitSuccessResp(100), NSM_ERROR));

    auto coro = iface->getPowerCapFromDevice();
    // powerCap unchanged since postPatchIO failed before decode
    EXPECT_EQ(iface->powerCap(), 0xDEAD);
}

// getPowerCapFromDevice: error CC → doesn't set powerCap
TEST_F(NsmPowerCapIfaceTest, GetPowerCapFromDevice_ErrorCC_DoesNotSetPowerCap)
{
    auto iface =
        makePowerCapIntf("/xyz/openbmc_project/control/power_cap/pc_errcc");
    iface->powerCap(0xBEEF);

    std::vector<uint8_t> errResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(errResp.data());
    encode_get_power_limit_resp(0, NSM_ERROR, ERR_INVALID_RQD, 0, 0, 0, msg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errResp));

    auto coro = iface->getPowerCapFromDevice();
    EXPECT_EQ(iface->powerCap(), 0xBEEF);
}

// getPowerCapFromDevice: success with valid limit → sets powerCap in watts
TEST_F(NsmPowerCapIfaceTest, GetPowerCapFromDevice_Success_SetsPowerCap)
{
    auto iface =
        makePowerCapIntf("/xyz/openbmc_project/control/power_cap/pc_ok");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitSuccessResp(3000)));

    auto coro = iface->getPowerCapFromDevice();
    EXPECT_EQ(iface->powerCap(), 3u); // 3000 mW → 3 W
}

// getPowerCapFromDevice: INVALID_POWER_LIMIT → propagated as-is
TEST_F(NsmPowerCapIfaceTest,
       GetPowerCapFromDevice_InvalidLimit_SetsInvalidMarker)
{
    auto iface =
        makePowerCapIntf("/xyz/openbmc_project/control/power_cap/pc_inv");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeGetPowerLimitSuccessResp(INVALID_POWER_LIMIT)));

    auto coro = iface->getPowerCapFromDevice();
    EXPECT_EQ(iface->powerCap(), INVALID_POWER_LIMIT);
}

// setPowerCap: non-tuple value → throws InvalidArgument
TEST_F(NsmPowerCapIfaceTest, SetPowerCap_NonTupleValue_ThrowsInvalidArgument)
{
    auto iface =
        makePowerCapIntf("/xyz/openbmc_project/control/power_cap/pc_nt");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("invalid");

    EXPECT_THROW_COROUTINE(
        iface->setPowerCap(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// setPowerCap: powerLimit > max → InvalidArgument
TEST_F(NsmPowerCapIfaceTest, SetPowerCap_AboveMax_ReturnsInvalidArgument)
{
    auto iface =
        makePowerCapIntf("/xyz/openbmc_project/control/power_cap/pc_amax");
    iface->maxPowerCapValue(100);
    iface->minPowerCapValue(0);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::make_tuple(true, 200u); // 200 > 100

    auto coro = iface->setPowerCap(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// setPowerCap: powerLimit < min → InvalidArgument
TEST_F(NsmPowerCapIfaceTest, SetPowerCap_BelowMin_ReturnsInvalidArgument)
{
    auto iface =
        makePowerCapIntf("/xyz/openbmc_project/control/power_cap/pc_bmin");
    iface->maxPowerCapValue(100);
    iface->minPowerCapValue(50);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::make_tuple(true, 10u); // 10 < 50

    auto coro = iface->setPowerCap(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// setPowerCapOnDevice: postPatchIO fails → WriteFailure
TEST_F(NsmPowerCapIfaceTest, SetPowerCapOnDevice_PostPatchIOFails_WriteFailure)
{
    auto iface =
        makePowerCapIntf("/xyz/openbmc_project/control/power_cap/pc_spod_piof");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitSuccessResp(), NSM_ERROR));

    auto coro = iface->setPowerCapOnDevice(100, &status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setPowerCapOnDevice: error CC in response → WriteFailure
TEST_F(NsmPowerCapIfaceTest, SetPowerCapOnDevice_ErrorCC_WriteFailure)
{
    auto iface = makePowerCapIntf(
        "/xyz/openbmc_project/control/power_cap/pc_spod_errcc");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    std::vector<uint8_t> errResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                 0);
    auto msg = reinterpret_cast<nsm_msg*>(errResp.data());
    encode_set_power_limit_resp(0, NSM_ERROR, ERR_INVALID_RQD, msg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errResp));

    auto coro = iface->setPowerCapOnDevice(100, &status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setPowerCapOnDevice: success → calls getPowerCapFromDevice (two postPatchIO)
TEST_F(NsmPowerCapIfaceTest, SetPowerCapOnDevice_Success_EmptyParents)
{
    auto iface =
        makePowerCapIntf("/xyz/openbmc_project/control/power_cap/pc_spod_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // First: set power limit succeeds; second: verify getPowerCapFromDevice
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPowerLimitSuccessResp()))
        .WillOnce(mockPostPatchIO(makeGetPowerLimitSuccessResp(100000)));

    auto coro = iface->setPowerCapOnDevice(100, &status, true);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// doClearPowerCap and NsmChassisClearPowerCapAsyncIntf::clearPowerCap
// =============================================================================

// Forward-declare doClearPowerCap (free function in nsm namespace)
namespace nsm
{
requester::Coroutine
    doClearPowerCap(std::shared_ptr<AsyncStatusIntf> statusInterface);
} // namespace nsm

// doClearPowerCap: empty defaultPowerCapList → loop never entered →
// statusInterface->status(Success) → co_return NSM_SW_SUCCESS //

TEST_F(NsmPowerControlExtTest, DoClearPowerCap_EmptyList_StatusSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    // Ensure defaultPowerCapList is empty (it is by default in tests)
    SensorManager& manager = SensorManager::getInstance();
    manager.defaultPowerCapList.clear();

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/do_clear_1");

    // Act: call doClearPowerCap directly
    EXPECT_NO_THROW(nsm::doClearPowerCap(statusIntf));

    // Assert: status should be Success (loop never ran, status stays Success)
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// NsmChassisClearPowerCapAsyncIntf::clearPowerCap: AsyncOperationManager
// allocates a status interface → objectPath is non-empty → doClearPowerCap
// is called with empty defaultPowerCapList → returns a valid objectPath
TEST_F(NsmPowerControlExtTest,
       ChassisClearPowerCap_EmptyPowerCapList_ReturnsPath)
{
    // Ensure defaultPowerCapList is empty
    SensorManager& manager = SensorManager::getInstance();
    manager.defaultPowerCapList.clear();

    // Act: call clearPowerCap on the NsmChassisClearPowerCapAsyncIntf
    // AsyncOperationManager is a static singleton, it will allocate an object
    sdbusplus::object_path path;
    EXPECT_NO_THROW(
        path = powerControl->clearPowerCapAsyncIntf->clearPowerCap());

    // Assert: a valid (non-empty) path is returned
    EXPECT_FALSE(std::string(path).empty());
}
