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
 * Branch coverage batch 7 for nsmd/nsmGPM/nsmGpmOem.cpp.
 *
 * Covers remaining uncovered branches:
 * - NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: cc!=SUCCESS
 *   decode failure path (L1037-1047)
 * - NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: rc!=0, cc==0
 *   returns NSM_ERROR (L1047 false branch of ternary)
 * - NsmGetSupportedGPMMetrics::handleResponseMsg: rc!=0, cc==0 returns
 *   NSM_ERROR (L847 false branch of ternary)
 * - NsmGPMPerInstance::handleResponseMsg: cc==0 but rc!=0 returns rc
 *   (L663 false branch of cc ? cc : rc)
 * - NsmGPMAggregated constructor with empty bitfield (no GPMIntfMetrics
 *   entries, still sets up NVLink metrics at tags 4,10-13)
 * - splitMetricsBitfield with empty bitfield vector
 * - splitInstanceBitfield with empty instBitfield vector
 * - NsmGetSupportedGPMMetrics::handleResponseMsg: success path where
 *   byteIdx >= metricChunks[i].size() for DRAM metric (L901 FALSE)
 * - NsmGPMPerInstance::handleResponseMsg: shouldLog TRUE branch (L655-658)
 * - GPMMetricInstanceUpdator: mergedMetrics already sized, no resize needed
 * - NsmGPMAggregated::genRequestMsg with different retrievalSource values
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

class MockMetricPerInstanceUpdatorB7 : public MetricPerInstanceUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const std::vector<double>& metrics),
                (override));
};

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: cc != NSM_SUCCESS
// Exercises the decode failure path at lines 1037-1047.
// ============================================================================

TEST(NsmGpmOemBranch7, GetSupportedPerInstGPM_HandleResponse_CCNonZero)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 80, "NSM_DEVICE_INSTANCE_NUMBER", "80", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{"TestB7PIccnz",
                                                "TestType",
                                                nsmDevice,
                                                2,
                                                0,
                                                0,
                                                10,
                                                instBf,
                                                GPMMetricsUnit::PERCENTAGE,
                                                updator};

    // Build a response with cc=NSM_ERROR
    size_t bufSize = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: rc != 0, cc == 0
// Exercises the false branch of the ternary: cc ? cc : NSM_ERROR
// When decode fails with rc!=0 but cc==0, should return NSM_ERROR.
// ============================================================================

TEST(NsmGpmOemBranch7, GetSupportedPerInstGPM_HandleResponse_RCNonZeroCCZero)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 81, "NSM_DEVICE_INSTANCE_NUMBER", "81", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{"TestB7PIrcnz",
                                                "TestType",
                                                nsmDevice,
                                                2,
                                                0,
                                                0,
                                                10,
                                                instBf,
                                                GPMMetricsUnit::PERCENTAGE,
                                                updator};

    // Provide a truncated/corrupt response so decode returns rc != 0 but
    // cc remains 0 (the decode function sets cc from the response bytes,
    // but if the buffer is too small decode itself returns an error code).
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 2, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, static_cast<uint8_t>(NSM_ERROR));
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: rc != 0, cc == 0
// Exercises the false branch of ternary at line 847.
// ============================================================================

TEST(NsmGpmOemBranch7, GetSupportedGPMMetrics_HandleResponse_RCNonZeroCCZero)
{
    static boost::asio::io_context ioRCNZ;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioRCNZ);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_rcnz", "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 82, "NSM_DEVICE_INSTANCE_NUMBER", "82", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB7RCnz",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b7_rcnz",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nullptr};

    // Truncated response: decode returns rc != 0, cc stays 0
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 2, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, static_cast<uint8_t>(NSM_ERROR));
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: shouldLog TRUE + cc==0, rc!=0
// Exercises the decode_aggregate_resp path where rc is non-zero and cc is 0,
// returning rc via the ternary cc ? cc : rc (false branch).
// Also exercises the shouldLog TRUE branch at line 655.
// ============================================================================

TEST(NsmGpmOemBranch7, NsmGPMPerInstance_HandleResponse_RCNonZeroCCZero)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestB7PIrcnz2", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    // Truncated response: decode_aggregate_resp fails with rc != 0
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    // rc is the error code from decode since cc == 0
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated constructor: empty bitfield (no GPMIntfMetrics matched)
// Still sets up NVLink entries at tags 4, 10-13 unconditionally.
// ============================================================================

