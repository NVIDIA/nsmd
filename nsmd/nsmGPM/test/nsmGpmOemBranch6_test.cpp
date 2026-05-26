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
 * Branch coverage batch 6 for nsmd/nsmGPM/nsmGpmOem.cpp.
 *
 * Covers additional zero-covered branches:
 * - NsmGPMAggregated::handleSample: tag > metricsTable.size() (special tag),
 *   decodeFunc/updater null, decode fail, decode success with value change,
 *   same value (no update)
 * - NsmGPMInterfaceCreator: createGPMIntf null branch, addGpmIntfProperty
 *   with null intf, addGpmIntfProperty bitfield variant, VectorDouble type
 * - DRAMUsageMetricUpdator: updateMetric with same/different values
 * - NVLinkMetricUpdator: updateMetric with value change and no change
 * - PortMetricPerInstanceUpdator: updateMetric with NaN skip, value change,
 *   no change, previousMetrics resize
 * - NsmGetSupportedGPMMetrics::genRequestMsg: encode success + fail
 * - NsmGetSupportedGPMMetrics::handleResponseMsg: responseReceived guard
 * - NsmGetSupportedPerInstanceGPMMetrics::genRequestMsg: success + fail
 * - NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: responseReceived
 * - splitMetricsBitfield: maxMetricsPerCommand == 0
 * - splitInstanceBitfield: maskSize == 0
 * - NsmGPMAggregated constructor: metricsTable entries with NVLink updators
 * - GPMMetricInstanceUpdator: null gpmIntf
 * - makeNVLink*PerInstanceUpdator factory functions
 * - decodePercentage / decodeBandwidth
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

class MockMetricPerInstanceUpdatorB6 : public MetricPerInstanceUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const std::vector<double>& metrics),
                (override));
};

// ============================================================================
// decodePercentage / decodeBandwidth: success and fail
// ============================================================================

TEST(NsmGpmOemBranch6, DecodePercentage_Success)
{
    uint32_t pctData = 5000;
    auto [rc, val] = decodePercentage(reinterpret_cast<uint8_t*>(&pctData), 4);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmGpmOemBranch6, DecodePercentage_Fail)
{
    uint8_t shortData = 0;
    auto [rc, val] = decodePercentage(&shortData, 0);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmGpmOemBranch6, DecodeBandwidth_Success)
{
    uint64_t bwData = 1024ULL * 1024 * 128;
    auto [rc, val] = decodeBandwidth(reinterpret_cast<uint8_t*>(&bwData), 8);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(val, 1.0);
}

TEST(NsmGpmOemBranch6, DecodeBandwidth_Fail)
{
    uint8_t shortData = 0;
    auto [rc, val] = decodeBandwidth(&shortData, 0);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated::handleSample: tag > metricsTable.size() (special tag)
// ============================================================================

TEST(NsmGpmOemBranch6, NsmGPMAggregated_HandleSample_SpecialTag)
{
    static boost::asio::io_context ioSpecial;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioSpecial);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b6_special"},
        "com.nvidia.GPMMetrics");

    NsmGPMAggregated gpm("TestSpecialTag", "TestType",
                         "/xyz/test/gpm_b6_special", 2, 0, 0, {0x01}, gpmIntf,
                         nullptr);

    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample.data = nullptr;
    sample.data_len = 0;

    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated::handleSample: metric with null decodeFunc
// ============================================================================

TEST(NsmGpmOemBranch6, NsmGPMAggregated_HandleSample_NullDecodeFunc)
{
    static boost::asio::io_context ioNullDec;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioNullDec);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b6_nulldec"},
        "com.nvidia.GPMMetrics");

    NsmGPMAggregated gpm("TestNullDec", "TestType", "/xyz/test/gpm_b6_nulldec",
                         2, 0, 0, {0x01}, gpmIntf, nullptr);

    // Metric at tag=4 has no updater (decodePercentage but updater is empty)
    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 4;
    uint32_t pctData = 5000;
    sample.data = reinterpret_cast<uint8_t*>(&pctData);
    sample.data_len = 4;

    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated::handleSample: valid sample with decode success
// ============================================================================

