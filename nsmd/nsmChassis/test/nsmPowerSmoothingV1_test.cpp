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
//   1. nsmPowerSmoothingPowerProfileIntf.hpp (V1) - 8 uncovered functions
//   2. nsmResetIface.hpp - 8 uncovered functions (coroutines)
//   3. nsmClearPowerCapIface.hpp - 6 uncovered functions (coroutines)
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
// Inline copy of NsmClearPowerCapAsyncIntf from nsmClearPowerCapIface.hpp.
// We cannot include nsmClearPowerCapIface.hpp directly because it defines
// class nsm::NsmClearPowerCapIntf which conflicts with the same-named class
// in nsmProcessorModulePowerControl.hpp.
// =============================================================================
namespace nsm
{
class NsmClearPowerCapAsyncIntf :
    public ClearPowerCapAsyncIntf,
    public StateChangeLogger
{
  public:
    NsmClearPowerCapAsyncIntf(
        sdbusplus::bus_t& bus, const char* path,
        std::shared_ptr<NsmDevice> device,
        std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
        std::shared_ptr<ClearPowerCapIntf> clearPowerCapIntf) :
        ClearPowerCapAsyncIntf(bus, path), device(device),
        powerCapIntf(powerCapIntf), clearPowerCapIntf(clearPowerCapIntf)
    {}

    requester::Coroutine getPowerCapFromDevice()
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_power_limit_req));
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
        auto rc = encode_get_device_power_limit_req(0, requestMsg);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "getPowerCapFromDevice encode_get_device_power_limit_req failed.eid = {EID} rc ={RC}",
                "EID", eid, "RC", rc);
            // coverity[missing_return]
            co_return rc;
        }
        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        rc = co_await device->postPatchIO(eid, request, responseMsg,
                                          responseLen);
        if (rc)
        {
            lg2::error("postPatchIO failed.eid={EID} rc={RC}", "EID", eid, "RC",
                       rc);
            // coverity[missing_return]
            co_return rc;
        }
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        uint16_t dataSize = 0;
        uint32_t requestedPersistentLimit = 0;
        uint32_t requestedOneshotLimit = 0;
        uint32_t enforcedLimit = 0;

        rc = decode_get_power_limit_resp(
            responseMsg.get(), responseLen, &cc, &dataSize, &reasonCode,
            &requestedPersistentLimit, &requestedOneshotLimit, &enforcedLimit);

        if (shouldLog("decode_get_power_limit_resp", reasonCode, cc, rc))
        {
            LG2_ERROR(
                "decode_get_power_limit_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
                "REASONCODE", utils::nsmReasonCodeToString(reasonCode), "CC",
                utils::nsmCompletionCodeToString(cc), "RC",
                utils::nsmSwCodeToString(rc));
        }
        if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
        {
            uint32_t reading = (enforcedLimit == INVALID_POWER_LIMIT)
                                   ? INVALID_POWER_LIMIT
                                   : enforcedLimit / 1000;
            powerCapIntf->PowerCapIntf::powerCap(reading);
        }
        // coverity[missing_return]
        co_return cc ? cc : rc;
    }

    requester::Coroutine clearPowerCapOnDevice(AsyncOperationStatusType* status)
    {
        SensorManager& manager = SensorManager::getInstance();
        auto eid = manager.getEid(device);
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_limit_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_set_device_power_limit_req(
            0, DEFAULT_LIMIT, PERSISTENT,
            powerCapIntf->PowerCapIntf::defaultPowerCap(), requestMsg);

        if (rc)
        {
            lg2::error(
                "clearPowerCapOnDevice encode_set_device_power_limit_req failed. eid={EID}, rc={RC}",
                "EID", eid, "RC", rc);
            *status = AsyncOperationStatusType::WriteFailure;
            // coverity[missing_return]
            co_return rc;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        rc = co_await device->postPatchIO(eid, request, responseMsg,
                                          responseLen);
        if (rc)
        {
            lg2::error(
                "clearPowerCapOnDevice postPatchIO failed for while setting power limit for eid = {EID} rc = {RC}",
                "EID", eid, "RC", utils::nsmSwCodeToString(rc));
            *status = AsyncOperationStatusType::WriteFailure;
            // coverity[missing_return]
            co_return rc;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = ERR_NULL;
        uint16_t dataSize = 0;
        rc = decode_set_power_limit_resp(responseMsg.get(), responseLen, &cc,
                                         &dataSize, &reasonCode);

        LG2_ERROR_FLT(
            "decode_set_power_limit_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
        {
            co_await getPowerCapFromDevice();
            for (auto it = powerCapIntf->parents.begin();
                 it != powerCapIntf->parents.end();)
            {
                auto sensorIt = manager.objectPathToSensorMap.find(*it);
                if (sensorIt != manager.objectPathToSensorMap.end())
                {
                    auto sensor = sensorIt->second;
                    if (sensor)
                    {
                        powerCapIntf->sensorCache.emplace_back(
                            std::dynamic_pointer_cast<NsmPowerControl>(sensor));
                        it = powerCapIntf->parents.erase(it);
                        continue;
                    }
                }
                ++it;
            }

            for (const auto& sensor : powerCapIntf->sensorCache)
            {
                sensor->updatePowerCapValue(
                    powerCapIntf->name, powerCapIntf->PowerCapIntf::powerCap());
            }
        }
        else
        {
            *status = AsyncOperationStatusType::WriteFailure;
        }
        // coverity[missing_return]
        co_return cc ? cc : rc;
    }

    requester::Coroutine doClearPowerCapOnDevice(
        std::shared_ptr<AsyncStatusIntf> statusInterface)
    {
        AsyncOperationStatusType status{AsyncOperationStatusType::Success};

        auto rc = co_await clearPowerCapOnDevice(&status);

        statusInterface->status(status);
        // coverity[missing_return]
        co_return rc;
    }

    sdbusplus::object_path clearPowerCap() override
    {
        const auto [objectPath, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();

        if (objectPath.empty())
        {
            lg2::error(
                "ClearPowerCap failed. No available result Object to allocate for the Post request.");
            throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
        }

        doClearPowerCapOnDevice(statusInterface).detach();

        return objectPath;
    }

  private:
    std::shared_ptr<NsmDevice> device;
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf = nullptr;
    std::shared_ptr<ClearPowerCapIntf> clearPowerCapIntf = nullptr;
};
} // namespace nsm

// =============================================================================
// Test Fixture: PowerProfile V1
// =============================================================================

struct PowerProfileV1Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string parentPath = "/xyz/openbmc_project/inventory/test/gpu0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    PowerProfileV1Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~PowerProfileV1Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// =============================================================================
// OemPowerProfileIntf (V1) -- Constructor and getters
// =============================================================================

TEST_F(PowerProfileV1Test, Constructor_ProfileId0_CreatesCorrectPath)
{
    // Arrange & Act
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    // Assert
    EXPECT_NE(profileIntf, nullptr);
    EXPECT_EQ(profileIntf->getProfileId(), 0);
    EXPECT_EQ(profileIntf->getInventoryObjPath(),
              parentPath + "/profile/" + std::to_string(profileId));
}

TEST_F(PowerProfileV1Test, Constructor_ProfileId5_CreatesCorrectPath)
{
    // Arrange & Act
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 5;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    // Assert
    EXPECT_EQ(profileIntf->getProfileId(), 5);
    EXPECT_EQ(profileIntf->getInventoryObjPath(), parentPath + "/profile/5");
}

TEST_F(PowerProfileV1Test, GetInventoryObjPath_ReturnsCorrectPath)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 2;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    std::string expected = parentPath + "/profile/2";
    EXPECT_EQ(profileIntf->getInventoryObjPath(), expected);
}

TEST_F(PowerProfileV1Test, GetProfileId_ReturnsCorrectId)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 3;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    EXPECT_EQ(profileIntf->getProfileId(), 3);
}

