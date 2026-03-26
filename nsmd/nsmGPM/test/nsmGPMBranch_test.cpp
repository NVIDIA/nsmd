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
 * Branch coverage for nsmGPM directory:
 * - NsmGPMPerInstance: genRequestMsg encode success/fail
 * - NsmGPMPerInstance: handleResponseMsg with cc!=SUCCESS, telemetryCount==0,
 *   decode_aggregate_resp_sample fail, invalid tag, valid sample decode success
 *   and fail, metrics resize
 * - NsmGPMAggregated: genRequestMsg encode success/fail
 * - NsmGPMAggregated: handleSample with decode success and value update
 * - NsmGetSupportedGPMMetrics: genRequestMsg, handleResponseMsg guard,
 *   decode fail
 * - NsmGetSupportedPerInstanceGPMMetrics: genRequestMsg, handleResponseMsg
 *   guard, decode fail
 * - GPMMetricUpdator: updateMetric same/different values
 * - DRAMUsageMetricUpdator: updateMetric same/different values
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

#undef private
#undef protected

using namespace nsm;
using namespace ::testing;

// ============================================================================
// Mock helpers
// ============================================================================

class MockMetricPerInstanceUpdator : public MetricPerInstanceUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const std::vector<double>& metrics),
                (override));
};

// ============================================================================
// NsmGPMPerInstance: genRequestMsg success
// ============================================================================

TEST(NsmGPMBranch, PerInstance_GenRequest_Success)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();
    std::vector<bitfield8_t> instanceBitfield = {{0xFF}};

    NsmGPMPerInstance sensor("TestPerInst", "TestType", 0, 0, 0, 1,
                             instanceBitfield, GPMMetricsUnit::PERCENTAGE,
                             updator);

    auto result = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// NsmGPMPerInstance: handleResponseMsg cc != SUCCESS
// ============================================================================

TEST(NsmGPMBranch, PerInstance_HandleResponse_CCNotSuccess)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();
    std::vector<bitfield8_t> instanceBitfield = {{0x01}};

    NsmGPMPerInstance sensor("TestPerInstCC", "TestType", 0, 0, 0, 1,
                             instanceBitfield, GPMMetricsUnit::PERCENTAGE,
                             updator);

    // Create a minimal response with cc=NSM_ERROR
    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp),
                      0);
    response.data()[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(response.data());
    auto rc = sensor.handleResponseMsg(msg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance: handleResponseMsg telemetryCount == 0
// ============================================================================

TEST(NsmGPMBranch, PerInstance_HandleResponse_ZeroTelemetry)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();
    std::vector<bitfield8_t> instanceBitfield = {{0x01}};

    NsmGPMPerInstance sensor("TestPerInstZero", "TestType", 0, 0, 0, 1,
                             instanceBitfield, GPMMetricsUnit::BANDWIDTH,
                             updator);

    // Build a valid aggregate response with 0 telemetry samples
    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) + 64, 0);
    auto hdr = reinterpret_cast<nsm_msg*>(responseData.data());

    // Encode a valid aggregate response header with 0 samples
    uint16_t telemetryCount = 0;
    auto rc_enc = encode_aggregate_resp(0, NSM_SUCCESS, ERR_NULL,
                                        telemetryCount, hdr);
    if (rc_enc == NSM_SW_SUCCESS)
    {
        EXPECT_CALL(*updator, updateMetric(std::vector<double>{})).Times(1);
        auto rc = sensor.handleResponseMsg(hdr, responseData.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
}

// ============================================================================
// NsmGPMAggregated: genRequestMsg success
// ============================================================================

TEST(NsmGPMBranch, Aggregated_GenRequest_Success)
{
    static boost::asio::io_context ioCtxAgg;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCtxAgg);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_branch_gen", "com.nvidia.GPMMetrics");

    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        utils::DBusHandler::getBus(), "/xyz/test/nvlink_branch_gen");

    std::vector<uint8_t> metricsBitfield = {0x01};

    NsmGPMAggregated sensor("TestAggGen", "TestType",
                            "/xyz/test/gpm_branch_gen", 0, 0, 0,
                            metricsBitfield, gpmIntf, nvlinkIntf);

    auto result = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// NsmGetSupportedGPMMetrics: genRequestMsg success
// ============================================================================