TEST(NsmGpmOemBranch6, NsmGPMAggregated_HandleSample_DecodeSuccess)
{
    static boost::asio::io_context ioDecOk;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioDecOk);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b6_decok"},
        "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "GraphicsEngineActivityPercent",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    NsmGPMAggregated gpm("TestDecOk", "TestType", "/xyz/test/gpm_b6_decok", 2,
                         0, 0, {0x01}, gpmIntf, nullptr);

    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 0;
    uint32_t pctData = 5000;
    sample.data = reinterpret_cast<uint8_t*>(&pctData);
    sample.data_len = 4;

    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Call again with same value (previousValue == val, FALSE branch)
    rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated::handleSample: decode fail
// ============================================================================

TEST(NsmGpmOemBranch6, NsmGPMAggregated_HandleSample_DecodeFail)
{
    static boost::asio::io_context ioDecFail;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioDecFail);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b6_decfail"},
        "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "GraphicsEngineActivityPercent",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    NsmGPMAggregated gpm("TestDecFail", "TestType", "/xyz/test/gpm_b6_decfail",
                         2, 0, 0, {0x01}, gpmIntf, nullptr);

    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 0;
    uint8_t shortData = 0;
    sample.data = &shortData;
    sample.data_len = 0; // too short for percentage decode

    auto rc = gpm.handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated constructor: NVLink metrics with non-null nvlinkMetricsIntf
// Exercises metricsTable entries 10-13 with NVLinkMetricUpdator
// ============================================================================

TEST(NsmGpmOemBranch6, NsmGPMAggregated_Constructor_NVLinkMetrics)
{
    static boost::asio::io_context ioNVLink;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioNVLink);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b6_nvlink"},
        "com.nvidia.GPMMetrics");

    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/nvlink_b6");

    // Bitfield with bits 0-13 set
    std::vector<uint8_t> metricsBf = {0xFF, 0x3F};

    NsmGPMAggregated gpm("TestNVLink", "TestType", "/xyz/test/gpm_b6_nvlink", 2,
                         0, 0, metricsBf, gpmIntf, nvlinkIntf);

    // Verify NVLink metric entries exist at tags 10-13
    auto info10 = gpm.getMetricInfo(10);
    EXPECT_GT(info10.size(), 0u);
    auto info11 = gpm.getMetricInfo(11);
    EXPECT_GT(info11.size(), 0u);
    auto info12 = gpm.getMetricInfo(12);
    EXPECT_GT(info12.size(), 0u);
    auto info13 = gpm.getMetricInfo(13);
    EXPECT_GT(info13.size(), 0u);

    // Exercise NVLinkMetricUpdator with value change
    info10[0]->updater->updateMetric(1.5);
    // Same value -> no-change branch
    info10[0]->updater->updateMetric(1.5);
    // Different value -> update
    info10[0]->updater->updateMetric(2.5);
}

// ============================================================================
// DRAMUsageMetricUpdator: updateMetric paths
// ============================================================================

TEST(NsmGpmOemBranch6, DRAMUsageMetricUpdator_UpdatePaths)
{
    auto bus = sdbusplus::bus::new_default();
    auto dimmIntf = std::make_shared<DimmIntf>(bus, "/xyz/test/dimm_b6");

    DRAMUsageMetricUpdator updator(dimmIntf, "/xyz/test/dimm_b6");

    // First call: value changes from NaN to 50.0
    updator.updateMetric(50.0);
    // Same value: no-change branch
    updator.updateMetric(50.0);
    // Different value
    updator.updateMetric(75.0);
}

// ============================================================================
// GPMMetricInstanceUpdator: null gpmIntf branch
// ============================================================================

TEST(NsmGpmOemBranch6, GPMMetricInstanceUpdator_NullGpmIntf)
{
    auto updator = makeGPMPerInstanceUpdator("TestNullGpm",
                                             "/xyz/test/gpm_b6_null", nullptr);

    // Values change but gpmIntf is null -> skip set_property
    std::vector<double> metrics1 = {1.0, 2.0};
    updator->updateMetric(metrics1);

    // Same values -> previousMetrics == mergedMetrics -> no set_property
    updator->updateMetric(metrics1);
}

// ============================================================================
// PortMetricPerInstanceUpdator: NaN skip, value change, no change, resize
// ============================================================================

