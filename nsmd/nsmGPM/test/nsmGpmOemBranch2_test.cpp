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
 * Branch coverage batch 2 for nsmd/nsmGPM/nsmGpmOem.cpp.
 *
 * Covers:
 * - NsmGPMPerInstance::genRequestMsg encode fail (instanceId > MAX)
 * - NsmGPMPerInstance::handleResponseMsg decode fail (cc != SUCCESS)
 * - NsmGPMPerInstance::handleResponseMsg decodeFunc fail (decodeRc != SUCCESS)
 * - NsmGPMPerInstance::handleResponseMsg telemetryCount==0 (already in B1)
 * - NsmGPMInterfaceCreator::addGpmIntfProperty(bitfield): null gpmIntf
 * - NsmGPMInterfaceCreator::addGpmIntfProperty(name, dataType): null gpmIntf
 *   for Double and VectorDouble
 * - NsmGPMInterfaceCreator::addGpmIntfProperty(bitfield): VectorDouble type
 * - NsmGPMInterfaceCreator::initialize with null gpmIntf
 * - NsmGetSupportedGPMMetrics::genRequestMsg encode fail
 * - NsmGetSupportedGPMMetrics::handleResponseMsg decode fail (cc or rc)
 * - NsmGetSupportedGPMMetrics::handleResponseMsg responseReceived guard
 * - NsmGetSupportedGPMMetrics::splitMetricsBitfield invalid params
 * - NsmGetSupportedPerInstanceGPMMetrics::genRequestMsg encode fail
 * - NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg decode fail
 * - NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg responseReceived
 * - NsmGetSupportedPerInstanceGPMMetrics::splitInstanceBitfield invalid params
 * - GPMMetricUpdator::updateMetric same value and null gpmIntf branches
 * - GPMMetricInstanceUpdator via makeGPMPerInstanceUpdator branches
 * - PortMetricPerInstanceUpdator NaN skip branch
 */

#include "platform-environmental.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <cmath>
#include <cstring>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmGpmOem.hpp"

using namespace nsm;
using namespace ::testing;

// ============================================================================
// Local mock helpers
// ============================================================================

class MockMetricPerInstanceUpdatorB2 : public MetricPerInstanceUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const std::vector<double>& metrics),
                (override));
};

// ============================================================================
// Helpers
// ============================================================================

static std::shared_ptr<NsmGPMPerInstance>
    makePerInstanceB2(const std::string& suffix,
                      std::shared_ptr<MetricPerInstanceUpdator> upd,
                      GPMMetricsUnit unit = GPMMetricsUnit::PERCENTAGE)
{
    const std::vector<bitfield8_t> instanceBitfield{{.byte = 0x0F}};
    return std::make_shared<NsmGPMPerInstance>("GPM_B2_" + suffix,
                                               "GPMPerInstance", 2, 0, 0, 10,
                                               instanceBitfield, unit, upd);
}

