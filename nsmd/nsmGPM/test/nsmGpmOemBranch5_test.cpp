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
 * Branch coverage batch 5 for nsmd/nsmGPM/nsmGpmOem.cpp.
 *
 * Covers untaken half-covered branches:
 * - NsmGPMPerInstance constructor: GPMMetricsUnit::BANDWIDTH path
 * - NsmGPMPerInstance::genRequestMsg: encode success + fail
 * - NsmGPMPerInstance::handleResponseMsg: decode_aggregate_resp fail,
 *   cc!=SUCCESS, telemetryCount==0, decode_aggregate_resp_sample fail,
 *   !valid || tag > max, tag >= metrics.size() resize, decode success,
 *   decode fail
 * - NsmGPMAggregated::genRequestMsg: encode success + fail
 * - GPMMetricInstanceUpdator: merge with NaN skip, previousMetrics != merged
 *   FALSE, gpmIntf valid
 * - NsmGetSupportedGPMMetrics::handleResponseMsg: success with dimmIntf +
 *   DRAM usage configured, success with non-empty chunks
 * - NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: success with
 *   non-empty chunks
 * - splitMetricsBitfield: empty bitfield, normal split
 * - splitInstanceBitfield: empty bitfield
 * - GPMMetricUpdator: gpmIntf null (FALSE branch for if gpmIntf)
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
// Mock helpers
// ============================================================================

class MockMetricPerInstanceUpdatorB5 : public MetricPerInstanceUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const std::vector<double>& metrics),
                (override));
};

// ============================================================================
// NsmGPMPerInstance constructor: BANDWIDTH unit
// ============================================================================

TEST(NsmGpmOemBranch5, NsmGPMPerInstance_Constructor_BandwidthUnit)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestBW", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::BANDWIDTH, updator);

    // decodeFunc should be set to decodeBandwidth
    EXPECT_NE(sensor.decodeFunc, nullptr);
}

// ============================================================================
// NsmGPMPerInstance::genRequestMsg: success + fail
// ============================================================================

TEST(NsmGpmOemBranch5, NsmGPMPerInstance_GenRequestMsg_Success)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestGenOk", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    auto req = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(req.has_value());
}

TEST(NsmGpmOemBranch5, NsmGPMPerInstance_GenRequestMsg_EncodeFail)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestGenFail", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    auto req = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: decode_aggregate_resp fail (cc error)
// ============================================================================

