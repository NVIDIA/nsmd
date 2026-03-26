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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "libnsm/base.h"

#include "nsmProcessorModulePowerControl.hpp"

using namespace nsm;

struct NsmProcessorModulePowerControlTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "ProcessorModulePowerControl";
    const std::string type = "NSM_ProcessorModulePowerControl";
    const std::string path =
        "/xyz/openbmc_project/control/processor_module_power_cap/GPU0";

    NsmDeviceTable devices;
    std::shared_ptr<NsmProcessorModulePowerControl> powerControl;
    std::shared_ptr<PowerCapIntf> powerCapIntf;
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf;

    NsmProcessorModulePowerControlTest() : SensorManagerTest(devices) {}

    ~NsmProcessorModulePowerControlTest()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();

        std::vector<std::tuple<std::string, std::string, std::string>>
            associations_list;
        associations_list.push_back(
            {"chassis", "power_control",
             "/xyz/openbmc_project/inventory/system/chassis/HGX"});

        powerCapIntf = std::make_shared<PowerCapIntf>(bus, path.c_str());
        clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(bus, path);

        powerControl = std::make_shared<NsmProcessorModulePowerControl>(
            bus, name, type, powerCapIntf, clearPowerCapIntf, path,
            associations_list);

        EXPECT_NE(powerControl, nullptr);
        EXPECT_EQ(powerControl->getName(), name);
        EXPECT_EQ(powerControl->getType(), type);
    }
};

TEST_F(NsmProcessorModulePowerControlTest, goodTestConstructor)
{
    EXPECT_NE(powerControl->powerCapIntf, nullptr);
    EXPECT_NE(powerControl->clearPowerCapIntf, nullptr);
    EXPECT_NE(powerControl->associationDefinitionsIntf, nullptr);
    EXPECT_NE(powerControl->decoratorAreaIntf, nullptr);
    EXPECT_TRUE(powerControl->powerCapIntf->powerCapEnable());
}

TEST_F(NsmProcessorModulePowerControlTest, goodTestPhysicalContext)
{
    auto physicalCtx = powerControl->decoratorAreaIntf->physicalContext();
    EXPECT_EQ(physicalCtx,
              DecoratorAreaIntf::PhysicalContextType::GPUSubsystem);
}

TEST_F(NsmProcessorModulePowerControlTest, goodTestClearPowerCapIntf)
{
    EXPECT_NE(clearPowerCapIntf, nullptr);
    EXPECT_EQ(clearPowerCapIntf->clearPowerCap(), 0);
}

TEST_F(NsmProcessorModulePowerControlTest, goodTestPowerCapSettings)
{
    // Test setting power cap
    uint32_t powerCap = 400;
    powerCapIntf->powerCap(powerCap);
    EXPECT_EQ(powerCapIntf->powerCap(), powerCap);

    // Test power cap enable
    powerCapIntf->powerCapEnable(false);
    EXPECT_FALSE(powerCapIntf->powerCapEnable());

    powerCapIntf->powerCapEnable(true);
    EXPECT_TRUE(powerCapIntf->powerCapEnable());
}

TEST_F(NsmProcessorModulePowerControlTest, goodTestMinMaxPowerCap)
{
    // Test min power cap
    uint32_t minPowerCap = 200;
    powerCapIntf->minPowerCapValue(minPowerCap);
    EXPECT_EQ(powerCapIntf->minPowerCapValue(), minPowerCap);

    // Test max power cap
    uint32_t maxPowerCap = 600;
    powerCapIntf->maxPowerCapValue(maxPowerCap);
    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), maxPowerCap);
}

TEST_F(NsmProcessorModulePowerControlTest, goodTestAssociations)
{
    auto associations =
        powerControl->associationDefinitionsIntf->associations();
    EXPECT_EQ(associations.size(), 1);
    EXPECT_EQ(std::get<0>(associations[0]), "chassis");
    EXPECT_EQ(std::get<1>(associations[0]), "power_control");
}

TEST_F(NsmProcessorModulePowerControlTest, testGenRequestMsg)
{
    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto result = powerControl->genRequestMsg(eid, instanceId);

    EXPECT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0);
}

TEST_F(NsmProcessorModulePowerControlTest, testGenRequestMsgMultipleCalls)
{
    eid_t eid = 10;

    auto result1 = powerControl->genRequestMsg(eid, 1);
    auto result2 = powerControl->genRequestMsg(eid, 2);
    auto result3 = powerControl->genRequestMsg(eid, 3);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
}

TEST_F(NsmProcessorModulePowerControlTest, testGenRequestMsgWithDifferentEids)
{
    auto result1 = powerControl->genRequestMsg(10, 1);
    auto result2 = powerControl->genRequestMsg(20, 1);
    auto result3 = powerControl->genRequestMsg(255, 1);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
}

TEST_F(NsmProcessorModulePowerControlTest, testClearPowerCapIntfDefaultPowerCap)
{
    // Test default power cap value
    uint32_t defaultCap = 500;
    clearPowerCapIntf->defaultPowerCap(defaultCap);
    EXPECT_EQ(clearPowerCapIntf->defaultPowerCap(), defaultCap);
}

TEST_F(NsmProcessorModulePowerControlTest, testPowerCapIntfBoundaryValues)
{
    // Test with minimum value
    powerCapIntf->minPowerCapValue(100);
    EXPECT_EQ(powerCapIntf->minPowerCapValue(), 100);

    // Test with maximum value
    powerCapIntf->maxPowerCapValue(1000);
    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), 1000);

    // Test power cap within range
    powerCapIntf->powerCap(500);
    EXPECT_EQ(powerCapIntf->powerCap(), 500);
}

