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

#include <cmath>
#include <limits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Truly;

#include "base.h"
#include "platform-environmental.h"

#include "nsmNumericSensorValue_mock.hpp"

#define private public
#define protected public

#include "nsmThreshold.hpp"

using namespace nsm;

// handleResponseMsg – success path (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
// → calls updateReading(threshold, 0)
TEST(NsmThreshold, HandleResponseMsg_Success_UpdatesReading)
{
    auto mock = std::make_shared<MockNsmNumericSensorValueAggregate>();
    NsmThreshold sensor{"Threshold", "NSM_Threshold", 0, mock};

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto respMsg = reinterpret_cast<nsm_msg*>(response.data());

    const int32_t threshold = 85;
    auto rc = encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 threshold, respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*mock, updateReading(double(threshold), 0)).Times(1);

    auto result = sensor.handleResponseMsg(respMsg, response.size());
    EXPECT_EQ(result, NSM_SUCCESS);
}

// handleResponseMsg – cc != NSM_SUCCESS → else branch: updateReading(NaN)
// and return cc (non-zero).  Tests the `return cc ? cc : rc` ternary.
TEST(NsmThreshold, HandleResponseMsg_CompletionCodeError_UpdatesNaN)
{
    auto mock = std::make_shared<MockNsmNumericSensorValueAggregate>();
    NsmThreshold sensor{"Threshold", "NSM_Threshold", 0, mock};

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp), 0);
    auto respMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = encode_read_thermal_parameter_resp(0, NSM_ERROR, ERR_NULL, 0,
                                                 respMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*mock,
                updateReading(Truly([](double v) { return std::isnan(v); }), 0))
        .Times(1);

    auto result = sensor.handleResponseMsg(respMsg, response.size());
    EXPECT_NE(result, NSM_SUCCESS); // cc != 0 → returns cc
}

// genRequestMsg – success path returns a properly-sized message.
TEST(NsmThreshold, GenRequestMsg_ReturnsValidMessage)
{
    auto mock = std::make_shared<MockNsmNumericSensorValueAggregate>();
    NsmThreshold sensor{"Threshold", "NSM_Threshold", 1, mock};

    auto result = sensor.genRequestMsg(10, 0);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req));
}

// handleResponseMsg – decode failure path: 7-byte buffer is too short for
// decode_read_thermal_parameter_resp (needs 11+ bytes minimum).
TEST(NsmThreshold, HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto mock = std::make_shared<MockNsmNumericSensorValueAggregate>();
    NsmThreshold sensor{"Threshold", "NSM_Threshold", 0, mock};

    // 7-byte buffer: decode_common_resp requires 11 bytes minimum
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto respMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto result = sensor.handleResponseMsg(respMsg, response.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// handleResponseMsg – rc==NSM_SW_SUCCESS but cc!=NSM_SUCCESS
// (Branch 12→14: decode succeeds but device reported error completion code).
// Requires msg_len == sizeof(nsm_msg_hdr)+sizeof(nsm_common_non_success_resp)
// so decode_reason_code_and_cc returns NSM_SW_SUCCESS with cc=NSM_ERROR.
TEST(NsmThreshold, HandleResponseMsg_DecodeSuccessNonZeroCC_UpdatesNaN)
{
    auto mock = std::make_shared<MockNsmNumericSensorValueAggregate>();
    NsmThreshold sensor{"Threshold", "NSM_Threshold", 0, mock};

    // Exactly sizeof(nsm_msg_hdr)+sizeof(nsm_common_non_success_resp)=9 bytes
    // so decode_reason_code_and_cc returns NSM_SW_SUCCESS with cc=NSM_ERROR.
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    response[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code

    auto respMsg = reinterpret_cast<const nsm_msg*>(response.data());

    EXPECT_CALL(*mock,
                updateReading(Truly([](double v) { return std::isnan(v); }), 0))
        .Times(1);

    auto result = sensor.handleResponseMsg(respMsg, response.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// getSensorType – covers lines 34 and 36 in nsmThreshold.hpp
TEST(NsmThreshold, GetSensorType_ReturnsThreshold)
{
    auto mock = std::make_shared<MockNsmNumericSensorValueAggregate>();
    NsmThreshold sensor{"Threshold", "NSM_Threshold", 0, mock};
    EXPECT_EQ(sensor.getSensorType(), "threshold");
}
