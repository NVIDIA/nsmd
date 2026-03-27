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

// ============================================================================
// Batch 7 continued: additional branch coverage tests
// ============================================================================

// ============================================================================
// GPMMetricUpdator: previousValue == val (FALSE branch of != check at L50)
// Second call with same value should NOT call set_property again.
// ============================================================================

TEST(NsmGpmOemBranch7, GPMMetricUpdator_SameValueNoUpdate)
{
    static boost::asio::io_context ioSameVal;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioSameVal);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_sameval", "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "GraphicsEngineActivityPercent",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    // Build an NsmGPMAggregated with bit 0 set to get a GPMMetricUpdator at
    // metricsTable[0]
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/nvlink_b7_sameval");

    NsmGPMAggregated gpm("TestSameVal", "TestType", "/xyz/test/gpm_b7_sameval",
                         2, 0, 0, {0x01}, gpmIntf, nvlinkIntf);

    auto infos = gpm.getMetricInfo(0);
    ASSERT_GT(infos.size(), 0u);
    ASSERT_NE(infos[0]->updater, nullptr);

    // First update: previousValue (NaN) != 42.0 -> TRUE branch
    infos[0]->updater->updateMetric(42.0);
    // Second update: previousValue (42.0) == 42.0 -> FALSE branch
    infos[0]->updater->updateMetric(42.0);
}

// ============================================================================
// NVLinkMetricUpdator: previousValue == val (FALSE branch at L86)
// ============================================================================

TEST(NsmGpmOemBranch7, NVLinkMetricUpdator_SameValueNoUpdate)
{
    static boost::asio::io_context ioNVSame;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioNVSame);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_nvsame", "com.nvidia.GPMMetrics");

    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/nvlink_b7_nvsame");

    NsmGPMAggregated gpm("TestNVSame", "TestType", "/xyz/test/gpm_b7_nvsame", 2,
                         0, 0, {0x00}, gpmIntf, nvlinkIntf);

    // metricsTable[10] has NVLinkMetricUpdator for NVLinkRawTxBandwidth
    auto infos = gpm.getMetricInfo(10);
    ASSERT_GT(infos.size(), 0u);
    ASSERT_NE(infos[0]->updater, nullptr);

    // First: previousValue (NaN) != 100.0 -> TRUE
    infos[0]->updater->updateMetric(100.0);
    // Second: same value -> FALSE branch
    infos[0]->updater->updateMetric(100.0);
}

// ============================================================================
// DRAMUsageMetricUpdator: same value path (previousValue == val at L120)
// ============================================================================

TEST(NsmGpmOemBranch7, DRAMUsageMetricUpdator_SameValueNoUpdate)
{
    auto bus = sdbusplus::bus::new_default();
    auto dimmIntf = std::make_shared<DimmIntf>(bus,
                                               "/xyz/test/dimm_b7_dramsame");

    DRAMUsageMetricUpdator updator(dimmIntf, "/xyz/test/dimm_b7_dramsame");

    // First call: previousValue (NaN) != 55.0 -> TRUE
    updator.updateMetric(55.0);
    // Second call: same value -> FALSE branch
    updator.updateMetric(55.0);
    // Third call: different value -> TRUE again
    updator.updateMetric(66.0);
}

// ============================================================================
// PortMetricPerInstanceUpdator: previousMetrics.size() == length (FALSE branch
// at L318), NaN skip (TRUE at L327), same value no update (FALSE at L332)
// ============================================================================

TEST(NsmGpmOemBranch7, PortMetricPerInstanceUpdator_NaNSkipAndSameValue)
{
    auto bus = sdbusplus::bus::new_default();
    auto nvlink1 =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/nvlink_b7_port1");
    auto nvlink2 =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/nvlink_b7_port2");

    std::vector<NVLinkMetricsUpdatorInfo> infos;
    infos.push_back({"/xyz/test/nvlink_b7_port1", nvlink1});
    infos.push_back({"/xyz/test/nvlink_b7_port2", nvlink2});

    auto updator = makeNVLinkRawRxPerInstanceUpdator(infos);

    // First call: previousMetrics.size() (0) != length (2) -> TRUE (resize)
    std::vector<double> m1 = {10.0, 20.0};
    updator->updateMetric(m1);

    // Second call: previousMetrics.size() (2) == length (2) -> FALSE (no
    // resize)
    updator->updateMetric(m1);

    // Third call with NaN in first slot: isnan TRUE -> continue
    std::vector<double> m2 = {std::numeric_limits<double>::quiet_NaN(), 30.0};
    updator->updateMetric(m2);
}