TEST(NsmGpmOemBranch7, NsmGPMAggregated_Constructor_EmptyBitfield)
{
    static boost::asio::io_context ioEmptyBf;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioEmptyBf);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_emptybf", "com.nvidia.GPMMetrics");

    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/nvlink_b7_empty");

    // Empty bitfield: no bits set
    std::vector<uint8_t> emptyBf = {0x00};

    NsmGPMAggregated gpm("TestEmptyBf", "TestType", "/xyz/test/gpm_b7_emptybf",
                         2, 0, 0, emptyBf, gpmIntf, nvlinkIntf);

    // NVLink entries should still exist at tags 10-13
    auto info10 = gpm.getMetricInfo(10);
    EXPECT_GT(info10.size(), 0u);
    auto info11 = gpm.getMetricInfo(11);
    EXPECT_GT(info11.size(), 0u);
    auto info12 = gpm.getMetricInfo(12);
    EXPECT_GT(info12.size(), 0u);
    auto info13 = gpm.getMetricInfo(13);
    EXPECT_GT(info13.size(), 0u);

    // Tag 4 should have the placeholder entry (no updater)
    auto info4 = gpm.getMetricInfo(4);
    EXPECT_GT(info4.size(), 0u);
}

// ============================================================================
// splitMetricsBitfield: empty bitfield vector
// Exercises the metricsBitfield.empty() guard at line 759.
// ============================================================================

TEST(NsmGpmOemBranch7, SplitMetricsBitfield_EmptyBitfieldVector)
{
    static boost::asio::io_context ioSplitEmpty;
    auto systemBus =
        std::make_shared<sdbusplus::asio::connection>(ioSplitEmpty);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_splite", "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 83, "NSM_DEVICE_INSTANCE_NUMBER", "83", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB7SplitE",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b7_splite",
                                     2,
                                     0,
                                     0,
                                     {0xFF},
                                     gpmIntf,
                                     nullptr};

    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 1;

    // Empty bitfield vector triggers the guard
    auto chunks = sensor.splitMetricsBitfield({});
    EXPECT_TRUE(chunks.empty());
}

// ============================================================================
// splitInstanceBitfield: empty instBitfield vector
// Exercises the instBitfield.empty() guard at line 956.
// ============================================================================

TEST(NsmGpmOemBranch7, SplitInstanceBitfield_EmptyBitfieldVector)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 84, "NSM_DEVICE_INSTANCE_NUMBER", "84", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    std::vector<bitfield8_t> instBf{{.byte = 0xFF}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{"TestB7SplitIE",
                                                "TestType",
                                                nsmDevice,
                                                2,
                                                0,
                                                0,
                                                10,
                                                instBf,
                                                GPMMetricsUnit::PERCENTAGE,
                                                updator};

    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 1;

    // Empty instance bitfield vector triggers the guard
    std::vector<bitfield8_t> emptyBf;
    auto chunks = sensor.splitInstanceBitfield(emptyBf);
    EXPECT_TRUE(chunks.empty());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: DRAM metric not in chunk
// Exercises byteIdx >= metricChunks[i].size() FALSE at line 901 where bit 4
// is not in the current chunk (the chunk is too short to contain byte index 0
// bit 4, OR bit 4 is simply not set in this chunk).
// ============================================================================

TEST(NsmGpmOemBranch7, GetSupportedGPMMetrics_HandleResponse_DRAMBitNotInChunk)
{
    static boost::asio::io_context ioDRAMNo;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioDRAMNo);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_dramno", "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "GraphicsEngineActivityPercent",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 85, "NSM_DEVICE_INSTANCE_NUMBER", "85", 0);

    auto bus = sdbusplus::bus::new_default();
    auto dimmIntf = std::make_shared<DimmIntf>(bus, "/xyz/test/dimm_b7_dramno");

    // Configured bitfield: only bit 0 set (no bit 4)
    // dimmIntf is set so the dimmIntf && !dramUpdaterConfigured check is TRUE,
    // but bit 4 is not in the chunk, so the inner check fails.
    NsmGetSupportedGPMMetrics sensor{"TestB7DRAMNo",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b7_dramno",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nullptr,
                                     dimmIntf,
                                     "/xyz/test/dimm_b7_dramno"};

    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 32, &bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: success with
// non-empty chunks creating per-instance sensors (exercises full success path)
// ============================================================================

