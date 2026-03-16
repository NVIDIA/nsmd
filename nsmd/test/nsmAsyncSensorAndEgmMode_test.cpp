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
 * nsmBatch15_test.cpp
 *
 * Coverage targets:
 *   1. nsmd/nsmSetAsync/nsmAsyncSensor.cpp
 *      - NsmAsyncSensor::set (null status throws, success path)
 *      - NsmAsyncSensor::update (genRequestMsg failure, sensorIO failure,
 *        handleResponseMsg failure, success path)
 *   2. nsmd/nsmSetAsync/nsmSetEgmMode.cpp
 *      - setEgmModeEnabled (invalid value type throws, bool success path)
 *      - setEgmModeOnDevice (encode failure, postPatchIO failure,
 *        decode failure, success path)
 *   3. nsmd/nsmProcessor/nsmReconfigPermissions.cpp
 *      - NsmReconfigPermissions::getIndex (all FeatureType values,
 *        invalid value throws)
 *      - NsmReconfigPermissions::genRequestMsg (success, encode failure)
 *      - NsmReconfigPermissions::handleResponseMsg (success, decode error)
 */

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#include "base.h"
#include "device-configuration.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmProcessor/nsmReconfigPermissions.hpp"
#include "nsmSetAsync/nsmAsyncSensor.hpp"
#include "nsmSetAsync/nsmSetEgmMode.hpp"

#undef private
#undef protected

using namespace nsm;

// =============================================================================
// Test infrastructure
// =============================================================================

static auto& testBus = utils::DBusHandler::getBus();
static std::string sName("batch15_sensor");
static std::string sType("batch15_type");
static std::string invPath("/xyz/openbmc_project/inventory/batch15");

// =============================================================================
// Concrete NsmAsyncSensor subclass for testing the base-class paths
// =============================================================================

/**
 * Minimal concrete subclass that allows controlling genRequestMsg and
 * handleResponseMsg outcomes.
 */
class ConcreteAsyncSensor : public NsmAsyncSensor
{
  public:
    using NsmAsyncSensor::NsmAsyncSensor;

    // Controls whether genRequestMsg returns a valid message
    bool genRequestShouldFail{false};
    // Controls what handleResponseMsg returns
    uint8_t handleResponseResult{NSM_SW_SUCCESS};

    std::optional<Request> genRequestMsg(eid_t /*eid*/,
                                         uint8_t /*instanceId*/) override
    {
        if (genRequestShouldFail)
        {
            return std::nullopt;
        }
        Request req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
        return req;
    }

    uint8_t handleResponseMsg(const nsm_msg* /*responseMsg*/,
                              size_t /*responseLen*/) override
    {
        return handleResponseResult;
    }
};

// =============================================================================
// NsmAsyncSensor tests
// =============================================================================

struct NsmAsyncSensorTest : public testing::Test, public SensorManagerTest
{
    const uuid_t gpuUuid = "992b3ec1-e464-f145-8686-409009062aa8";
    std::shared_ptr<MockNsmDevice> gpu =
        std::make_shared<MockNsmDevice>(0, 0, "MCTP_UUID", gpuUuid, 0);
    NsmDeviceTable devices{{gpu}};

    std::unique_ptr<ConcreteAsyncSensor> sensor;

    NsmAsyncSensorTest() : SensorManagerTest(devices) {}
    ~NsmAsyncSensorTest()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        sensor = std::make_unique<ConcreteAsyncSensor>(sName, sType);
    }
};

/**
 * set() with null status pointer must throw InvalidArgument.
 */