TEST(NsmGpmOemBranch5, NsmGPMPerInstance_HandleResponse_AggregateRespFail)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestAggFail", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    // Build a response with cc=NSM_ERROR
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);
    std::vector<uint8_t> response(headerSize, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, 0x01, NSM_ERROR, 0, responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: telemetryCount == 0
// ============================================================================

TEST(NsmGpmOemBranch5, NsmGPMPerInstance_HandleResponse_TelemetryCountZero)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestTelZero", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);
    std::vector<uint8_t> response(headerSize, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 0, responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: valid samples with decode success
// ============================================================================

TEST(NsmGpmOemBranch5, NsmGPMPerInstance_HandleResponse_ValidSamples)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestValidSamp", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Sample 1: tag=0, valid, percentage data
    uint32_t pctData = 5000;
    size_t s1Len = 0;
    uint8_t s1Buf[32] = {};
    encode_aggregate_resp_sample(
        0, true, reinterpret_cast<uint8_t*>(&pctData), 4,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    // Sample 2: tag=2, valid, percentage data (triggers resize)
    uint32_t pctData2 = 8000;
    size_t s2Len = 0;
    uint8_t s2Buf[32] = {};
    encode_aggregate_resp_sample(
        2, true, reinterpret_cast<uint8_t*>(&pctData2), 4,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s2Buf), &s2Len);

    std::vector<uint8_t> response(headerSize + s1Len + s2Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 2, responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, s1Buf, s1Len);
    offset += s1Len;
    memcpy(response.data() + offset, s2Buf, s2Len);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: invalid sample (!valid)
// ============================================================================

TEST(NsmGpmOemBranch5, NsmGPMPerInstance_HandleResponse_InvalidSample)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestInvSamp", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Sample with valid=false
    uint32_t pctData = 5000;
    size_t s1Len = 0;
    uint8_t s1Buf[32] = {};
    encode_aggregate_resp_sample(
        0, false, reinterpret_cast<uint8_t*>(&pctData), 4,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    std::vector<uint8_t> response(headerSize + s1Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 1, responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, s1Buf, s1Len);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: tag > max (skipped)
// ============================================================================

TEST(NsmGpmOemBranch5, NsmGPMPerInstance_HandleResponse_TagTooLarge)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestBigTag", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Sample with tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE
    uint32_t pctData = 5000;
    size_t s1Len = 0;
    uint8_t s1Buf[32] = {};
    encode_aggregate_resp_sample(
        NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1, true,
        reinterpret_cast<uint8_t*>(&pctData), 4,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    std::vector<uint8_t> response(headerSize + s1Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 1, responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, s1Buf, s1Len);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: decode fail for sample data
// ============================================================================

TEST(NsmGpmOemBranch5, NsmGPMPerInstance_HandleResponse_DecodeFailSample)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestDecFail", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Sample with bad data length for percentage decode
    uint8_t shortData = 0;
    size_t s1Len = 0;
    uint8_t s1Buf[32] = {};
    encode_aggregate_resp_sample(
        0, true, &shortData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    std::vector<uint8_t> response(headerSize + s1Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 1, responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, s1Buf, s1Len);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated::genRequestMsg: success
// ============================================================================

TEST(NsmGpmOemBranch5, NsmGPMAggregated_GenRequestMsg_Success)
{
    static boost::asio::io_context ioGenOk;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioGenOk);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b5_genok"},
        "com.nvidia.GPMMetrics");

    NsmGPMAggregated gpm("TestGenOk", "TestType", "/xyz/test/gpm_b5_genok", 2,
                         0, 0, {0x01}, gpmIntf, nullptr);

    auto req = gpm.genRequestMsg(12, 0);
    EXPECT_TRUE(req.has_value());
}

// ============================================================================
// NsmGPMAggregated::genRequestMsg: encode fail
// ============================================================================

TEST(NsmGpmOemBranch5, NsmGPMAggregated_GenRequestMsg_EncodeFail)
{
    static boost::asio::io_context ioGenFail;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioGenFail);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b5_genfail"},
        "com.nvidia.GPMMetrics");

    NsmGPMAggregated gpm("TestGenFail", "TestType", "/xyz/test/gpm_b5_genfail",
                         2, 0, 0, {0x01}, gpmIntf, nullptr);

    auto req = gpm.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

// ============================================================================
// GPMMetricInstanceUpdator: merge with NaN, previousMetrics!=merged TRUE,
// gpmIntf valid
// ============================================================================

TEST(NsmGpmOemBranch5, GPMMetricInstanceUpdator_MergeAndUpdate)
{
    static boost::asio::io_context ioMerge;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioMerge);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b5_merge"},
        "com.nvidia.GPMMetrics");

    gpmIntf->register_property("TestMerge", std::vector<double>{});
    gpmIntf->initialize();

    auto updator = makeGPMPerInstanceUpdator("TestMerge",
                                             "/xyz/test/gpm_b5_merge", gpmIntf);

    // First call: metrics with values
    std::vector<double> metrics1 = {1.0, 2.0, 3.0};
    updator->updateMetric(metrics1);

    // Second call: same metrics (previousMetrics == mergedMetrics, FALSE
    // branch)
    updator->updateMetric(metrics1);

    // Third call: partial update with NaN (exercises NaN skip + merge)
    std::vector<double> metrics2 = {std::numeric_limits<double>::quiet_NaN(),
                                    5.0};
    updator->updateMetric(metrics2);

    // Fourth call: larger vector triggers resize
    std::vector<double> metrics3 = {10.0, 20.0, 30.0, 40.0};
    updator->updateMetric(metrics3);
}