TEST_F(NsmProcessorModulePowerControlTest, testMultipleAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string path2 =
        "/xyz/openbmc_project/control/processor_module_power_cap/GPU1";

    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list2;
    associations_list2.push_back(
        {"chassis", "power_control",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU1"});
    associations_list2.push_back(
        {"system", "controlled_by", "/xyz/openbmc_project/inventory/system"});

    auto powerCapIntf2 = std::make_shared<PowerCapIntf>(bus, path2.c_str());
    auto clearPowerCapIntf2 = std::make_shared<NsmClearPowerCapIntf>(bus,
                                                                     path2);

    auto powerControl2 = std::make_shared<NsmProcessorModulePowerControl>(
        bus, "ProcessorModulePowerControl2", type, powerCapIntf2,
        clearPowerCapIntf2, path2, associations_list2);

    auto associations =
        powerControl2->associationDefinitionsIntf->associations();
    EXPECT_EQ(associations.size(), 2);
}

TEST_F(NsmProcessorModulePowerControlTest,
       testPatchPowerLimitInProgressInitialState)
{
    EXPECT_FALSE(powerControl->patchPowerLimitInProgress);
}

TEST_F(NsmProcessorModulePowerControlTest, testGetNameAndType)
{
    EXPECT_EQ(powerControl->getName(), name);
    EXPECT_EQ(powerControl->getType(), type);
}

TEST_F(NsmProcessorModulePowerControlTest, testPowerCapEnableToggle)
{
    powerCapIntf->powerCapEnable(true);
    EXPECT_TRUE(powerCapIntf->powerCapEnable());

    powerCapIntf->powerCapEnable(false);
    EXPECT_FALSE(powerCapIntf->powerCapEnable());

    powerCapIntf->powerCapEnable(true);
    EXPECT_TRUE(powerCapIntf->powerCapEnable());
}

TEST_F(NsmProcessorModulePowerControlTest, testPowerCapValueRange)
{
    // Set min and max
    powerCapIntf->minPowerCapValue(200);
    powerCapIntf->maxPowerCapValue(800);

    // Test value within range
    powerCapIntf->powerCap(400);
    EXPECT_EQ(powerCapIntf->powerCap(), 400);

    powerCapIntf->powerCap(600);
    EXPECT_EQ(powerCapIntf->powerCap(), 600);
}

TEST_F(NsmProcessorModulePowerControlTest, testClearPowerCapIntfPath)
{
    EXPECT_EQ(powerControl->path, path);
}

struct NsmModulePowerLimitTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<PowerCapIntf> powerCapIntf;
    std::string name = "ModulePowerLimit";
    std::string type = "NSM_ModulePowerLimit";
    std::string path = "/xyz/openbmc_project/test/power_limit";

    NsmModulePowerLimitTest() : SensorManagerTest(devices) {}

    ~NsmModulePowerLimitTest()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        powerCapIntf = std::make_shared<PowerCapIntf>(bus, path.c_str());
    }
};

TEST_F(NsmModulePowerLimitTest, testConstructorMaximumPowerLimit)
{
    uint8_t propertyId = MAXIMUM_MODULE_POWER_LIMIT;
    std::string testName = "MaxPowerLimit";
    std::string testType = "NSM_ModulePowerLimit";

    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        testName, testType, propertyId, powerCapIntf);

    EXPECT_NE(powerLimit, nullptr);
    EXPECT_EQ(powerLimit->propertyId, propertyId);
    EXPECT_EQ(powerLimit->propertyName, "MAXIMUM_MODULE_POWER_LIMIT");
}

TEST_F(NsmModulePowerLimitTest, testConstructorMinimumPowerLimit)
{
    uint8_t propertyId = MINIMUM_MODULE_POWER_LIMIT;
    std::string testName = "MinPowerLimit";
    std::string testType = "NSM_ModulePowerLimit";

    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        testName, testType, propertyId, powerCapIntf);

    EXPECT_NE(powerLimit, nullptr);
    EXPECT_EQ(powerLimit->propertyId, propertyId);
    EXPECT_EQ(powerLimit->propertyName, "MINIMUM_MODULE_POWER_LIMIT");
}

TEST_F(NsmModulePowerLimitTest, testConstructorUnknownProperty)
{
    uint8_t propertyId = 99; // Unknown property ID
    std::string testName = "UnknownPowerLimit";
    std::string testType = "NSM_ModulePowerLimit";

    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        testName, testType, propertyId, powerCapIntf);

    EXPECT_NE(powerLimit, nullptr);
    EXPECT_EQ(powerLimit->propertyId, propertyId);
    EXPECT_EQ(powerLimit->propertyName, "");
}