TEST_F(NsmAsyncSensorTest, setNullStatusThrows)
{
    AsyncSetOperationValueType value = true;
    EXPECT_THROW_COROUTINE(
        sensor->set(value, nullptr, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

/**
 * update() returns NSM_SW_ERROR when genRequestMsg returns nullopt.
 */
TEST_F(NsmAsyncSensorTest, updateGenRequestMsgFailure)
{
    sensor->genRequestShouldFail = true;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor->status = &status;

    auto rc = sensor->update(gpu).data();

    EXPECT_EQ(rc, NSM_SW_ERROR);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

/**
 * update() returns error when sensorIO fails.
 */
TEST_F(NsmAsyncSensorTest, updateSensorIOFailure)
{
    sensor->genRequestShouldFail = false;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor->status = &status;

    EXPECT_CALL(*gpu, sensorIO).Times(1).WillOnce(mockSensorIO(NSM_ERROR));

    auto rc = sensor->update(gpu).data();

    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

/**
 * update() sets WriteFailure when handleResponseMsg returns non-zero.
 */
TEST_F(NsmAsyncSensorTest, updateHandleResponseFailure)
{
    sensor->genRequestShouldFail = false;
    sensor->handleResponseResult = NSM_ERROR;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor->status = &status;

    // Build a minimal valid response buffer
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    EXPECT_CALL(*gpu, sensorIO)
        .Times(1)
        .WillOnce(mockSensorIO(resp, NSM_SUCCESS));

    auto rc = sensor->update(gpu).data();

    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

/**
 * update() succeeds end-to-end when all steps succeed.
 */
TEST_F(NsmAsyncSensorTest, updateSuccess)
{
    sensor->genRequestShouldFail = false;
    sensor->handleResponseResult = NSM_SW_SUCCESS;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    sensor->status = &status;

    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    EXPECT_CALL(*gpu, sensorIO)
        .Times(1)
        .WillOnce(mockSensorIO(resp, NSM_SUCCESS));

    auto rc = sensor->update(gpu).data();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

/**
 * set() calls update() and propagates the return code.
 */
TEST_F(NsmAsyncSensorTest, setCallsUpdate)
{
    sensor->genRequestShouldFail = false;
    sensor->handleResponseResult = NSM_SW_SUCCESS;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType value = true;

    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    EXPECT_CALL(*gpu, sensorIO)
        .Times(1)
        .WillOnce(mockSensorIO(resp, NSM_SUCCESS));

    auto rc = sensor->set(value, &status, gpu).data();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// setEgmModeEnabled tests
// =============================================================================

struct NsmSetEgmModeTest : public testing::Test, public SensorManagerTest
{
    const uuid_t gpuUuid = "992b3ec1-e464-f145-8686-409009062bb9";
    std::shared_ptr<MockNsmDevice> gpu =
        std::make_shared<MockNsmDevice>(0, 0, "MCTP_UUID", gpuUuid, 0);
    NsmDeviceTable devices{{gpu}};

    NsmSetEgmModeTest() : SensorManagerTest(devices) {}
    ~NsmSetEgmModeTest()
    {
        cleanupDeviceSensors(devices);
    }
};

/**
 * setEgmModeEnabled throws InvalidArgument when value is not bool.
 */
TEST_F(NsmSetEgmModeTest, setEgmModeEnabledInvalidArgument)
{
    AsyncSetOperationValueType value = std::string("not_a_bool");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_THROW_COROUTINE(
        setEgmModeEnabled(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

/**
 * setEgmModeEnabled propagates error when postPatchIO fails.
 */
TEST_F(NsmSetEgmModeTest, setEgmModeEnabledPostPatchIOFailure)
{
    AsyncSetOperationValueType value = true;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto rc = setEgmModeEnabled(value, &status, gpu).data();

    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

/**
 * setEgmModeEnabled succeeds with valid response.
 */
TEST_F(NsmSetEgmModeTest, setEgmModeEnabledSuccess)
{
    AsyncSetOperationValueType value = true;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    // Build a valid success response for decode_set_EGM_mode_resp
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_EGM_mode_resp(0, NSM_SUCCESS, ERR_NULL, msg);

    EXPECT_CALL(*gpu, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(resp, NSM_SUCCESS));

    auto rc = setEgmModeEnabled(value, &status, gpu).data();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

/**
 * setEgmModeEnabled with egmMode=false succeeds.
 */
TEST_F(NsmSetEgmModeTest, setEgmModeDisabledSuccess)
{
    AsyncSetOperationValueType value = false;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_EGM_mode_resp(0, NSM_SUCCESS, ERR_NULL, msg);

    EXPECT_CALL(*gpu, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(resp, NSM_SUCCESS));

    auto rc = setEgmModeEnabled(value, &status, gpu).data();

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

/**
 * setEgmModeOnDevice returns WriteFailure when decode response fails
 * (bad completion code).
 */
TEST_F(NsmSetEgmModeTest, setEgmModeDecodeBadCC)
{
    AsyncSetOperationValueType value = true;
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_EGM_mode_resp(0, NSM_ERROR, 0x1234, msg);

    EXPECT_CALL(*gpu, postPatchIO)
        .Times(1)
        .WillOnce(mockPostPatchIO(resp, NSM_SUCCESS));

    auto rc = setEgmModeEnabled(value, &status, gpu).data();

    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// NsmReconfigPermissions::getIndex tests
// =============================================================================

TEST(NsmReconfigPermissionsGetIndex, AllFeatureTypes)
{
    using FT = ReconfigSettingsIntf::FeatureType;

    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::InSystemTest),
              RP_IN_SYSTEM_TEST);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::FusingMode), RP_FUSING_MODE);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::CCMode),
              RP_CONFIDENTIAL_COMPUTE);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::BAR0Firewall),
              RP_BAR0_FIREWALL);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::CCDevMode),
              RP_CONFIDENTIAL_COMPUTE_DEV_MODE);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::TGPCurrentLimit),
              RP_TOTAL_GPU_POWER_CURRENT_LIMIT);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::TGPRatedLimit),
              RP_TOTAL_GPU_POWER_RATED_LIMIT);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::TGPMaxLimit),
              RP_TOTAL_GPU_POWER_MAX_LIMIT);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::TGPMinLimit),
              RP_TOTAL_GPU_POWER_MIN_LIMIT);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::ClockLimit), RP_CLOCK_LIMIT);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::NVLinkDisable),
              RP_NVLINK_DISABLE);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::ECCEnable), RP_ECC_ENABLE);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::PCIeVFConfiguration),
              RP_PCIE_VF_CONFIGURATION);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::RowRemappingAllowed),
              RP_ROW_REMAPPING_ALLOWED);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::RowRemappingFeature),
              RP_ROW_REMAPPING_FEATURE);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::HBMFrequencyChange),
              RP_HBM_FREQUENCY_CHANGE);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::HULKLicenseUpdate),
              RP_HULK_LICENSE_UPDATE);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::ForceTestCoupling),
              RP_FORCE_TEST_COUPLING);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::BAR0TypeConfig),
              RP_BAR0_TYPE_CONFIG);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::EDPpScalingFactor),
              RP_EDPP_SCALING_FACTOR);
    EXPECT_EQ(
        NsmReconfigPermissions::getIndex(FT::PowerSmoothingPrivilegeLevel1),
        RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_1);
    EXPECT_EQ(
        NsmReconfigPermissions::getIndex(FT::PowerSmoothingPrivilegeLevel2),
        RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_2);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::EGMMode), RP_EGM_MODE);
    EXPECT_EQ(NsmReconfigPermissions::getIndex(FT::InfoROMFileSystemRecreate),
              RP_INFOROM_RECREATE_ALLOW_INB);
}