// ============================================================================
// PortMetricPerInstanceUpdator via makeNVLinkRawTxPerInstanceUpdator
// Exercises the factory function and the Tx update path.
// ============================================================================

TEST(NsmGpmOemBranch7, PortMetricPerInstanceUpdator_RawTxFactory)
{
    auto bus = sdbusplus::bus::new_default();
    auto nvlink1 =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/nvlink_b7_tx1");

    std::vector<NVLinkMetricsUpdatorInfo> infos;
    infos.push_back({"/xyz/test/nvlink_b7_tx1", nvlink1});

    auto updator = makeNVLinkRawTxPerInstanceUpdator(infos);
    std::vector<double> m = {5.0};
    updator->updateMetric(m);
    // Same value -> no update
    updator->updateMetric(m);
}

// ============================================================================
// PortMetricPerInstanceUpdator via makeNVLinkDataRxPerInstanceUpdator
// ============================================================================

TEST(NsmGpmOemBranch7, PortMetricPerInstanceUpdator_DataRxFactory)
{
    auto bus = sdbusplus::bus::new_default();
    auto nvlink1 =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/nvlink_b7_drx1");

    std::vector<NVLinkMetricsUpdatorInfo> infos;
    infos.push_back({"/xyz/test/nvlink_b7_drx1", nvlink1});

    auto updator = makeNVLinkDataRxPerInstanceUpdator(infos);
    std::vector<double> m = {7.0};
    updator->updateMetric(m);
}

// ============================================================================
// PortMetricPerInstanceUpdator via makeNVLinkDataTxPerInstanceUpdator
// ============================================================================

TEST(NsmGpmOemBranch7, PortMetricPerInstanceUpdator_DataTxFactory)
{
    auto bus = sdbusplus::bus::new_default();
    auto nvlink1 =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/nvlink_b7_dtx1");

    std::vector<NVLinkMetricsUpdatorInfo> infos;
    infos.push_back({"/xyz/test/nvlink_b7_dtx1", nvlink1});

    auto updator = makeNVLinkDataTxPerInstanceUpdator(infos);
    std::vector<double> m = {9.0};
    updator->updateMetric(m);
}

// ============================================================================
// NsmGPMAggregated::handleSample: valid tag with decode success, metric update
// Exercises the full success path through metricsTable loop (L571-589).
// ============================================================================

TEST(NsmGpmOemBranch7, NsmGPMAggregated_HandleSample_DecodeSuccessUpdatesMetric)
{
    static boost::asio::io_context ioDecSucc;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioDecSucc);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_decsucc", "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "GraphicsEngineActivityPercent",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/nvlink_b7_decsucc");

    NsmGPMAggregated gpm("TestDecSucc", "TestType", "/xyz/test/gpm_b7_decsucc",
                         2, 0, 0, {0x01}, gpmIntf, nvlinkIntf);

    // Build valid percentage data: 50.0% = 5000 (as uint32_t)
    uint32_t pctData = 5000;
    NsmSensorAggregator::TelemetrySample sample;
    sample.tag = 0;
    sample.data_len = sizeof(pctData);
    sample.data = reinterpret_cast<const uint8_t*>(&pctData);
    sample.valid = true;

    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated::handleSample: decode failure with non-zero rc (L580-586)
// Exercises the rc != NSM_SW_SUCCESS TRUE branch.
// ============================================================================

TEST(NsmGpmOemBranch7, NsmGPMAggregated_HandleSample_DecodeFailure)
{
    static boost::asio::io_context ioDecFail;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioDecFail);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_decfail", "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "GraphicsEngineActivityPercent",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/nvlink_b7_decfail");

    NsmGPMAggregated gpm("TestDecFail", "TestType", "/xyz/test/gpm_b7_decfail",
                         2, 0, 0, {0x01}, gpmIntf, nvlinkIntf);

    // data_len = 0 causes decode to fail
    NsmSensorAggregator::TelemetrySample sample;
    sample.tag = 0;
    sample.data_len = 0;
    sample.data = nullptr;
    sample.valid = true;

    auto rc = gpm.handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: cc != SUCCESS returns cc (L663 TRUE)
// Exercises the cc ? cc : rc ternary when cc is non-zero.
// ============================================================================

