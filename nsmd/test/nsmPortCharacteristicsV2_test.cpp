/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

// Unit tests for NsmPortCharacteristicsV2 (Query Port Characteristics v2,
// 0x12): the strict record walk, the 0x42 fallback state machine of update(),
// the publish rules of the Link Health record and the Clear Port Metric State
// (0x13) flow behind ClearEarlyHealthIndication.
//
// The device is a MockNsmDevice whose sensorIO / postPatchIO answer from
// scripted byte buffers; every request the sensor sends is recorded so the
// tests can assert which command (0x12 / 0x42 / 0x13) went out.

#include "base.h"
#include "network-ports.h"
#include "platform-environmental.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <array>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmPort/nsmPort.hpp"

#undef protected
#undef private

using namespace nsm;
using ::testing::NiceMock;

namespace
{

constexpr uint8_t kPortNumber = 3;
constexpr uint8_t kEid = 30;

// One aggregate record as it goes on the wire.
struct Record
{
    uint8_t tag;
    bool valid;
    std::vector<uint8_t> data;
};

Response encodeRecord(const Record& record)
{
    std::array<uint8_t, sizeof(nsm_aggregate_resp_sample) + 8> buffer{};
    size_t sampleLen = 0;
    auto sample = reinterpret_cast<nsm_aggregate_resp_sample*>(buffer.data());
    EXPECT_EQ(encode_aggregate_resp_sample(
                  record.tag, record.valid, record.data.data(),
                  record.data.size(), sample, &sampleLen),
              NSM_SW_SUCCESS);
    return Response(buffer.begin(), buffer.begin() + sampleLen);
}

Response v2Header(uint8_t cc, uint16_t count, uint16_t reason = ERR_NULL)
{
    Response buffer(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    EXPECT_EQ(
        encode_query_port_characteristics_v2_resp(
            0, cc, reason, count, reinterpret_cast<nsm_msg*>(buffer.data())),
        NSM_SW_SUCCESS);
    return buffer;
}

// Success response carrying `records`; `count` overrides the record count
// field to build Count mismatches.
Response v2Response(const std::vector<Record>& records,
                    std::optional<uint16_t> count = std::nullopt)
{
    auto response = v2Header(
        NSM_SUCCESS, count.value_or(static_cast<uint16_t>(records.size())));
    for (const auto& record : records)
    {
        auto encoded = encodeRecord(record);
        response.insert(response.end(), encoded.begin(), encoded.end());
    }
    return response;
}

Response v2ErrorResponse(uint8_t cc, uint16_t reason = ERR_NULL)
{
    return v2Header(cc, 0, reason);
}

std::vector<uint8_t> u32Data(uint32_t value)
{
    std::vector<uint8_t> data(sizeof(uint32_t));
    size_t dataLen = 0;
    EXPECT_EQ(
        encode_port_characteristics_v2_u32_record(value, data.data(), &dataLen),
        NSM_SW_SUCCESS);
    return data;
}

Record u32Record(uint8_t tag, uint32_t value, bool valid = true)
{
    return {tag, valid, u32Data(value)};
}

Record healthRecord(uint8_t health, uint8_t trigger = NSM_ATTENTION_TRIGGER_NA,
                    uint8_t metric = 0,
                    uint8_t config = NSM_LINK_HEALTH_CONFIG_NA,
                    bool valid = true)
{
    nsm_link_health_record record{};
    record.link_health = health;
    record.attention_trigger = trigger;
    record.attention_trigger_metric = metric;
    record.link_health_config_changed = config;
    std::vector<uint8_t> data(sizeof(uint32_t));
    size_t dataLen = 0;
    EXPECT_EQ(encode_link_health_record(&record, data.data(), &dataLen),
              NSM_SW_SUCCESS);
    return {NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH, valid, data};
}

// 0x42 status word with the given link-down reason (bits 15:10).
constexpr uint32_t portStatusWord(uint32_t downReason)
{
    return downReason << 10;
}

constexpr uint32_t kLineRateMbps = 400000;
constexpr uint32_t kDataRateKbps = 200000;
constexpr uint32_t kLaneInfo = 0x04;

// Tags 0x00-0x03 with GPU characteristics plus the given Tag 0x04 record.
std::vector<Record> fullRecordSet(const Record& health)
{
    return {u32Record(NSM_PORT_CHARACTERISTICS_V2_TAG_PORT_STATUS,
                      portStatusWord(NSM_PORT_DOWN_REASON_CODE_HI_SER_BER)),
            u32Record(NSM_PORT_CHARACTERISTICS_V2_TAG_LINE_RATE, kLineRateMbps),
            u32Record(NSM_PORT_CHARACTERISTICS_V2_TAG_DATA_RATE, kDataRateKbps),
            u32Record(NSM_PORT_CHARACTERISTICS_V2_TAG_LANE_INFO, kLaneInfo),
            health};
}

Response attentionResponse()
{
    return v2Response(fullRecordSet(healthRecord(
        NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_EFFECTIVE_BER, 3,
        NSM_LINK_HEALTH_CONFIG_CURRENT)));
}

Response healthyResponse()
{
    return v2Response(fullRecordSet(healthRecord(NSM_LINK_HEALTH_HEALTHY)));
}

// Query Port Characteristics (0x42) success response; link_health sits in
// bits 17:16 of the status word.
Response phase1Response(uint8_t linkHealth = NSM_LINK_HEALTH_HEALTHY)
{
    nsm_port_characteristics_data data{};
    const uint32_t statusWord = static_cast<uint32_t>(linkHealth) << 16;
    std::memcpy(&data.port_status, &statusWord, sizeof(statusWord));
    data.nv_port_line_rate_mbps = 100000;
    data.nv_port_data_rate_kbps = 50000;
    data.status_lane_info = 2;
    Response buffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_resp), 0);
    EXPECT_EQ(encode_query_port_characteristics_resp(
                  0, NSM_SUCCESS, ERR_NULL, &data,
                  reinterpret_cast<nsm_msg*>(buffer.data())),
              NSM_SW_SUCCESS);
    return buffer;
}

// Clear Port Metric State (0x13) response; a non-success cc uses the
// reason-code layout, exactly as the device (and the mockup responder) send.
Response clearResponse(uint8_t cc, uint16_t reason = ERR_NULL)
{
    Response buffer(sizeof(nsm_msg_hdr) +
                        (cc == NSM_SUCCESS
                             ? sizeof(nsm_clear_port_metric_state_resp)
                             : sizeof(nsm_common_non_success_resp)),
                    0);
    EXPECT_EQ(encode_clear_port_metric_state_resp(
                  0, cc, reason, reinterpret_cast<nsm_msg*>(buffer.data())),
              NSM_SW_SUCCESS);
    return buffer;
}

} // namespace

class NsmPortCharacteristicsV2Test : public ::testing::Test
{
  protected:
    static int& counter()
    {
        static int n = 0;
        return n;
    }

    NsmPortCharacteristicsV2Test()
    {
        const int id = ++counter();
        objPath = "/xyz/openbmc_project/test/port_char_v2_" +
                  std::to_string(id);
        portName = "NVLink_V2_" + std::to_string(id);

        gpu = std::make_shared<NiceMock<MockNsmDevice>>(
            NSM_DEV_ID_GPU, 0, "MCTP_EID", std::to_string(kEid),
            NSM_DEV_ROLE_RESERVED);

        iBPortIntf = std::make_shared<IBPortIntf>(bus, objPath.c_str());
        portMetricsOem3Intf =
            std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());
        portHealthMetricsIntf =
            std::make_shared<PortHealthMetricsIntf>(bus, objPath.c_str());