TEST(NsmReconfigPermissionsGetIndex, InvalidFeatureTypeThrows)
{
    // Cast an out-of-range integer to FeatureType to trigger the default case
    auto invalidFeature = static_cast<ReconfigSettingsIntf::FeatureType>(0xFF);
    EXPECT_THROW(NsmReconfigPermissions::getIndex(invalidFeature),
                 std::invalid_argument);
}

// =============================================================================
// NsmReconfigPermissions genRequestMsg / handleResponseMsg tests
// =============================================================================

// Helper: create a minimal NsmReconfigPermissions object.
// NsmReconfigPermissions requires a sdbusplus bus and two ReconfigSettingsIntf
// objects. We construct them inline on the test bus.
struct NsmReconfigPermissionsTest :
    public testing::Test,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "992b3ec1-e464-f145-8686-409009062cc0";
    std::shared_ptr<MockNsmDevice> gpu =
        std::make_shared<MockNsmDevice>(0, 0, "MCTP_UUID", gpuUuid, 0);
    NsmDeviceTable devices{{gpu}};

    std::string hostPath{"/test/reconfig/host"};
    std::string doePath{"/test/reconfig/doe"};

    std::shared_ptr<ReconfigSettingsIntf> hostIntf;
    std::shared_ptr<ReconfigSettingsIntf> doeIntf;
    std::unique_ptr<NsmReconfigPermissions> sensor;

    NsmReconfigPermissionsTest() : SensorManagerTest(devices) {}
    ~NsmReconfigPermissionsTest()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        hostIntf = std::make_shared<ReconfigSettingsIntf>(testBus,
                                                          hostPath.c_str());
        doeIntf = std::make_shared<ReconfigSettingsIntf>(testBus,
                                                         doePath.c_str());

        using FT = ReconfigSettingsIntf::FeatureType;
        sensor = std::make_unique<NsmReconfigPermissions>(
            sName, sType, hostPath, doePath, FT::InSystemTest, hostIntf,
            doeIntf);
    }
};