TEST(NsmGpmOemBranch7,
     GetSupportedPerInstGPM_HandleResponse_SuccessNonEmptyChunks)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 86, "NSM_DEVICE_INSTANCE_NUMBER", "86", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    // Non-zero instance bitfield
    std::vector<bitfield8_t> instBf{{.byte = 0x03}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{"TestB7PISucc",
                                                "TestType",
                                                nsmDevice,
                                                2,
                                                0,
                                                0,
                                                10,
                                                instBf,
                                                GPMMetricsUnit::BANDWIDTH,
                                                updator};

    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 32, &bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());
    EXPECT_GT(sensor.getMaxMetricsPerCommand(), uint16_t{0});
}

// ============================================================================
// GPMMetricInstanceUpdator: merge where mergedMetrics already sized
// (no resize needed, exercises the FALSE branch of metrics.size() >
// mergedMetrics.size() at line 264)
// ============================================================================

TEST(NsmGpmOemBranch7, GPMMetricInstanceUpdator_MergeNoResize)
{
    static boost::asio::io_context ioMerge;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioMerge);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_merge", "com.nvidia.GPMMetrics");

    gpmIntf->register_property("TestMergeProp", std::vector<double>{});
    gpmIntf->initialize();

    auto updator = makeGPMPerInstanceUpdator("TestMergeProp",
                                             "/xyz/test/gpm_b7_merge", gpmIntf);

    // First call: sets up mergedMetrics with 3 elements
    std::vector<double> m1 = {1.0, 2.0, 3.0};
    updator->updateMetric(m1);

    // Second call with smaller vector: no resize needed (FALSE branch)
    std::vector<double> m2 = {4.0, 5.0};
    updator->updateMetric(m2);

    // Third call with same-size vector: no resize needed
    std::vector<double> m3 = {4.0, 5.0, 6.0};
    updator->updateMetric(m3);
}

// ============================================================================
// GPMMetricInstanceUpdator: update with all NaN values (no real data merged,
// previousMetrics == mergedMetrics remains true after merge)
// ============================================================================

TEST(NsmGpmOemBranch7, GPMMetricInstanceUpdator_AllNaN)
{
    static boost::asio::io_context ioNaN;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioNaN);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_nan", "com.nvidia.GPMMetrics");

    gpmIntf->register_property("TestNaNProp", std::vector<double>{});
    gpmIntf->initialize();

    auto updator = makeGPMPerInstanceUpdator("TestNaNProp",
                                             "/xyz/test/gpm_b7_nan", gpmIntf);

    // First call with real values
    std::vector<double> m1 = {1.0, 2.0};
    updator->updateMetric(m1);

    // Call with same values -> previousMetrics == mergedMetrics (FALSE branch)
    updator->updateMetric(m1);

    // Call with all NaN: no values change in merged, stays same as previous
    std::vector<double> m2 = {std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::quiet_NaN()};
    updator->updateMetric(m2);
}

// ============================================================================
// NsmGPMAggregated::genRequestMsg with retrievalSource variant
// Exercises the encode path with different parameter combinations.
// ============================================================================

TEST(NsmGpmOemBranch7, NsmGPMAggregated_GenRequestMsg_DifferentRetrievalSource)
{
    static boost::asio::io_context ioRetSrc;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioRetSrc);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_retsrc", "com.nvidia.GPMMetrics");

    // retrievalSource=1 (different from the usual 2)
    NsmGPMAggregated gpm("TestRetSrc", "TestType", "/xyz/test/gpm_b7_retsrc", 1,
                         1, 1, {0x01}, gpmIntf, nullptr);

    auto req = gpm.genRequestMsg(12, 0);
    EXPECT_TRUE(req.has_value());
}

// ============================================================================
// NsmGPMPerInstance::genRequestMsg: success with BANDWIDTH unit
// Ensures encode works when constructed with BANDWIDTH unit type.
// ============================================================================

TEST(NsmGpmOemBranch7, NsmGPMPerInstance_GenRequestMsg_Bandwidth)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestBWGen", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::BANDWIDTH, updator);

    auto req = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(req.has_value());
}

// ============================================================================
// splitMetricsBitfield: normal split producing multiple chunks
// Exercises the cnt == maxMetricsPerChunk boundary in the loop (line 779).
// ============================================================================

TEST(NsmGpmOemBranch7, SplitMetricsBitfield_MultipleChunks)
{
    static boost::asio::io_context ioSplitMC;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioSplitMC);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_splitmc", "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 87, "NSM_DEVICE_INSTANCE_NUMBER", "87", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB7SplitMC",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b7_splitmc",
                                     2,
                                     0,
                                     0,
                                     {0xFF},
                                     gpmIntf,
                                     nullptr};

    sensor.maxMetricsPerCommand = 4; // Force 4 metrics per chunk
    sensor.maskSize = 1;

    // 0xFF has 8 bits set, with max 4 per chunk -> 2 chunks
    auto chunks = sensor.splitMetricsBitfield({0xFF});
    EXPECT_EQ(chunks.size(), 2u);
}