// ============================================================================
// GPMMetricUpdator: null gpmIntf (FALSE branch for if gpmIntf)
// ============================================================================

TEST(NsmGpmOemBranch5, GPMMetricUpdator_NullGpmIntf_ValueChange)
{
    static boost::asio::io_context ioNullUpd;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioNullUpd);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b5_nullupd"},
        "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "GraphicsEngineActivityPercent",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    NsmGPMAggregated gpm("TestNullUpd", "TestType", "/xyz/test/gpm_b5_nullupd",
                         2, 0, 0, {0x01}, gpmIntf, nullptr);

    // Set the updater's gpmIntf to null to test the false branch
    auto infos = gpm.getMetricInfo(0);
    ASSERT_GT(infos.size(), 0u);

    // The GPMMetricUpdator is inside the MetricInfo, we can't directly null
    // its gpmIntf, but we CAN construct a separate NsmGPMAggregated with
    // nullptr gpmIntf
    NsmGPMAggregated gpmNull("TestNullUpd2", "TestType",
                             "/xyz/test/gpm_b5_nullupd2", 2, 0, 0, {0x01},
                             nullptr, nullptr);

    auto infosNull = gpmNull.getMetricInfo(0);
    ASSERT_GT(infosNull.size(), 0u);
    ASSERT_NE(infosNull[0]->updater, nullptr);

    // Value changes but gpmIntf is null -> skip set_property (FALSE branch)
    infosNull[0]->updater->updateMetric(42.0);
    // Same value -> no-change (FALSE branch of previousValue != val)
    infosNull[0]->updater->updateMetric(42.0);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: success with non-empty chunks
// and dimmIntf configured (DRAM usage metric ID 4 in chunk)
// ============================================================================

TEST(NsmGpmOemBranch5,
     GetSupportedGPMMetrics_HandleResponse_SuccessWithDramUsage)
{
    static boost::asio::io_context ioDram;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioDram);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b5_dram"},
        "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "GraphicsEngineActivityPercent",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 40, "NSM_DEVICE_INSTANCE_NUMBER", "40", 0);

    auto bus = sdbusplus::bus::new_default();
    auto dimmIntf = std::make_shared<DimmIntf>(bus, "/xyz/test/dimm_b5_dram");

    // configuredMetricsBitfield: bit 0 and bit 4 (DRAM usage) set
    std::vector<uint8_t> configBf = {0x11}; // bits 0 and 4

    NsmGetSupportedGPMMetrics sensor{"TestB5Dram",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b5_dram",
                                     2,
                                     0,
                                     0,
                                     configBf,
                                     gpmIntf,
                                     nullptr,
                                     dimmIntf,
                                     "/xyz/test/dimm_b5_dram"};

    // Build a valid response
    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 32, &bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: success with non-empty chunks
// (no dimmIntf)
// ============================================================================

TEST(NsmGpmOemBranch5, GetSupportedGPMMetrics_HandleResponse_SuccessNoDimm)
{
    static boost::asio::io_context ioNoDimm;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioNoDimm);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b5_nodimm"},
        "com.nvidia.GPMMetrics");

    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 41, "NSM_DEVICE_INSTANCE_NUMBER", "41", 0);

    // configuredMetricsBitfield: bit 0 set only (no DRAM metric)
    std::vector<uint8_t> configBf = {0x01};

    NsmGetSupportedGPMMetrics sensor{"TestB5NoDimm",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b5_nodimm",
                                     2,
                                     0,
                                     0,
                                     configBf,
                                     gpmIntf,
                                     nullptr};

    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 32, &bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: success with
// non-empty chunks
// ============================================================================

TEST(NsmGpmOemBranch5, GetSupportedPerInstanceGPMMetrics_HandleResponse_Success)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 42, "NSM_DEVICE_INSTANCE_NUMBER", "42", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB5PerInstOk",          "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 32, &bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::splitMetricsBitfield: empty bitfield
// ============================================================================