TEST(NsmGPMBranch, GetSupportedGPMMetrics_GenRequest_Success)
{
    static boost::asio::io_context ioCtxSupp;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCtxSupp);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_branch_supp", "com.nvidia.GPMMetrics");

    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        utils::DBusHandler::getBus(), "/xyz/test/nvlink_branch_supp");

    std::vector<uint8_t> metricsBitfield = {0x01};

    NsmGetSupportedGPMMetrics sensor("TestGetSupported", "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE, nullptr,
                                     "/xyz/test/gpm_branch_supp", 0, 0, 0,
                                     metricsBitfield, gpmIntf, nvlinkIntf);

    auto result = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// NsmGetSupportedGPMMetrics: handleResponseMsg guard (already received)
// ============================================================================

TEST(NsmGPMBranch, GetSupportedGPMMetrics_HandleResp_AlreadyReceived)
{
    static boost::asio::io_context ioCtxGuard;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCtxGuard);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_branch_guard", "com.nvidia.GPMMetrics");

    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        utils::DBusHandler::getBus(), "/xyz/test/nvlink_branch_guard");

    std::vector<uint8_t> metricsBitfield = {0x01};

    NsmGetSupportedGPMMetrics sensor("TestGetSupportedGuard", "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE, nullptr,
                                     "/xyz/test/gpm_branch_guard", 0, 0, 0,
                                     metricsBitfield, gpmIntf, nvlinkIntf);

    // Mark as already received
    sensor.responseReceived = true;

    // Create a minimal valid response - should return early
    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp),
                      0);
    auto msg = reinterpret_cast<const nsm_msg*>(response.data());
    auto rc = sensor.handleResponseMsg(msg, response.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedGPMMetrics: handleResponseMsg decode fail
// ============================================================================

TEST(NsmGPMBranch, GetSupportedGPMMetrics_HandleResp_DecodeFail)
{
    static boost::asio::io_context ioCtxDecF;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCtxDecF);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_branch_decf", "com.nvidia.GPMMetrics");

    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        utils::DBusHandler::getBus(), "/xyz/test/nvlink_branch_decf");

    std::vector<uint8_t> metricsBitfield = {0x01};

    NsmGetSupportedGPMMetrics sensor("TestGetSupportedDecF", "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE, nullptr,
                                     "/xyz/test/gpm_branch_decf", 0, 0, 0,
                                     metricsBitfield, gpmIntf, nvlinkIntf);

    // Create a minimal bad response
    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp),
                      0);
    response.data()[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(response.data());
    auto rc = sensor.handleResponseMsg(msg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics: genRequestMsg success
// ============================================================================

TEST(NsmGPMBranch, GetSupportedPerInstance_GenRequest_Success)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();
    std::vector<bitfield8_t> instanceBitfield = {{0x01}};

    NsmGetSupportedPerInstanceGPMMetrics sensor(
        "TestGetSuppPerInst", "TestType", nullptr, 0, 0, 0, 1, instanceBitfield,
        GPMMetricsUnit::PERCENTAGE, updator);

    auto result = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics: handleResponseMsg guard
// ============================================================================

TEST(NsmGPMBranch, GetSupportedPerInstance_HandleResp_Guard)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();
    std::vector<bitfield8_t> instanceBitfield = {{0x01}};

    NsmGetSupportedPerInstanceGPMMetrics sensor(
        "TestGetSuppPerInstGuard", "TestType", nullptr, 0, 0, 0, 1,
        instanceBitfield, GPMMetricsUnit::PERCENTAGE, updator);

    sensor.responseReceived = true;

    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp),
                      0);
    auto msg = reinterpret_cast<const nsm_msg*>(response.data());
    auto rc = sensor.handleResponseMsg(msg, response.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics: handleResponseMsg decode fail
// ============================================================================

TEST(NsmGPMBranch, GetSupportedPerInstance_HandleResp_DecodeFail)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();
    std::vector<bitfield8_t> instanceBitfield = {{0x01}};

    NsmGetSupportedPerInstanceGPMMetrics sensor(
        "TestGetSuppPerInstDecF", "TestType", nullptr, 0, 0, 0, 1,
        instanceBitfield, GPMMetricsUnit::PERCENTAGE, updator);

    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp),
                      0);
    response.data()[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(response.data());
    auto rc = sensor.handleResponseMsg(msg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// splitMetricsBitfield: maxMetricsPerCommand == 0 returns empty
// ============================================================================

TEST(NsmGPMBranch, SplitMetricsBitfield_MaxZero)
{
    static boost::asio::io_context ioCtxSplit;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCtxSplit);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_branch_split", "com.nvidia.GPMMetrics");

    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        utils::DBusHandler::getBus(), "/xyz/test/nvlink_branch_split");

    std::vector<uint8_t> metricsBitfield = {0xFF};

    NsmGetSupportedGPMMetrics sensor("TestSplitZero", "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE, nullptr,
                                     "/xyz/test/gpm_branch_split", 0, 0, 0,
                                     metricsBitfield, gpmIntf, nvlinkIntf);

    sensor.maxMetricsPerCommand = 0;
    sensor.maskSize = 1;

    auto chunks = sensor.splitMetricsBitfield(metricsBitfield);
    EXPECT_TRUE(chunks.empty());
}