TEST(NsmGpmOemBranch6, PortMetricPerInstanceUpdator_AllBranches)
{
    static boost::asio::io_context ioPort;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioPort);

    auto bus = sdbusplus::bus::new_default();

    auto nvlinkIntf1 =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/port_b6_1");
    auto nvlinkIntf2 =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/port_b6_2");

    std::vector<NVLinkMetricsUpdatorInfo> infos = {
        {"/xyz/test/port_b6_1", nvlinkIntf1},
        {"/xyz/test/port_b6_2", nvlinkIntf2}};

    // Use makeNVLinkRawRxPerInstanceUpdator factory
    auto updator = makeNVLinkRawRxPerInstanceUpdator(infos);

    // First call: both values set, triggers resize and set
    std::vector<double> m1 = {1.0, 2.0};
    updator->updateMetric(m1);

    // Same values -> no change branch
    updator->updateMetric(m1);

    // NaN in first position -> skip first, change second
    std::vector<double> m2 = {std::numeric_limits<double>::quiet_NaN(), 3.0};
    updator->updateMetric(m2);

    // More metrics than updatorInfos -> length clamped
    std::vector<double> m3 = {5.0, 6.0, 7.0};
    updator->updateMetric(m3);
}

// ============================================================================
// makeNVLink*PerInstanceUpdator factory functions
// ============================================================================

TEST(NsmGpmOemBranch6, MakeNVLinkFactoryFunctions)
{
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf1 =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/factory_b6_1");
    std::vector<NVLinkMetricsUpdatorInfo> infos = {
        {"/xyz/test/factory_b6_1", nvlinkIntf1}};

    auto rxUpdator = makeNVLinkRawRxPerInstanceUpdator(infos);
    EXPECT_NE(rxUpdator, nullptr);

    auto txUpdator = makeNVLinkRawTxPerInstanceUpdator(infos);
    EXPECT_NE(txUpdator, nullptr);

    auto dataRxUpdator = makeNVLinkDataRxPerInstanceUpdator(infos);
    EXPECT_NE(dataRxUpdator, nullptr);

    auto dataTxUpdator = makeNVLinkDataTxPerInstanceUpdator(infos);
    EXPECT_NE(dataTxUpdator, nullptr);

    // Exercise each updator with values
    std::vector<double> vals = {10.0};
    rxUpdator->updateMetric(vals);
    txUpdator->updateMetric(vals);
    dataRxUpdator->updateMetric(vals);
    dataTxUpdator->updateMetric(vals);
}

// ============================================================================
// NsmGPMInterfaceCreator: addGpmIntfProperty with VectorDouble type
// ============================================================================

TEST(NsmGpmOemBranch6, NsmGPMInterfaceCreator_AddPropertyVectorDouble)
{
    static boost::asio::io_context ioCreator;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCreator);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/xyz/test/gpm_b6_creator");

    // Add properties by name+type
    creator.addGpmIntfProperty("TestDouble", DataType::Double);
    creator.addGpmIntfProperty("TestVecDouble", DataType::VectorDouble);

    // Add properties by bitfield
    std::vector<uint8_t> bitfield = {0x03}; // bits 0 and 1
    creator.addGpmIntfProperty(bitfield);

    auto intf = creator.getGPMIntf();
    EXPECT_NE(intf, nullptr);
    creator.initialize();
}

// ============================================================================
// NsmGPMInterfaceCreator: null gpmIntf branches
// ============================================================================

TEST(NsmGpmOemBranch6, NsmGPMInterfaceCreator_NullIntfBranches)
{
    static boost::asio::io_context ioNullCreator;
    auto systemBus =
        std::make_shared<sdbusplus::asio::connection>(ioNullCreator);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/xyz/test/gpm_b6_nullcreator");

    // Force null gpmIntf
    creator.gpmIntf = nullptr;

    // These should hit the early-return branches
    creator.addGpmIntfProperty("TestProp", DataType::Double);
    std::vector<uint8_t> bf = {0x01};
    creator.addGpmIntfProperty(bf);
    creator.initialize();
}

// ============================================================================
// NsmGetSupportedGPMMetrics::genRequestMsg: success + fail
// ============================================================================

