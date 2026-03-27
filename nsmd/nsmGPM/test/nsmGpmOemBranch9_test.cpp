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
 * Branch coverage batch 9 for nsmd/nsmGPM/nsmGpmOem.cpp.
 *
 * Covers:
 * - NsmGPMAggregated constructor (lines 484-533): bitfield with supported
 *   metrics → creates metricsTable entries (inner loop coverage)
 * - NsmGPMPerInstance constructor (lines 593-613):
 *   GPMMetricsUnit::BANDWIDTH switch case (line 610-612)
 * - NsmGetSupportedGPMMetrics constructor (lines 738-752):
 *   simple member-store ctor
 * - NsmGetSupportedPerInstanceGPMMetrics constructor (lines 939-949):
 *   simple member-store ctor
 * - NsmGPMPerInstance::genRequestMsg: encode failure → nullopt
 *   (instanceId > NSM_INSTANCE_MAX triggers failure)
 * - NsmGetSupportedGPMMetrics::splitMetricsBitfield:
 *   invalid parameters (maxMetricsPerCommand==0) → empty return
 */

#include "platform-environmental.h"

#include <cstring>
#include <limits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmGpmOem.hpp"

#undef private
#undef protected

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

using namespace nsm;
using namespace ::testing;

// Shared boost/asio infrastructure for all tests in this file
static boost::asio::io_context sIo;
static auto sBus = std::make_shared<sdbusplus::asio::connection>(sIo);
static auto sObjServer = std::make_shared<sdbusplus::asio::object_server>(sBus);

// ============================================================================
// NsmGPMAggregated constructor: bitfield with bit 0 set (metric 0 present in
// GPMIntfMetricsTable) → metricsTable[0] gets populated
// Lines 484-533 (branch coverage for inner loop and map lookup)
// ============================================================================

TEST(NsmGpmOemBranch9, GpmAggregated_CtorWithBitfieldMetric0)
{
    // Create gpmIntf via object server so it's valid
    auto gpmIntf = sObjServer->add_interface(
        "/xyz/openbmc_project/test/gpm_branch9a", "com.nvidia.GPMMetrics");
    ASSERT_NE(gpmIntf, nullptr);
    gpmIntf->initialize();

    // Create NVLinkMetricsIntf
    auto& bus = utils::DBusHandler::getBus();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/test/gpm_branch9a");

    // metricsBitfield with bit 0 set (metric 0) — covers the
    // supportedGPMMetrics[i]==true && map.find(i)!=end() TRUE branch
    std::vector<uint8_t> bitfield = {0x01}; // bit 0 set

    EXPECT_NO_THROW({
        NsmGPMAggregated agg("gpm_agg_b9", "type_b9",
                             "/xyz/openbmc_project/test/gpm_branch9a", 0, 0, 0,
                             bitfield, gpmIntf, nvlinkIntf);
    });
}

TEST(NsmGpmOemBranch9, GpmAggregated_CtorWithEmptyBitfield)
{
    auto gpmIntf = sObjServer->add_interface(
        "/xyz/openbmc_project/test/gpm_branch9b", "com.nvidia.GPMMetrics");
    ASSERT_NE(gpmIntf, nullptr);
    gpmIntf->initialize();

    auto& bus = utils::DBusHandler::getBus();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/test/gpm_branch9b");

    // Empty bitfield: loop body never executes (FALSE branch for
    // supportedGPMMetrics[i])
    std::vector<uint8_t> bitfield = {0x00};

    EXPECT_NO_THROW({
        NsmGPMAggregated agg("gpm_agg_empty", "type_b9",
                             "/xyz/openbmc_project/test/gpm_branch9b", 0, 0, 0,
                             bitfield, gpmIntf, nvlinkIntf);
    });
}