        sensor = std::make_shared<NsmPortCharacteristicsV2>(
            bus, portName, kPortNumber, "NSM_NVLink", NSM_DEV_ID_GPU,
            portMetricsOem3Intf, iBPortIntf, portHealthMetricsIntf, objPath,
            gpu);
    }

    static void deliver(const Response& response,
                        std::shared_ptr<const nsm_msg>& responseMsg,
                        size_t& responseLen)
    {
        responseLen = response.size();
        if (responseLen == 0)
        {
            responseMsg.reset();
            return;
        }
        auto buffer = std::make_shared<Response>(response);
        responseMsg = std::shared_ptr<const nsm_msg>(
            buffer, reinterpret_cast<const nsm_msg*>(buffer->data()));
    }

    // sensorIO action: records the command byte and answers with `response`.
    auto sensorAnswer(Response response, uint8_t code = NSM_SW_SUCCESS)
    {
        return [this, response,
                code](eid_t, Request& request,
                      std::shared_ptr<const nsm_msg>& responseMsg,
                      size_t& responseLen, bool) -> requester::Coroutine {
            sentCommands.push_back(request[sizeof(nsm_msg_hdr)]);
            deliver(response, responseMsg, responseLen);
            // coverity[missing_return]
            co_return code;
        };
    }

    // sensorIO action of a device that answers 0x12 with `v2` and 0x42 with
    // `phase1`.
    auto scriptedDevice(Response v2, Response phase1)
    {
        return [this, v2, phase1](eid_t, Request& request,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t& responseLen,
                                  bool) -> requester::Coroutine {
            const uint8_t command = request[sizeof(nsm_msg_hdr)];
            sentCommands.push_back(command);
            deliver(command == NSM_QUERY_PORT_CHARACTERISTICS_V2 ? v2 : phase1,
                    responseMsg, responseLen);
            // coverity[missing_return]
            co_return NSM_SW_SUCCESS;
        };
    }

    // postPatchIO action: records the request and answers with `response`.
    auto patchAnswer(Response response, uint8_t code = NSM_SW_SUCCESS)
    {
        return [this, response,
                code](eid_t, Request& request,
                      std::shared_ptr<const nsm_msg>& responseMsg,
                      size_t& responseLen) -> requester::Coroutine {
            patchRequests.push_back(request);
            deliver(response, responseMsg, responseLen);
            // coverity[missing_return]
            co_return code;
        };
    }

    uint8_t poll()
    {
        return sensor->update(gpu).data();
    }

    // One successful 0x12 poll: leaves the sensor with the extension
    // confirmed and Attention published.
    void confirmExtension()
    {
        EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(attentionResponse()));
        ASSERT_EQ(poll(), NSM_SW_SUCCESS);
        ASSERT_TRUE(
            portHealthMetricsIntf->clearEarlyHealthIndicationSupported());
    }

    // 0x12 unsupported by the device: the sensor falls back to 0x42.
    void enterFallback()
    {
        EXPECT_CALL(*gpu, sensorIO)
            .WillRepeatedly(scriptedDevice(
                v2ErrorResponse(NSM_ERR_UNSUPPORTED_COMMAND_CODE),
                phase1Response()));
        ASSERT_EQ(poll(), NSM_SW_SUCCESS);
        ASSERT_TRUE(sensor->fallbackActive);
    }

    void advertiseOnly0x42()
    {
        bitfield8_t commands[SUPPORTED_COMMAND_CODE_DATA_SIZE]{};
        commands[NSM_QUERY_PORT_CHARACTERISTICS / 8].byte |=
            static_cast<uint8_t>(1 << (NSM_QUERY_PORT_CHARACTERISTICS % 8));
        gpu->updateMessageTypesToCommandCodeMatrix(NSM_TYPE_NETWORK_PORT,
                                                   commands, sizeof(commands));
        ASSERT_TRUE(gpu->isCommandSupported(NSM_TYPE_NETWORK_PORT,
                                            NSM_QUERY_PORT_CHARACTERISTICS));
        ASSERT_FALSE(gpu->isCommandSupported(
            NSM_TYPE_NETWORK_PORT, NSM_QUERY_PORT_CHARACTERISTICS_V2));
    }

    std::shared_ptr<AsyncStatusIntf> newStatus(const std::string& suffix)
    {
        auto status = std::make_shared<AsyncStatusIntf>(
            bus, (objPath + "/clear_" + suffix).c_str());
        status->status(AsyncOperationStatusType::InProgress);
        return status;
    }

    // Result object handed out by AsyncOperationManager for `path`.
    static std::shared_ptr<AsyncStatusIntf>
        statusAt(const sdbusplus::object_path& path)
    {
        auto* manager = AsyncOperationManager::getInstance();
        for (const auto& [index, statusInterface] : manager->statusInterfaces)
        {
            if (std::string{AsyncOperationResultObjPath} + "/" +
                    std::to_string(index) ==
                path.str)
            {
                return statusInterface;
            }
        }
        return nullptr;
    }

    void expectHealth(EarlyHealthIndicationValues state,
                      AttentionTriggerReasonValues reason, uint8_t metricId,
                      AttentionTriggerConfigurationValues configuration)
    {
        EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(), state);
        EXPECT_EQ(portHealthMetricsIntf->attentionTriggerReason(), reason);
        EXPECT_EQ(portHealthMetricsIntf->attentionTriggerMetricId(), metricId);
        EXPECT_EQ(portHealthMetricsIntf->attentionTriggerConfiguration(),
                  configuration);
    }

    void expectNeutral(EarlyHealthIndicationValues state)
    {
        expectHealth(state, AttentionTriggerReasonValues::Unknown, 0,
                     AttentionTriggerConfigurationValues::Unknown);
    }

    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    std::string objPath;
    std::string portName;
    std::shared_ptr<NiceMock<MockNsmDevice>> gpu;
    std::shared_ptr<IBPortIntf> iBPortIntf;
    std::shared_ptr<PortMetricsOem3Intf> portMetricsOem3Intf;
    std::shared_ptr<PortHealthMetricsIntf> portHealthMetricsIntf;
    std::shared_ptr<NsmPortCharacteristicsV2> sensor;

    std::vector<uint8_t> sentCommands;
    std::vector<Request> patchRequests;
};

// ===========================================================================
// Construction
// ===========================================================================

TEST_F(NsmPortCharacteristicsV2Test, Ctor_DefaultsAndRequest)
{
    EXPECT_FALSE(portHealthMetricsIntf->clearEarlyHealthIndicationSupported());
    EXPECT_FALSE(sensor->fallbackActive);
    EXPECT_FALSE(sensor->extensionConfirmed);
    EXPECT_FALSE(sensor->operatorClearPending);
    EXPECT_NE(sensor->phase1, nullptr);
    EXPECT_TRUE(static_cast<bool>(portHealthMetricsIntf->clearHandler));
    expectNeutral(EarlyHealthIndicationValues::Unknown);

    auto request = sensor->genRequestMsg(kEid, 0);
    ASSERT_TRUE(request.has_value());
    ASSERT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_port_characteristics_v2_req));
    uint16_t portNumber = 0;
    EXPECT_EQ(decode_query_port_characteristics_v2_req(
                  reinterpret_cast<const nsm_msg*>(request->data()),
                  request->size(), &portNumber),
              NSM_SW_SUCCESS);
    EXPECT_EQ(portNumber, kPortNumber);
}

// ===========================================================================
// decodeRecords: strict record walk
// ===========================================================================

TEST_F(NsmPortCharacteristicsV2Test, DecodeRecords_ShortResponse_Aborts)
{
    Response tiny(sizeof(nsm_msg_hdr) + 1, 0);
    bool unsupported = true;
    auto rc =
        sensor->decodeRecords(reinterpret_cast<const nsm_msg*>(tiny.data()),
                              tiny.size(), unsupported);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
    EXPECT_FALSE(unsupported);
    EXPECT_FALSE(sensor->records.linkHealthPresent);
}