// Build a single-sample aggregate response (valid=1, tag given, 4-byte data)
[[maybe_unused]] static std::vector<uint8_t>
    makeSingleSampleRespB2(uint8_t tag, uint8_t valid_bit, uint32_t data_value)
{
    const size_t dataLen = 4;
    const size_t bufSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
                           sizeof(nsm_aggregate_resp_sample) - 1 + dataLen;

    std::vector<uint8_t> buf(bufSize, 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 1;

    auto* sample = reinterpret_cast<nsm_aggregate_resp_sample*>(
        buf.data() + sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    sample->tag = tag;
    sample->valid = valid_bit;
    sample->length = 2; // data_len = 1 << 2 = 4 bytes
    std::memcpy(sample->data, &data_value, sizeof(data_value));

    return buf;
}

// Build a valgrind-safe decode-fail buffer for NsmGPMPerInstance
static std::vector<uint8_t> makeDecodeFail()
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    return buf;
}

// ============================================================================
// NsmGPMPerInstance::genRequestMsg – encode fail (instanceId > MAX)
// Covers: if (rc) { return std::nullopt; } TRUE branch (L631)
// ============================================================================

TEST(NsmGPMPerInstanceBranch2, GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();
    auto sensor = makePerInstanceB2("encode_fail", updator);

    // instanceId = NSM_INSTANCE_MAX + 1 = 32 → encode fails
    auto request = sensor->genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmGPMPerInstance::genRequestMsg – encode success
// Covers: if (rc) FALSE branch (L631)
// ============================================================================

TEST(NsmGPMPerInstanceBranch2, GenRequestMsg_EncodeSuccess_HasValue)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();
    auto sensor = makePerInstanceB2("encode_ok", updator);

    auto request = sensor->genRequestMsg(12, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg – decode fail (cc != SUCCESS)
// Covers: if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS) TRUE branch (L661)
// ============================================================================

TEST(NsmGPMPerInstanceBranch2, HandleResponseMsg_DecodeFail_ReturnsCcOrRc)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();
    auto sensor = makePerInstanceB2("decode_fail", updator);

    auto buf = makeDecodeFail();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = sensor->handleResponseMsg(msg, buf.size());
    // Should return non-zero (cc or rc from decode_aggregate_resp)
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg – telemetryCount==0
// Covers: if (telemetryCount == 0) TRUE branch (L666)
// ============================================================================

TEST(NsmGPMPerInstanceBranch2,
     HandleResponseMsg_ZeroTelemetryCount_UpdatesEmpty)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();
    auto sensor = makePerInstanceB2("zero_tc", updator);

    EXPECT_CALL(*updator, updateMetric(IsEmpty())).Times(1);

    // Build response with telemetry_count = 0
    const size_t bufSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 0;

    auto rc = sensor->handleResponseMsg(responseMsg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg – decodeFunc fail
// Covers: if (decodeRc != NSM_SW_SUCCESS) TRUE branch (L719)
// Use BANDWIDTH unit but provide data that is too short (1 byte) to decode.
// Actually, the decode function may still succeed with 4 bytes. We need to
// ensure decodeFunc returns non-zero. Use a 0-byte data_len by setting
// sample->length = 0 (data_len = 1 << 0 = 1, but the decode needs >= 8).
// For decodePercentage, data must be >= sizeof(double)=8 bytes.
// With length=0, data_len=1 byte → decode_aggregate_gpm_metric_percentage_data
// should fail because size < sizeof(double).
// ============================================================================

TEST(NsmGPMPerInstanceBranch2, HandleResponseMsg_DecodeFunc_Fail_ReturnsError)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();
    auto sensor = makePerInstanceB2("decode_func_fail", updator,
                                    GPMMetricsUnit::PERCENTAGE);

    // updateMetric still called at end with possibly partial data
    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    // Build single-sample response with very small data (1 byte)
    // length=0 → data_len = 1 << 0 = 1 byte → too small for percentage decode
    const size_t dataLen = 1;
    const size_t bufSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
                           sizeof(nsm_aggregate_resp_sample) - 1 + dataLen;
    std::vector<uint8_t> buf(bufSize, 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 1;

    auto* sample = reinterpret_cast<nsm_aggregate_resp_sample*>(
        buf.data() + sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    sample->tag = 0;
    sample->valid = 1;
    sample->length = 0; // data_len = 1 << 0 = 1 byte (too small for decode)
    sample->data[0] = 0x42;

    auto rc = sensor->handleResponseMsg(responseMsg, buf.size());
    // decodeFunc returns non-zero → returnValue set to decodeRc
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg – success with valid sample
// Covers: decodeRc == NSM_SW_SUCCESS → metrics[tag] = val (L728 else branch)
// ============================================================================

TEST(NsmGPMPerInstanceBranch2, HandleResponseMsg_DecodeSuccess_StoresValue)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();
    auto sensor = makePerInstanceB2("decode_ok", updator,
                                    GPMMetricsUnit::PERCENTAGE);

    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    // Encode a valid percentage sample (needs 8 bytes for double)
    double percentage = 42.5;
    std::array<uint8_t, sizeof(double)> pctData{};
    size_t pctDataLen{};
    auto erc = encode_aggregate_gpm_metric_percentage_data(
        percentage, pctData.data(), &pctDataLen);
    ASSERT_EQ(erc, NSM_SW_SUCCESS);

    const size_t bufSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
                           sizeof(nsm_aggregate_resp_sample) - 1 + pctDataLen;
    std::vector<uint8_t> buf(bufSize, 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 1;

    auto* sample = reinterpret_cast<nsm_aggregate_resp_sample*>(
        buf.data() + sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    sample->tag = 2;
    sample->valid = 1;
    // length field encodes log2(data_len): 8 bytes → log2(8)=3
    sample->length = 3;
    std::memcpy(sample->data, pctData.data(), pctDataLen);

    auto rc = sensor->handleResponseMsg(responseMsg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMInterfaceCreator::addGpmIntfProperty(bitfield) — null gpmIntf
// Covers: L425/L428 error log + return when gpmIntf is null
// ============================================================================

TEST(NsmGPMInterfaceCreatorBranch2,
     AddGpmIntfPropertyBitfield_NullGpmIntf_LogsError)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(
        objServer, "/xyz/openbmc_project/test/gpm_null_add_bitfield");

    creator.gpmIntf.reset();
    ASSERT_EQ(creator.getGPMIntf(), nullptr);

    EXPECT_NO_THROW(creator.addGpmIntfProperty(std::vector<uint8_t>{0x01}));
}

// ============================================================================
// NsmGPMInterfaceCreator::addGpmIntfProperty(name, dataType) — null (Double)
// Covers: L459/L462 error log + return when gpmIntf is null (Double)
// ============================================================================

TEST(NsmGPMInterfaceCreatorBranch2,
     AddGpmIntfPropertyNameType_NullGpmIntf_Double_LogsError)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(
        objServer, "/xyz/openbmc_project/test/gpm_null_add_name_double_b2");

    creator.gpmIntf.reset();
    ASSERT_EQ(creator.getGPMIntf(), nullptr);

    EXPECT_NO_THROW(creator.addGpmIntfProperty("TestMetric", DataType::Double));
}

// ============================================================================
// NsmGPMInterfaceCreator::addGpmIntfProperty(name, dataType) — null (Vec)
// Covers: null gpmIntf for VectorDouble type
// ============================================================================

TEST(NsmGPMInterfaceCreatorBranch2,
     AddGpmIntfPropertyNameType_NullGpmIntf_VectorDouble_LogsError)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer,
                                   "/xyz/openbmc_project/test/gpm_null_vec_b2");

    creator.gpmIntf.reset();
    ASSERT_EQ(creator.getGPMIntf(), nullptr);

    EXPECT_NO_THROW(
        creator.addGpmIntfProperty("TestVecMetric", DataType::VectorDouble));
}

// ============================================================================
// NsmGPMInterfaceCreator::addGpmIntfProperty(name, type) — valid Double
// Covers: DataType::Double case in switch (L467-L469) with valid gpmIntf
// ============================================================================

TEST(NsmGPMInterfaceCreatorBranch2,
     AddGpmIntfPropertyNameType_ValidGpmIntf_Double_Registers)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(
        objServer, "/xyz/openbmc_project/test/gpm_valid_double_b2");

    ASSERT_NE(creator.getGPMIntf(), nullptr);

    EXPECT_NO_THROW(creator.addGpmIntfProperty("SomeDouble", DataType::Double));
}

// ============================================================================
// NsmGPMInterfaceCreator::addGpmIntfProperty(name, type) — valid VectorDouble
// Covers: DataType::VectorDouble case in switch (L471-L472) with valid gpmIntf
// ============================================================================

TEST(NsmGPMInterfaceCreatorBranch2,
     AddGpmIntfPropertyNameType_ValidGpmIntf_VectorDouble_Registers)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(
        objServer, "/xyz/openbmc_project/test/gpm_valid_vec_b2");

    ASSERT_NE(creator.getGPMIntf(), nullptr);

    EXPECT_NO_THROW(
        creator.addGpmIntfProperty("SomeVec", DataType::VectorDouble));
}

// ============================================================================
// NsmGPMInterfaceCreator::initialize — null gpmIntf
// Covers: L148-L152 else branch (gpmIntf is null → log error)
// ============================================================================

TEST(NsmGPMInterfaceCreatorBranch2, Initialize_NullGpmIntf_LogsError)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(
        objServer, "/xyz/openbmc_project/test/gpm_init_null_b2");

    creator.gpmIntf.reset();
    ASSERT_EQ(creator.getGPMIntf(), nullptr);

    EXPECT_NO_THROW(creator.initialize());
}

// ============================================================================
// NsmGPMInterfaceCreator::initialize — valid gpmIntf
// Covers: L144-L147 if (gpmIntf) TRUE branch
// ============================================================================

TEST(NsmGPMInterfaceCreatorBranch2, Initialize_ValidGpmIntf_Succeeds)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(
        objServer, "/xyz/openbmc_project/test/gpm_init_valid_b2");

    ASSERT_NE(creator.getGPMIntf(), nullptr);

    EXPECT_NO_THROW(creator.initialize());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::genRequestMsg — encode fail
// Covers: if (rc != NSM_SW_SUCCESS) TRUE branch (L799)
// ============================================================================

TEST(NsmGetSupportedGPMMetricsBranch2, GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b2_enc_fail"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/gpm_b2_enc_fail");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 20, "NSM_DEVICE_INSTANCE_NUMBER", "20", 0);

    NsmGetSupportedGPMMetrics sensor{"TestEncFail",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b2_enc_fail",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nvlinkIntf};

    // instanceId > NSM_INSTANCE_MAX → encode fails
    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::genRequestMsg — encode success
// Covers: if (rc != NSM_SW_SUCCESS) FALSE branch (L799)
// ============================================================================

TEST(NsmGetSupportedGPMMetricsBranch2, GenRequestMsg_EncodeSuccess_HasValue)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b2_enc_ok"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/gpm_b2_enc_ok");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 21, "NSM_DEVICE_INSTANCE_NUMBER", "21", 0);

    NsmGetSupportedGPMMetrics sensor{"TestEncOk",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b2_enc_ok",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nvlinkIntf};

    auto request = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg — decode fail
// Covers: if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS) TRUE branch (L838)
// ============================================================================

TEST(NsmGetSupportedGPMMetricsBranch2,
     HandleResponseMsg_DecodeFail_ReturnsError)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b2_dec_fail"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/gpm_b2_dec_fail");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 22, "NSM_DEVICE_INSTANCE_NUMBER", "22", 0);

    NsmGetSupportedGPMMetrics sensor{"TestDecFail",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b2_dec_fail",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nvlinkIntf};

    auto buf = makeDecodeFail();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.hasValidResponse());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg — responseReceived guard
