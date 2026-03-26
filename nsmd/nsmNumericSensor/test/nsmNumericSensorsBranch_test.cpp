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
 * Branch coverage tests for nsmd/nsmNumericSensor numeric sensor sources.
 *
 * Covers the "rc == NSM_SW_SUCCESS && cc != NSM_SUCCESS" (false) branch in
 * handleResponseMsg for: NsmTemp, NsmPower, NsmEnergy, NsmVoltage,
 * NsmPeakPower, NsmAltitudePressure.
 *
 * When decode_reason_code_and_cc is called with cc != NSM_SUCCESS the msg_len
 * must equal sizeof(nsm_msg_hdr)+sizeof(nsm_common_non_success_resp) (= 9
 * bytes) for the function to return NSM_SW_SUCCESS.  Any larger buffer causes
 * it to return NSM_SW_ERROR_LENGTH, hitting the "rc false" path instead.
 */

#include "base.h"
#include "platform-environmental.h"

#include "utils.hpp"

#include <cmath>
#include <limits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Truly;

#define private public
#define protected public

#include "nsmAltitudePressure.hpp"
#include "nsmEnergy.hpp"
#include "nsmNumericSensorValue_mock.hpp"
#include "nsmPeakPower.hpp"
#include "nsmPower.hpp"
#include "nsmTemp.hpp"
#include "nsmVoltage.hpp"

using namespace nsm;

static auto& bus = utils::DBusHandler::getBus();
static const std::string sensorName("branch_sensor");
static const std::string sensorType("branch_type");
static const std::vector<utils::Association>
    associations({{"chassis", "all_sensors",
                   "/xyz/openbmc_project/inventory/branch_device"}});
static const std::string physCtx("GPU");
static const double maxVal{std::numeric_limits<double>::infinity()};
static const std::string readBasis("Headroom");
static const std::string descr("branch_sensor");

// Helper: 9-byte buffer with completion_code = NSM_ERROR
// decode_reason_code_and_cc: if cc != NSM_SUCCESS and
//   msg_len == sizeof(nsm_msg_hdr)+sizeof(nsm_common_non_success_resp)
//   → returns NSM_SW_SUCCESS with cc=NSM_ERROR.
static std::vector<uint8_t> makeNonSuccessCCBuf()
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // payload[1] = completion_code
    return buf;
}

// ── NsmTemp ──────────────────────────────────────────────────────────────────

// handleResponseMsg: rc==NSM_SW_SUCCESS, cc==NSM_ERROR → reading = NaN
TEST(nsmTempBranch, HandleResponseMsg_DecodeSuccessNonZeroCC_UpdatesNaN)
{
    NsmTemp sensor{bus,          sensorName,
                   sensorType,   1,
                   associations, associations[0].absolutePath,
                   physCtx,      nullptr,
                   maxVal,       std::numeric_limits<double>::infinity(),
                   0.0,          &readBasis,
                   &descr};
    auto value = std::make_shared<MockNsmNumericSensorValueAggregate>();
    sensor.sensorValue = value;

    auto buf = makeNonSuccessCCBuf();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    EXPECT_CALL(*value,
                updateReading(Truly([](double v) { return std::isnan(v); }),
                              testing::_))
        .Times(1);

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ── NsmPower ─────────────────────────────────────────────────────────────────

TEST(nsmPowerBranch, HandleResponseMsg_DecodeSuccessNonZeroCC_UpdatesNaN)
{
    NsmPower sensor{bus,
                    sensorName,
                    sensorType,
                    1,
                    1,
                    associations,
                    associations[0].absolutePath,
                    physCtx,
                    nullptr,
                    maxVal,
                    std::numeric_limits<double>::infinity(),
                    0.0,
                    &readBasis,
                    &descr};
    auto value = std::make_shared<MockNsmNumericSensorValueAggregate>();
    sensor.sensorValue = value;

    auto buf = makeNonSuccessCCBuf();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    EXPECT_CALL(*value,
                updateReading(Truly([](double v) { return std::isnan(v); }),
                              testing::_))
        .Times(1);

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ── NsmEnergy ────────────────────────────────────────────────────────────────

TEST(nsmEnergyBranch, HandleResponseMsg_DecodeSuccessNonZeroCC_UpdatesNaN)
{
    NsmEnergy sensor{bus,          sensorName,
                     sensorType,   1,
                     associations, associations[0].absolutePath,
                     physCtx,      nullptr,
                     maxVal,       std::numeric_limits<double>::infinity(),
                     0.0,          &readBasis,
                     &descr};
    auto value = std::make_shared<MockNsmNumericSensorValueAggregate>();
    sensor.sensorValue = value;

    auto buf = makeNonSuccessCCBuf();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    EXPECT_CALL(*value,
                updateReading(Truly([](double v) { return std::isnan(v); }),
                              testing::_))
        .Times(1);

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ── NsmVoltage ───────────────────────────────────────────────────────────────

TEST(nsmVoltageBranch, HandleResponseMsg_DecodeSuccessNonZeroCC_UpdatesNaN)
{
    NsmVoltage sensor{
        bus,     sensorName,   sensorType,
        1,       associations, physCtx,
        nullptr, maxVal,       std::numeric_limits<double>::infinity(),
        0.0,     &readBasis,   &descr};
    auto value = std::make_shared<MockNsmNumericSensorValueAggregate>();
    sensor.sensorValue = value;

    auto buf = makeNonSuccessCCBuf();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    EXPECT_CALL(*value,
                updateReading(Truly([](double v) { return std::isnan(v); }),
                              testing::_))
        .Times(1);

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ── NsmPeakPower ─────────────────────────────────────────────────────────────

TEST(nsmPeakPowerBranch, HandleResponseMsg_DecodeSuccessNonZeroCC_UpdatesNaN)
{
    NsmPeakPower sensor{bus, sensorName, sensorType, 1, 1};
    auto value = std::make_shared<MockNsmNumericSensorValueAggregate>();
    sensor.sensorValue = value;

    auto buf = makeNonSuccessCCBuf();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    EXPECT_CALL(*value,
                updateReading(Truly([](double v) { return std::isnan(v); }),
                              testing::_))
        .Times(1);

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ── NsmAltitudePressure ──────────────────────────────────────────────────────

// handleResponseMsg: rc==NSM_SW_SUCCESS, cc==NSM_ERROR → else branch
// → sensorValue->updateReading(NaN)
TEST(nsmAltitudePressureBranch,
     HandleResponseMsg_DecodeSuccessNonZeroCC_UpdatesNaN)
{
    NsmAltitudePressure sensor{
        bus,        sensorName,
        sensorType, associations,
        physCtx,    nullptr,
        maxVal,     std::numeric_limits<double>::infinity(),
        0.0};
    auto value = std::make_shared<MockNsmNumericSensorValueAggregate>();
    sensor.sensorValue = value;

    auto buf = makeNonSuccessCCBuf();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    EXPECT_CALL(*value,
                updateReading(Truly([](double v) { return std::isnan(v); }), 0))
        .Times(1);

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}