TEST_F(NsmPortCharacteristicsV2Test, DecodeRecords_UnsupportedCc_SetsDeviceFlag)
{
    auto response = v2ErrorResponse(NSM_ERR_UNSUPPORTED_COMMAND_CODE,
                                    ERR_NOT_SUPPORTED);
    bool unsupported = false;
    auto rc =
        sensor->decodeRecords(reinterpret_cast<const nsm_msg*>(response.data()),
                              response.size(), unsupported);
    EXPECT_EQ(rc, NSM_ERR_UNSUPPORTED_COMMAND_CODE);
    EXPECT_TRUE(unsupported);
}

TEST_F(NsmPortCharacteristicsV2Test, DecodeRecords_OtherErrorCc_NoFlagNoPublish)
{
    auto response = v2ErrorResponse(NSM_ERR_NOT_READY);
    bool unsupported = true;
    auto rc =
        sensor->decodeRecords(reinterpret_cast<const nsm_msg*>(response.data()),
                              response.size(), unsupported);
    EXPECT_EQ(rc, NSM_ERR_NOT_READY);
    EXPECT_FALSE(unsupported);
    EXPECT_FALSE(sensor->records.linkHealthPresent);
    expectNeutral(EarlyHealthIndicationValues::Unknown);
}

TEST_F(NsmPortCharacteristicsV2Test, DecodeRecords_CountExceedsPayload_Aborts)
{
    // Count says two records, the payload holds one.
    auto response = v2Response(
        {u32Record(NSM_PORT_CHARACTERISTICS_V2_TAG_LINE_RATE, kLineRateMbps)},
        2);
    bool unsupported = false;
    auto rc =
        sensor->decodeRecords(reinterpret_cast<const nsm_msg*>(response.data()),
                              response.size(), unsupported);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(NsmPortCharacteristicsV2Test, DecodeRecords_TrailingBytes_Aborts)
{
    // Count says one record, the payload holds two.
    auto response = v2Response(
        {u32Record(NSM_PORT_CHARACTERISTICS_V2_TAG_LINE_RATE, kLineRateMbps),
         u32Record(NSM_PORT_CHARACTERISTICS_V2_TAG_DATA_RATE, kDataRateKbps)},
        1);
    bool unsupported = false;
    auto rc =
        sensor->decodeRecords(reinterpret_cast<const nsm_msg*>(response.data()),
                              response.size(), unsupported);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(NsmPortCharacteristicsV2Test, DecodeRecords_OverrunningLength_Aborts)
{
    // Record metadata claims 8 data bytes (length exponent 3) but only four
    // follow.
    auto response = v2Header(NSM_SUCCESS, 1);
    const std::vector<uint8_t> sample{
        NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH,
        static_cast<uint8_t>(0x01 | (3 << 1)), // valid, length = 2^3
        0x02,
        0x00,
        0x00,
        0x00};
    response.insert(response.end(), sample.begin(), sample.end());

    bool unsupported = false;
    auto rc =
        sensor->decodeRecords(reinterpret_cast<const nsm_msg*>(response.data()),
                              response.size(), unsupported);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
    EXPECT_FALSE(sensor->records.linkHealthPresent);
}

TEST_F(NsmPortCharacteristicsV2Test, DecodeRecords_RecordDecodeError_Aborts)
{
    // Tag 0x04 with a 2-byte payload
    auto shortHealth = v2Response(
        {Record{NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH, true, {1, 0}}});
    bool unsupported = false;
    EXPECT_EQ(sensor->decodeRecords(
                  reinterpret_cast<const nsm_msg*>(shortHealth.data()),
                  shortHealth.size(), unsupported),
              NSM_SW_ERROR_LENGTH);

    // Tag 0x01 with an 8-byte payload
    auto longU32 = v2Response({Record{NSM_PORT_CHARACTERISTICS_V2_TAG_LINE_RATE,
                                      true, std::vector<uint8_t>(8, 0)}});
    EXPECT_EQ(
        sensor->decodeRecords(reinterpret_cast<const nsm_msg*>(longU32.data()),
                              longU32.size(), unsupported),
        NSM_SW_ERROR_LENGTH);
    EXPECT_FALSE(sensor->records.lineRate.has_value());
}

TEST_F(NsmPortCharacteristicsV2Test, DecodeRecords_GoodWalk_CollectsAndSkips)
{
    auto records = fullRecordSet(healthRecord(
        NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_EFFECTIVE_BER, 3,
        NSM_LINK_HEALTH_CONFIG_CURRENT));
    // Reserved tag and the aggregate timestamp are skipped by their Length.
    records.push_back(Record{0x20, true, {1, 2, 3, 4}});
    records.push_back(Record{NSM_PORT_CHARACTERISTICS_V2_TAG_TIMESTAMP, true,
                             std::vector<uint8_t>(8, 0xAA)});
    auto response = v2Response(records);

    bool unsupported = true;
    auto rc =
        sensor->decodeRecords(reinterpret_cast<const nsm_msg*>(response.data()),
                              response.size(), unsupported);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(unsupported);
    EXPECT_EQ(sensor->sampleTags.size(), 7u);

    ASSERT_TRUE(sensor->records.portStatus.has_value());
    EXPECT_EQ(*sensor->records.portStatus,
              portStatusWord(NSM_PORT_DOWN_REASON_CODE_HI_SER_BER));
    ASSERT_TRUE(sensor->records.lineRate.has_value());
    EXPECT_EQ(*sensor->records.lineRate, kLineRateMbps);
    ASSERT_TRUE(sensor->records.dataRate.has_value());
    EXPECT_EQ(*sensor->records.dataRate, kDataRateKbps);
    ASSERT_TRUE(sensor->records.laneInfo.has_value());
    EXPECT_EQ(*sensor->records.laneInfo, kLaneInfo);
    EXPECT_TRUE(sensor->records.linkHealthPresent);
    EXPECT_TRUE(sensor->records.linkHealthValid);
    EXPECT_EQ(sensor->records.linkHealth.link_health,
              NSM_LINK_HEALTH_ATTENTION);
    EXPECT_EQ(sensor->records.linkHealth.attention_trigger,
              NSM_ATTENTION_TRIGGER_EFFECTIVE_BER);
    EXPECT_EQ(sensor->records.linkHealth.attention_trigger_metric, 3);
    EXPECT_EQ(sensor->records.linkHealth.link_health_config_changed,
              NSM_LINK_HEALTH_CONFIG_CURRENT);

    // Decoding alone publishes nothing.
    expectNeutral(EarlyHealthIndicationValues::Unknown);
}

// ===========================================================================
// update(): mode machine
// ===========================================================================

TEST_F(NsmPortCharacteristicsV2Test, Update_Success_ConfirmsClearSupportedOnce)
{
    EXPECT_CALL(*gpu, sensorIO)
        .WillRepeatedly(scriptedDevice(healthyResponse(), phase1Response()));

    // Empty command matrix: no evidence against 0x12, so it is probed.
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_EQ(sentCommands,
              std::vector<uint8_t>{NSM_QUERY_PORT_CHARACTERISTICS_V2});
    EXPECT_FALSE(sensor->fallbackActive);
    EXPECT_TRUE(sensor->extensionConfirmed);
    EXPECT_TRUE(portHealthMetricsIntf->clearEarlyHealthIndicationSupported());
    expectNeutral(EarlyHealthIndicationValues::Healthy);

    // The property is written once on confirmation, not on every poll.
    portHealthMetricsIntf->clearEarlyHealthIndicationSupported(false);
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_FALSE(portHealthMetricsIntf->clearEarlyHealthIndicationSupported());
}

TEST_F(NsmPortCharacteristicsV2Test, Update_UnsupportedCc_EntersFallback)
{
    EXPECT_CALL(*gpu, sensorIO)
        .WillRepeatedly(
            scriptedDevice(v2ErrorResponse(NSM_ERR_UNSUPPORTED_COMMAND_CODE),
                           phase1Response(NSM_LINK_HEALTH_HEALTHY)));

    // 0x12 answered unsupported, then 0x42 served the port in the same cycle.
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_EQ(sentCommands,
              (std::vector<uint8_t>{NSM_QUERY_PORT_CHARACTERISTICS_V2,
                                    NSM_QUERY_PORT_CHARACTERISTICS}));
    EXPECT_TRUE(sensor->fallbackActive);
    EXPECT_FALSE(sensor->extensionConfirmed);
    EXPECT_FALSE(portHealthMetricsIntf->clearEarlyHealthIndicationSupported());
    // Published by the Phase 1 path from the 0x42 status word.
    expectNeutral(EarlyHealthIndicationValues::Healthy);
}

TEST_F(NsmPortCharacteristicsV2Test, Update_Matrix42Only_FallsBackWithoutProbe)
{
    advertiseOnly0x42();
    EXPECT_CALL(*gpu, sensorIO)
        .WillRepeatedly(scriptedDevice(healthyResponse(), phase1Response()));

    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    // The matrix is positive evidence: 0x12 is not even sent.
    EXPECT_EQ(sentCommands,
              std::vector<uint8_t>{NSM_QUERY_PORT_CHARACTERISTICS});
    EXPECT_TRUE(sensor->fallbackActive);
    EXPECT_FALSE(portHealthMetricsIntf->clearEarlyHealthIndicationSupported());
}

TEST_F(NsmPortCharacteristicsV2Test, Update_OfflineDevice_MatrixNotConsulted)
{
    // Offline: the matrix cannot be trusted, so it is no evidence against
    // 0x12 and the current (non-fallback) mode is kept.
    advertiseOnly0x42();
    gpu->isDeviceActive = false;
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(sensorAnswer(Response{}, NSM_SW_ERROR));

    EXPECT_EQ(poll(), NSM_SW_ERROR);
    EXPECT_EQ(sentCommands,
              std::vector<uint8_t>{NSM_QUERY_PORT_CHARACTERISTICS_V2});
    EXPECT_FALSE(sensor->fallbackActive);
}

TEST_F(NsmPortCharacteristicsV2Test, Update_TransportError_KeepsMode)
{
    // Not in fallback: a timeout is no evidence about 0x12.
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(sensorAnswer(Response{}, NSM_SW_ERROR_TIMEOUT));
    EXPECT_EQ(poll(), NSM_SW_ERROR_TIMEOUT);
    EXPECT_FALSE(sensor->fallbackActive);
    EXPECT_FALSE(sensor->extensionConfirmed);
    expectNeutral(EarlyHealthIndicationValues::Unknown);

    // In fallback: a failed re-probe keeps the fallback as well.
    enterFallback();
    sensor->onDeviceOnline();
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(sensorAnswer(Response{}, NSM_SW_ERROR_TIMEOUT));
    EXPECT_EQ(poll(), NSM_SW_ERROR_TIMEOUT);
    EXPECT_EQ(sentCommands.back(), NSM_QUERY_PORT_CHARACTERISTICS_V2);
    EXPECT_TRUE(sensor->fallbackActive);
}

TEST_F(NsmPortCharacteristicsV2Test, Update_Fallback_Reprobes16thCycle)
{
    enterFallback();
    sentCommands.clear();

    // Cycles 1..15 stay on 0x42.
    for (size_t cycle = 1;
         cycle < NsmPortCharacteristicsV2::kFallbackReprobeInterval; ++cycle)
    {
        EXPECT_EQ(poll(), NSM_SW_SUCCESS);
        EXPECT_TRUE(sensor->fallbackActive);
        EXPECT_EQ(sensor->fallbackCycles, cycle);
    }
    EXPECT_EQ(sentCommands,
              std::vector<uint8_t>(
                  NsmPortCharacteristicsV2::kFallbackReprobeInterval - 1,
                  NSM_QUERY_PORT_CHARACTERISTICS));

    // Cycle 16 probes 0x12; the device answers now, so the sensor returns to
    // the v2 query and confirms the extension.
    sentCommands.clear();
    EXPECT_CALL(*gpu, sensorIO)
        .WillRepeatedly(scriptedDevice(healthyResponse(), phase1Response()));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_EQ(sentCommands,
              std::vector<uint8_t>{NSM_QUERY_PORT_CHARACTERISTICS_V2});
    EXPECT_FALSE(sensor->fallbackActive);
    EXPECT_EQ(sensor->fallbackCycles, 0u);
    EXPECT_TRUE(portHealthMetricsIntf->clearEarlyHealthIndicationSupported());
}

TEST_F(NsmPortCharacteristicsV2Test, Update_Fallback_ReprobeUnsupported_Stays)
{
    enterFallback();
    for (size_t cycle = 1;
         cycle < NsmPortCharacteristicsV2::kFallbackReprobeInterval; ++cycle)
    {
        EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    }
    sentCommands.clear();

    // Still unsupported on the re-probe: 0x12 then 0x42 once that cycle, and
    // the cycle counter restarts.
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_EQ(sentCommands,
              (std::vector<uint8_t>{NSM_QUERY_PORT_CHARACTERISTICS_V2,
                                    NSM_QUERY_PORT_CHARACTERISTICS}));
    EXPECT_TRUE(sensor->fallbackActive);
    EXPECT_EQ(sensor->fallbackCycles, 0u);
    EXPECT_FALSE(portHealthMetricsIntf->clearEarlyHealthIndicationSupported());
}

TEST_F(NsmPortCharacteristicsV2Test, Update_OnDeviceOnline_ReprobesNextPoll)
{
    enterFallback();
    EXPECT_EQ(poll(), NSM_SW_SUCCESS); // one ordinary fallback cycle
    EXPECT_EQ(sentCommands.back(), NSM_QUERY_PORT_CHARACTERISTICS);

    // Re-discovery: the next poll probes 0x12 whatever the cycle counter.
    sensor->onDeviceOnline();
    EXPECT_TRUE(sensor->reprobeRequested);
    sentCommands.clear();
    EXPECT_CALL(*gpu, sensorIO)
        .WillRepeatedly(scriptedDevice(healthyResponse(), phase1Response()));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_EQ(sentCommands,
              std::vector<uint8_t>{NSM_QUERY_PORT_CHARACTERISTICS_V2});
    EXPECT_FALSE(sensor->reprobeRequested);
    EXPECT_FALSE(sensor->fallbackActive);
}

TEST_F(NsmPortCharacteristicsV2Test, Update_Fallback_ClearsOperatorFlag)
{
    // Entering the fallback drops a pending operator-clear marker ...
    sensor->operatorClearPending = true;
    enterFallback();
    EXPECT_FALSE(sensor->operatorClearPending);

    // ... and so does every Phase 1 cycle, so the flag never leaks into a
    // 0x42 transition.
    sensor->operatorClearPending = true;
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_EQ(sentCommands.back(), NSM_QUERY_PORT_CHARACTERISTICS);
    EXPECT_FALSE(sensor->operatorClearPending);
}

// ===========================================================================
// publishRecords / handleSample
// ===========================================================================

TEST_F(NsmPortCharacteristicsV2Test, Publish_Attention_PublishesAllFourFields)
{
    EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(attentionResponse()));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    expectHealth(EarlyHealthIndicationValues::Attention,
                 AttentionTriggerReasonValues::EffectiveBER, 3,
                 AttentionTriggerConfigurationValues::Current);
}

TEST_F(NsmPortCharacteristicsV2Test, Publish_Healthy_NeutralTriggerFields)
{
    confirmExtension();
    // Trigger fields are defined only in Attention: whatever the device puts
    // there is published neutral.
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(sensorAnswer(v2Response(fullRecordSet(
            healthRecord(NSM_LINK_HEALTH_HEALTHY, NSM_ATTENTION_TRIGGER_RAW_BER,
                         7, NSM_LINK_HEALTH_CONFIG_PREVIOUS)))));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    expectNeutral(EarlyHealthIndicationValues::Healthy);
}

TEST_F(NsmPortCharacteristicsV2Test, Publish_ValidZero_UnavailableNeutral)
{
    confirmExtension();
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(sensorAnswer(v2Response(fullRecordSet(healthRecord(
            NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_EFFECTIVE_BER, 3,
            NSM_LINK_HEALTH_CONFIG_CURRENT, /*valid=*/false)))));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor->records.linkHealthPresent);
    EXPECT_FALSE(sensor->records.linkHealthValid);
    // Never stale: the whole health set goes neutral.
    expectNeutral(EarlyHealthIndicationValues::Unavailable);
}