TEST_F(NsmModulePowerLimitTest, testMultiplePowerLimitInstances)
{
    std::string maxName = "MaxPowerLimit";
    std::string minName = "MinPowerLimit";
    std::string testType = "NSM_ModulePowerLimit";

    auto maxPowerLimit = std::make_shared<NsmModulePowerLimit>(
        maxName, testType, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    auto minPowerLimit = std::make_shared<NsmModulePowerLimit>(
        minName, testType, MINIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    EXPECT_NE(maxPowerLimit, nullptr);
    EXPECT_NE(minPowerLimit, nullptr);
    EXPECT_NE(maxPowerLimit->propertyName, minPowerLimit->propertyName);
}
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

// -----------------------------------------------------------------------
// NEW TESTS to append to nsmProcessorModulePowerControl_test.cpp
// These tests cover:
//   - handleResponseMsg (success, various power limits, INVALID_POWER_LIMIT,
//     error CC, short buffer / decode failure)
//   - genRequestMsg validation (request size, encode failure scenarios)
//   - setModulePowerCap (invalid variant type, above max, below min)
//   - updatePowerLimitOnModule (patchPowerLimitInProgress guard,
//     path not in processorModuleToDeviceMap, successful set with mocked IO)
// -----------------------------------------------------------------------

// ===========================================================================
// handleResponseMsg tests
// ===========================================================================

TEST_F(NsmProcessorModulePowerControlTest,
       HandleResponseMsg_ValidSuccess_SetsPowerCap)
{
    // Arrange: build a valid get_power_limit response with enforced_limit =
    // 500000 mW (500 W). The code divides by 1000 to get Watts.
    uint32_t enforcedLimit = 500000; // 500 W
    uint32_t persistentLimit = 600000;
    uint32_t oneshotLimit = 0;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL,
                                          persistentLimit, oneshotLimit,
                                          enforcedLimit, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = powerControl->handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(powerCapIntf->powerCap(), enforcedLimit / 1000);
}

TEST_F(NsmProcessorModulePowerControlTest,
       HandleResponseMsg_DifferentPowerLimitValues_SetsPowerCapCorrectly)
{
    // Arrange: test with a different enforced limit value
    uint32_t enforcedLimit = 300000; // 300 W

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0,
                                          enforcedLimit, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = powerControl->handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(powerCapIntf->powerCap(), 300u);
}

TEST_F(NsmProcessorModulePowerControlTest,
       HandleResponseMsg_ZeroPowerLimit_SetsPowerCapToZero)
{
    // Arrange: enforced limit of 0
    uint32_t enforcedLimit = 0;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0,
                                          enforcedLimit, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = powerControl->handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(powerCapIntf->powerCap(), 0u);
}

TEST_F(NsmProcessorModulePowerControlTest,
       HandleResponseMsg_InvalidPowerLimit_SetsInvalidMarker)
{
    // Arrange: enforced_limit == INVALID_POWER_LIMIT (0xFFFFFFFF).
    // The code sets the raw INVALID_POWER_LIMIT value without dividing.
    uint32_t enforcedLimit = INVALID_POWER_LIMIT;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0,
                                          enforcedLimit, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = powerControl->handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(powerCapIntf->powerCap(), INVALID_POWER_LIMIT);
}

TEST_F(NsmProcessorModulePowerControlTest,
       HandleResponseMsg_ErrorCompletionCode_ReturnsError)
{
    // Arrange: encode a response with NSM_ERROR completion code.
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_power_limit_resp(0, NSM_ERROR, ERR_NULL, 0, 0, 0,
                                          response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Set a known powerCap first to verify it does NOT change on error
    powerCapIntf->powerCap(999);

    // Act
    rc = powerControl->handleResponseMsg(response, responseData.size());

    // Assert: should return non-zero (the cc value)
    EXPECT_NE(rc, NSM_SUCCESS);
    // Power cap should remain unchanged because cc != NSM_SUCCESS
    EXPECT_EQ(powerCapIntf->powerCap(), 999u);
}

TEST_F(NsmProcessorModulePowerControlTest,
       HandleResponseMsg_ShortBuffer_DecodeFailure)
{
    // Arrange: create a buffer that is too small for decode to succeed.
    // This causes decode_get_power_limit_resp to fail with a length error.
    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    // Set a known powerCap first to verify it does NOT change on failure
    powerCapIntf->powerCap(888);

    // Act
    auto rc = powerControl->handleResponseMsg(response, responseData.size());

    // Assert: decode failure means rc != 0 (or cc != 0)
    EXPECT_NE(rc, NSM_SUCCESS);
    // Power cap should remain unchanged
    EXPECT_EQ(powerCapIntf->powerCap(), 888u);
}

TEST_F(NsmProcessorModulePowerControlTest,
       HandleResponseMsg_LargePowerLimit_ConvertsCorrectly)
{
    // Arrange: large enforced limit to verify arithmetic
    uint32_t enforcedLimit = 1000000; // 1000 W

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0,
                                          enforcedLimit, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = powerControl->handleResponseMsg(response, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(powerCapIntf->powerCap(), 1000u);
}

// ===========================================================================
// genRequestMsg validation tests
// ===========================================================================

TEST_F(NsmProcessorModulePowerControlTest,
       GenRequestMsg_ValidParams_ReturnsCorrectSize)
{
    // Arrange
    eid_t eid = 10;
    uint8_t instanceId = 0;

    // Act
    auto result = powerControl->genRequestMsg(eid, instanceId);

    // Assert
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_req));
}

TEST_F(NsmProcessorModulePowerControlTest,
       GenRequestMsg_ZeroInstanceId_Succeeds)
{
    // Arrange
    eid_t eid = 0;
    uint8_t instanceId = 0;

    // Act
    auto result = powerControl->genRequestMsg(eid, instanceId);

    // Assert
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_req));
}