TEST(NsmGpmOemBranch9, GpmAggregated_CtorWithAllMetricsBitfield)
{
    auto gpmIntf = sObjServer->add_interface(
        "/xyz/openbmc_project/test/gpm_branch9c", "com.nvidia.GPMMetrics");
    ASSERT_NE(gpmIntf, nullptr);
    gpmIntf->initialize();

    auto& bus = utils::DBusHandler::getBus();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/test/gpm_branch9c");

    // All bits set: covers both TRUE (supported metric in map) and FALSE
    // (metric not in GPMIntfMetricsTable) branches of the inner if
    std::vector<uint8_t> bitfield = {0xFF, 0xFF, 0xFF, 0xFF,
                                     0xFF, 0xFF, 0xFF, 0xFF};

    EXPECT_NO_THROW({
        NsmGPMAggregated agg("gpm_agg_all", "type_b9",
                             "/xyz/openbmc_project/test/gpm_branch9c", 0, 0, 0,
                             bitfield, gpmIntf, nvlinkIntf);
    });
}

// ============================================================================
// NsmGPMPerInstance constructor: BANDWIDTH unit → decodeFunc = decodeBandwidth
// Lines 593-613 (covers switch case GPMMetricsUnit::BANDWIDTH)
// ============================================================================

TEST(NsmGpmOemBranch9, GpmPerInstance_CtorBandwidthUnit)
{
    // Create a null MetricPerInstanceUpdator (nullptr is valid - just stored)
    std::shared_ptr<MetricPerInstanceUpdator> updator = nullptr;

    std::vector<bitfield8_t> instanceBitfield = {{0x01}};

    EXPECT_NO_THROW({
        NsmGPMPerInstance sensor("per_inst_bw", "type_b9", 0, 0, 0,
                                 /*metricId=*/10, instanceBitfield,
                                 GPMMetricsUnit::BANDWIDTH, updator);
        // Verify BANDWIDTH decodeFunc is set to decodeBandwidth
        EXPECT_EQ(sensor.decodeFunc, decodeBandwidth);
    });
}

TEST(NsmGpmOemBranch9, GpmPerInstance_CtorPercentageUnit)
{
    std::shared_ptr<MetricPerInstanceUpdator> updator = nullptr;
    std::vector<bitfield8_t> instanceBitfield = {{0x01}};

    EXPECT_NO_THROW({
        NsmGPMPerInstance sensor("per_inst_pct", "type_b9", 0, 0, 0,
                                 /*metricId=*/4, instanceBitfield,
                                 GPMMetricsUnit::PERCENTAGE, updator);
        EXPECT_EQ(sensor.decodeFunc, decodePercentage);
    });
}

// ============================================================================
// NsmGPMPerInstance::genRequestMsg: encode failure (instanceId too large)
// Lines 617-639 (FALSE branch: rc != 0 → return std::nullopt)
// ============================================================================