TEST_F(NsmPortCharacteristicsV2Test, Publish_NoLinkHealthRecord_Unknown)
{
    confirmExtension();
    auto records = fullRecordSet(healthRecord(NSM_LINK_HEALTH_HEALTHY));
    records.pop_back();
    EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(v2Response(records)));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor->records.linkHealthPresent);
    expectNeutral(EarlyHealthIndicationValues::Unknown);
}

TEST_F(NsmPortCharacteristicsV2Test, Publish_ReservedEncodings_Unknown)
{
    // Reserved state: Unknown with neutral trigger fields.
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(sensorAnswer(v2Response(
            fullRecordSet(healthRecord(3, NSM_ATTENTION_TRIGGER_EFFECTIVE_BER,
                                       3, NSM_LINK_HEALTH_CONFIG_CURRENT)))));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    expectNeutral(EarlyHealthIndicationValues::Unknown);

    // Attention with reserved trigger (10), slot (20) and configuration (3).
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(sensorAnswer(v2Response(fullRecordSet(
            healthRecord(NSM_LINK_HEALTH_ATTENTION, 10, 20, 3)))));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    expectNeutral(EarlyHealthIndicationValues::Attention);
}

TEST_F(NsmPortCharacteristicsV2Test, Publish_GpuCharacteristicsFromU32Records)
{
    EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(healthyResponse()));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);

    // Tag 0x00: only the link-down reason is taken from the status word.
    EXPECT_EQ(iBPortIntf->linkDownReasonCode(),
              LinkDownReasonCodes::HighBitErrorRate);
    // Tag 0x01 / 0x02 / 0x03 through the Phase 1 publishers.
    ASSERT_NE(sensor->phase1->portInfoIntf, nullptr);
    EXPECT_EQ(sensor->phase1->portInfoIntf->maxSpeed(), kLineRateMbps / 1000);
    EXPECT_DOUBLE_EQ(sensor->phase1->portInfoIntf->currentSpeed(),
                     kDataRateKbps * 1e-6);
    EXPECT_EQ(portMetricsOem3Intf->txNoProtocolBytes(), kDataRateKbps);
    EXPECT_EQ(portMetricsOem3Intf->rxNoProtocolBytes(), kDataRateKbps);
    EXPECT_EQ(portMetricsOem3Intf->txWidth(), kLaneInfo);
    EXPECT_EQ(portMetricsOem3Intf->rxWidth(), kLaneInfo);
}