// =============================================================================
// OemPowerProfileIntf (V1) -- resetParam
// =============================================================================

TEST_F(PowerProfileV1Test, ResetParam_InvalidPowerLimit_ReturnsTrue)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    // INVALID_POWER_LIMIT = 0xFFFFFFFF
    double invalidReading = static_cast<double>(INVALID_POWER_LIMIT);
    EXPECT_TRUE(profileIntf->resetParam(invalidReading));
}

TEST_F(PowerProfileV1Test, ResetParam_ValidValue_ReturnsFalse)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    EXPECT_FALSE(profileIntf->resetParam(500.0));
}

TEST_F(PowerProfileV1Test, ResetParam_ZeroValue_ReturnsFalse)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    EXPECT_FALSE(profileIntf->resetParam(0.0));
}

// =============================================================================
// OemPowerProfileIntf (V1) -- getProfileInfoFromDevice coroutine
// =============================================================================

TEST_F(PowerProfileV1Test,
       GetProfileInfoFromDevice_Success_SetsProfileProperties)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    // Build a valid get_preset_profile response with 1 profile
    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;

    nsm_preset_profile_data profileData = {};
    profileData.tmp_floor_setting_in_percent = 2048; // UFXP4.12 for ~0.5
    profileData.ramp_up_rate_in_miliwattspersec = 5000;
    profileData.ramp_down_rate_in_miliwattspersec = 3000;
    profileData.ramp_hysterisis_rate_in_milisec = 1000;

    uint8_t numProfiles = 1;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL,
                                             &metaData, &profileData,
                                             numProfiles, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(responseData));

    // Act
    EXPECT_NO_THROW(profileIntf->getProfileInfoFromDevice());
}