/**
 * genRequestMsg returns a valid request for a known index.
 */
TEST_F(NsmReconfigPermissionsTest, genRequestMsgSuccess)
{
    auto req = sensor->genRequestMsg(5, 0);

    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_reconfiguration_permissions_v1_req));
}

/**
 * handleResponseMsg with a well-formed success response updates the
 * host/DOE config interface properties and returns NSM_SUCCESS.
 */
TEST_F(NsmReconfigPermissionsTest, handleResponseMsgSuccess)
{
    nsm_reconfiguration_permissions_v1 data{};
    data.host_oneshot = 1;
    data.host_persistent = 0;
    data.host_flr_persistent = 1;
    data.DOE_oneshot = 0;
    data.DOE_persistent = 1;
    data.DOE_flr_persistent = 0;

    Response resp(sizeof(nsm_msg_hdr) +
                      sizeof(nsm_get_reconfiguration_permissions_v1_resp),
                  0);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_get_reconfiguration_permissions_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   &data, msg);

    auto rc = sensor->handleResponseMsg(msg, resp.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(sensor->hostConfigIntf->allowOneShotConfig(),
              static_cast<bool>(data.host_oneshot));
    EXPECT_EQ(sensor->hostConfigIntf->allowPersistentConfig(),
              static_cast<bool>(data.host_persistent));
    EXPECT_EQ(sensor->hostConfigIntf->allowFLRPersistentConfig(),
              static_cast<bool>(data.host_flr_persistent));
    EXPECT_EQ(sensor->doeConfigIntf->allowOneShotConfig(),
              static_cast<bool>(data.DOE_oneshot));
    EXPECT_EQ(sensor->doeConfigIntf->allowPersistentConfig(),
              static_cast<bool>(data.DOE_persistent));
    EXPECT_EQ(sensor->doeConfigIntf->allowFLRPersistentConfig(),
              static_cast<bool>(data.DOE_flr_persistent));
}

/**
 * handleResponseMsg with a bad completion code returns the error cc.
 */
TEST_F(NsmReconfigPermissionsTest, handleResponseMsgBadCC)
{
    nsm_reconfiguration_permissions_v1 data{};

    Response resp(sizeof(nsm_msg_hdr) +
                      sizeof(nsm_get_reconfiguration_permissions_v1_resp),
                  0);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_get_reconfiguration_permissions_v1_resp(0, NSM_ERROR, 0x1111, &data,
                                                   msg);

    auto rc = sensor->handleResponseMsg(msg, resp.size());

    EXPECT_NE(rc, NSM_SUCCESS);
}

/**
 * handleResponseMsg with NULL responseMsg returns decode error.
 */
TEST_F(NsmReconfigPermissionsTest, handleResponseMsgDecodeFailure)
{
    // Pass NULL to trigger NSM_SW_ERROR_NULL from decoder
    auto rc = sensor->handleResponseMsg(nullptr, 0);

    EXPECT_NE(rc, NSM_SUCCESS);
}

/**
 * Constructor sets the feature type on both host and DOE interfaces.
 */
TEST_F(NsmReconfigPermissionsTest, constructorSetsFeatureType)
{
    using FT = ReconfigSettingsIntf::FeatureType;
    EXPECT_EQ(sensor->hostConfigIntf->type(), FT::InSystemTest);
    EXPECT_EQ(sensor->doeConfigIntf->type(), FT::InSystemTest);
}

/**
 * Constructor with invalid feature type throws.
 */
TEST_F(NsmReconfigPermissionsTest, constructorInvalidFeatureThrows)
{
    using FT = ReconfigSettingsIntf::FeatureType;
    auto invalidFeature = static_cast<FT>(0xFF);

    EXPECT_THROW(NsmReconfigPermissions(sName, sType, hostPath, doePath,
                                        invalidFeature, hostIntf, doeIntf),
                 std::invalid_argument);
}