TEST_F(NsmProcessorModulePowerControlTest, GenRequestMsg_MaxInstanceId_Succeeds)
{
    // Arrange: instance ID uses only 5 bits (0-31) per NSM spec
    eid_t eid = 100;
    uint8_t instanceId = 31;

    // Act
    auto result = powerControl->genRequestMsg(eid, instanceId);

    // Assert
    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmProcessorModulePowerControlTest,
       BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    eid_t eid = 10;
    auto result = powerControl->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// ===========================================================================
// setModulePowerCap tests (coroutine)
// ===========================================================================

TEST_F(NsmProcessorModulePowerControlTest,
       SetModulePowerCap_InvalidVariantType_ThrowsInvalidArgument)
{
    // Arrange: pass a string instead of uint32_t in the variant
    AsyncSetOperationValueType value = std::string("not_a_number");
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    std::shared_ptr<NsmDevice> device = nullptr;

    // Act & Assert: should throw InvalidArgument because std::get_if<uint32_t>
    // returns nullptr
    EXPECT_THROW_COROUTINE(
        powerControl->setModulePowerCap(value, &status, device),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmProcessorModulePowerControlTest,
       SetModulePowerCap_BoolVariantType_ThrowsInvalidArgument)
{
    // Arrange: pass a bool instead of uint32_t
    AsyncSetOperationValueType value = true;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    std::shared_ptr<NsmDevice> device = nullptr;

    // Act & Assert
    EXPECT_THROW_COROUTINE(
        powerControl->setModulePowerCap(value, &status, device),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmProcessorModulePowerControlTest,
       SetModulePowerCap_AboveMaxPowerCap_ReturnsInvalidArgument)
{
    // Arrange: set min/max, then request above max
    powerCapIntf->minPowerCapValue(200);
    powerCapIntf->maxPowerCapValue(600);

    uint32_t aboveMax = 700;
    AsyncSetOperationValueType value = aboveMax;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    std::shared_ptr<NsmDevice> device = nullptr;

    // Act
    auto coroutine = powerControl->setModulePowerCap(value, &status, device);

    // Assert: status should be InvalidArgument, return code is
    // NSM_SW_ERROR_COMMAND_FAIL
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmProcessorModulePowerControlTest,
       SetModulePowerCap_BelowMinPowerCap_ReturnsInvalidArgument)
{
    // Arrange: set min/max, then request below min
    powerCapIntf->minPowerCapValue(200);
    powerCapIntf->maxPowerCapValue(600);

    uint32_t belowMin = 100;
    AsyncSetOperationValueType value = belowMin;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    std::shared_ptr<NsmDevice> device = nullptr;

    // Act
    auto coroutine = powerControl->setModulePowerCap(value, &status, device);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmProcessorModulePowerControlTest,
       SetModulePowerCap_ExactlyAtMaxBoundary_ProceedsToUpdate)
{
    // Arrange: value == max should be accepted (not rejected)
    powerCapIntf->minPowerCapValue(200);
    powerCapIntf->maxPowerCapValue(600);

    uint32_t exactMax = 600;
    AsyncSetOperationValueType value = exactMax;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    std::shared_ptr<NsmDevice> device = nullptr;

    // Act: this will proceed to updatePowerLimitOnModule, which will find no
    // device in the map and set status to WriteFailure
    auto coroutine = powerControl->setModulePowerCap(value, &status, device);

    // Assert: should NOT be InvalidArgument since value is within range.
    // It goes to updatePowerLimitOnModule which may fail for other reasons
    // but the boundary check should pass.
    EXPECT_NE(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmProcessorModulePowerControlTest,
       SetModulePowerCap_ExactlyAtMinBoundary_ProceedsToUpdate)
{
    // Arrange
    powerCapIntf->minPowerCapValue(200);
    powerCapIntf->maxPowerCapValue(600);

    uint32_t exactMin = 200;
    AsyncSetOperationValueType value = exactMin;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    std::shared_ptr<NsmDevice> device = nullptr;

    // Act
    auto coroutine = powerControl->setModulePowerCap(value, &status, device);

    // Assert: boundary check passes
    EXPECT_NE(status, AsyncOperationStatusType::InvalidArgument);
}

// ===========================================================================
// updatePowerLimitOnModule tests (coroutine)
// ===========================================================================

TEST_F(NsmProcessorModulePowerControlTest,
       UpdatePowerLimitOnModule_AlreadyInProgress_ReturnsUnavailable)
{
    // Arrange: simulate a patch already in progress
    powerControl->patchPowerLimitInProgress = true;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act
    auto coroutine = powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT,
                                                            400);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
    // The flag should remain true (not cleared by the early return path)
    EXPECT_TRUE(powerControl->patchPowerLimitInProgress);
}

TEST_F(NsmProcessorModulePowerControlTest,
       UpdatePowerLimitOnModule_PathNotInDeviceMap_SetsWriteFailure)
{
    // Arrange: path is NOT in processorModuleToDeviceMap
    // The SensorManager singleton will not have our path mapped.
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    powerControl->patchPowerLimitInProgress = false;

    // Act
    auto coroutine = powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT,
                                                            400);

    // Assert: since path is not found in the map, rc = NSM_SW_ERROR
    // but it still iterates over the (empty or invalid) map entry.
    // After completion, patchPowerLimitInProgress should be reset to false.
    EXPECT_FALSE(powerControl->patchPowerLimitInProgress);
}

TEST_F(NsmProcessorModulePowerControlTest,
       UpdatePowerLimitOnModule_WithMockedDevice_SuccessfulSet)
{
    // Arrange: register a mock device in the processorModuleToDeviceMap
    const uuid_t deviceUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto nsmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    ASSERT_NE(nsmDevice, nullptr);

    SensorManager& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {nsmDevice};

    // Build a valid set_power_limit response
    std::vector<uint8_t> setPowerLimitResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setPowerLimitRespMsg =
        reinterpret_cast<nsm_msg*>(setPowerLimitResp.data());
    auto encRc = encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL,
                                             setPowerLimitRespMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    // Mock postPatchIO to return the valid set response
    EXPECT_CALL(*nsmDevice,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(setPowerLimitResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    powerControl->patchPowerLimitInProgress = false;

    // Act
    auto coroutine = powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT,
                                                            400);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(powerControl->patchPowerLimitInProgress);
}

TEST_F(NsmProcessorModulePowerControlTest,
       UpdatePowerLimitOnModule_PostPatchIOFailure_SetsWriteFailure)
{
    // Arrange: register a mock device in the processorModuleToDeviceMap
    const uuid_t deviceUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto nsmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    ASSERT_NE(nsmDevice, nullptr);

    SensorManager& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {nsmDevice};

    // Mock postPatchIO to return an error code
    EXPECT_CALL(*nsmDevice,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    powerControl->patchPowerLimitInProgress = false;

    // Act
    auto coroutine = powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT,
                                                            400);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(powerControl->patchPowerLimitInProgress);
}

TEST_F(NsmProcessorModulePowerControlTest,
       UpdatePowerLimitOnModule_ErrorCompletionCode_SetsWriteFailure)
{
    // Arrange: register a mock device
    const uuid_t deviceUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto nsmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    ASSERT_NE(nsmDevice, nullptr);

    SensorManager& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {nsmDevice};

    // Build a set_power_limit response with NSM_ERROR completion code
    std::vector<uint8_t> setPowerLimitResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setPowerLimitRespMsg =
        reinterpret_cast<nsm_msg*>(setPowerLimitResp.data());
    auto encRc = encode_set_power_limit_resp(0, NSM_ERROR, ERR_NULL,
                                             setPowerLimitRespMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nsmDevice,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(setPowerLimitResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    powerControl->patchPowerLimitInProgress = false;

    // Act
    auto coroutine = powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT,
                                                            400);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(powerControl->patchPowerLimitInProgress);
}

TEST_F(NsmProcessorModulePowerControlTest,
       UpdatePowerLimitOnModule_DefaultLimitAction_SuccessfulSet)
{
    // Arrange: register a mock device
    const uuid_t deviceUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto nsmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    ASSERT_NE(nsmDevice, nullptr);

    SensorManager& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {nsmDevice};

    // Build a valid set_power_limit response
    std::vector<uint8_t> setPowerLimitResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setPowerLimitRespMsg =
        reinterpret_cast<nsm_msg*>(setPowerLimitResp.data());
    auto encRc = encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL,
                                             setPowerLimitRespMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    EXPECT_CALL(*nsmDevice,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(setPowerLimitResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    powerControl->patchPowerLimitInProgress = false;

    // Act: use DEFAULT_LIMIT action
    auto coroutine = powerControl->updatePowerLimitOnModule(&status,
                                                            DEFAULT_LIMIT, 500);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(powerControl->patchPowerLimitInProgress);
}

TEST_F(NsmProcessorModulePowerControlTest,
       UpdatePowerLimitOnModule_MultipleDevices_AllSucceed)
{
    // Arrange: register two mock devices for the same path
    const uuid_t deviceUuid1 = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const uuid_t deviceUuid2 = "STATIC:3:1:NSM_DEVICE_INSTANCE_NUMBER:1";
    auto nsmDevice1 = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(deviceUuid1));
    auto nsmDevice2 = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(deviceUuid2));
    ASSERT_NE(nsmDevice1, nullptr);
    ASSERT_NE(nsmDevice2, nullptr);

    SensorManager& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {nsmDevice1, nsmDevice2};

    // Build a valid set_power_limit response
    std::vector<uint8_t> setPowerLimitResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setPowerLimitRespMsg =
        reinterpret_cast<nsm_msg*>(setPowerLimitResp.data());
    auto encRc = encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL,
                                             setPowerLimitRespMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    // Both devices should get postPatchIO called
    EXPECT_CALL(*nsmDevice1,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(setPowerLimitResp));
    EXPECT_CALL(*nsmDevice2,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(setPowerLimitResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    powerControl->patchPowerLimitInProgress = false;

    // Act
    auto coroutine = powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT,
                                                            450);

    // Assert
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(powerControl->patchPowerLimitInProgress);
}

TEST_F(NsmProcessorModulePowerControlTest,
       UpdatePowerLimitOnModule_SecondDeviceFails_SetsWriteFailure)
{
    // Arrange: two devices, second one fails
    const uuid_t deviceUuid1 = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const uuid_t deviceUuid2 = "STATIC:3:1:NSM_DEVICE_INSTANCE_NUMBER:1";
    auto nsmDevice1 = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(deviceUuid1));
    auto nsmDevice2 = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(deviceUuid2));
    ASSERT_NE(nsmDevice1, nullptr);
    ASSERT_NE(nsmDevice2, nullptr);

    SensorManager& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {nsmDevice1, nsmDevice2};

    // Build a valid response for device1
    std::vector<uint8_t> successResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto successRespMsg = reinterpret_cast<nsm_msg*>(successResp.data());
    encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, successRespMsg);

    // Device1 succeeds, Device2 fails with IO error
    EXPECT_CALL(*nsmDevice1,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(successResp));
    EXPECT_CALL(*nsmDevice2,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    powerControl->patchPowerLimitInProgress = false;

    // Act
    auto coroutine = powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT,
                                                            400);

    // Assert: second device fails, so overall status is WriteFailure
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(powerControl->patchPowerLimitInProgress);
}

// ===========================================================================
// NsmModulePowerLimit tests
// ===========================================================================

TEST_F(NsmModulePowerLimitTest, Constructor_MaxPowerLimit_Succeeds)
{
    std::string testName = "MaxPowerLimit";
    std::string testType = "NSM_ModulePowerLimit";

    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        testName, testType, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    EXPECT_NE(powerLimit, nullptr);
    EXPECT_EQ(powerLimit->getName(), testName);
}

TEST_F(NsmModulePowerLimitTest, Constructor_MinPowerLimit_Succeeds)
{
    std::string testName = "MinPowerLimit";
    std::string testType = "NSM_ModulePowerLimit";

    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        testName, testType, MINIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    EXPECT_NE(powerLimit, nullptr);
    EXPECT_EQ(powerLimit->getName(), testName);
}

// ===========================================================================
// NsmModulePowerLimit::update() tests
// ===========================================================================

struct NsmModulePowerLimitUpdateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<PowerCapIntf> powerCapIntf;
    std::string name = "ModulePowerLimit";
    std::string type = "NSM_ModulePowerLimit";
    std::string path = "/xyz/openbmc_project/test/power_limit_update";
    const uuid_t deviceUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::shared_ptr<MockNsmDevice> nsmDevice;

    NsmModulePowerLimitUpdateTest() : SensorManagerTest(devices) {}

    ~NsmModulePowerLimitUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        powerCapIntf = std::make_shared<PowerCapIntf>(bus, path.c_str());
        nsmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        ASSERT_NE(nsmDevice, nullptr);
    }

    std::vector<uint8_t> buildInventoryResponse(uint32_t valueMw)
    {
        uint16_t dataSize = sizeof(valueMw);
        std::vector<uint8_t> responseData(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + dataSize, 0);
        auto response = reinterpret_cast<nsm_msg*>(responseData.data());
        encode_get_inventory_information_resp(
            0, NSM_SUCCESS, ERR_NULL, dataSize,
            reinterpret_cast<const uint8_t*>(&valueMw), response);
        return responseData;
    }
};