TEST_F(PowerProfileV1Test,
       GetProfileInfoFromDevice_PostPatchIOFailure_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    // Mock postPatchIO to return error
    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    // Act
    EXPECT_NO_THROW(profileIntf->getProfileInfoFromDevice());
}

TEST_F(PowerProfileV1Test,
       GetProfileInfoFromDevice_InvalidTmpFloor_SetsInvalidUint32)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;

    nsm_preset_profile_data profileData = {};
    profileData.tmp_floor_setting_in_percent = INVALID_UINT16_VALUE;
    profileData.ramp_up_rate_in_miliwattspersec = 5000;
    profileData.ramp_down_rate_in_miliwattspersec = 3000;
    profileData.ramp_hysterisis_rate_in_milisec = 1000;

    uint8_t numProfiles = 1;
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL,
                                             &metaData, &profileData,
                                             numProfiles, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(responseData));

    // Act
    EXPECT_NO_THROW(profileIntf->getProfileInfoFromDevice());

    // Assert: when INVALID_UINT16_VALUE is set, dbus should show
    // INVALID_UINT32_VALUE
    EXPECT_EQ(profileIntf->PowerProfileIntf::tmpFloorPercent(),
              INVALID_UINT32_VALUE);
}

TEST_F(PowerProfileV1Test,
       GetProfileInfoFromDevice_ErrorCC_ReturnsCompletionCode)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    // Build a response with error CC
    nsm_get_all_preset_profile_meta_data metaData = {};
    nsm_preset_profile_data profileData = {};
    uint8_t numProfiles = 0;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = encode_get_preset_profile_resp(
        0, NSM_ERROR, ERR_NULL, &metaData, &profileData, numProfiles, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(responseData));

    // Act
    EXPECT_NO_THROW(profileIntf->getProfileInfoFromDevice());
}

// =============================================================================
// OemPowerProfileIntf (V1) -- updateProfileInfoOnDevice coroutine
// =============================================================================