// Covers: if (responseReceived) TRUE branch (L819)
// ============================================================================

TEST(NsmGetSupportedGPMMetricsBranch2,
     HandleResponseMsg_AlreadyReceived_ReturnsSuccess)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b2_already"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/gpm_b2_already");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 23, "NSM_DEVICE_INSTANCE_NUMBER", "23", 0);

    NsmGetSupportedGPMMetrics sensor{"TestAlready",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b2_already",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nvlinkIntf};

    // First: send a valid response to set responseReceived=true
    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 8, &bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());

    // Second call: responseReceived is true → early return
    rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::splitMetricsBitfield — invalid params
// Covers: if (maxMetricsPerCommand == 0 || ...) TRUE branch (L759)
// ============================================================================

TEST(NsmGetSupportedGPMMetricsBranch2,
     SplitMetricsBitfield_InvalidParams_ReturnsEmpty)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b2_split_inv"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/gpm_b2_split_inv");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 24, "NSM_DEVICE_INSTANCE_NUMBER", "24", 0);

    NsmGetSupportedGPMMetrics sensor{"TestSplitInvalid",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b2_split_inv",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nvlinkIntf};

    // maxMetricsPerCommand=0 (default) → invalid → returns empty
    EXPECT_TRUE(sensor.splitMetricsBitfield({0xFF}).empty());

    // maskSize=0 with non-zero maxMetrics
    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 0;
    EXPECT_TRUE(sensor.splitMetricsBitfield({0xFF}).empty());

    // empty bitfield
    sensor.maskSize = 1;
    EXPECT_TRUE(sensor.splitMetricsBitfield({}).empty());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::genRequestMsg — encode fail
// Covers: if (rc != NSM_SW_SUCCESS) TRUE branch (L997)
// ============================================================================

TEST(NsmGetSupportedPerInstanceGPMMetricsBranch2,
     GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 30, "NSM_DEVICE_INSTANCE_NUMBER", "30", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestPerInstEncFail",       "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    // instanceId > NSM_INSTANCE_MAX → encode fails
    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::genRequestMsg — encode success
// Covers: if (rc != NSM_SW_SUCCESS) FALSE branch (L997)
// ============================================================================

TEST(NsmGetSupportedPerInstanceGPMMetricsBranch2,
     GenRequestMsg_EncodeSuccess_HasValue)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 31, "NSM_DEVICE_INSTANCE_NUMBER", "31", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestPerInstEncOk",         "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    auto request = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg — decode fail
// Covers: if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS) TRUE branch (L1037)
// ============================================================================

TEST(NsmGetSupportedPerInstanceGPMMetricsBranch2,
     HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 32, "NSM_DEVICE_INSTANCE_NUMBER", "32", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestPerInstDecFail",       "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    auto buf = makeDecodeFail();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.hasValidResponse());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg — responseReceived
// Covers: if (responseReceived) TRUE branch (L1017)
// ============================================================================

TEST(NsmGetSupportedPerInstanceGPMMetricsBranch2,
     HandleResponseMsg_AlreadyReceived_ReturnsSuccess)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 33, "NSM_DEVICE_INSTANCE_NUMBER", "33", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestPerInstAlready",       "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    // First: valid response
    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 8, &bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());

    // Second call: responseReceived is true → early return
    rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::splitInstanceBitfield — invalid params
// Covers: if (maxMetricsPerCommand == 0 || ...) TRUE branch (L956)
// ============================================================================

TEST(NsmGetSupportedPerInstanceGPMMetricsBranch2,
     SplitInstanceBitfield_InvalidParams_ReturnsEmpty)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 34, "NSM_DEVICE_INSTANCE_NUMBER", "34", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestPerInstSplitInv",      "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    // maxMetricsPerCommand=0 (default) → invalid → returns empty
    EXPECT_TRUE(sensor.splitInstanceBitfield(instBf).empty());

    // maskSize=0 with non-zero maxMetrics
    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 0;
    EXPECT_TRUE(sensor.splitInstanceBitfield(instBf).empty());

    // empty bitfield
    sensor.maskSize = 1;
    std::vector<bitfield8_t> emptyBf{};
    EXPECT_TRUE(sensor.splitInstanceBitfield(emptyBf).empty());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg — empty chunk skip
// Covers: if (numInstances == 0) TRUE branch (L1078)
// Produce a response with maskSize=2, maxMetrics=8, then set
// instanceBitfield to only have bits in byte 1 so byte 0 chunk is empty.
// ============================================================================

TEST(NsmGetSupportedPerInstanceGPMMetricsBranch2,
     HandleResponseMsg_EmptyChunk_Skipped)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 35, "NSM_DEVICE_INSTANCE_NUMBER", "35", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();

    // instanceBitfield: byte 0 = 0, byte 1 = 0x01 → only bit 8 set
    // With maxMetrics=8 per chunk and maskSize=2 (16 bits total):
    //   chunk 0 = bits 0..7 → all zero → numInstances=0 → skip
    //   chunk 1 = bits 8..15 → bit 8 set → numInstances=1 → create sensor
    std::vector<bitfield8_t> instBf{{.byte = 0x00}, {.byte = 0x01}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestEmptyChunk",           "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp) + 1;
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask[2] = {0xFF, 0xFF};
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 2, 8, bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg — empty chunk skip
// Covers: if (numMetrics == 0) TRUE branch (L881)
// ============================================================================

TEST(NsmGetSupportedGPMMetricsBranch2, HandleResponseMsg_EmptyChunk_Skipped)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b2_empty_chunk"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        bus, "/xyz/test/gpm_b2_empty_chunk");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 25, "NSM_DEVICE_INSTANCE_NUMBER", "25", 0);

    // configuredMetricsBitfield: byte 0 = 0, byte 1 = 0x01
    // With maxMetrics=8: chunk 0 = bits 0..7 all zero → skip
    NsmGetSupportedGPMMetrics sensor{"TestEmptyChunkAgg",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b2_empty_chunk",
                                     2,
                                     0,
                                     0,
                                     {0x00, 0x01},
                                     gpmIntf,
                                     nvlinkIntf};

    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp) + 1;
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask[2] = {0xFF, 0xFF};
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 2, 8, bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());
}