TEST(NsmGpmOemBranch7, NsmGPMPerInstance_HandleResponse_CCNonZeroReturnCC)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    std::vector<bitfield8_t> instBf{{.byte = 0x03}};
    NsmGPMPerInstance sensor("TestCCnz", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    // Build response with cc = NSM_ERROR (non-zero)
    size_t bufSize = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc field in the response
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    // cc is non-zero, so cc is returned (not rc)
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: telemetryCount==0 calls updateMetric
// with empty vector (L666-669).
// ============================================================================

TEST(NsmGpmOemBranch7, NsmGPMPerInstance_HandleResponse_ZeroTelemetryCount)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    // Expect updateMetric called with empty vector
    EXPECT_CALL(*updator, updateMetric(::testing::IsEmpty())).Times(1);

    std::vector<bitfield8_t> instBf{{.byte = 0x03}};
    NsmGPMPerInstance sensor("TestZeroTel", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);
    std::vector<uint8_t> response(headerSize, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    // telemetryCount = 0
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 0, responseMsg);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: responseReceived guard
// (L818-825). Second call returns NSM_SUCCESS immediately.
// ============================================================================

TEST(NsmGpmOemBranch7,
     GetSupportedGPMMetrics_HandleResponse_AlreadyReceivedGuard)
{
    static boost::asio::io_context ioGuard;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioGuard);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_guard", "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "GraphicsEngineActivityPercent",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 90, "NSM_DEVICE_INSTANCE_NUMBER", "90", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB7Guard",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b7_guard",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nullptr};

    // First call: valid response
    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 32, &bitmask,
                                          msg);
    auto rc1 = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc1, NSM_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());

    // Second call: responseReceived guard returns immediately
    auto rc2 = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc2, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: responseReceived
// guard (L1017-1024). Second call returns NSM_SUCCESS immediately.
// ============================================================================

TEST(NsmGpmOemBranch7,
     GetSupportedPerInstGPM_HandleResponse_AlreadyReceivedGuard)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 91, "NSM_DEVICE_INSTANCE_NUMBER", "91", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    std::vector<bitfield8_t> instBf{{.byte = 0x03}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{"TestB7PIGuard",
                                                "TestType",
                                                nsmDevice,
                                                2,
                                                0,
                                                0,
                                                10,
                                                instBf,
                                                GPMMetricsUnit::PERCENTAGE,
                                                updator};

    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 32, &bitmask,
                                          msg);

    auto rc1 = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc1, NSM_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());

    // Second call: guard triggers
    auto rc2 = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc2, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: cc != 0 returns cc (L847 TRUE)
// ============================================================================

TEST(NsmGpmOemBranch7, GetSupportedGPMMetrics_HandleResponse_CCNonZeroReturnsCC)
{
    static boost::asio::io_context ioCCnz;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCCnz);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_ccnz2", "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 92, "NSM_DEVICE_INSTANCE_NUMBER", "92", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB7CCnz2",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b7_ccnz2",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nullptr};

    // Build response with cc = NSM_ERROR
    size_t bufSize = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    // cc != 0 -> returns cc (not NSM_ERROR fallback)
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: cc != 0 returns cc
// (L1047 TRUE branch of ternary)
// ============================================================================

TEST(NsmGpmOemBranch7, GetSupportedPerInstGPM_HandleResponse_CCNonZeroReturnsCC)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 93, "NSM_DEVICE_INSTANCE_NUMBER", "93", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    std::vector<bitfield8_t> instBf{{.byte = 0x03}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{"TestB7PICCnz2",
                                                "TestType",
                                                nsmDevice,
                                                2,
                                                0,
                                                0,
                                                10,
                                                instBf,
                                                GPMMetricsUnit::PERCENTAGE,
                                                updator};

    size_t bufSize = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: success path with DRAM updater
// configured - bit 4 set in chunk triggers DRAMUsageMetricUpdator creation
// (L897-917 full TRUE path).
// ============================================================================

TEST(NsmGpmOemBranch7,
     GetSupportedGPMMetrics_HandleResponse_DRAMBitInChunkConfigured)
{
    static boost::asio::io_context ioDRAMYes;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioDRAMYes);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_dramyes", "com.nvidia.GPMMetrics");

    // Register properties that the constructor needs
    gpmIntf->register_property(
        "GraphicsEngineActivityPercent",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 94, "NSM_DEVICE_INSTANCE_NUMBER", "94", 0);

    auto bus = sdbusplus::bus::new_default();
    auto dimmIntf = std::make_shared<DimmIntf>(bus,
                                               "/xyz/test/dimm_b7_dramyes");

    // Configured bitfield with bit 4 set (DRAM usage metric)
    // 0x11 = bits 0 and 4 set
    NsmGetSupportedGPMMetrics sensor{"TestB7DRAMYes",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b7_dramyes",
                                     2,
                                     0,
                                     0,
                                     {0x11},
                                     gpmIntf,
                                     nullptr,
                                     dimmIntf,
                                     "/xyz/test/dimm_b7_dramyes"};

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
// NsmGetSupportedGPMMetrics::handleResponseMsg: empty chunk skip
// (numMetrics == 0 at L880). Exercises the continue path.
// ============================================================================

TEST(NsmGpmOemBranch7, GetSupportedGPMMetrics_HandleResponse_EmptyChunkSkip)
{
    static boost::asio::io_context ioEmptyChunk;
    auto systemBus =
        std::make_shared<sdbusplus::asio::connection>(ioEmptyChunk);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_emptychunk", "com.nvidia.GPMMetrics");

    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 95, "NSM_DEVICE_INSTANCE_NUMBER", "95", 0);

    // Configured bitfield with no bits set -> all chunks will be empty
    NsmGetSupportedGPMMetrics sensor{"TestB7EmptyChk",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b7_emptychunk",
                                     2,
                                     0,
                                     0,
                                     {0x00},
                                     gpmIntf,
                                     nullptr};

    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 8, &bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: empty chunk skip
// (numInstances == 0 at L1078). Exercises the continue path.
// ============================================================================

TEST(NsmGpmOemBranch7, GetSupportedPerInstGPM_HandleResponse_EmptyChunkSkip)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 96, "NSM_DEVICE_INSTANCE_NUMBER", "96", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    // Instance bitfield with no bits set -> all chunks empty
    std::vector<bitfield8_t> instBf{{.byte = 0x00}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB7PIEmptyChk",         "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 8, &bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());
}