TEST(NsmGpmOemBranch5, GetSupportedGPMMetrics_SplitBitfield_EmptyBitfield)
{
    static boost::asio::io_context ioEmpty;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioEmpty);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b5_empty_bf"},
        "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 43, "NSM_DEVICE_INSTANCE_NUMBER", "43", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB5EmptyBf",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b5_empty_bf",
                                     2,
                                     0,
                                     0,
                                     {0xFF},
                                     gpmIntf,
                                     nullptr};

    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 1;

    // Empty bitfield
    auto chunks = sensor.splitMetricsBitfield({});
    EXPECT_TRUE(chunks.empty());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::splitMetricsBitfield: normal split
// ============================================================================

TEST(NsmGpmOemBranch5, GetSupportedGPMMetrics_SplitBitfield_NormalSplit)
{
    static boost::asio::io_context ioNorm;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioNorm);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b5_norm_bf"},
        "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 44, "NSM_DEVICE_INSTANCE_NUMBER", "44", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB5NormBf",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b5_norm_bf",
                                     2,
                                     0,
                                     0,
                                     {0xFF},
                                     gpmIntf,
                                     nullptr};

    sensor.maxMetricsPerCommand = 4;
    sensor.maskSize = 2;

    // 16 bits, maxMetricsPerChunk = min(4, 16) = 4
    auto chunks = sensor.splitMetricsBitfield({0xFF, 0xFF});
    EXPECT_EQ(chunks.size(), 4u);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::splitInstanceBitfield: empty bitfield
// ============================================================================

TEST(NsmGpmOemBranch5, GetSupportedPerInstanceGPMMetrics_SplitBitfield_Empty)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 45, "NSM_DEVICE_INSTANCE_NUMBER", "45", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0xFF}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB5PerInstEmpty",       "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 1;

    // Empty bitfield
    auto chunks = sensor.splitInstanceBitfield({});
    EXPECT_TRUE(chunks.empty());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::splitInstanceBitfield: normal split
// ============================================================================

TEST(NsmGpmOemBranch5,
     GetSupportedPerInstanceGPMMetrics_SplitBitfield_NormalSplit)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 46, "NSM_DEVICE_INSTANCE_NUMBER", "46", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0xFF}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB5PerInstNorm",        "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    sensor.maxMetricsPerCommand = 4;
    sensor.maskSize = 2;

    // 16 bits, maxMetricsPerChunk = min(4, 16) = 4
    std::vector<bitfield8_t> bf{{.byte = 0xFF}, {.byte = 0xFF}};
    auto chunks = sensor.splitInstanceBitfield(bf);
    EXPECT_EQ(chunks.size(), 4u);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: cc=0 but rc!=0 path
// (cc ? cc : NSM_ERROR returns NSM_ERROR)
// ============================================================================

TEST(NsmGpmOemBranch5, GetSupportedGPMMetrics_HandleResponse_RcFailCcZero)
{
    static boost::asio::io_context ioRcFail;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioRcFail);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b5_rcfail"},
        "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 47, "NSM_DEVICE_INSTANCE_NUMBER", "47", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB5RcFail",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b5_rcfail",
                                     2,
                                     0,
                                     0,
                                     {0xFF},
                                     gpmIntf,
                                     nullptr};

    // Tiny buffer causes decode to fail with rc != 0 but cc = 0
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 2, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    // rc != 0 => returns NSM_ERROR (cc ? cc : NSM_ERROR)
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: rc fail cc=0
// ============================================================================

TEST(NsmGpmOemBranch5,
     GetSupportedPerInstanceGPMMetrics_HandleResponse_RcFailCcZero)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 48, "NSM_DEVICE_INSTANCE_NUMBER", "48", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB5PerInstRcFail",      "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    // Tiny buffer causes decode to fail with rc != 0 but cc = 0
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 2, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: cc=0, rc!=0 path
// ============================================================================

TEST(NsmGpmOemBranch5, NsmGPMPerInstance_HandleResponse_RcFail)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB5>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestRcFail", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    // Tiny buffer causes decode_aggregate_resp to fail
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 2, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}