TEST_F(PowerProfileV1Test, UpdateProfileInfoOnDevice_Success_ReturnsSwSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    // Build a valid update_preset_profile_param response
    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    auto rc = encode_update_preset_profile_param_resp(0, NSM_SUCCESS, ERR_NULL,
                                                      updateRespMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Also need a get profile response for the verification call
    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;
    nsm_preset_profile_data profileData = {};
    profileData.tmp_floor_setting_in_percent = 2048;
    profileData.ramp_up_rate_in_miliwattspersec = 5000;
    profileData.ramp_down_rate_in_miliwattspersec = 3000;
    profileData.ramp_hysterisis_rate_in_milisec = 1000;

    uint8_t numProfiles = 1;
    std::vector<uint8_t> getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    rc = encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, &metaData,
                                        &profileData, numProfiles, getRespMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // First call = update, second call = get (verification)
    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp))
        .WillOnce(mockPostPatchIO(getResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act: parameterId=1 (ramp up rate), paramValue = 5.0
    EXPECT_NO_THROW(profileIntf->updateProfileInfoOnDevice(1, 5.0, &status));
}

TEST_F(PowerProfileV1Test,
       UpdateProfileInfoOnDevice_PostPatchIOFailure_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act
    EXPECT_NO_THROW(profileIntf->updateProfileInfoOnDevice(1, 5.0, &status));

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(PowerProfileV1Test, UpdateProfileInfoOnDevice_ErrorCC_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    // Build an error response for update
    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    auto rc = encode_update_preset_profile_param_resp(0, NSM_ERROR, ERR_NULL,
                                                      updateRespMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act
    EXPECT_NO_THROW(profileIntf->updateProfileInfoOnDevice(1, 5.0, &status));

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(PowerProfileV1Test,
       UpdateProfileInfoOnDevice_ParameterId0_ConvertsFractionToUFXP)
{
    // When parameterId == 0, the value should be converted via
    // doubleToNvUFXP4_12
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    encode_update_preset_profile_param_resp(0, NSM_SUCCESS, ERR_NULL,
                                            updateRespMsg);

    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;
    nsm_preset_profile_data profileData = {};
    profileData.tmp_floor_setting_in_percent = 2048;
    uint8_t numProfiles = 1;
    std::vector<uint8_t> getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, &metaData,
                                   &profileData, numProfiles, getRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp))
        .WillOnce(mockPostPatchIO(getResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act: parameterId=0 (TMP floor), paramValue = 0.5 (fraction)
    EXPECT_NO_THROW(profileIntf->updateProfileInfoOnDevice(0, 0.5, &status));
}

// =============================================================================
// OemPowerProfileIntf (V1) -- resetProfileInfoOnDevice coroutine
// =============================================================================

TEST_F(PowerProfileV1Test, ResetProfileInfoOnDevice_Success_ReturnsSwSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    encode_update_preset_profile_param_resp(0, NSM_SUCCESS, ERR_NULL,
                                            updateRespMsg);

    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;
    nsm_preset_profile_data profileData = {};
    uint8_t numProfiles = 1;
    std::vector<uint8_t> getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, &metaData,
                                   &profileData, numProfiles, getRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp))
        .WillOnce(mockPostPatchIO(getResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act: parameterId=1 (ramp up rate)
    EXPECT_NO_THROW(profileIntf->resetProfileInfoOnDevice(1, &status));
}

TEST_F(PowerProfileV1Test,
       ResetProfileInfoOnDevice_PostPatchIOFailure_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act
    EXPECT_NO_THROW(profileIntf->resetProfileInfoOnDevice(1, &status));

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(PowerProfileV1Test, ResetProfileInfoOnDevice_ErrorCC_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath,
                                                             profileId, gpu);

    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    encode_update_preset_profile_param_resp(0, NSM_ERROR, ERR_NULL,
                                            updateRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act
    EXPECT_NO_THROW(profileIntf->resetProfileInfoOnDevice(2, &status));

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// OemPowerProfileIntf (V1) -- setTmpFloorPercent coroutine
// =============================================================================

TEST_F(PowerProfileV1Test,
       SetTmpFloorPercent_InvalidVariant_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    AsyncSetOperationValueType value = std::string("invalid");
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    EXPECT_THROW_COROUTINE(
        profileIntf->setTmpFloorPercent(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerProfileV1Test, SetTmpFloorPercent_ResetValue_CallsResetProfileInfo)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    // INVALID_POWER_LIMIT cast to double triggers reset path
    double resetValue = static_cast<double>(INVALID_POWER_LIMIT);
    AsyncSetOperationValueType value = resetValue;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Mock postPatchIO for the reset path
    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    encode_update_preset_profile_param_resp(0, NSM_SUCCESS, ERR_NULL,
                                            updateRespMsg);

    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;
    nsm_preset_profile_data profileData = {};
    uint8_t numProfiles = 1;
    std::vector<uint8_t> getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, &metaData,
                                   &profileData, numProfiles, getRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp))
        .WillOnce(mockPostPatchIO(getResp));

    // Act
    EXPECT_NO_THROW(profileIntf->setTmpFloorPercent(value, &status, gpu));
}

TEST_F(PowerProfileV1Test,
       SetTmpFloorPercent_NormalValue_CallsUpdateProfileInfo)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    double normalValue = 50.0; // 50% -> 0.5 fraction
    AsyncSetOperationValueType value = normalValue;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    encode_update_preset_profile_param_resp(0, NSM_SUCCESS, ERR_NULL,
                                            updateRespMsg);

    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;
    nsm_preset_profile_data profileData = {};
    uint8_t numProfiles = 1;
    std::vector<uint8_t> getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, &metaData,
                                   &profileData, numProfiles, getRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp))
        .WillOnce(mockPostPatchIO(getResp));

    // Act
    EXPECT_NO_THROW(profileIntf->setTmpFloorPercent(value, &status, gpu));
}

// =============================================================================
// OemPowerProfileIntf (V1) -- setRampUpRate coroutine
// =============================================================================

TEST_F(PowerProfileV1Test, SetRampUpRate_InvalidVariant_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    AsyncSetOperationValueType value = std::string("invalid");
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    EXPECT_THROW_COROUTINE(
        profileIntf->setRampUpRate(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerProfileV1Test, SetRampUpRate_ResetValue_CallsResetProfileInfo)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    double resetValue = static_cast<double>(INVALID_POWER_LIMIT);
    AsyncSetOperationValueType value = resetValue;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    encode_update_preset_profile_param_resp(0, NSM_SUCCESS, ERR_NULL,
                                            updateRespMsg);

    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;
    nsm_preset_profile_data profileData = {};
    uint8_t numProfiles = 1;
    std::vector<uint8_t> getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, &metaData,
                                   &profileData, numProfiles, getRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp))
        .WillOnce(mockPostPatchIO(getResp));

    EXPECT_NO_THROW(profileIntf->setRampUpRate(value, &status, gpu));
}

TEST_F(PowerProfileV1Test, SetRampUpRate_NormalValue_CallsUpdateProfileInfo)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    double normalValue = 5.0; // watts/sec
    AsyncSetOperationValueType value = normalValue;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    encode_update_preset_profile_param_resp(0, NSM_SUCCESS, ERR_NULL,
                                            updateRespMsg);

    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;
    nsm_preset_profile_data profileData = {};
    uint8_t numProfiles = 1;
    std::vector<uint8_t> getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, &metaData,
                                   &profileData, numProfiles, getRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp))
        .WillOnce(mockPostPatchIO(getResp));

    EXPECT_NO_THROW(profileIntf->setRampUpRate(value, &status, gpu));
}

// =============================================================================
// OemPowerProfileIntf (V1) -- setRampDownRate coroutine
// =============================================================================

