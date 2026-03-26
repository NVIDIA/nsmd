/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for nsmd/nsmSetAsync/nsmSetECCMode.cpp
 *
 * Covers:
 *  - handleResponseMsg (non-long-running): decode fails (rc != 0) with
 *    cc == NSM_SUCCESS → returns rc (the "cc ? cc : rc" false branch)
 *  - handleResponseMsg (long-running): decode fails (rc != 0) with
 *    cc == NSM_SUCCESS → returns rc
 */

#include <gtest/gtest.h>

#define private public
#define protected public

#include "platform-environmental.h"

#include "asyncOperationManager.hpp"
#include "nsmSetECCMode.hpp"

using namespace nsm;

class NsmSetECCModeBranchTest : public ::testing::Test
{
  protected:
    std::shared_ptr<NsmDevice> mockDevice;

    void SetUp() override
    {
        mockDevice = nullptr;
    }
};

/**
 * handleResponseMsg (non-long-running): buffer has cc == NSM_SUCCESS but is
 * too short for nsm_common_resp → decode_set_ECC_mode_resp returns
 * NSM_SW_ERROR_LENGTH while cc stays 0 → "cc ? cc : rc" evaluates to rc.
 *
 * Buffer size: sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp)
 * (valgrind-safe minimum).  completion_code byte is explicitly zeroed.
 */
TEST_F(NsmSetECCModeBranchTest, HandleResponseMsg_NonLR_DecodeFailCcZero)
{
    NsmSetEccMode sensor(false, mockDevice);

    // Build a buffer that is large enough for decode_reason_code_and_cc to
    // read completion_code == 0 (SUCCESS) but too small for the full
    // nsm_common_resp, causing decode_common_resp to return
    // NSM_SW_ERROR_LENGTH.
    const size_t bufSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    // Ensure completion_code (payload[1]) is NSM_SUCCESS (0)
    msg->payload[1] = NSM_SUCCESS;

    uint8_t result = sensor.handleResponseMsg(msg, buf.size());

    // cc == 0, rc != 0  →  returns rc (non-zero)
    EXPECT_NE(result, NSM_SUCCESS);
}

/**
 * handleResponseMsg (long-running): similarly, provide a buffer that passes
 * the initial NULL checks but fails later decode steps, yielding rc != 0
 * with cc == NSM_SUCCESS.
 */
TEST_F(NsmSetECCModeBranchTest, HandleResponseMsg_LR_DecodeFailCcZero)
{
    NsmSetEccMode sensor(true, mockDevice);

    // For the long-running path (decode_set_ECC_mode_event_resp →
    // decode_long_running_resp), a too-small buffer will fail with rc != 0.
    // We use the valgrind-safe minimum size.
    const size_t bufSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    // completion_code at payload[1] set to SUCCESS so cc stays 0
    msg->payload[1] = NSM_SUCCESS;

    uint8_t result = sensor.handleResponseMsg(msg, buf.size());

    // cc == 0, rc != 0  →  returns rc (non-zero)
    EXPECT_NE(result, NSM_SUCCESS);
}

/**
 * handleResponseMsg (long-running): error CC in event response.
 * Covers the "cc ? cc : rc" true branch for the long-running path.
 */
TEST_F(NsmSetECCModeBranchTest, HandleResponseMsg_LR_ErrorCC)
{
    NsmSetEccMode sensor(true, mockDevice);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                 sizeof(nsm_long_running_resp),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    encode_set_ECC_mode_event_resp(0, NSM_ERROR, 0xABCD, msg);

    uint8_t result = sensor.handleResponseMsg(msg, buf.size());

    EXPECT_EQ(result, NSM_ERROR);
}