TEST_F(NsmPortCharacteristicsV2Test, Publish_SwitchPublishesHealthOnly)
{
    // A switch exposes the health properties only: no PortInfo, no Oem3.
    std::string switchPath = objPath + "_switch";
    std::string switchName = portName + "_switch";
    auto switchIBPort = std::make_shared<IBPortIntf>(bus, switchPath.c_str());
    auto switchHealth =
        std::make_shared<PortHealthMetricsIntf>(bus, switchPath.c_str());
    std::shared_ptr<PortMetricsOem3Intf> nullOem3;
    auto switchSensor = std::make_shared<NsmPortCharacteristicsV2>(
        bus, switchName, kPortNumber, "NSM_NVLink", NSM_DEV_ID_SWITCH, nullOem3,
        switchIBPort, switchHealth, switchPath, gpu);
    EXPECT_EQ(switchSensor->phase1->portInfoIntf, nullptr);
    const auto linkDownBefore = switchIBPort->linkDownReasonCode();

    EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(attentionResponse()));
    EXPECT_EQ(switchSensor->update(gpu).data(), NSM_SW_SUCCESS);
    EXPECT_EQ(switchHealth->earlyHealthIndication(),
              EarlyHealthIndicationValues::Attention);
    EXPECT_EQ(switchHealth->attentionTriggerReason(),
              AttentionTriggerReasonValues::EffectiveBER);
    EXPECT_EQ(switchHealth->attentionTriggerMetricId(), 3);
    EXPECT_EQ(switchHealth->attentionTriggerConfiguration(),
              AttentionTriggerConfigurationValues::Current);
    // The u32 records were walked but not published for a switch.
    EXPECT_EQ(switchIBPort->linkDownReasonCode(), linkDownBefore);
}

TEST_F(NsmPortCharacteristicsV2Test, Publish_WrongLengthRecord_AbortsUnchanged)
{
    confirmExtension();
    auto records = fullRecordSet(healthRecord(NSM_LINK_HEALTH_HEALTHY));
    records[1] = Record{NSM_PORT_CHARACTERISTICS_V2_TAG_LINE_RATE, true,
                        std::vector<uint8_t>(8, 0)};
    EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(v2Response(records)));

    EXPECT_EQ(poll(), NSM_SW_ERROR_LENGTH);
    // Nothing of the malformed poll is published.
    expectHealth(EarlyHealthIndicationValues::Attention,
                 AttentionTriggerReasonValues::EffectiveBER, 3,
                 AttentionTriggerConfigurationValues::Current);
    EXPECT_EQ(sensor->phase1->portInfoIntf->maxSpeed(), kLineRateMbps / 1000);
    EXPECT_TRUE(portHealthMetricsIntf->clearEarlyHealthIndicationSupported());
}

TEST_F(NsmPortCharacteristicsV2Test, Publish_InvalidU32Record_KeepsLastValue)
{
    confirmExtension();
    auto records = fullRecordSet(healthRecord(NSM_LINK_HEALTH_HEALTHY));
    records[1] = u32Record(NSM_PORT_CHARACTERISTICS_V2_TAG_LINE_RATE, 0,
                           /*valid=*/false);
    EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(v2Response(records)));

    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor->records.lineRate.has_value());
    // The unreadable characteristic keeps its last value; the rest publishes.
    EXPECT_EQ(sensor->phase1->portInfoIntf->maxSpeed(), kLineRateMbps / 1000);
    expectNeutral(EarlyHealthIndicationValues::Healthy);
}

// ===========================================================================
// Throttled logging, reserved encodings and skipped records
// ===========================================================================

TEST_F(NsmPortCharacteristicsV2Test, DecodeRecords_RepeatedErrors_Throttled)
{
    // The same failure on consecutive polls is logged once: each error
    // path takes its "already reported" side on the second poll.
    Response shortResponse(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) - 1,
                           0);
    EXPECT_CALL(*gpu, sensorIO)
        .Times(2)
        .WillRepeatedly(sensorAnswer(shortResponse));
    EXPECT_NE(poll(), NSM_SW_SUCCESS);
    EXPECT_NE(poll(), NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO)
        .Times(2)
        .WillRepeatedly(sensorAnswer(v2ErrorResponse(NSM_BUSY)));
    EXPECT_EQ(poll(), NSM_BUSY);
    EXPECT_EQ(poll(), NSM_BUSY);
    EXPECT_FALSE(sensor->fallbackActive);

    // Malformed record (Length overruns the payload) twice.
    auto overrunning = v2Response({healthRecord(NSM_LINK_HEALTH_HEALTHY)});
    overrunning.resize(overrunning.size() - 1);
    EXPECT_CALL(*gpu, sensorIO)
        .Times(2)
        .WillRepeatedly(sensorAnswer(overrunning));
    EXPECT_NE(poll(), NSM_SW_SUCCESS);
    EXPECT_NE(poll(), NSM_SW_SUCCESS);

    // Trailing bytes (Count smaller than the payload) twice.
    auto trailing = v2Response({healthRecord(NSM_LINK_HEALTH_HEALTHY),
                                healthRecord(NSM_LINK_HEALTH_HEALTHY)},
                               1);
    EXPECT_CALL(*gpu, sensorIO).Times(2).WillRepeatedly(sensorAnswer(trailing));
    EXPECT_EQ(poll(), NSM_SW_ERROR_LENGTH);
    EXPECT_EQ(poll(), NSM_SW_ERROR_LENGTH);

    // Nothing was published along the way.
    expectNeutral(EarlyHealthIndicationValues::Unknown);
}