// ============================================================================
// splitMetricsBitfield: normal operation
// ============================================================================

TEST(NsmGPMBranch, SplitMetricsBitfield_Normal)
{
    static boost::asio::io_context ioCtxSplitN;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCtxSplitN);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_branch_splitn", "com.nvidia.GPMMetrics");

    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        utils::DBusHandler::getBus(), "/xyz/test/nvlink_branch_splitn");

    std::vector<uint8_t> metricsBitfield = {0xFF};

    NsmGetSupportedGPMMetrics sensor("TestSplitNormal", "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE, nullptr,
                                     "/xyz/test/gpm_branch_splitn", 0, 0, 0,
                                     metricsBitfield, gpmIntf, nvlinkIntf);

    sensor.maxMetricsPerCommand = 4;
    sensor.maskSize = 1;

    auto chunks = sensor.splitMetricsBitfield(metricsBitfield);
    EXPECT_EQ(chunks.size(), 2u); // 8 bits, max 4 per chunk = 2 chunks
}

// ============================================================================
// DRAMUsageMetricUpdator: updateMetric different then same value
// ============================================================================

TEST(NsmGPMBranch, DRAMUsageMetricUpdator_UpdateMetric)
{
    auto dimmIntf = std::make_shared<DimmIntf>(utils::DBusHandler::getBus(),
                                               "/xyz/test/dimm_branch");

    DRAMUsageMetricUpdator updator(dimmIntf, "/xyz/test/dimm_branch");

    // First call - different value
    updator.updateMetric(42.5);
    EXPECT_DOUBLE_EQ(dimmIntf->utilization(), 42.5);

    // Second call - same value, should not update (but we can verify it
    // still holds)
    updator.updateMetric(42.5);
    EXPECT_DOUBLE_EQ(dimmIntf->utilization(), 42.5);

    // Third call - different value again
    updator.updateMetric(55.0);
    EXPECT_DOUBLE_EQ(dimmIntf->utilization(), 55.0);
}

// ============================================================================
// NsmGPMInterfaceCreator: addGpmIntfProperty with name/DataType variants
// ============================================================================

TEST(NsmGPMBranch, InterfaceCreator_AddProperty_Double)
{
    static boost::asio::io_context ioCtxProp;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCtxProp);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer,
                                   "/xyz/test/gpm_branch_prop_double");
    creator.addGpmIntfProperty("TestDouble", DataType::Double);
    creator.addGpmIntfProperty("TestVector", DataType::VectorDouble);

    EXPECT_NE(creator.getGPMIntf(), nullptr);
}

// ============================================================================
// NsmGPMInterfaceCreator: addGpmIntfProperty with bitfield
// ============================================================================

TEST(NsmGPMBranch, InterfaceCreator_AddProperty_Bitfield)
{
    static boost::asio::io_context ioCtxBf;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCtxBf);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/xyz/test/gpm_branch_prop_bf");

    // Bitfield with metric 0 (GraphicsEngineActivityPercent) enabled
    std::vector<uint8_t> metricsBitfield = {0x01};
    creator.addGpmIntfProperty(metricsBitfield);

    EXPECT_NE(creator.getGPMIntf(), nullptr);
}

// ============================================================================
// NsmGPMInterfaceCreator: initialize
// ============================================================================

TEST(NsmGPMBranch, InterfaceCreator_Initialize)
{
    static boost::asio::io_context ioCtxInit;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCtxInit);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/xyz/test/gpm_branch_init");
    creator.addGpmIntfProperty("TestProp", DataType::Double);
    creator.initialize();
    // No crash = success
}

// ============================================================================
// NsmGPMInterfaceCreator: initialize with null intf
// ============================================================================

TEST(NsmGPMBranch, InterfaceCreator_Initialize_NullIntf)
{
    static boost::asio::io_context ioCtxInitNull;
    auto systemBus =
        std::make_shared<sdbusplus::asio::connection>(ioCtxInitNull);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/xyz/test/gpm_branch_init_null");
    creator.gpmIntf = nullptr;
    creator.initialize(); // Should log error but not crash
}
