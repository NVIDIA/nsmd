/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"

#include "asyncOperationManager.hpp"
#include "nsmSetMigMode.hpp"

using namespace nsm;

class NsmSetMigModeTest : public ::testing::Test
{
  protected:
    std::shared_ptr<NsmDevice> mockDevice;

    void SetUp() override
    {
        mockDevice = nullptr;
    }
};

/**
 * Test NsmSetMigMode constructor
 */
TEST_F(NsmSetMigModeTest, testConstructor)
{
    // Test with isLongRunning = false
    NsmSetMigMode sensor1(false, mockDevice);
    EXPECT_EQ(sensor1.isLongRunning, false);

    // Test with isLongRunning = true
    NsmSetMigMode sensor2(true, mockDevice);
    EXPECT_EQ(sensor2.isLongRunning, true);
}

/**
 * Test genRequestMsg with valid parameters
 */
TEST_F(NsmSetMigModeTest, testGenRequestMsgValidParameters)
{
    NsmSetMigMode sensor(false, mockDevice);

    // Set the internal value directly (using #define private public)
    AsyncSetOperationValueType migValue = true;
    sensor.value = &migValue;

    eid_t eid = 5;
    uint8_t instanceId = 0;

    auto request = sensor.genRequestMsg(eid, instanceId);

    EXPECT_TRUE(request.has_value());

    if (request.has_value())
    {
        size_t expectedSize = sizeof(nsm_msg_hdr) +
                              sizeof(nsm_set_MIG_mode_req);
        EXPECT_EQ(request->size(), expectedSize);
    }
}

/**
 * Test genRequestMsg with false MIG mode
 */
TEST_F(NsmSetMigModeTest, testGenRequestMsgFalseMode)
{
    NsmSetMigMode sensor(false, mockDevice);

    // Set the internal value directly (using #define private public)
    AsyncSetOperationValueType migValue = false;
    sensor.value = &migValue;

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto request = sensor.genRequestMsg(eid, instanceId);

    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), 0);
}

/**
 * Test handleResponseMsg with valid response (non-long-running)
 */
TEST_F(NsmSetMigModeTest, testHandleResponseMsgValidNonLongRunning)
{
    NsmSetMigMode sensor(false, mockDevice);

    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_common_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;

    encode_set_MIG_mode_resp(0, cc, reasonCode, responseMsg);

    uint8_t result = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_EQ(result, NSM_SUCCESS);
}

/**
 * Test handleResponseMsg with valid response (long-running)
 */
TEST_F(NsmSetMigModeTest, testHandleResponseMsgValidLongRunning)
{
    NsmSetMigMode sensor(true, mockDevice);

    // encode_set_MIG_mode_event_resp uses encode_long_running_resp which
    // writes sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
    // sizeof(nsm_long_running_resp)
    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                      sizeof(nsm_long_running_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;

    encode_set_MIG_mode_event_resp(0, cc, reasonCode, responseMsg);

    uint8_t result = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_EQ(result, NSM_SUCCESS);
}

/**
 * Test handleResponseMsg with error completion code
 */
TEST_F(NsmSetMigModeTest, testHandleResponseMsgErrorCC)
{
    NsmSetMigMode sensor(false, mockDevice);

    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_common_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = 0x5678;

    encode_set_MIG_mode_resp(0, cc, reasonCode, responseMsg);

    uint8_t result = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_EQ(result, NSM_ERROR);
}

/**
 * Test handleResponseMsg with decode failure
 */
TEST_F(NsmSetMigModeTest, testHandleResponseMsgDecodeFailure)
{
    NsmSetMigMode sensor(false, mockDevice);

    std::vector<uint8_t> responseData(10);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    uint8_t result = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_NE(result, NSM_SUCCESS);
}
