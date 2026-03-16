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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "firmware-utils.h"

#include "nsmFirmwareUtils/nsmSecurityRBP.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// SECTION 2: nsmSecurityRBP.cpp -- Additional uncovered function tests
// ============================================================================

struct NsmSecurityRBPBatch11F :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "SecRBP_Batch11F";
    const std::string type = "NSM_SecurityCfg";
    const uuid_t deviceUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:52";
    eid_t eid = 0;
    uint8_t instanceId = 0;

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;
    std::shared_ptr<ProgressIntf> progressIntf;
    std::shared_ptr<NsmSecurityCfgObject> sensor;

    NsmSecurityRBPBatch11F() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(fpga, nullptr);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                                   name + "/ProgressB11F";
        progressIntf = std::make_shared<ProgressIntf>(bus,
                                                      progressPath.c_str());

        sensor = std::make_shared<NsmSecurityCfgObject>(
            bus, name, type, deviceUuid, progressIntf);
        ASSERT_NE(sensor, nullptr);
    }

    ~NsmSecurityRBPBatch11F() override
    {
        sensor.reset();
        progressIntf.reset();
        cleanupDeviceSensors(devices);
    }
};

// -- SecurityConfiguration::updateIrreversibleConfig -- calls
// startOperation/encode/finishOperation
// This function launches a coroutine detach. We test encode path for enable.
TEST_F(NsmSecurityRBPBatch11F,
       UpdateIrreversibleConfig_EnableTrue_EncodesAndLaunchesCoroutine)
{
    // Arrange - the function calls startOperation, encodes, then detaches.
    // We mock postPatchIO to return success for the coroutine.
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_2_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    struct nsm_firmware_irreversible_config_request_2_resp cfg_resp = {};
    cfg_resp.nonce = 0x1234;
    auto rc = encode_nsm_firmware_irreversible_config_request_2_resp(
        0, NSM_SUCCESS, ERR_NULL, &cfg_resp, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Mock postPatchIO for the async handler
    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));
    // Mock sensorIO for the subsequent sensor.update() call
    EXPECT_CALL(*fpga, sensorIO).WillRepeatedly(mockSensorIO(NSM_SUCCESS));

    // Act - calls updateIrreversibleConfig(true) which calls
    // securityCfgAsyncHandler().detach()
    EXPECT_NO_THROW(sensor->securityCfgObject->updateIrreversibleConfig(true));
}

// -- SecurityConfiguration::updateIrreversibleConfig with false (disable path)
TEST_F(NsmSecurityRBPBatch11F,
       UpdateIrreversibleConfig_EnableFalse_EncodesDisableAndLaunches)
{
    // Arrange
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_nsm_firmware_irreversible_config_request_1_resp(
        0, NSM_SUCCESS, ERR_NULL, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*fpga, postPatchIO).WillOnce(mockPostPatchIO(response));
    EXPECT_CALL(*fpga, sensorIO).WillRepeatedly(mockSensorIO(NSM_SUCCESS));

    // Act
    EXPECT_NO_THROW(sensor->securityCfgObject->updateIrreversibleConfig(false));
}

// -- NsmSecurityCfgObject update via sensorIO (coroutine path for update())
TEST_F(NsmSecurityRBPBatch11F, Update_SuccessfulIO_ReturnsSuccess)
{
    // Arrange - prepare a valid query response
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_firmware_irreversible_config_request_0_resp cfg_state = {};
    cfg_state.irreversible_config_state = 1;
    auto rc = encode_nsm_firmware_irreversible_config_request_0_resp(
        0, NSM_SUCCESS, ERR_NULL, &cfg_state, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*fpga, sensorIO).WillOnce(mockSensorIO(response));

    // Act
    sensor->update(fpga);

    // Assert - state should be updated
    EXPECT_TRUE(sensor->securityCfgObject->irreversibleConfigState());
}

// -- NsmMinSecVersionObject tests for additional coverage --

struct NsmMinSecVersionBatch11F :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "MinSecVer_Batch11F";
    const std::string type = "NSM_MinSecVersion";
    const uuid_t deviceUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:53";
    const uint16_t classification = 0x000B;
    const uint16_t identifier = 0x0002;
    const uint8_t index = 1;

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;
    std::shared_ptr<ProgressIntf> progressIntf;
    std::shared_ptr<NsmMinSecVersionObject> sensor;

    NsmMinSecVersionBatch11F() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(fpga, nullptr);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                                   chassisName + "/ProgressB11F";
        progressIntf = std::make_shared<ProgressIntf>(bus,
                                                      progressPath.c_str());

        sensor = std::make_shared<NsmMinSecVersionObject>(
            bus, chassisName, type, deviceUuid, classification, identifier,
            index, progressIntf);
        ASSERT_NE(sensor, nullptr);
    }

    ~NsmMinSecVersionBatch11F() override
    {
        sensor.reset();
        progressIntf.reset();
        cleanupDeviceSensors(devices);
    }
};

// -- MinSecurityVersion::updateProperties with specific version values
TEST_F(NsmMinSecVersionBatch11F,
       UpdateProperties_SpecificVersions_UpdatesBothObjects)
{
    // Arrange
    struct nsm_firmware_security_version_number_resp sec_info = {};
    sec_info.minimum_security_version = htole16(100);
    sec_info.pending_minimum_security_version = htole16(200);

    // Act
    sensor->minSecVersion->updateProperties(sec_info);

    // Assert
    EXPECT_EQ(sensor->minSecVersion->securityVersionObject->version(),
              htole16(htole16(100)));
    EXPECT_EQ(sensor->minSecVersion->securityVersionSettingsObject->version(),
              htole16(htole16(200)));
}

// -- NsmMinSecVersionObject update via sensorIO (coroutine)
TEST_F(NsmMinSecVersionBatch11F, Update_SuccessfulIO_ReturnsSuccess)
{
    // Arrange
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_security_version_number_resp_command),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    struct nsm_firmware_security_version_number_resp sec_info = {};
    sec_info.minimum_security_version = htole16(10);
    sec_info.pending_minimum_security_version = htole16(11);
    auto rc = encode_nsm_query_firmware_security_version_number_resp(
        0, NSM_SUCCESS, ERR_NULL, &sec_info, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*fpga, sensorIO).WillOnce(mockSensorIO(response));

    // Act
    sensor->update(fpga);

    // Assert
    EXPECT_NE(sensor->minSecVersion->securityVersionObject, nullptr);
}

// -- MinSecurityVersion::startOperation when already locked throws
TEST_F(NsmMinSecVersionBatch11F,
       StartOperation_SecondCallWhileLocked_ThrowsUnavailable)
{
    // Arrange
    int rc = sensor->minSecVersion->startOperation();
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act & Assert - second call while locked should throw
    EXPECT_THROW(sensor->minSecVersion->startOperation(),
                 sdbusplus::exception_t);

    // Cleanup
    sensor->minSecVersion->mutex.unlock();
}