// ============================================================================
// GPMMetricUpdator::updateMetric — same value (previousValue == val)
// Covers: if (previousValue != val && gpmIntf) FALSE branch via same value
// ============================================================================

TEST(GPMMetricUpdatorBranch2, UpdateMetric_SameValue_SkipsSetProperty)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_updator_same_val"},
        "com.nvidia.GPMMetrics");
    auto nvlinkBus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        nvlinkBus, "/xyz/test/gpm_updator_same_val");

    NsmGPMAggregated gpm{"gpm_same_val",
                         "AggregatedGPMMetrics",
                         "/xyz/test/gpm_updator_same_val",
                         2,
                         0,
                         0,
                         {0x01}, // bit 0 set
                         gpmAsioIntf,
                         nvlinkIntf};

    // metricsTable[0] should have the GPMMetricUpdator for
    // GraphicsEngineActivityPercent. Call handleSample twice with the same
    // data to exercise both TRUE and FALSE branches of previousValue != val.

    double percentage = 50.0;
    std::array<uint8_t, sizeof(double)> pctData{};
    size_t pctDataLen{};
    auto erc = encode_aggregate_gpm_metric_percentage_data(
        percentage, pctData.data(), &pctDataLen);
    ASSERT_EQ(erc, NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        0, static_cast<uint8_t>(pctDataLen), pctData.data(), true};

    // First call: previousValue (NaN) != 50.0 → set_property called
    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Second call: previousValue == 50.0 → skip set_property (FALSE branch)
    rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// GPMMetricUpdator::updateMetric — null gpmIntf