TEST_F(PowerProfileV1Test, SetRampDownRate_InvalidVariant_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    AsyncSetOperationValueType value = std::string("invalid");
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    EXPECT_THROW_COROUTINE(
        profileIntf->setRampDownRate(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerProfileV1Test, SetRampDownRate_ResetValue_CallsResetProfileInfo)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    double resetValue = static_cast<double>(INVALID_POWER_LIMIT);
    AsyncSetOperationValueType value = resetValue;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    encode_update_preset_profile_param_resp(0, NSM_SUCCESS, ERR_NULL,
                                            updateRespMsg);

    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;
    nsm_preset_profile_data profileData = {};
    uint8_t numProfiles = 1;
    std::vector<uint8_t> getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, &metaData,
                                   &profileData, numProfiles, getRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp))
        .WillOnce(mockPostPatchIO(getResp));

    EXPECT_NO_THROW(profileIntf->setRampDownRate(value, &status, gpu));
}

TEST_F(PowerProfileV1Test, SetRampDownRate_NormalValue_CallsUpdateProfileInfo)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    double normalValue = 3.0;
    AsyncSetOperationValueType value = normalValue;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    encode_update_preset_profile_param_resp(0, NSM_SUCCESS, ERR_NULL,
                                            updateRespMsg);

    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;
    nsm_preset_profile_data profileData = {};
    uint8_t numProfiles = 1;
    std::vector<uint8_t> getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, &metaData,
                                   &profileData, numProfiles, getRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp))
        .WillOnce(mockPostPatchIO(getResp));

    EXPECT_NO_THROW(profileIntf->setRampDownRate(value, &status, gpu));
}

// =============================================================================
// OemPowerProfileIntf (V1) -- setRampDownHysteresis coroutine
// =============================================================================

TEST_F(PowerProfileV1Test,
       SetRampDownHysteresis_InvalidVariant_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    AsyncSetOperationValueType value = std::string("invalid");
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    EXPECT_THROW_COROUTINE(
        profileIntf->setRampDownHysteresis(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerProfileV1Test,
       SetRampDownHysteresis_ResetValue_CallsResetProfileInfo)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    double resetValue = static_cast<double>(INVALID_POWER_LIMIT);
    AsyncSetOperationValueType value = resetValue;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    encode_update_preset_profile_param_resp(0, NSM_SUCCESS, ERR_NULL,
                                            updateRespMsg);

    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;
    nsm_preset_profile_data profileData = {};
    uint8_t numProfiles = 1;
    std::vector<uint8_t> getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, &metaData,
                                   &profileData, numProfiles, getRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp))
        .WillOnce(mockPostPatchIO(getResp));

    EXPECT_NO_THROW(profileIntf->setRampDownHysteresis(value, &status, gpu));
}

TEST_F(PowerProfileV1Test,
       SetRampDownHysteresis_NormalValue_CallsUpdateProfileInfo)
{
    auto& bus = utils::DBusHandler::getBus();
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, parentPath, 0,
                                                             gpu);

    double normalValue = 2.0; // seconds
    AsyncSetOperationValueType value = normalValue;
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    std::vector<uint8_t> updateResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto updateRespMsg = reinterpret_cast<nsm_msg*>(updateResp.data());
    encode_update_preset_profile_param_resp(0, NSM_SUCCESS, ERR_NULL,
                                            updateRespMsg);

    nsm_get_all_preset_profile_meta_data metaData = {};
    metaData.max_profiles_supported = 1;
    nsm_preset_profile_data profileData = {};
    uint8_t numProfiles = 1;
    std::vector<uint8_t> getResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp) +
            numProfiles * sizeof(nsm_preset_profile_data),
        0);
    auto getRespMsg = reinterpret_cast<nsm_msg*>(getResp.data());
    encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, &metaData,
                                   &profileData, numProfiles, getRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(updateResp))
        .WillOnce(mockPostPatchIO(getResp));

    EXPECT_NO_THROW(profileIntf->setRampDownHysteresis(value, &status, gpu));
}

// =============================================================================
// Test Fixture: Reset Interface coroutines
// =============================================================================

struct NsmResetIfaceCoroutineTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string resetPath =
        "/xyz/openbmc_project/control/processor/reset_coro0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmResetIfaceCoroutineTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        EXPECT_CALL(mockManager, getEid(testing::_))
            .WillRepeatedly(testing::Return(0x10));
    }

    ~NsmResetIfaceCoroutineTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// =============================================================================
// NsmResetAsyncIntf -- assertFundamentalReset coroutine
// =============================================================================

TEST_F(NsmResetIfaceCoroutineTest, AssertFundamentalReset_Success_ReturnsZero)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t deviceIndex = 0;
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, resetPath.c_str(), gpu, deviceIndex);

    // Build a valid assert_pcie_fundamental_reset response
    std::vector<uint8_t> resetResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto resetRespMsg = reinterpret_cast<nsm_msg*>(resetResp.data());
    auto rc = encode_assert_pcie_fundamental_reset_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, resetRespMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(resetResp));

    // Act
    EXPECT_NO_THROW(resetAsyncIntf->assertFundamentalReset(RESET));
}