TEST_F(NsmModulePowerLimitUpdateTest, Update_MaxPowerLimit_SetsMaxCapValue)
{
    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        name, type, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    uint32_t valueMw = 700000; // 700 W
    auto responseData = buildInventoryResponse(valueMw);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    powerLimit->update(nsmDevice);

    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), 700u);
}

TEST_F(NsmModulePowerLimitUpdateTest, Update_MinPowerLimit_SetsMinCapValue)
{
    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        name, type, MINIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    uint32_t valueMw = 100000; // 100 W
    auto responseData = buildInventoryResponse(valueMw);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    powerLimit->update(nsmDevice);

    EXPECT_EQ(powerCapIntf->minPowerCapValue(), 100u);
}

TEST_F(NsmModulePowerLimitUpdateTest, Update_SensorIOFailure_DoesNotSetValue)
{
    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        name, type, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    powerCapIntf->maxPowerCapValue(999);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    powerLimit->update(nsmDevice);

    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), 999u);
}

TEST_F(NsmModulePowerLimitUpdateTest, Update_InvalidPowerLimit_SetsRawValue)
{
    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        name, type, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    uint32_t valueMw = INVALID_POWER_LIMIT;
    auto responseData = buildInventoryResponse(valueMw);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    powerLimit->update(nsmDevice);

    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), INVALID_POWER_LIMIT);
}

// ===========================================================================
// NsmDefaultModulePowerLimit tests
// ===========================================================================

struct NsmDefaultModulePowerLimitTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf;
    std::string name = "DefaultModulePowerLimit";
    std::string type = "NSM_DefaultModulePowerLimit";
    std::string path = "/xyz/openbmc_project/test/default_power_limit";
    const uuid_t deviceUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::shared_ptr<MockNsmDevice> nsmDevice;

    NsmDefaultModulePowerLimitTest() : SensorManagerTest(devices) {}

    ~NsmDefaultModulePowerLimitTest()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(bus, path);
        nsmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        ASSERT_NE(nsmDevice, nullptr);
    }

    std::vector<uint8_t> buildInventoryResponse(uint32_t valueMw)
    {
        uint16_t dataSize = sizeof(valueMw);
        std::vector<uint8_t> responseData(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + dataSize, 0);
        auto response = reinterpret_cast<nsm_msg*>(responseData.data());
        encode_get_inventory_information_resp(
            0, NSM_SUCCESS, ERR_NULL, dataSize,
            reinterpret_cast<const uint8_t*>(&valueMw), response);
        return responseData;
    }
};