TEST(NsmGpmOemBranch6, GetSupportedGPMMetrics_GenRequestMsg_Success)
{
    static boost::asio::io_context ioGenOk;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioGenOk);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b6_gen_ok"},
        "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 60, "NSM_DEVICE_INSTANCE_NUMBER", "60", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB6GenOk",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b6_gen_ok",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nullptr};

    auto req = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(req.has_value());
}

TEST(NsmGpmOemBranch6, GetSupportedGPMMetrics_GenRequestMsg_Fail)
{
    static boost::asio::io_context ioGenFail;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioGenFail);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b6_gen_fail"},
        "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 61, "NSM_DEVICE_INSTANCE_NUMBER", "61", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB6GenFail",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b6_gen_fail",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nullptr};

    auto req = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: responseReceived guard
// ============================================================================

TEST(NsmGpmOemBranch6, GetSupportedGPMMetrics_HandleResponse_AlreadyReceived)
{
    static boost::asio::io_context ioRecv;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioRecv);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b6_recv"},
        "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 62, "NSM_DEVICE_INSTANCE_NUMBER", "62", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB6Recv",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b6_recv",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nullptr};

    sensor.responseReceived = true;

    // Any message should be ignored
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 10, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: cc != 0 returns cc
// ============================================================================

TEST(NsmGpmOemBranch6, GetSupportedGPMMetrics_HandleResponse_CCNonZero)
{
    static boost::asio::io_context ioCCNZ;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCCNZ);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b6_ccnz"},
        "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 63, "NSM_DEVICE_INSTANCE_NUMBER", "63", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB6CCnz",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b6_ccnz",
                                     2,
                                     0,
                                     0,
                                     {0x01},
                                     gpmIntf,
                                     nullptr};

    // Encode a response with cc=NSM_ERROR
    size_t bufSize = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: empty chunk skip
// ============================================================================

TEST(NsmGpmOemBranch6, GetSupportedGPMMetrics_HandleResponse_EmptyChunkSkip)
{
    static boost::asio::io_context ioEmpty;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioEmpty);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b6_emptyck"},
        "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 64, "NSM_DEVICE_INSTANCE_NUMBER", "64", 0);

    // Configured bitfield: all zeros (empty chunks)
    NsmGetSupportedGPMMetrics sensor{"TestB6EmptyCk",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b6_emptyck",
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
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 32, &bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::genRequestMsg: success + fail
// ============================================================================

TEST(NsmGpmOemBranch6, GetSupportedPerInstGPM_GenRequestMsg_Success)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 65, "NSM_DEVICE_INSTANCE_NUMBER", "65", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB6>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{"TestB6PIGenOk",
                                                "TestType",
                                                nsmDevice,
                                                2,
                                                0,
                                                0,
                                                10,
                                                instBf,
                                                GPMMetricsUnit::PERCENTAGE,
                                                updator};

    auto req = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(req.has_value());
}

TEST(NsmGpmOemBranch6, GetSupportedPerInstGPM_GenRequestMsg_Fail)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 66, "NSM_DEVICE_INSTANCE_NUMBER", "66", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB6>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB6PIGenFail",          "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    auto req = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::handleResponseMsg: responseReceived
// ============================================================================

TEST(NsmGpmOemBranch6, GetSupportedPerInstGPM_HandleResponse_AlreadyReceived)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 67, "NSM_DEVICE_INSTANCE_NUMBER", "67", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB6>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{"TestB6PIRecv",
                                                "TestType",
                                                nsmDevice,
                                                2,
                                                0,
                                                0,
                                                10,
                                                instBf,
                                                GPMMetricsUnit::PERCENTAGE,
                                                updator};

    sensor.responseReceived = true;

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 10, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics: empty chunk skip
// ============================================================================