TEST_F(NsmResetIfaceCoroutineTest,
       AssertFundamentalReset_PostPatchIOFailure_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t deviceIndex = 0;
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, resetPath.c_str(), gpu, deviceIndex);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    // Act
    EXPECT_NO_THROW(resetAsyncIntf->assertFundamentalReset(RESET));
}

TEST_F(NsmResetIfaceCoroutineTest,
       AssertFundamentalReset_ErrorCC_ReturnsNonZero)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t deviceIndex = 0;
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, resetPath.c_str(), gpu, deviceIndex);

    std::vector<uint8_t> resetResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto resetRespMsg = reinterpret_cast<nsm_msg*>(resetResp.data());
    encode_assert_pcie_fundamental_reset_resp(0, NSM_ERROR, ERR_NULL,
                                              resetRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(resetResp));

    // Act
    EXPECT_NO_THROW(resetAsyncIntf->assertFundamentalReset(NOT_RESET));
}

// =============================================================================
// NsmResetAsyncIntf -- doResetOnDevice coroutine
// =============================================================================

TEST_F(NsmResetIfaceCoroutineTest,
       DoResetOnDevice_BothCallsSucceed_StatusSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t deviceIndex = 0;
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, resetPath.c_str(), gpu, deviceIndex);

    // Build two success responses (one for RESET, one for NOT_RESET)
    std::vector<uint8_t> resetResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto resetRespMsg = reinterpret_cast<nsm_msg*>(resetResp.data());
    encode_assert_pcie_fundamental_reset_resp(0, NSM_SUCCESS, ERR_NULL,
                                              resetRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(resetResp))
        .WillOnce(mockPostPatchIO(resetResp));

    // Create a mock status interface
    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/test_reset1");

    // Act
    EXPECT_NO_THROW(resetAsyncIntf->doResetOnDevice(statusIntf));

    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmResetIfaceCoroutineTest,
       DoResetOnDevice_FirstCallFails_StatusInternalFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t deviceIndex = 0;
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, resetPath.c_str(), gpu, deviceIndex);

    // First call (RESET) fails
    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/test_reset2");

    // Act
    EXPECT_NO_THROW(resetAsyncIntf->doResetOnDevice(statusIntf));

    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

TEST_F(NsmResetIfaceCoroutineTest,
       DoResetOnDevice_SecondCallFails_StatusInternalFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t deviceIndex = 0;
    auto resetAsyncIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, resetPath.c_str(), gpu, deviceIndex);

    // First call (RESET) succeeds, second call (NOT_RESET) fails
    std::vector<uint8_t> successResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto successRespMsg = reinterpret_cast<nsm_msg*>(successResp.data());
    encode_assert_pcie_fundamental_reset_resp(0, NSM_SUCCESS, ERR_NULL,
                                              successRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(successResp))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/test_reset3");

    // Act
    EXPECT_NO_THROW(resetAsyncIntf->doResetOnDevice(statusIntf));

    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// =============================================================================
// NsmNetworkDeviceResetAsyncIntf -- resetOnDevice coroutine
// =============================================================================

TEST_F(NsmResetIfaceCoroutineTest, NetworkResetOnDevice_Success_StatusSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string networkResetPath =
        "/xyz/openbmc_project/control/network_reset_coro0";
    auto networkResetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, networkResetPath.c_str(), gpu);

    // Build a valid reset_network_device response
    std::vector<uint8_t> resetResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto resetRespMsg = reinterpret_cast<nsm_msg*>(resetResp.data());
    auto rc = encode_reset_network_device_resp(0, ERR_NULL, resetRespMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(resetResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act
    EXPECT_NO_THROW(networkResetIntf->resetOnDevice(&status));
}

TEST_F(NsmResetIfaceCoroutineTest,
       NetworkResetOnDevice_PostPatchIOFailure_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string networkResetPath =
        "/xyz/openbmc_project/control/network_reset_coro1";
    auto networkResetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, networkResetPath.c_str(), gpu);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act
    EXPECT_NO_THROW(networkResetIntf->resetOnDevice(&status));

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmResetIfaceCoroutineTest, NetworkDoResetOnDevice_Success_StatusSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string networkResetPath =
        "/xyz/openbmc_project/control/network_reset_coro2";
    auto networkResetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, networkResetPath.c_str(), gpu);

    std::vector<uint8_t> resetResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto resetRespMsg = reinterpret_cast<nsm_msg*>(resetResp.data());
    encode_reset_network_device_resp(0, ERR_NULL, resetRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(resetResp));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/test_net_reset1");

    // Act
    EXPECT_NO_THROW(networkResetIntf->doResetOnDevice(statusIntf));

    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmResetIfaceCoroutineTest, NetworkDoResetOnDevice_Failure_StatusUpdated)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string networkResetPath =
        "/xyz/openbmc_project/control/network_reset_coro3";
    auto networkResetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, networkResetPath.c_str(), gpu);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/test_net_reset2");

    // Act
    EXPECT_NO_THROW(networkResetIntf->doResetOnDevice(statusIntf));

    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// Test Fixture: ClearPowerCap Interface coroutines