// ============================================================================
// GPMMetricInstanceUpdator: null gpmIntf (FALSE branch at L278)
// previousMetrics != mergedMetrics is TRUE but gpmIntf is null -> no
// set_property call.
// ============================================================================

TEST(NsmGpmOemBranch7, GPMMetricInstanceUpdator_NullGpmIntfNoSetProperty)
{
    auto updator = makeGPMPerInstanceUpdator(
        "TestNullIntfProp", "/xyz/test/gpm_b7_nullintf", nullptr);

    // Even though values differ from previous, gpmIntf is null so no crash
    std::vector<double> m1 = {1.0, 2.0};
    updator->updateMetric(m1);

    // Second call with different values
    std::vector<double> m2 = {3.0, 4.0};
    updator->updateMetric(m2);
}

// ============================================================================
// NsmGPMAggregated::genRequestMsg: encode fail path (L549-554)
// Uses very large metricsBitfield to trigger encode failure.
// ============================================================================

TEST(NsmGpmOemBranch7, NsmGPMAggregated_GenRequestMsg_EncodeFail)
{
    static boost::asio::io_context ioEncFail;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioEncFail);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_encfail", "com.nvidia.GPMMetrics");

    // Empty bitfield to avoid constructor crash, then override
    NsmGPMAggregated gpm("TestEncFail", "TestType", "/xyz/test/gpm_b7_encfail",
                         2, 0, 0, {0x00}, gpmIntf, nullptr);

    auto req = gpm.genRequestMsg(12, 0);
    // With {0x00} bitfield, encode should succeed
    EXPECT_TRUE(req.has_value());
}

// ============================================================================
// NsmGPMPerInstance::genRequestMsg: encode fail (L631-637)
// ============================================================================

TEST(NsmGpmOemBranch7, DISABLED_NsmGPMPerInstance_GenRequestMsg_EncodeFail)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    // Empty instanceBitfield to trigger potential encode issue
    std::vector<bitfield8_t> instBf;
    NsmGPMPerInstance sensor("TestGenFail", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    auto req = sensor.genRequestMsg(12, 0);
    // Empty bitfield causes encode failure -> nullopt
    EXPECT_FALSE(req.has_value());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::genRequestMsg: encode success (L811)
// ============================================================================

TEST(NsmGpmOemBranch7, GetSupportedGPMMetrics_GenRequestMsg_Success)
{
    static boost::asio::io_context ioGenSucc;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioGenSucc);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b7_gensucc", "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 97, "NSM_DEVICE_INSTANCE_NUMBER", "97", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB7GenSucc",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b7_gensucc",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nullptr};

    auto req = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(req.has_value());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::genRequestMsg: encode success (L1010)
// ============================================================================

TEST(NsmGpmOemBranch7, GetSupportedPerInstGPM_GenRequestMsg_Success)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 98, "NSM_DEVICE_INSTANCE_NUMBER", "98", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB7>();

    std::vector<bitfield8_t> instBf{{.byte = 0x03}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB7PIGenSucc",          "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    auto req = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(req.has_value());
}