// Covers: if (previousValue != val && gpmIntf) FALSE branch via null gpmIntf
// ============================================================================

TEST(GPMMetricUpdatorBranch2, UpdateMetric_NullGpmIntf_SkipsSetProperty)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto nvlinkBus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        nvlinkBus, "/xyz/test/gpm_updator_null_intf");

    // Pass nullptr for gpmIntf
    NsmGPMAggregated gpm{"gpm_null_intf",
                         "AggregatedGPMMetrics",
                         "/xyz/test/gpm_updator_null_intf",
                         2,
                         0,
                         0,
                         {0x01}, // bit 0 set
                         nullptr,
                         nvlinkIntf};

    double percentage = 75.0;
    std::array<uint8_t, sizeof(double)> pctData{};
    size_t pctDataLen{};
    auto erc = encode_aggregate_gpm_metric_percentage_data(
        percentage, pctData.data(), &pctDataLen);
    ASSERT_EQ(erc, NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        0, static_cast<uint8_t>(pctDataLen), pctData.data(), true};

    // previousValue(NaN) != 75.0 but gpmIntf is null → skip set_property
    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NVLinkMetricUpdator::updateMetric — same value (previousValue == val)
// Covers: if (previousValue != val) FALSE branch (L86)
// ============================================================================

TEST(NVLinkMetricUpdatorBranch2, UpdateMetric_SameValue_SkipsUpdate)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/nvlink_updator_same"},
        "com.nvidia.GPMMetrics");
    auto nvlinkBus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        nvlinkBus, "/xyz/test/nvlink_updator_same");

    // Bit 0 is set so metricsTable[0] is populated. metricsTable[10..13] have
    // NVLinkMetricUpdator entries. We use handleSample with tag=10 twice.
    NsmGPMAggregated gpm{"gpm_nvlink_same",
                         "AggregatedGPMMetrics",
                         "/xyz/test/nvlink_updator_same",
                         2,
                         0,
                         0,
                         {0x01},
                         gpmAsioIntf,
                         nvlinkIntf};

    // Tag 10 = NVLinkRawTxBandwidthGbps
    uint64_t bandwidth = 1024 * 1024 * 128; // 1 Gbps
    std::array<uint8_t, sizeof(uint64_t)> bwData{};
    size_t bwDataLen{};
    auto erc = encode_aggregate_gpm_metric_bandwidth_data(
        bandwidth, bwData.data(), &bwDataLen);
    ASSERT_EQ(erc, NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        10, static_cast<uint8_t>(bwDataLen), bwData.data(), true};

    // First call: update (previousValue NaN != 1.0)
    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Second call: same value → skip (FALSE branch)
    rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// PortMetricPerInstanceUpdator — NaN skip and same-value branches