// =============================================================================

struct NsmClearPowerCapIfaceTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string clearPowerCapPath =
        "/xyz/openbmc_project/control/clear_power_cap0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmClearPowerCapIfaceTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        EXPECT_CALL(mockManager, getEid(testing::_))
            .WillRepeatedly(testing::Return(0x10));
    }

    ~NsmClearPowerCapIfaceTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// =============================================================================
// NsmClearPowerCapIntf (from nsmProcessorModulePowerControl.hpp) --
// clearPowerCap stub
// =============================================================================

TEST_F(NsmClearPowerCapIfaceTest, NsmClearPowerCapIntf_ClearPowerCap_Returns0)
{
    auto& bus = utils::DBusHandler::getBus();
    auto clearIntf = std::make_shared<nsm::NsmClearPowerCapIntf>(
        bus, clearPowerCapPath.c_str());

    EXPECT_EQ(clearIntf->clearPowerCap(), 0);
}

// =============================================================================
// NsmClearPowerCapAsyncIntf -- getPowerCapFromDevice coroutine
// =============================================================================

TEST_F(NsmClearPowerCapIfaceTest, GetPowerCapFromDevice_Success_UpdatesPowerCap)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string powerCapName = "gpu_power_cap";
    std::vector<std::string> parents;
    auto powerCapIntf = std::make_shared<nsm::NsmPowerCapIntf>(
        bus, clearPowerCapPath.c_str(), powerCapName, parents, gpu);
    auto clearPowerCapIntf =
        std::make_shared<NsmClearPowerCapIntf>(bus, clearPowerCapPath.c_str());

    auto asyncIntf = std::make_shared<NsmClearPowerCapAsyncIntf>(
        bus, clearPowerCapPath.c_str(), gpu, powerCapIntf, clearPowerCapIntf);

    // Build a valid get_power_limit response
    uint32_t enforcedLimit = 300000; // 300 W
    std::vector<uint8_t> getPowerLimitResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto getPowerLimitRespMsg =
        reinterpret_cast<nsm_msg*>(getPowerLimitResp.data());
    auto rc = encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0,
                                          enforcedLimit, getPowerLimitRespMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(getPowerLimitResp));

    // Act
    EXPECT_NO_THROW(asyncIntf->getPowerCapFromDevice());

    // Assert: 300000 / 1000 = 300
    EXPECT_EQ(powerCapIntf->PowerCapIntf::powerCap(), 300u);
}

TEST_F(NsmClearPowerCapIfaceTest,
       GetPowerCapFromDevice_PostPatchIOFailure_DoesNotUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string powerCapName = "gpu_power_cap2";
    std::vector<std::string> parents;
    auto powerCapIntf = std::make_shared<nsm::NsmPowerCapIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap1", powerCapName,
        parents, gpu);
    auto clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap1");

    auto asyncIntf = std::make_shared<NsmClearPowerCapAsyncIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap1", gpu, powerCapIntf,
        clearPowerCapIntf);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    // Act
    EXPECT_NO_THROW(asyncIntf->getPowerCapFromDevice());
}

TEST_F(NsmClearPowerCapIfaceTest,
       GetPowerCapFromDevice_InvalidPowerLimit_SetsInvalidMarker)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string powerCapName = "gpu_power_cap3";
    std::vector<std::string> parents;
    auto powerCapIntf = std::make_shared<nsm::NsmPowerCapIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap2", powerCapName,
        parents, gpu);
    auto clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap2");

    auto asyncIntf = std::make_shared<NsmClearPowerCapAsyncIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap2", gpu, powerCapIntf,
        clearPowerCapIntf);

    uint32_t enforcedLimit = INVALID_POWER_LIMIT;
    std::vector<uint8_t> getPowerLimitResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto getPowerLimitRespMsg =
        reinterpret_cast<nsm_msg*>(getPowerLimitResp.data());
    encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0, enforcedLimit,
                                getPowerLimitRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(getPowerLimitResp));

    // Act
    EXPECT_NO_THROW(asyncIntf->getPowerCapFromDevice());

    EXPECT_EQ(powerCapIntf->PowerCapIntf::powerCap(), INVALID_POWER_LIMIT);
}

// =============================================================================
// NsmClearPowerCapAsyncIntf -- clearPowerCapOnDevice coroutine
// =============================================================================