TEST(NsmGpmOemBranch6, GetSupportedPerInstGPM_HandleResponse_EmptyChunkSkip)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 68, "NSM_DEVICE_INSTANCE_NUMBER", "68", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB6>();

    // All-zero instance bitfield -> empty chunks
    std::vector<bitfield8_t> instBf{{.byte = 0x00}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB6PIEmptyCk",          "TestType", nsmDevice, 2, 0, 0, 10, instBf,
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
// splitMetricsBitfield: maxMetricsPerCommand == 0
// ============================================================================

TEST(NsmGpmOemBranch6, SplitMetricsBitfield_MaxMetricsZero)
{
    static boost::asio::io_context ioSplitZero;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioSplitZero);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b6_splitz"},
        "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 69, "NSM_DEVICE_INSTANCE_NUMBER", "69", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB6SplitZ",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b6_splitz",
                                     2,
                                     0,
                                     0,
                                     {0xFF},
                                     gpmIntf,
                                     nullptr};

    sensor.maxMetricsPerCommand = 0; // triggers error return
    sensor.maskSize = 1;

    auto chunks = sensor.splitMetricsBitfield({0xFF});
    EXPECT_TRUE(chunks.empty());
}

// ============================================================================
// splitInstanceBitfield: maskSize == 0
// ============================================================================

TEST(NsmGpmOemBranch6, SplitInstanceBitfield_MaskSizeZero)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 70, "NSM_DEVICE_INSTANCE_NUMBER", "70", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB6>();

    std::vector<bitfield8_t> instBf{{.byte = 0xFF}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{"TestB6SplitMZ",
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
    sensor.maskSize = 0; // triggers error return

    std::vector<bitfield8_t> bf{{.byte = 0xFF}};
    auto chunks = sensor.splitInstanceBitfield(bf);
    EXPECT_TRUE(chunks.empty());
}

// ============================================================================
// NsmGPMPerInstance constructor: PERCENTAGE unit (explicit test)
// ============================================================================

TEST(NsmGpmOemBranch6, NsmGPMPerInstance_Constructor_PercentageUnit)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB6>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestPct", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::PERCENTAGE, updator);

    EXPECT_NE(sensor.decodeFunc, nullptr);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg: bandwidth decode
// ============================================================================

TEST(NsmGpmOemBranch6, NsmGPMPerInstance_HandleResponse_BandwidthDecode)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB6>();

    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGPMPerInstance sensor("TestBWDec", "TestType", 2, 0, 0, 10, instBf,
                             GPMMetricsUnit::BANDWIDTH, updator);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    uint64_t bwData = 1024ULL * 1024 * 128 * 5;
    size_t s1Len = 0;
    uint8_t s1Buf[32] = {};
    encode_aggregate_resp_sample(
        0, true, reinterpret_cast<uint8_t*>(&bwData), 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(s1Buf), &s1Len);

    std::vector<uint8_t> response(headerSize + s1Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 1, responseMsg);

    memcpy(response.data() + headerSize, s1Buf, s1Len);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated: handleSample with bandwidth metric (tag 8 or 9)
// ============================================================================

TEST(NsmGpmOemBranch6, NsmGPMAggregated_HandleSample_BandwidthMetric)
{
    static boost::asio::io_context ioBW;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioBW);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_b6_bw"},
        "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "PCIeRawTxBandwidthGbps",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    // Bit 8 set (PCIeRawTxBandwidthGbps)
    std::vector<uint8_t> bf = {0x00, 0x01};
    NsmGPMAggregated gpm("TestBW", "TestType", "/xyz/test/gpm_b6_bw", 2, 0, 0,
                         bf, gpmIntf, nullptr);

    NsmSensorAggregator::TelemetrySample sample{};
    sample.tag = 8;
    uint64_t bwData = 1024ULL * 1024 * 128;
    sample.data = reinterpret_cast<uint8_t*>(&bwData);
    sample.data_len = 8;

    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// convertBitfieldToVector: cover both true and false paths per bit
// ============================================================================

TEST(NsmGpmOemBranch6, ConvertBitfieldToVector_MixedBits)
{
    std::vector<uint8_t> bitfield = {0xAA}; // bits 1,3,5,7 set
    std::vector<bool> result;
    utils::convertBitfieldToVector(bitfield, result);
    ASSERT_EQ(result.size(), 8u);
    EXPECT_FALSE(result[0]);
    EXPECT_TRUE(result[1]);
    EXPECT_FALSE(result[2]);
    EXPECT_TRUE(result[3]);
    EXPECT_FALSE(result[4]);
    EXPECT_TRUE(result[5]);
    EXPECT_FALSE(result[6]);
    EXPECT_TRUE(result[7]);
}