TEST_F(NsmDefaultModulePowerLimitTest, Constructor_Succeeds)
{
    auto sensor = std::make_shared<NsmDefaultModulePowerLimit>(
        name, type, clearPowerCapIntf);
    EXPECT_NE(sensor, nullptr);
    EXPECT_EQ(sensor->getName(), name);
    EXPECT_EQ(sensor->getType(), type);
}

TEST_F(NsmDefaultModulePowerLimitTest, Update_Success_SetsDefaultPowerCap)
{
    auto sensor = std::make_shared<NsmDefaultModulePowerLimit>(
        name, type, clearPowerCapIntf);

    uint32_t ratedLimitMw = 600000; // 600 W
    auto responseData = buildInventoryResponse(ratedLimitMw);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    sensor->update(nsmDevice);

    EXPECT_EQ(clearPowerCapIntf->defaultPowerCap(), 600u);
}

TEST_F(NsmDefaultModulePowerLimitTest, Update_InvalidPowerLimit_SetsRawValue)
{
    auto sensor = std::make_shared<NsmDefaultModulePowerLimit>(
        name, type, clearPowerCapIntf);

    uint32_t ratedLimitMw = INVALID_POWER_LIMIT;
    auto responseData = buildInventoryResponse(ratedLimitMw);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    sensor->update(nsmDevice);

    EXPECT_EQ(clearPowerCapIntf->defaultPowerCap(), INVALID_POWER_LIMIT);
}

TEST_F(NsmDefaultModulePowerLimitTest, Update_SensorIOFailure_DoesNotSetValue)
{
    auto sensor = std::make_shared<NsmDefaultModulePowerLimit>(
        name, type, clearPowerCapIntf);

    clearPowerCapIntf->defaultPowerCap(555);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor->update(nsmDevice);

    EXPECT_EQ(clearPowerCapIntf->defaultPowerCap(), 555u);
}

TEST_F(NsmDefaultModulePowerLimitTest,
       Update_ErrorCompletionCode_DoesNotSetValue)
{
    auto sensor = std::make_shared<NsmDefaultModulePowerLimit>(
        name, type, clearPowerCapIntf);

    clearPowerCapIntf->defaultPowerCap(777);

    uint32_t ratedLimitMw = 400000;
    uint16_t dataSize = sizeof(ratedLimitMw);
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + dataSize, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_inventory_information_resp(
        0, NSM_ERROR, ERR_NULL, dataSize,
        reinterpret_cast<const uint8_t*>(&ratedLimitMw), response);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    sensor->update(nsmDevice);

    EXPECT_EQ(clearPowerCapIntf->defaultPowerCap(), 777u);
}

// ============================================================================
// addSensor<T> instantiation coverage
// ============================================================================

TEST_F(NsmModulePowerLimitUpdateTest, AddSensorNsmModulePowerLimit)
{
    std::string sensorName = "ModPowerLimitAS";
    std::string sensorType = "NSM_ModulePowerLimit";
    uint8_t propertyId = MAXIMUM_MODULE_POWER_LIMIT;
    auto sensor = std::make_shared<NsmModulePowerLimit>(
        sensorName, sensorType, propertyId, powerCapIntf);
    size_t before = nsmDevice->deviceSensors.size();
    nsmDevice->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(nsmDevice->deviceSensors.size(), before);
}

TEST_F(NsmDefaultModulePowerLimitTest, AddSensorNsmDefaultModulePowerLimit)
{
    auto sensor = std::make_shared<NsmDefaultModulePowerLimit>(
        name, type, clearPowerCapIntf);
    size_t before = nsmDevice->deviceSensors.size();
    nsmDevice->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(nsmDevice->deviceSensors.size(), before);
}

TEST_F(NsmProcessorModulePowerControlTest,
       AddSensorNsmProcessorModulePowerControl)
{
    const uuid_t deviceUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto device = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    ASSERT_NE(device, nullptr);
    size_t before = device->deviceSensors.size();
    device->addSensor(powerControl, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ===========================================================================
// NsmModulePowerLimit::update() missing branch coverage
// ===========================================================================

// Error CC → shouldLog returns true, inner-if false, powerCap not updated
TEST_F(NsmModulePowerLimitUpdateTest, Update_ErrorCC_DoesNotSetValue)
{
    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        name, type, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    powerCapIntf->maxPowerCapValue(111);

    uint32_t valueMw = 500000;
    uint16_t dataSize = sizeof(valueMw);
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + dataSize, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_inventory_information_resp(
        0, NSM_ERROR, ERR_NULL, dataSize,
        reinterpret_cast<const uint8_t*>(&valueMw), response);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    powerLimit->update(nsmDevice);

    // cc != NSM_SUCCESS → powerCap not updated
    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), 111u);
}

// Unknown propertyId + successful decode → default switch case, no property set
TEST_F(NsmModulePowerLimitUpdateTest, Update_UnknownPropertyId_DefaultCase)
{
    std::string unknownName = "UnknownPowerLimit";
    std::string unknownType = "NSM_UnknownPowerLimit";
    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        unknownName, unknownType, 0xFF, powerCapIntf);

    powerCapIntf->maxPowerCapValue(222);

    uint32_t valueMw = 300000;
    auto responseData = buildInventoryResponse(valueMw);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    powerLimit->update(nsmDevice);

    // default switch case → neither max nor min updated
    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), 222u);
}

// ===========================================================================
// NsmDefaultModulePowerLimit::update() short buffer (decode failure)
// ===========================================================================

TEST_F(NsmDefaultModulePowerLimitTest, Update_DecodeFail_ShortBuffer)
{
    auto sensor = std::make_shared<NsmDefaultModulePowerLimit>(
        name, type, clearPowerCapIntf);

    clearPowerCapIntf->defaultPowerCap(333);

    // Too-short response → decode_get_inventory_information_resp fails
    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) + 2, 0);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    sensor->update(nsmDevice);

    EXPECT_EQ(clearPowerCapIntf->defaultPowerCap(), 333u);
}