TEST_F(NsmClearPowerCapIfaceTest, ClearPowerCapOnDevice_Success_UpdatesPowerCap)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string powerCapName = "gpu_power_cap4";
    std::vector<std::string> parents;
    auto powerCapIntf = std::make_shared<nsm::NsmPowerCapIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap3", powerCapName,
        parents, gpu);
    auto clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap3");
    powerCapIntf->PowerCapIntf::defaultPowerCap(500);

    auto asyncIntf = std::make_shared<NsmClearPowerCapAsyncIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap3", gpu, powerCapIntf,
        clearPowerCapIntf);

    // First call: set_power_limit response (success)
    std::vector<uint8_t> setPowerLimitResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setPowerLimitRespMsg =
        reinterpret_cast<nsm_msg*>(setPowerLimitResp.data());
    encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, setPowerLimitRespMsg);

    // Second call: get_power_limit response (verification)
    uint32_t enforcedLimit = 500000;
    std::vector<uint8_t> getPowerLimitResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto getPowerLimitRespMsg =
        reinterpret_cast<nsm_msg*>(getPowerLimitResp.data());
    encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0, enforcedLimit,
                                getPowerLimitRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(setPowerLimitResp))
        .WillOnce(mockPostPatchIO(getPowerLimitResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act
    EXPECT_NO_THROW(asyncIntf->clearPowerCapOnDevice(&status));
}

TEST_F(NsmClearPowerCapIfaceTest,
       ClearPowerCapOnDevice_PostPatchIOFailure_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string powerCapName = "gpu_power_cap5";
    std::vector<std::string> parents;
    auto powerCapIntf = std::make_shared<nsm::NsmPowerCapIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap4", powerCapName,
        parents, gpu);
    auto clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap4");
    powerCapIntf->PowerCapIntf::defaultPowerCap(500);

    auto asyncIntf = std::make_shared<NsmClearPowerCapAsyncIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap4", gpu, powerCapIntf,
        clearPowerCapIntf);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act
    EXPECT_NO_THROW(asyncIntf->clearPowerCapOnDevice(&status));

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmClearPowerCapIfaceTest,
       ClearPowerCapOnDevice_ErrorCC_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string powerCapName = "gpu_power_cap6";
    std::vector<std::string> parents;
    auto powerCapIntf = std::make_shared<nsm::NsmPowerCapIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap5", powerCapName,
        parents, gpu);
    auto clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap5");
    powerCapIntf->PowerCapIntf::defaultPowerCap(500);

    auto asyncIntf = std::make_shared<NsmClearPowerCapAsyncIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap5", gpu, powerCapIntf,
        clearPowerCapIntf);

    std::vector<uint8_t> setPowerLimitResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setPowerLimitRespMsg =
        reinterpret_cast<nsm_msg*>(setPowerLimitResp.data());
    encode_set_power_limit_resp(0, NSM_ERROR, ERR_NULL, setPowerLimitRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(setPowerLimitResp));

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // Act
    EXPECT_NO_THROW(asyncIntf->clearPowerCapOnDevice(&status));

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// NsmClearPowerCapAsyncIntf -- doClearPowerCapOnDevice coroutine
// =============================================================================

TEST_F(NsmClearPowerCapIfaceTest, DoClearPowerCapOnDevice_Success_StatusSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string powerCapName = "gpu_power_cap7";
    std::vector<std::string> parents;
    auto powerCapIntf = std::make_shared<nsm::NsmPowerCapIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap6", powerCapName,
        parents, gpu);
    auto clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap6");
    powerCapIntf->PowerCapIntf::defaultPowerCap(500);

    auto asyncIntf = std::make_shared<NsmClearPowerCapAsyncIntf>(
        bus, "/xyz/openbmc_project/control/clear_power_cap6", gpu, powerCapIntf,
        clearPowerCapIntf);

    std::vector<uint8_t> setPowerLimitResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setPowerLimitRespMsg =
        reinterpret_cast<nsm_msg*>(setPowerLimitResp.data());
    encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, setPowerLimitRespMsg);

    uint32_t enforcedLimit = 500000;
    std::vector<uint8_t> getPowerLimitResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto getPowerLimitRespMsg =
        reinterpret_cast<nsm_msg*>(getPowerLimitResp.data());
    encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0, enforcedLimit,
                                getPowerLimitRespMsg);

    EXPECT_CALL(*gpu,
                postPatchIO(testing::_, testing::_, testing::_, testing::_))
        .WillOnce(mockPostPatchIO(setPowerLimitResp))
        .WillOnce(mockPostPatchIO(getPowerLimitResp));

    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/xyz/openbmc_project/async/status/test_clear_cap1");

    // Act
    EXPECT_NO_THROW(asyncIntf->doClearPowerCapOnDevice(statusIntf));

    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}