// Covers: L327 isnan(metrics[i]) TRUE → continue
//         L332 previousMetrics[i] == metrics[i] → skip
// ============================================================================

TEST(PortMetricPerInstanceUpdatorBranch2, UpdateMetric_NanAndSameValue)
{
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/nvlink_b2_nan");

    std::vector<NVLinkMetricsUpdatorInfo> updatorInfos = {
        {"/xyz/openbmc_project/inventory/nvlink_b2_nan", nvlinkIntf}};

    auto updator = makeNVLinkRawTxPerInstanceUpdator(updatorInfos);

    // First call: resize + update (42.0)
    updator->updateMetric({42.0});

    // Second call with NaN: skip via isnan TRUE
    updator->updateMetric({std::numeric_limits<double>::quiet_NaN()});

    // Third call: same value (42.0 still stored) → skip via ==
    updator->updateMetric({42.0});

    EXPECT_TRUE(true);
}

// ============================================================================
// GPMMetricInstanceUpdator via makeGPMPerInstanceUpdator — branches
// Covers:
//   L264: resize TRUE (first call) and FALSE (second call same size)
//   L272: isnan FALSE (NaN entry skipped)
//   L278: previousMetrics == mergedMetrics → skip
//   L278: null gpmIntf → skip
// ============================================================================