TEST_F(NsmPortCharacteristicsV2Test, DecodeRecords_WrongLengthLinkHealth_Aborts)
{
    // A Link Health record that is not one u32 aborts the poll (twice: the
    // second one is throttled) and keeps the last published state.
    confirmExtension();
    auto records = fullRecordSet(healthRecord(NSM_LINK_HEALTH_HEALTHY));
    records.back().data.resize(2);
    EXPECT_CALL(*gpu, sensorIO)
        .Times(2)
        .WillRepeatedly(sensorAnswer(v2Response(records)));
    EXPECT_EQ(poll(), NSM_SW_ERROR_LENGTH);
    EXPECT_EQ(poll(), NSM_SW_ERROR_LENGTH);
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Attention);
}

TEST_F(NsmPortCharacteristicsV2Test, DecodeRecords_TimestampRecord_Skipped)
{
    // Tag 0xFF (timestamp) is accepted and ignored.
    auto records = fullRecordSet(healthRecord(NSM_LINK_HEALTH_HEALTHY));
    records.push_back({NSM_PORT_CHARACTERISTICS_V2_TAG_TIMESTAMP, true,
                       std::vector<uint8_t>(8, 0x5A)});
    EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(v2Response(records)));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    expectNeutral(EarlyHealthIndicationValues::Healthy);
    EXPECT_EQ(sensor->sampleTags.size(), 6u);
}

TEST_F(NsmPortCharacteristicsV2Test, Publish_ConfigPrevious_Modified)
{
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(sensorAnswer(v2Response(fullRecordSet(healthRecord(
            NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_RAW_BER, 7,
            NSM_LINK_HEALTH_CONFIG_PREVIOUS)))));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    expectHealth(EarlyHealthIndicationValues::Attention,
                 AttentionTriggerReasonValues::RawBER, 7,
                 AttentionTriggerConfigurationValues::Modified);
}

TEST_F(NsmPortCharacteristicsV2Test, Publish_ReservedThenValid_RearmsThrottles)
{
    // Reserved slot / configuration twice (warned once), then valid values:
    // the throttles re-arm and the fields publish normally.
    EXPECT_CALL(*gpu, sensorIO)
        .Times(2)
        .WillRepeatedly(sensorAnswer(v2Response(fullRecordSet(
            healthRecord(NSM_LINK_HEALTH_ATTENTION,
                         NSM_ATTENTION_TRIGGER_EFFECTIVE_BER, 20, 3)))));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    expectHealth(EarlyHealthIndicationValues::Attention,
                 AttentionTriggerReasonValues::EffectiveBER, 0,
                 AttentionTriggerConfigurationValues::Unknown);

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(sensorAnswer(v2Response(fullRecordSet(healthRecord(
            NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_EFFECTIVE_BER,
            NSM_LINK_HEALTH_METRIC_SLOT_MAX,
            NSM_LINK_HEALTH_CONFIG_CURRENT)))));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    expectHealth(EarlyHealthIndicationValues::Attention,
                 AttentionTriggerReasonValues::EffectiveBER,
                 NSM_LINK_HEALTH_METRIC_SLOT_MAX,
                 AttentionTriggerConfigurationValues::Current);

    // Unreadable twice (warned once), then readable again.
    EXPECT_CALL(*gpu, sensorIO)
        .Times(2)
        .WillRepeatedly(sensorAnswer(v2Response(fullRecordSet(
            healthRecord(NSM_LINK_HEALTH_HEALTHY, NSM_ATTENTION_TRIGGER_NA, 0,
                         NSM_LINK_HEALTH_CONFIG_NA, false)))));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    expectNeutral(EarlyHealthIndicationValues::Unavailable);
    EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(healthyResponse()));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    expectNeutral(EarlyHealthIndicationValues::Healthy);
}

TEST_F(NsmPortCharacteristicsV2Test, Update_MatrixBothAdvertised_QueriesV2)
{
    // 0x42 and 0x12 both advertised: no evidence for the fallback, 0x12 is
    // queried.
    bitfield8_t commands[SUPPORTED_COMMAND_CODE_DATA_SIZE]{};
    for (auto command :
         {NSM_QUERY_PORT_CHARACTERISTICS, NSM_QUERY_PORT_CHARACTERISTICS_V2})
    {
        commands[command / 8].byte |= static_cast<uint8_t>(1 << (command % 8));
    }
    gpu->updateMessageTypesToCommandCodeMatrix(NSM_TYPE_NETWORK_PORT, commands,
                                               sizeof(commands));
    ASSERT_TRUE(gpu->isCommandSupported(NSM_TYPE_NETWORK_PORT,
                                        NSM_QUERY_PORT_CHARACTERISTICS_V2));

    EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(attentionResponse()));
    EXPECT_EQ(poll(), NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor->fallbackActive);
    EXPECT_EQ(sentCommands.back(), NSM_QUERY_PORT_CHARACTERISTICS_V2);
    EXPECT_TRUE(portHealthMetricsIntf->clearEarlyHealthIndicationSupported());
}

// ===========================================================================
// doClearOnDevice: Clear Port Metric State (0x13)
// ===========================================================================

TEST_F(NsmPortCharacteristicsV2Test, Clear_DeviceGone_Unavailable)
{
    std::string orphanPath = objPath + "_orphan";
    std::string orphanName = portName + "_orphan";
    auto orphanIBPort = std::make_shared<IBPortIntf>(bus, orphanPath.c_str());
    auto orphanOem3 = std::make_shared<PortMetricsOem3Intf>(bus,
                                                            orphanPath.c_str());
    auto orphanHealth =
        std::make_shared<PortHealthMetricsIntf>(bus, orphanPath.c_str());
    std::shared_ptr<NsmDevice> orphanDevice =
        std::make_shared<NiceMock<MockNsmDevice>>(NSM_DEV_ID_GPU, 1, "MCTP_EID",
                                                  "31", NSM_DEV_ROLE_RESERVED);
    auto orphanSensor = std::make_shared<NsmPortCharacteristicsV2>(
        bus, orphanName, kPortNumber, "NSM_NVLink", NSM_DEV_ID_GPU, orphanOem3,
        orphanIBPort, orphanHealth, orphanPath, orphanDevice);

    // The sensor holds the device weakly: once it is gone, no I/O.
    orphanDevice.reset();
    auto status = newStatus("orphan");
    orphanSensor->clearInFlight = true;
    EXPECT_EQ(orphanSensor->doClearOnDevice(status).data(), NSM_SW_ERROR);
    EXPECT_EQ(status->status(), AsyncOperationStatusType::Unavailable);
    EXPECT_FALSE(orphanSensor->clearInFlight);
}

TEST_F(NsmPortCharacteristicsV2Test, Clear_PostPatchFails_Offline_Unavailable)
{
    confirmExtension();
    gpu->isDeviceActive = false;
    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce(patchAnswer(Response{}, NSM_SW_ERROR_TIMEOUT));
    EXPECT_CALL(*gpu, sensorIO).Times(0);

    auto status = newStatus("offline");
    sensor->clearInFlight = true;
    EXPECT_EQ(sensor->doClearOnDevice(status).data(), NSM_SW_ERROR_TIMEOUT);
    // postPatchIO refused an offline device: Unavailable whatever the code.
    EXPECT_EQ(status->status(), AsyncOperationStatusType::Unavailable);
    EXPECT_FALSE(sensor->clearInFlight);
}