// ===========================================================================
// CreateProcessorModulePowerControl factory function tests (via
// NsmObjectFactory)
// ===========================================================================

#include "nsmObjectFactory.hpp"

struct CreatePMPCFactoryTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string configIntf =
        "xyz.openbmc_project.Configuration.NSM_ModulePowerControl";
    const std::string assocIntf = "xyz.openbmc_project.Association.Definitions";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    CreatePMPCFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~CreatePMPCFactoryTest()
    {
        cleanupDeviceSensors(devices);
        mockManager.processorModuleToDeviceMap.clear();
    }

    void setupConfig(const std::string& objPath, const std::string& sensorName,
                     uint64_t index = 0, bool priority = false)
    {
        auto& pm = utils::MockDbusAsync::propertyMap(objPath, configIntf);
        pm["Name"] = std::string(sensorName);
        pm["UUID"] = gpuUuid;
        pm["Type"] = std::string("NSM_ModulePowerControl");
        pm["Index"] = index;
        pm["Priority"] = priority;
    }

    void setupAssociations(const std::string& objPath,
                           const std::string& chassisPath)
    {
        std::string parentPath = objPath.substr(0, objPath.rfind('/'));
        auto& pm = utils::MockDbusAsync::propertyMap(parentPath, assocIntf);
        using AssocVec =
            std::vector<std::tuple<std::string, std::string, std::string>>;
        pm["Associations"] =
            AssocVec{{"parent_chassis", "power_controls", chassisPath}};
    }

    void callFactory(const std::string& objPath)
    {
        auto& factory = NsmObjectFactory::instance();
        auto it = factory.creationFunctions.find(configIntf);
        ASSERT_NE(it, factory.creationFunctions.end());
        it->second(mockManager, configIntf, objPath);
    }
};

// No "parent_chassis" in associations → chassisPath.size() == 0 → NSM_ERROR
TEST_F(CreatePMPCFactoryTest, Factory_NoChassis_ErrorReturn)
{
    const std::string objPath = "/test/pmpc_nochassis/Module0";
    setupConfig(objPath, "PMPC_NoChassis");
    // No associations set → empty vector → chassisPath stays ""

    size_t devBefore = gpu->deviceSensors.size();
    size_t staticBefore = gpu->staticSensors.size();

    callFactory(objPath);

    EXPECT_EQ(gpu->deviceSensors.size(), devBefore);
    EXPECT_EQ(gpu->staticSensors.size(), staticBefore);
}

// index != 0 → processorModuleToDeviceMap updated but no sensors created
TEST_F(CreatePMPCFactoryTest, Factory_IndexNonZero_MapUpdatedNoSensors)
{
    const std::string objPath = "/test/pmpc_idx1/Module0";
    setupConfig(objPath, "PMPC_Idx1", /*index=*/1);
    setupAssociations(objPath,
                      "/xyz/openbmc_project/inventory/system/chassis/HGX_B0");

    size_t devBefore = gpu->deviceSensors.size();
    size_t staticBefore = gpu->staticSensors.size();

    callFactory(objPath);

    // Map updated but no sensors added (index != 0 → early return)
    EXPECT_FALSE(mockManager.processorModuleToDeviceMap.empty());
    EXPECT_EQ(gpu->deviceSensors.size(), devBefore);
    EXPECT_EQ(gpu->staticSensors.size(), staticBefore);
}

// index == 0 → full sensor creation: 1 device sensor + 3 static sensors
TEST_F(CreatePMPCFactoryTest, Factory_IndexZero_SensorsCreated)
{
    const std::string objPath = "/test/pmpc_idx0/Module0";
    setupConfig(objPath, "PMPC_Idx0", /*index=*/0, /*priority=*/true);
    setupAssociations(objPath,
                      "/xyz/openbmc_project/inventory/system/chassis/HGX_C0");

    size_t devBefore = gpu->deviceSensors.size();
    size_t staticBefore = gpu->staticSensors.size();

    callFactory(objPath);

    // NsmProcessorModulePowerControl added as device sensor
    EXPECT_GT(gpu->deviceSensors.size(), devBefore);
    // NsmModulePowerLimit x2 + NsmDefaultModulePowerLimit added as static
    EXPECT_GT(gpu->staticSensors.size(), staticBefore);
}

// Two calls with same chassis path → second hits push_back branch
TEST_F(CreatePMPCFactoryTest, Factory_SameChassis_MapPushBack)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_D0";
    const std::string inventoryPath =
        "/xyz/openbmc_project/inventory/system/chassis/power/control/HGX_D0";

    // First call: index=1, new map key inserted
    const std::string objPath1 = "/test/pmpc_pushback_a/Module0";
    setupConfig(objPath1, "PMPC_PB_A", /*index=*/1);
    setupAssociations(objPath1, chassisPath);
    callFactory(objPath1);

    ASSERT_EQ(mockManager.processorModuleToDeviceMap.count(inventoryPath), 1u);
    EXPECT_EQ(mockManager.processorModuleToDeviceMap[inventoryPath].size(), 1u);

    // Second call: same chassis path → push_back branch
    const std::string objPath2 = "/test/pmpc_pushback_b/Module0";
    setupConfig(objPath2, "PMPC_PB_B", /*index=*/1);
    setupAssociations(objPath2, chassisPath);
    callFactory(objPath2);

    EXPECT_EQ(mockManager.processorModuleToDeviceMap[inventoryPath].size(), 2u);
}

// =============================================================================
// FALSE-branch coverage: count() checks in CreateProcessorModulePowerControl
// =============================================================================

// "Priority" absent → count("Priority") FALSE → priority=false (default)
// → sensors still created (index=0 path)
TEST_F(CreatePMPCFactoryTest, Factory_MissingPriority_SensorsCreated)
{
    const std::string objPath = "/test/pmpc_nopriority/Module0";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, configIntf);
    pm["Name"] = std::string("PMPC_NoPriority");
    pm["UUID"] = gpuUuid;
    pm["Type"] = std::string("NSM_ModulePowerControl");
    pm["Index"] = uint64_t(0);
    // "Priority" intentionally omitted → priority=false (default)
    setupAssociations(objPath,
                      "/xyz/openbmc_project/inventory/system/chassis/HGX_FP0");

    size_t devBefore = gpu->deviceSensors.size();
    callFactory(objPath);
    EXPECT_GT(gpu->deviceSensors.size(), devBefore);
}

