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
#include "nsmSetECCMode.hpp"

using namespace nsm;

class NsmSetECCModeTest : public ::testing::Test
{
  protected:
    std::shared_ptr<NsmDevice> mockDevice;

    void SetUp() override
    {
        // Create a minimal mock device (null is acceptable for basic tests)
        mockDevice = nullptr;
    }
};

/**
 * Test NsmSetEccMode constructor
 */
TEST_F(NsmSetECCModeTest, testConstructor)
{
    // Test with isLongRunning = false
    NsmSetEccMode sensor1(false, mockDevice);
    EXPECT_EQ(sensor1.isLongRunning, false);

    // Test with isLongRunning = true
    NsmSetEccMode sensor2(true, mockDevice);
    EXPECT_EQ(sensor2.isLongRunning, true);
}

/**
 * Test genRequestMsg with valid parameters
 */
TEST_F(NsmSetECCModeTest, testGenRequestMsgValidParameters)
{
    NsmSetEccMode sensor(false, mockDevice);

    // Set the internal value directly (using #define private public)
    AsyncSetOperationValueType eccValue = true;
    sensor.value = &eccValue;

    eid_t eid = 5;
    uint8_t instanceId = 0;

    auto request = sensor.genRequestMsg(eid, instanceId);

    // Should return a valid request
    EXPECT_TRUE(request.has_value());

    if (request.has_value())
    {
        // Check request size
        size_t expectedSize = sizeof(nsm_msg_hdr) +
                              sizeof(nsm_set_ECC_mode_req);
        EXPECT_EQ(request->size(), expectedSize);
    }
}

/**
 * Test genRequestMsg with false ECC mode
 */
TEST_F(NsmSetECCModeTest, testGenRequestMsgFalseMode)
{
    NsmSetEccMode sensor(false, mockDevice);

    // Set the internal value directly (using #define private public)
    AsyncSetOperationValueType eccValue = false;
    sensor.value = &eccValue;

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto request = sensor.genRequestMsg(eid, instanceId);

    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), 0);
}

/**
 * Test handleResponseMsg with valid response (non-long-running)
 */
TEST_F(NsmSetECCModeTest, testHandleResponseMsgValidNonLongRunning)
{
    NsmSetEccMode sensor(false, mockDevice);

    // Create a mock response message
    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_common_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    // Encode a success response
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;

    encode_set_ECC_mode_resp(0, cc, reasonCode, responseMsg);

    uint8_t result = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Should return NSM_SUCCESS
    EXPECT_EQ(result, NSM_SUCCESS);
}

/**
 * Test handleResponseMsg with valid response (long-running)
 */
TEST_F(NsmSetECCModeTest, testHandleResponseMsgValidLongRunning)
{
    NsmSetEccMode sensor(true, mockDevice);

    // Create a mock response message for event-based response
    // encode_set_ECC_mode_event_resp uses encode_long_running_resp which
    // writes sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
    // sizeof(nsm_long_running_resp)
    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                      sizeof(nsm_long_running_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    // Encode a success response
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;

    encode_set_ECC_mode_event_resp(0, cc, reasonCode, responseMsg);

    uint8_t result = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_EQ(result, NSM_SUCCESS);
}

/**
 * Test handleResponseMsg with error completion code
 */
TEST_F(NsmSetECCModeTest, testHandleResponseMsgErrorCC)
{
    NsmSetEccMode sensor(false, mockDevice);

    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_common_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    // Encode an error response
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = 0x1234;

    encode_set_ECC_mode_resp(0, cc, reasonCode, responseMsg);

    uint8_t result = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Should return the error cc
    EXPECT_EQ(result, NSM_ERROR);
}

/**
 * Test handleResponseMsg with decode failure
 */
TEST_F(NsmSetECCModeTest, testHandleResponseMsgDecodeFailure)
{
    NsmSetEccMode sensor(false, mockDevice);

    // Create invalid/corrupted response (too small)
    std::vector<uint8_t> responseData(10);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    uint8_t result = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Should return an error code (decode will fail)
    EXPECT_NE(result, NSM_SUCCESS);
}
