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
 * Branch coverage tests for nsmd/nsmSetAsync/nsmSetMigMode.cpp
 *
 * Covers:
 *  - handleResponseMsg (non-long-running): decode fails with cc==NSM_SUCCESS
 *    → "cc ? cc : rc" FALSE branch → returns rc (non-zero)
 *  - handleResponseMsg (long-running): decode fails with cc==NSM_SUCCESS
 *    → "cc ? cc : rc" FALSE branch → returns rc (non-zero)
 *  - handleResponseMsg (long-running): error CC in event response
 *    → "cc ? cc : rc" TRUE branch → returns cc
 *  - handleResponseMsg (non-long-running): error CC
 *    → "cc ? cc : rc" TRUE branch → returns cc
 *  - isLongRunning TRUE branch: "decode_set_MIG_mode_event_resp"
 *    string path and long-running path
 *  - isLongRunning FALSE branch: "decode_set_MIG_mode_resp"
 *    string path and non-long-running path
 */

#include <gtest/gtest.h>

#define private public
#define protected public

#include "platform-environmental.h"

#include "asyncOperationManager.hpp"
#include "nsmSetMigMode.hpp"

using namespace nsm;

class NsmSetMigModeBranchTest : public ::testing::Test
{
  protected:
    std::shared_ptr<NsmDevice> mockDevice;

    void SetUp() override
    {
        mockDevice = nullptr;
    }
};

// handleResponseMsg (non-long-running): buffer has cc == NSM_SUCCESS but is
// too short → decode_set_MIG_mode_resp returns NSM_SW_ERROR_LENGTH
// → "cc ? cc : rc" evaluates to rc (non-zero)
TEST_F(NsmSetMigModeBranchTest, HandleResponseMsg_NonLR_DecodeFailCcZero)
{
    NsmSetMigMode sensor(false, mockDevice);

    const size_t bufSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    // Ensure completion_code (payload[1]) is NSM_SUCCESS (0)
    msg->payload[1] = NSM_SUCCESS;

    uint8_t result = sensor.handleResponseMsg(msg, buf.size());

    // cc == 0, rc != 0 → returns rc (non-zero)
    EXPECT_NE(result, NSM_SUCCESS);
}

// handleResponseMsg (long-running): similarly, too-small buffer causes
// decode failure with cc==NSM_SUCCESS → returns rc
TEST_F(NsmSetMigModeBranchTest, HandleResponseMsg_LR_DecodeFailCcZero)
{
    NsmSetMigMode sensor(true, mockDevice);

    const size_t bufSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    msg->payload[1] = NSM_SUCCESS;

    uint8_t result = sensor.handleResponseMsg(msg, buf.size());

    // cc == 0, rc != 0 → returns rc (non-zero)
    EXPECT_NE(result, NSM_SUCCESS);
}

// handleResponseMsg (long-running): error CC in event response
// → "cc ? cc : rc" TRUE branch for long-running path
TEST_F(NsmSetMigModeBranchTest, HandleResponseMsg_LR_ErrorCC)
{
    NsmSetMigMode sensor(true, mockDevice);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                 sizeof(nsm_long_running_resp),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    encode_set_MIG_mode_event_resp(0, NSM_ERROR, 0xABCD, msg);

    uint8_t result = sensor.handleResponseMsg(msg, buf.size());

    EXPECT_EQ(result, NSM_ERROR);
}

// handleResponseMsg (non-long-running): error CC in response
// → "cc ? cc : rc" TRUE branch for non-long-running path
TEST_F(NsmSetMigModeBranchTest, HandleResponseMsg_NonLR_ErrorCC)
{
    NsmSetMigMode sensor(false, mockDevice);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    // Set completion_code to NSM_ERROR (non-zero) → "cc ? cc : rc" = cc
    msg->payload[1] = NSM_ERROR;

    uint8_t result = sensor.handleResponseMsg(msg, buf.size());

    EXPECT_EQ(result, NSM_ERROR);
}