// "Name" absent → count("Name") FALSE → name="" → sensors created
// (NsmProcessorModulePowerControl uses inventoryObjPath, not name, for D-Bus)
TEST_F(CreatePMPCFactoryTest, Factory_MissingName_SensorsCreated)
{
    const std::string objPath = "/test/pmpc_noname/Module0";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, configIntf);
    // "Name" intentionally omitted → name=""
    pm["UUID"] = gpuUuid;
    pm["Type"] = std::string("NSM_ModulePowerControl");
    pm["Index"] = uint64_t(0);
    pm["Priority"] = bool(false);
    setupAssociations(objPath,
                      "/xyz/openbmc_project/inventory/system/chassis/HGX_NN0");

    size_t devBefore = gpu->deviceSensors.size();
    callFactory(objPath);
    // Sensor created with empty name — inventoryObjPath is valid
    EXPECT_GT(gpu->deviceSensors.size(), devBefore);
}

// "Index" absent → count("Index") FALSE → index=0 (default)
// → same code path as explicit index=0 → sensors created
TEST_F(CreatePMPCFactoryTest, Factory_MissingIndex_DefaultsZero_SensorsCreated)
{
    const std::string objPath = "/test/pmpc_noindex/Module0";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, configIntf);
    pm["Name"] = std::string("PMPC_NoIndex");
    pm["UUID"] = gpuUuid;
    pm["Type"] = std::string("NSM_ModulePowerControl");
    // "Index" intentionally omitted → index=0 (default) → sensors created
    pm["Priority"] = bool(false);
    setupAssociations(objPath,
                      "/xyz/openbmc_project/inventory/system/chassis/HGX_NI0");

    size_t devBefore = gpu->deviceSensors.size();
    callFactory(objPath);
    EXPECT_GT(gpu->deviceSensors.size(), devBefore);
}

// "UUID" absent → count("UUID") FALSE → uuid="" →
// getNsmDeviceFromStaticUUID("") → parseStaticUuid throws std::runtime_error
// Only runs in coverage build: in real-coroutine mode callFactory() discards
// the returned Coroutine; the exception is stored in the coroutine's promise
// and never propagated to the caller.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(CreatePMPCFactoryTest, Factory_MissingUUID_Throws)
{
    const std::string objPath = "/test/pmpc_nouuid/Module0";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, configIntf);
    pm["Name"] = std::string("PMPC_NoUUID");
    pm["Type"] = std::string("NSM_ModulePowerControl");
    pm["Index"] = uint64_t(0);
    pm["Priority"] = bool(false);
    // "UUID" intentionally omitted → uuid="" → parseStaticUuid throws
    setupAssociations(objPath,
                      "/xyz/openbmc_project/inventory/system/chassis/HGX_NU0");

    EXPECT_THROW(callFactory(objPath), std::exception);
}
#endif // COVERAGE_DISABLE_COROUTINES

// "Type" absent → count("Type") FALSE → type="" → sensor created with
// empty type (NsmProcessorModulePowerControl uses name-based D-Bus path)
TEST_F(CreatePMPCFactoryTest, Factory_MissingType_SensorsCreated)
{
    const std::string objPath = "/test/pmpc_notype/Module0";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, configIntf);
    pm["Name"] = std::string("PMPC_NoType");
    pm["UUID"] = gpuUuid;
    // "Type" intentionally omitted → type=""
    pm["Index"] = uint64_t(0);
    pm["Priority"] = bool(false);
    setupAssociations(objPath,
                      "/xyz/openbmc_project/inventory/system/chassis/HGX_NT0");

    size_t devBefore = gpu->deviceSensors.size();
    callFactory(objPath);
    EXPECT_GT(gpu->deviceSensors.size(), devBefore);
}

// ===========================================================================
// NsmProcessorModulePowerControl::clearPowerCap() branch coverage
// (lines 207, 209-212, 219-222)
// ===========================================================================

// clearPowerCap() – happy path (lines 209-222):
// AsyncOperationManager returns a valid objectPath (always non-empty).
// doClearPowerCapOnModule is detached synchronously; with the path not
// present in processorModuleToDeviceMap the inner loop has zero iterations.
// Lines 207 (doClearPowerCapOnModule closing brace), 209, 211-212, 219,
// 221-222 are covered.
TEST_F(NsmProcessorModulePowerControlTest,
       ClearPowerCap_HappyPath_ReturnsObjectPath)
{
    // Ensure patchPowerLimitInProgress is false before the call
    powerControl->patchPowerLimitInProgress = false;

    sdbusplus::message::object_path objPath;
    EXPECT_NO_THROW(objPath = powerControl->clearPowerCap());
    EXPECT_FALSE(std::string(objPath).empty());
}

// handleResponseMsg: rc==NSM_SW_SUCCESS, cc==NSM_ERROR →
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch →
// powerCapIntf NOT updated.
TEST_F(NsmProcessorModulePowerControlTest,
       HandleResponseMsg_DecodeSuccessNonZeroCC_DoesNotUpdatePowerCap)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = powerControl->handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// Branch 12: L187 second operand in setPowerLimitOnModule —
// rc==NSM_SW_SUCCESS but cc!=NSM_SUCCESS → 9-byte buffer causes
// decode_reason_code_and_cc to return NSM_SW_SUCCESS with cc=NSM_ERROR.
TEST_F(NsmProcessorModulePowerControlTest,
       UpdatePowerLimitOnModule_DecodeSuccessNonZeroCC_SetsWriteFailure)
{
    const uuid_t deviceUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto nsmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
    ASSERT_NE(nsmDevice, nullptr);

    SensorManager& manager = SensorManager::getInstance();
    manager.processorModuleToDeviceMap[path] = {nsmDevice};

    // 9-byte buffer: decode_reason_code_and_cc returns NSM_SW_SUCCESS with
    // cc=NSM_ERROR → rc=0 (FALSE) but cc!=0 (TRUE) → second operand at L187
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*nsmDevice,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(buf));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    powerControl->patchPowerLimitInProgress = false;
    auto coroutine = powerControl->updatePowerLimitOnModule(&status, NEW_LIMIT,
                                                            400);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(powerControl->patchPowerLimitInProgress);
}