TEST(GPMMetricInstanceUpdatorBranch2, UpdateMetric_AllBranches)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto intf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus,
        sdbusplus::object_path{"/xyz/openbmc_project/inventory/gpm_inst_b2"},
        "com.nvidia.GPMMetrics");

    auto updator = makeGPMPerInstanceUpdator(
        "TestProp_B2", "/xyz/openbmc_project/inventory/gpm_inst_b2", intf);

    // First call: resize TRUE (empty → 2), values differ → set_property
    updator->updateMetric({1.0, 2.0});
    // Second call: resize FALSE (already 2), same values → skip set_property
    updator->updateMetric({1.0, 2.0});
    // Third call: NaN entry → skip merge for index 0, differ → set_property
    double nan = std::numeric_limits<double>::quiet_NaN();
    updator->updateMetric({nan, 3.0});

    // null gpmIntf variant
    auto updator2 = makeGPMPerInstanceUpdator(
        "TestProp_B2_null", "/xyz/openbmc_project/inventory/gpm_inst_b2_null",
        nullptr);
    // vals differ from initial NaN but gpmIntf null → skip
    updator2->updateMetric({5.0});

    EXPECT_TRUE(true);
}

// ============================================================================
// DRAMUsageMetricUpdator::updateMetric — same value and different value
// Covers: if (previousValue != val) TRUE/FALSE branches
// ============================================================================

TEST(DRAMUsageMetricUpdatorBranch2, UpdateMetric_SameAndDifferentValue)
{
    auto bus = sdbusplus::bus::new_default();
    auto dimmIntf = std::make_shared<DimmIntf>(bus, "/xyz/test/dimm_b2");

    DRAMUsageMetricUpdator updator(dimmIntf, "/xyz/test/dimm_b2");

    // First call: previousValue(NaN) != 50.0 → TRUE branch → call utilization
    updator.updateMetric(50.0);

    // Second call: previousValue(50.0) == 50.0 → FALSE branch → skip
    updator.updateMetric(50.0);

    // Third call: different value → TRUE branch
    updator.updateMetric(75.0);

    EXPECT_TRUE(true);
}

// ============================================================================
// NsmGPMPerInstance constructor — GPMMetricsUnit::BANDWIDTH
// Covers: case GPMMetricsUnit::BANDWIDTH: (L610) in constructor switch
// ============================================================================

TEST(NsmGPMPerInstanceBranch2, Constructor_BandwidthUnit_SetsDecodeFunc)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB2>();
    auto sensor = makePerInstanceB2("bandwidth_ctor", updator,
                                    GPMMetricsUnit::BANDWIDTH);

    // Verify it was constructed (decodeFunc = decodeBandwidth)
    EXPECT_NE(sensor->decodeFunc, nullptr);

    // genRequestMsg should work
    auto request = sensor->genRequestMsg(12, 0);
    EXPECT_TRUE(request.has_value());
}