TEST_F(NsmPortCharacteristicsV2Test, Clear_PostPatchFails_Online_MappedSwCode)
{
    confirmExtension();
    EXPECT_CALL(*gpu, sensorIO).Times(0);

    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce(patchAnswer(Response{}, NSM_SW_ERROR_TIMEOUT));
    auto timedOut = newStatus("timeout");
    EXPECT_EQ(sensor->doClearOnDevice(timedOut).data(), NSM_SW_ERROR_TIMEOUT);
    EXPECT_EQ(timedOut->status(), AsyncOperationStatusType::Timeout);

    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce(patchAnswer(Response{}, NSM_SW_ERROR));
    auto failed = newStatus("error");
    EXPECT_EQ(sensor->doClearOnDevice(failed).data(), NSM_SW_ERROR);
    EXPECT_EQ(failed->status(), AsyncOperationStatusType::Unavailable);
}

TEST_F(NsmPortCharacteristicsV2Test, Clear_Accepted_InternalFailureNoRefresh)
{
    confirmExtension();
    // 0x13 is not long-running: NSM_ACCEPTED leaves nothing to poll.
    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce(patchAnswer(clearResponse(NSM_ACCEPTED)));
    EXPECT_CALL(*gpu, sensorIO).Times(0);

    auto status = newStatus("accepted");
    sensor->clearInFlight = true;
    EXPECT_EQ(sensor->doClearOnDevice(status).data(), NSM_ACCEPTED);
    EXPECT_EQ(status->status(), AsyncOperationStatusType::InternalFailure);
    EXPECT_FALSE(sensor->operatorClearPending);
    EXPECT_FALSE(sensor->clearInFlight);
}

TEST_F(NsmPortCharacteristicsV2Test, Clear_ErrorCc_MappedNoRefresh)
{
    confirmExtension();
    EXPECT_CALL(*gpu, sensorIO).Times(0);

    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce(patchAnswer(clearResponse(NSM_ERR_INVALID_DATA)));
    auto invalid = newStatus("invalid");
    EXPECT_EQ(sensor->doClearOnDevice(invalid).data(), NSM_ERR_INVALID_DATA);
    EXPECT_EQ(invalid->status(), AsyncOperationStatusType::InvalidArgument);

    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce(patchAnswer(clearResponse(NSM_BUSY)));
    auto busy = newStatus("busy");
    EXPECT_EQ(sensor->doClearOnDevice(busy).data(), NSM_BUSY);
    EXPECT_EQ(busy->status(), AsyncOperationStatusType::Unavailable);

    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce(patchAnswer(clearResponse(NSM_ERROR, ERR_NOT_SUPPORTED)));
    auto unsupported = newStatus("unsupported");
    sensor->clearInFlight = true;
    EXPECT_EQ(sensor->doClearOnDevice(unsupported).data(), NSM_ERROR);
    EXPECT_EQ(unsupported->status(),
              AsyncOperationStatusType::UnsupportedRequest);
    EXPECT_FALSE(sensor->clearInFlight);
    // Attention stays published: the device did not clear.
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Attention);
}

TEST_F(NsmPortCharacteristicsV2Test, Clear_Success_RefreshPublishesThenSuccess)
{
    confirmExtension();
    auto status = newStatus("success");
    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce(patchAnswer(clearResponse(NSM_SUCCESS)));

    // The refresh runs through update() while the result is still pending.
    auto statusAtRefresh = AsyncOperationStatusType::Success;
    auto operatorFlagAtRefresh = false;
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce([&](eid_t, Request& request,
                      std::shared_ptr<const nsm_msg>& responseMsg,
                      size_t& responseLen, bool) -> requester::Coroutine {
        statusAtRefresh = status->status();
        operatorFlagAtRefresh = sensor->operatorClearPending;
        sentCommands.push_back(request[sizeof(nsm_msg_hdr)]);
        deliver(healthyResponse(), responseMsg, responseLen);
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    });

    EXPECT_EQ(sensor->doClearOnDevice(status).data(), NSM_SW_SUCCESS);

    // 0x13 for this port with Tag 0x04 only, through the write path.
    ASSERT_EQ(patchRequests.size(), 1u);
    uint16_t portNumber = 0;
    uint16_t tagCount = 0;
    const uint8_t* tagIds = nullptr;
    ASSERT_EQ(decode_clear_port_metric_state_req(
                  reinterpret_cast<const nsm_msg*>(patchRequests[0].data()),
                  patchRequests[0].size(), &portNumber, &tagCount, &tagIds),
              NSM_SW_SUCCESS);
    EXPECT_EQ(portNumber, kPortNumber);
    EXPECT_EQ(tagCount, 1);
    EXPECT_EQ(tagIds[0], NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH);

    // Refresh (0x12) preceded Success and carried the operator-clear marker.
    EXPECT_EQ(sentCommands.back(), NSM_QUERY_PORT_CHARACTERISTICS_V2);
    EXPECT_EQ(statusAtRefresh, AsyncOperationStatusType::InProgress);
    EXPECT_TRUE(operatorFlagAtRefresh);
    EXPECT_FALSE(sensor->operatorClearPending);
    expectNeutral(EarlyHealthIndicationValues::Healthy);
    EXPECT_EQ(status->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmPortCharacteristicsV2Test, Clear_Success_RefreshFails_StillSuccess)
{
    confirmExtension();
    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce(patchAnswer(clearResponse(NSM_SUCCESS)));
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(sensorAnswer(Response{}, NSM_SW_ERROR_TIMEOUT));

    auto status = newStatus("refresh_failed");
    sensor->clearInFlight = true;
    EXPECT_EQ(sensor->doClearOnDevice(status).data(), NSM_SW_SUCCESS);
    // The device has cleared: Success, and the marker survives for the next
    // successful publish.
    EXPECT_EQ(status->status(), AsyncOperationStatusType::Success);
    EXPECT_TRUE(sensor->operatorClearPending);
    EXPECT_FALSE(sensor->clearInFlight);
    EXPECT_EQ(portHealthMetricsIntf->earlyHealthIndication(),
              EarlyHealthIndicationValues::Attention);
}

// ===========================================================================
// clearEarlyHealthIndication entry point and handler wiring
// ===========================================================================

TEST_F(NsmPortCharacteristicsV2Test, ClearEntry_ExtensionInactive_NoDeviceIO)
{
    EXPECT_CALL(*gpu, postPatchIO).Times(0);

    // 0x12 never answered yet.
    auto pending = sensor->clearEarlyHealthIndication();
    EXPECT_FALSE(pending.str.empty());
    auto pendingStatus = statusAt(pending);
    ASSERT_NE(pendingStatus, nullptr);
    EXPECT_EQ(pendingStatus->status(),
              AsyncOperationStatusType::UnsupportedRequest);

    // Confirmed once, then dropped to the fallback.
    confirmExtension();
    enterFallback();
    auto fallback = sensor->clearEarlyHealthIndication();
    auto fallbackStatus = statusAt(fallback);
    ASSERT_NE(fallbackStatus, nullptr);
    EXPECT_EQ(fallbackStatus->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

TEST_F(NsmPortCharacteristicsV2Test, ClearEntry_Confirmed_RoutedFromInterface)
{
    confirmExtension();
    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce(patchAnswer(clearResponse(NSM_SUCCESS)));
    EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(healthyResponse()));

    // The D-Bus method on the interface reaches the sensor through the
    // handler installed by the constructor.
    auto path = portHealthMetricsIntf->clearEarlyHealthIndication();
    auto status = statusAt(path);
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->status(), AsyncOperationStatusType::Success);
    EXPECT_EQ(patchRequests.size(), 1u);
    expectNeutral(EarlyHealthIndicationValues::Healthy);
}

// ===========================================================================
// In-flight guard: one Clear Port Metric State per port at a time
// ===========================================================================

TEST_F(NsmPortCharacteristicsV2Test, ClearGuard_SecondWhileInFlight_Conflicting)
{
    confirmExtension();
    EXPECT_FALSE(sensor->clearInFlight);

    // The second request arrives while the first one's 0x13 is on the wire.
    sdbusplus::object_path second;
    bool guardAtSecond = false;
    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce([&](eid_t, Request& request,
                      std::shared_ptr<const nsm_msg>& responseMsg,
                      size_t& responseLen) -> requester::Coroutine {
        patchRequests.push_back(request);
        guardAtSecond = sensor->clearInFlight;
        second = sensor->clearEarlyHealthIndication();
        deliver(clearResponse(NSM_SUCCESS), responseMsg, responseLen);
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    });
    EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(healthyResponse()));

    auto first = sensor->clearEarlyHealthIndication();

    // The second result object answers ConflictingOperation at once ...
    EXPECT_TRUE(guardAtSecond);
    EXPECT_NE(first.str, second.str);
    auto secondStatus = statusAt(second);
    ASSERT_NE(secondStatus, nullptr);
    EXPECT_EQ(secondStatus->status(),
              AsyncOperationStatusType::ConflictingOperation);
    // ... and the first clear is unaffected: one 0x13, one refresh, Success.
    auto firstStatus = statusAt(first);
    ASSERT_NE(firstStatus, nullptr);
    EXPECT_EQ(firstStatus->status(), AsyncOperationStatusType::Success);
    EXPECT_EQ(patchRequests.size(), 1u);
    EXPECT_EQ(sentCommands.back(), NSM_QUERY_PORT_CHARACTERISTICS_V2);
    expectNeutral(EarlyHealthIndicationValues::Healthy);
    EXPECT_FALSE(sensor->clearInFlight);
}