TEST(NsmGpmOemBranch9, GpmPerInstance_GenRequestMsg_EncodeFail)
{
    std::shared_ptr<MetricPerInstanceUpdator> updator = nullptr;
    std::vector<bitfield8_t> instanceBitfield = {{0x01}};
    NsmGPMPerInstance sensor("per_inst_enc_fail", "type_b9", 0, 0, 0,
                             /*metricId=*/4, instanceBitfield,
                             GPMMetricsUnit::PERCENTAGE, updator);

    // NSM_INSTANCE_MAX + 1 causes encode failure
    auto req = sensor.genRequestMsg(1, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

TEST(NsmGpmOemBranch9, GpmPerInstance_GenRequestMsg_Success)
{
    std::shared_ptr<MetricPerInstanceUpdator> updator = nullptr;
    std::vector<bitfield8_t> instanceBitfield = {{0x01}};
    NsmGPMPerInstance sensor("per_inst_gen_ok", "type_b9", 0, 0, 0,
                             /*metricId=*/4, instanceBitfield,
                             GPMMetricsUnit::PERCENTAGE, updator);

    auto req = sensor.genRequestMsg(1, 0);
    EXPECT_TRUE(req.has_value());
    EXPECT_GT(req->size(), 0u);
}

// ============================================================================
// NsmGetSupportedGPMMetrics constructor (lines 738-752): simple store ctor
// ============================================================================

TEST(NsmGpmOemBranch9, GetSupportedGPMMetrics_CtorStoresMembers)
{
    auto& bus = utils::DBusHandler::getBus();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/test/gpm_branch9_supported");

    auto gpmIntf = sObjServer->add_interface(
        "/xyz/openbmc_project/test/gpm_branch9_supported",
        "com.nvidia.GPMMetrics");
    ASSERT_NE(gpmIntf, nullptr);
    gpmIntf->initialize();

    NsmDeviceTable devices;
    SensorManagerTest smt(devices);
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "NSM_DEVICE_INSTANCE_NUMBER", "0", 0);

    std::vector<uint8_t> configBitfield = {0xFF};

    EXPECT_NO_THROW({
        NsmGetSupportedGPMMetrics sensor(
            "supp_gpm", "type_b9", GPM_METRIC_TYPE_AGGREGATE, nsmDevice,
            "/xyz/openbmc_project/test/gpm_branch9_supported", 0, 0, 0,
            configBitfield, gpmIntf, nvlinkIntf, nullptr, "");
        EXPECT_EQ(sensor.metricType, GPM_METRIC_TYPE_AGGREGATE);
        EXPECT_EQ(sensor.retrievalSource, 0u);
        EXPECT_EQ(sensor.gpuInstance, 0u);
        EXPECT_EQ(sensor.computeInstance, 0u);
    });
}

// ============================================================================
// NsmGetSupportedGPMMetrics::splitMetricsBitfield: invalid parameters
// → returns empty chunks (line 759-765)
// ============================================================================

TEST(NsmGpmOemBranch9, SplitMetricsBitfield_InvalidParams_ReturnsEmpty)
{
    auto& bus = utils::DBusHandler::getBus();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/test/gpm_branch9_split");

    auto gpmIntf = sObjServer->add_interface(
        "/xyz/openbmc_project/test/gpm_branch9_split", "com.nvidia.GPMMetrics");
    ASSERT_NE(gpmIntf, nullptr);
    gpmIntf->initialize();

    NsmDeviceTable devices;
    SensorManagerTest smt(devices);
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "NSM_DEVICE_INSTANCE_NUMBER", "0", 0);

    std::vector<uint8_t> configBitfield = {0xFF};

    NsmGetSupportedGPMMetrics sensor(
        "supp_gpm_split", "type_b9", GPM_METRIC_TYPE_AGGREGATE, nsmDevice,
        "/xyz/openbmc_project/test/gpm_branch9_split", 0, 0, 0, configBitfield,
        gpmIntf, nvlinkIntf, nullptr, "");

    // maxMetricsPerCommand is 0 (not set) → splitMetricsBitfield returns {}
    auto chunks = sensor.splitMetricsBitfield({0xFF});
    EXPECT_TRUE(chunks.empty());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics constructor (lines 939-949)
// ============================================================================

TEST(NsmGpmOemBranch9, GetSupportedPerInstanceGPMMetrics_CtorStoresMembers)
{
    NsmDeviceTable devices;
    SensorManagerTest smt(devices);
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "NSM_DEVICE_INSTANCE_NUMBER", "0", 0);

    std::shared_ptr<MetricPerInstanceUpdator> updator = nullptr;
    std::vector<bitfield8_t> instanceBitfield = {{0x03}};

    EXPECT_NO_THROW({
        NsmGetSupportedPerInstanceGPMMetrics sensor(
            "supp_per_inst", "type_b9", nsmDevice, 0, 0, 0,
            /*metricId=*/4, instanceBitfield, GPMMetricsUnit::PERCENTAGE,
            updator);
        EXPECT_EQ(sensor.metricId, 4u);
        EXPECT_EQ(sensor.unit, GPMMetricsUnit::PERCENTAGE);
    });
}