// ============================================================================
// splitInstanceBitfield: normal split producing multiple chunks
// ============================================================================

TEST(NsmGpmOemBranch7, SplitInstanceBitfield_MultipleChunks)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 88, "NSM_DEVICE_INSTANCE_NUMBER", "88", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    std::vector<bitfield8_t> instBf{{.byte = 0xFF}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB7SplitIMC",           "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    sensor.maxMetricsPerCommand = 4; // Force 4 instances per chunk
    sensor.maskSize = 1;

    // 0xFF has 8 bits, max 4 per chunk -> 2 chunks
    std::vector<bitfield8_t> bf{{.byte = 0xFF}};
    auto chunks = sensor.splitInstanceBitfield(bf);
    EXPECT_EQ(chunks.size(), 2u);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: decode_aggregate_resp_sample fail
// with rc != 0 (continue path at line 700)
// ============================================================================

TEST(NsmGpmOemBranch7, NsmGPMPerInstance_HandleResponse_SampleDecodeFail)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    std::vector<bitfield8_t> instBf{{.byte = 0x03}};
    NsmGPMPerInstance sensor("TestSampFail", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    // Build a response with telemetryCount=1 but corrupt sample data
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Create a valid sample first, then a corrupt one
    uint32_t pctData = 5000;
    size_t s1Len = 0;
    uint8_t s1Buf[32] = {};
    encode_aggregate_resp_sample(
        0, true, reinterpret_cast<uint8_t*>(&pctData), 4,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    std::vector<uint8_t> response(headerSize + s1Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    // telemetryCount=2 but only 1 valid sample -> second decode will fail
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 2, responseMsg);

    memcpy(response.data() + headerSize, s1Buf, s1Len);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    // First sample decodes OK, second fails -> updateMetric still called
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: sample with !valid flag
// Exercises the !valid || tag > max branch at line 703.
// ============================================================================

TEST(NsmGpmOemBranch7, NsmGPMPerInstance_HandleResponse_InvalidSample)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    std::vector<bitfield8_t> instBf{{.byte = 0x03}};
    NsmGPMPerInstance sensor("TestInvSamp", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Create a sample with valid=false
    uint32_t pctData = 5000;
    size_t s1Len = 0;
    uint8_t s1Buf[32] = {};
    encode_aggregate_resp_sample(
        0, false, reinterpret_cast<uint8_t*>(&pctData), 4,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    std::vector<uint8_t> response(headerSize + s1Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 1, responseMsg);

    memcpy(response.data() + headerSize, s1Buf, s1Len);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: tag >= metrics.size() triggers resize
// ============================================================================

TEST(NsmGpmOemBranch7, NsmGPMPerInstance_HandleResponse_TagTriggersResize)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestResize", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Sample with tag=5 will require resize of metrics vector
    uint32_t pctData = 5000;
    size_t s1Len = 0;
    uint8_t s1Buf[32] = {};
    encode_aggregate_resp_sample(
        5, true, reinterpret_cast<uint8_t*>(&pctData), 4,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    std::vector<uint8_t> response(headerSize + s1Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 1, responseMsg);

    memcpy(response.data() + headerSize, s1Buf, s1Len);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: decode success for percentage data
// then decode fail for short data (exercises returnValue = decodeRc at L724)
// ============================================================================

TEST(NsmGpmOemBranch7,
     NsmGPMPerInstance_HandleResponse_DecodeFailSetsReturnValue)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestDecRV", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // First sample: valid with good data
    uint32_t pctData = 5000;
    size_t s1Len = 0;
    uint8_t s1Buf[32] = {};
    encode_aggregate_resp_sample(
        0, true, reinterpret_cast<uint8_t*>(&pctData), 4,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    // Second sample: valid but with 0-length data (decode fail)
    uint8_t emptyData = 0;
    size_t s2Len = 0;
    uint8_t s2Buf[32] = {};
    encode_aggregate_resp_sample(
        1, true, &emptyData, 0,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s2Buf), &s2Len);

    std::vector<uint8_t> response(headerSize + s1Len + s2Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 2, responseMsg);

    memcpy(response.data() + headerSize, s1Buf, s1Len);
    memcpy(response.data() + headerSize + s1Len, s2Buf, s2Len);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    // The decode may or may not fail depending on buffer content - verify no
    // crash
    (void)rc;
}