TEST_F(NsmPortCharacteristicsV2Test, ClearGuard_AfterSuccess_NextClearProceeds)
{
    confirmExtension();
    EXPECT_CALL(*gpu, postPatchIO)
        .Times(2)
        .WillRepeatedly(patchAnswer(clearResponse(NSM_SUCCESS)));
    EXPECT_CALL(*gpu, sensorIO)
        .Times(2)
        .WillRepeatedly(sensorAnswer(healthyResponse()));

    auto first = sensor->clearEarlyHealthIndication();
    auto firstStatus = statusAt(first);
    ASSERT_NE(firstStatus, nullptr);
    EXPECT_EQ(firstStatus->status(), AsyncOperationStatusType::Success);
    EXPECT_FALSE(sensor->clearInFlight);

    // Terminal status seen: the next clear goes to the device again.
    auto second = sensor->clearEarlyHealthIndication();
    auto secondStatus = statusAt(second);
    ASSERT_NE(secondStatus, nullptr);
    EXPECT_EQ(secondStatus->status(), AsyncOperationStatusType::Success);
    EXPECT_EQ(patchRequests.size(), 2u);
    EXPECT_FALSE(sensor->clearInFlight);
}

TEST_F(NsmPortCharacteristicsV2Test, ClearGuard_AfterFailure_RetryAccepted)
{
    confirmExtension();
    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce(patchAnswer(Response{}, NSM_SW_ERROR_TIMEOUT))
        .WillOnce(patchAnswer(clearResponse(NSM_ERR_INVALID_DATA)))
        .WillOnce(patchAnswer(clearResponse(NSM_SUCCESS)));
    EXPECT_CALL(*gpu, sensorIO).WillOnce(sensorAnswer(healthyResponse()));

    // Transport failure: the mapped status is published and the guard drops.
    auto timedOut = sensor->clearEarlyHealthIndication();
    auto timedOutStatus = statusAt(timedOut);
    ASSERT_NE(timedOutStatus, nullptr);
    EXPECT_EQ(timedOutStatus->status(), AsyncOperationStatusType::Timeout);
    EXPECT_FALSE(sensor->clearInFlight);

    // Device-side error: likewise.
    auto rejected = sensor->clearEarlyHealthIndication();
    auto rejectedStatus = statusAt(rejected);
    ASSERT_NE(rejectedStatus, nullptr);
    EXPECT_EQ(rejectedStatus->status(),
              AsyncOperationStatusType::InvalidArgument);
    EXPECT_FALSE(sensor->clearInFlight);

    // The retry reaches the device.
    auto retry = sensor->clearEarlyHealthIndication();
    auto retryStatus = statusAt(retry);
    ASSERT_NE(retryStatus, nullptr);
    EXPECT_EQ(retryStatus->status(), AsyncOperationStatusType::Success);
    EXPECT_EQ(patchRequests.size(), 3u);
    EXPECT_FALSE(sensor->clearInFlight);
}

TEST_F(NsmPortCharacteristicsV2Test, ClearGuard_FallbackWhileInFlight_Resets)
{
    confirmExtension();
    // 0x12 stops answering while the clear is on the wire: the poll that
    // runs meanwhile enters the fallback and drops the guard ...
    EXPECT_CALL(*gpu, sensorIO)
        .WillRepeatedly(
            scriptedDevice(v2ErrorResponse(NSM_ERR_UNSUPPORTED_COMMAND_CODE),
                           phase1Response()));
    bool guardBeforePoll = false;
    bool guardAfterPoll = true;
    EXPECT_CALL(*gpu, postPatchIO)
        .WillOnce([&](eid_t, Request& request,
                      std::shared_ptr<const nsm_msg>& responseMsg,
                      size_t& responseLen) -> requester::Coroutine {
        patchRequests.push_back(request);
        guardBeforePoll = sensor->clearInFlight;
        EXPECT_EQ(poll(), NSM_SW_SUCCESS);
        guardAfterPoll = sensor->clearInFlight;
        deliver(clearResponse(NSM_SUCCESS), responseMsg, responseLen);
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    });

    auto first = sensor->clearEarlyHealthIndication();

    EXPECT_TRUE(guardBeforePoll);
    EXPECT_FALSE(guardAfterPoll);
    EXPECT_TRUE(sensor->fallbackActive);
    // ... while the clear itself still completes; its refresh took the 0x42
    // path, which does not carry the operator-clear marker.
    auto firstStatus = statusAt(first);
    ASSERT_NE(firstStatus, nullptr);
    EXPECT_EQ(firstStatus->status(), AsyncOperationStatusType::Success);
    EXPECT_EQ(sentCommands.back(), NSM_QUERY_PORT_CHARACTERISTICS);
    EXPECT_FALSE(sensor->operatorClearPending);
    EXPECT_FALSE(sensor->clearInFlight);

    // In the fallback the entry point answers UnsupportedRequest (not the
    // guard's ConflictingOperation) and sends nothing.
    auto next = sensor->clearEarlyHealthIndication();
    auto nextStatus = statusAt(next);
    ASSERT_NE(nextStatus, nullptr);
    EXPECT_EQ(nextStatus->status(),
              AsyncOperationStatusType::UnsupportedRequest);
    EXPECT_EQ(patchRequests.size(), 1u);
}

TEST_F(NsmPortCharacteristicsV2Test, Dtor_DetachesClearHandler)
{
    std::string scopedPath = objPath + "_scoped";
    std::string scopedName = portName + "_scoped";
    auto scopedIBPort = std::make_shared<IBPortIntf>(bus, scopedPath.c_str());
    auto scopedOem3 = std::make_shared<PortMetricsOem3Intf>(bus,
                                                            scopedPath.c_str());
    auto scopedHealth =
        std::make_shared<PortHealthMetricsIntf>(bus, scopedPath.c_str());
    {
        NsmPortCharacteristicsV2 scoped(
            bus, scopedName, kPortNumber, "NSM_NVLink", NSM_DEV_ID_GPU,
            scopedOem3, scopedIBPort, scopedHealth, scopedPath, gpu);
        EXPECT_TRUE(static_cast<bool>(scopedHealth->clearHandler));
    }
    EXPECT_FALSE(static_cast<bool>(scopedHealth->clearHandler));

    // A late method call takes the Phase 1-only path: no device I/O.
    EXPECT_CALL(*gpu, postPatchIO).Times(0);
    auto path = scopedHealth->clearEarlyHealthIndication();
    auto status = statusAt(path);
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->status(), AsyncOperationStatusType::UnsupportedRequest);
}
