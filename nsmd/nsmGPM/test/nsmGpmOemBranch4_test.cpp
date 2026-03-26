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
 * Branch coverage batch 4 for nsmd/nsmGPM/nsmGpmOem.cpp.
 *
 * Covers:
 * - GPMMetricUpdator::updateMetric: previousValue != val TRUE+FALSE, gpmIntf
 *   null FALSE
 * - NVLinkMetricUpdator::updateMetric: previousValue != val TRUE+FALSE
 * - DRAMUsageMetricUpdator::updateMetric: previousValue != val TRUE+FALSE
 * - NsmGPMInterfaceCreator: gpmIntf null paths (addGpmIntfProperty with null)
 * - NsmGPMInterfaceCreator::addGpmIntfProperty(bitfield): supported metrics
 * - NsmGPMInterfaceCreator::addGpmIntfProperty(name,type): Double+VectorDouble
 * - NsmGPMAggregated::handleSample: tag > metricsTable.size() (skip)
 * - NsmGPMAggregated::handleSample: !decodeFunc || !updater (skip)
 * - NsmGPMAggregated::handleSample: decode fail path
 * - NsmGPMAggregated::handleSample: decode success path
 * - NsmGetSupportedGPMMetrics::genRequestMsg: encode fail/success
 * - NsmGetSupportedGPMMetrics::handleResponseMsg: already received, decode fail
 * - NsmGetSupportedGPMMetrics::splitMetricsBitfield: maxMetricsPerCommand=0,
 *   maskSize=0
 * - NsmGetSupportedPerInstanceGPMMetrics: genRequestMsg encode fail/success
 * - NsmGetSupportedPerInstanceGPMMetrics: handleResponseMsg already received,
 *   decode fail, empty chunks
 * - NsmGetSupportedPerInstanceGPMMetrics::splitInstanceBitfield:
 *   maxMetricsPerCommand=0, maskSize=0
 * - PortMetricPerInstanceUpdator: resize, NaN skip, value change + no-change
 * - makeNVLinkRawRx/Tx/DataRx/DataTx PerInstanceUpdator factory functions
 * - decodePercentage and decodeBandwidth direct calls
 * - GPMMetricInstanceUpdator: null gpmIntf
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

class MockMetricPerInstanceUpdatorB4 : public MetricPerInstanceUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const std::vector<double>& metrics),
                (override));
};

// ============================================================================
// decodePercentage / decodeBandwidth direct calls
// ============================================================================

TEST(NsmGpmOemBranch4, DecodePercentage_ValidData)
{
    uint32_t pctData = 5000;
    auto [rc, val] = decodePercentage(reinterpret_cast<uint8_t*>(&pctData), 4);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmGpmOemBranch4, DecodePercentage_InvalidSize)
{
    uint8_t data = 0;
    auto [rc, val] = decodePercentage(&data, 1);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmGpmOemBranch4, DecodeBandwidth_ValidData)
{
    uint64_t bwData = 1024 * 1024 * 128; // 1 Gbps
    auto [rc, val] = decodeBandwidth(reinterpret_cast<uint8_t*>(&bwData), 8);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(val, 1.0);
}

TEST(NsmGpmOemBranch4, DecodeBandwidth_InvalidSize)
{
    uint8_t data = 0;
    auto [rc, val] = decodeBandwidth(&data, 1);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// GPMMetricUpdator: value change and no-change paths
// ============================================================================

TEST(NsmGpmOemBranch4, GPMMetricUpdator_ValueChange_And_NoChange)
{
    static boost::asio::io_context ioGpm;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioGpm);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_upd", "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "TestProp", double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    NsmGPMAggregated gpm("TestUpdator", "TestType", "/xyz/test/gpm_b4_upd", 2,
                         0, 0, {0x01}, gpmIntf, nullptr);

    auto infos = gpm.getMetricInfo(0);
    ASSERT_GT(infos.size(), 0u);
    ASSERT_NE(infos[0]->updater, nullptr);

    // First call: value changes (TRUE branch)
    infos[0]->updater->updateMetric(42.0);
    // Same value (FALSE branch)
    infos[0]->updater->updateMetric(42.0);
    // Different value (TRUE branch)
    infos[0]->updater->updateMetric(99.0);
}

// ============================================================================
// DRAMUsageMetricUpdator: value change and no-change
// ============================================================================

TEST(NsmGpmOemBranch4, DRAMUsageMetricUpdator_ValueChange_And_NoChange)
{
    auto bus = sdbusplus::bus::new_default();
    auto dimmIntf = std::make_shared<DimmIntf>(bus, "/xyz/test/dimm_b4_upd");

    DRAMUsageMetricUpdator updator(dimmIntf, "/xyz/test/dimm_b4_upd");

    updator.updateMetric(50.0);
    EXPECT_DOUBLE_EQ(dimmIntf->utilization(), 50.0);
    updator.updateMetric(50.0);
    updator.updateMetric(75.0);
    EXPECT_DOUBLE_EQ(dimmIntf->utilization(), 75.0);
}

// ============================================================================
// NVLinkMetricUpdator: value change and no-change
// ============================================================================

TEST(NsmGpmOemBranch4, NVLinkMetricUpdator_ValueChange_And_NoChange)
{
    static boost::asio::io_context ioNvl;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioNvl);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_nvl", "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/gpm_b4_nvl");

    NsmGPMAggregated gpm("TestNVLUpdator", "TestType", "/xyz/test/gpm_b4_nvl",
                         2, 0, 0, {0x00, 0x3C}, gpmIntf, nvlinkIntf);

    auto infos10 = gpm.getMetricInfo(10);
    ASSERT_GT(infos10.size(), 0u);
    ASSERT_NE(infos10[0]->updater, nullptr);

    infos10[0]->updater->updateMetric(10.0);
    infos10[0]->updater->updateMetric(10.0);
    infos10[0]->updater->updateMetric(20.0);
}

// ============================================================================
// NsmGPMInterfaceCreator: addGpmIntfProperty(bitfield)
// ============================================================================

TEST(NsmGpmOemBranch4, NsmGPMInterfaceCreator_AddPropertyByBitfield)
{
    static boost::asio::io_context ioCreator;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCreator);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/xyz/test/gpm_b4_creator_bf");
    creator.addGpmIntfProperty({0x01, 0x01});
    EXPECT_NE(creator.getGPMIntf(), nullptr);
}

// ============================================================================
// NsmGPMInterfaceCreator: addGpmIntfProperty(name, type)
// ============================================================================

TEST(NsmGpmOemBranch4, NsmGPMInterfaceCreator_AddPropertyByName)
{
    static boost::asio::io_context ioCreator2;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioCreator2);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/xyz/test/gpm_b4_creator_name");
    creator.addGpmIntfProperty("TestDouble", DataType::Double);
    creator.addGpmIntfProperty("TestVecDouble", DataType::VectorDouble);
    EXPECT_NE(creator.getGPMIntf(), nullptr);
}

// ============================================================================
// NsmGPMInterfaceCreator: initialize with valid and null gpmIntf
// ============================================================================

TEST(NsmGpmOemBranch4, NsmGPMInterfaceCreator_Initialize)
{
    static boost::asio::io_context ioInit;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioInit);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/xyz/test/gpm_b4_init");
    creator.addGpmIntfProperty("Prop1", DataType::Double);
    EXPECT_NO_THROW(creator.initialize());
}

TEST(NsmGpmOemBranch4, NsmGPMInterfaceCreator_Initialize_NullIntf)
{
    static boost::asio::io_context ioInitNull;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioInitNull);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/xyz/test/gpm_b4_init_null");
    creator.gpmIntf = nullptr;
    EXPECT_NO_THROW(creator.initialize());
}

// ============================================================================
// NsmGPMInterfaceCreator: addGpmIntfProperty with null gpmIntf
// ============================================================================

TEST(NsmGpmOemBranch4, NsmGPMInterfaceCreator_AddPropertyBitfield_NullIntf)
{
    static boost::asio::io_context ioNullBf;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioNullBf);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/xyz/test/gpm_b4_null_bf");
    creator.gpmIntf = nullptr;
    EXPECT_NO_THROW(creator.addGpmIntfProperty({0xFF}));
}

TEST(NsmGpmOemBranch4, NsmGPMInterfaceCreator_AddPropertyName_NullIntf)
{
    static boost::asio::io_context ioNullName;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioNullName);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/xyz/test/gpm_b4_null_name");
    creator.gpmIntf = nullptr;
    EXPECT_NO_THROW(creator.addGpmIntfProperty("TestProp", DataType::Double));
}

// ============================================================================
// NsmGPMAggregated::handleSample: tag > metricsTable.size()
// ============================================================================

TEST(NsmGpmOemBranch4, GPMAggregated_HandleSample_TagTooLarge)
{
    static boost::asio::io_context ioTag;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioTag);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_tag", "com.nvidia.GPMMetrics");

    NsmGPMAggregated gpm("TestTagLarge", "TestType", "/xyz/test/gpm_b4_tag", 2,
                         0, 0, {0x01}, gpmIntf, nullptr);

    NsmSensorAggregator::TelemetrySample sample;
    sample.tag = 255;
    sample.data = nullptr;
    sample.data_len = 0;

    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated::handleSample: metric with no updater (tag 4)
// ============================================================================

TEST(NsmGpmOemBranch4, GPMAggregated_HandleSample_NoUpdater)
{
    static boost::asio::io_context ioNoUpd;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioNoUpd);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_noupd", "com.nvidia.GPMMetrics");

    NsmGPMAggregated gpm("TestNoUpd", "TestType", "/xyz/test/gpm_b4_noupd", 2,
                         0, 0, {0x01}, gpmIntf, nullptr);

    NsmSensorAggregator::TelemetrySample sample;
    sample.tag = 4;
    uint32_t pctData = 5000;
    sample.data = reinterpret_cast<uint8_t*>(&pctData);
    sample.data_len = 4;

    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated::handleSample: decode success
// ============================================================================

TEST(NsmGpmOemBranch4, GPMAggregated_HandleSample_DecodeSuccess)
{
    static boost::asio::io_context ioDecOk;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioDecOk);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_decok", "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "GraphicsEngineActivityPercent",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    NsmGPMAggregated gpm("TestDecOk", "TestType", "/xyz/test/gpm_b4_decok", 2,
                         0, 0, {0x01}, gpmIntf, nullptr);

    NsmSensorAggregator::TelemetrySample sample;
    sample.tag = 0;
    uint32_t pctData = 5000;
    sample.data = reinterpret_cast<uint8_t*>(&pctData);
    sample.data_len = 4;

    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated::handleSample: decode fail
// ============================================================================

TEST(NsmGpmOemBranch4, GPMAggregated_HandleSample_DecodeFail)
{
    static boost::asio::io_context ioDecFl;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioDecFl);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_decfail", "com.nvidia.GPMMetrics");

    gpmIntf->register_property(
        "GraphicsEngineActivityPercent",
        double{std::numeric_limits<double>::quiet_NaN()});
    gpmIntf->initialize();

    NsmGPMAggregated gpm("TestDecFail", "TestType", "/xyz/test/gpm_b4_decfail",
                         2, 0, 0, {0x01}, gpmIntf, nullptr);

    NsmSensorAggregator::TelemetrySample sample;
    sample.tag = 0;
    uint8_t shortData = 0;
    sample.data = &shortData;
    sample.data_len = 1;

    auto rc = gpm.handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::genRequestMsg: encode fail/success
// ============================================================================

TEST(NsmGpmOemBranch4, GetSupportedGPMMetrics_GenRequestMsg_EncodeFail)
{
    static boost::asio::io_context ioSgpFail;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioSgpFail);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_sgp_fail", "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 20, "NSM_DEVICE_INSTANCE_NUMBER", "20", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB4SgpFail",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b4_sgp_fail",
                                     2,
                                     0,
                                     0,
                                     {0xFF},
                                     gpmIntf,
                                     nullptr};

    auto req = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

TEST(NsmGpmOemBranch4, GetSupportedGPMMetrics_GenRequestMsg_Success)
{
    static boost::asio::io_context ioSgpOk;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioSgpOk);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_sgp_ok", "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 21, "NSM_DEVICE_INSTANCE_NUMBER", "21", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB4SgpOk",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b4_sgp_ok",
                                     2,
                                     0,
                                     0,
                                     {0xFF},
                                     gpmIntf,
                                     nullptr};

    auto req = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(req.has_value());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: already received guard
// ============================================================================

TEST(NsmGpmOemBranch4, GetSupportedGPMMetrics_HandleResponse_AlreadyReceived)
{
    static boost::asio::io_context ioAlready;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioAlready);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_already", "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 22, "NSM_DEVICE_INSTANCE_NUMBER", "22", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB4Already",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b4_already",
                                     2,
                                     0,
                                     0,
                                     {0xFF},
                                     gpmIntf,
                                     nullptr};

    sensor.responseReceived = true;

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 10, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg: decode fail (cc != SUCCESS)
// ============================================================================

TEST(NsmGpmOemBranch4, GetSupportedGPMMetrics_HandleResponse_DecodeFail)
{
    static boost::asio::io_context ioDecFailSgp;
    auto systemBus =
        std::make_shared<sdbusplus::asio::connection>(ioDecFailSgp);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_sgp_decfail", "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 23, "NSM_DEVICE_INSTANCE_NUMBER", "23", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB4SgpDecFail",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b4_sgp_decfail",
                                     2,
                                     0,
                                     0,
                                     {0xFF},
                                     gpmIntf,
                                     nullptr};

    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_ERROR, 0, 1, 8, &bitmask, msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_ERROR);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::splitMetricsBitfield: maxMetricsPerCommand=0
// ============================================================================

TEST(NsmGpmOemBranch4, GetSupportedGPMMetrics_SplitBitfield_MaxZero)
{
    static boost::asio::io_context ioMaxZero;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioMaxZero);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_maxzero", "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 24, "NSM_DEVICE_INSTANCE_NUMBER", "24", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB4MaxZero",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b4_maxzero",
                                     2,
                                     0,
                                     0,
                                     {0xFF},
                                     gpmIntf,
                                     nullptr};

    sensor.maxMetricsPerCommand = 0;
    sensor.maskSize = 1;

    auto chunks = sensor.splitMetricsBitfield({0xFF});
    EXPECT_TRUE(chunks.empty());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::splitMetricsBitfield: maskSize=0
// ============================================================================

TEST(NsmGpmOemBranch4, GetSupportedGPMMetrics_SplitBitfield_MaskSizeZero)
{
    static boost::asio::io_context ioMaskZero;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(ioMaskZero);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_maskzero", "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 32, "NSM_DEVICE_INSTANCE_NUMBER", "32", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB4MaskZero",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b4_maskzero",
                                     2,
                                     0,
                                     0,
                                     {0xFF},
                                     gpmIntf,
                                     nullptr};

    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 0;

    auto chunks = sensor.splitMetricsBitfield({0xFF});
    EXPECT_TRUE(chunks.empty());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics: genRequestMsg encode fail/success
// ============================================================================

TEST(NsmGpmOemBranch4,
     GetSupportedPerInstanceGPMMetrics_GenRequestMsg_EncodeFail)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 25, "NSM_DEVICE_INSTANCE_NUMBER", "25", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB4>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB4PerInstGenFail",     "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    auto req = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

TEST(NsmGpmOemBranch4, GetSupportedPerInstanceGPMMetrics_GenRequestMsg_Success)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 26, "NSM_DEVICE_INSTANCE_NUMBER", "26", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB4>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB4PerInstGenOk",       "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    auto req = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(req.has_value());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics: handleResponseMsg already received
// ============================================================================

TEST(NsmGpmOemBranch4,
     GetSupportedPerInstanceGPMMetrics_HandleResponse_AlreadyReceived)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 27, "NSM_DEVICE_INSTANCE_NUMBER", "27", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB4>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB4PerInstAlready",     "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    sensor.responseReceived = true;

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 10, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics: handleResponseMsg decode fail
// ============================================================================

TEST(NsmGpmOemBranch4,
     GetSupportedPerInstanceGPMMetrics_HandleResponse_DecodeFail)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 28, "NSM_DEVICE_INSTANCE_NUMBER", "28", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB4>();

    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB4PerInstDecFail",     "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_ERROR, 0, 1, 8, &bitmask, msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_ERROR);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics: splitInstanceBitfield max=0, mask=0
// ============================================================================

TEST(NsmGpmOemBranch4, GetSupportedPerInstanceGPMMetrics_SplitBitfield_MaxZero)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 29, "NSM_DEVICE_INSTANCE_NUMBER", "29", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB4>();

    std::vector<bitfield8_t> instBf{{.byte = 0xFF}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB4PerInstMaxZero",     "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    sensor.maxMetricsPerCommand = 0;
    sensor.maskSize = 1;

    auto chunks = sensor.splitInstanceBitfield(instBf);
    EXPECT_TRUE(chunks.empty());
}

TEST(NsmGpmOemBranch4,
     GetSupportedPerInstanceGPMMetrics_SplitBitfield_MaskSizeZero)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 33, "NSM_DEVICE_INSTANCE_NUMBER", "33", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB4>();

    std::vector<bitfield8_t> instBf{{.byte = 0xFF}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB4PerInstMaskZero",    "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 0;

    auto chunks = sensor.splitInstanceBitfield(instBf);
    EXPECT_TRUE(chunks.empty());
}

// ============================================================================
// PortMetricPerInstanceUpdator: resize, NaN skip, value change + no-change
// ============================================================================

TEST(NsmGpmOemBranch4, PortMetricPerInstanceUpdator_AllPaths)
{
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf1 =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/gpm_b4_port1");
    auto nvlinkIntf2 =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/gpm_b4_port2");

    std::vector<NVLinkMetricsUpdatorInfo> updatorInfos = {
        {"/xyz/test/gpm_b4_port1", nvlinkIntf1},
        {"/xyz/test/gpm_b4_port2", nvlinkIntf2}};

    auto updator = makeNVLinkRawRxPerInstanceUpdator(updatorInfos);

    std::vector<double> metrics1 = {10.0, 20.0};
    updator->updateMetric(metrics1);
    updator->updateMetric(metrics1);

    std::vector<double> metrics2 = {std::numeric_limits<double>::quiet_NaN(),
                                    30.0};
    updator->updateMetric(metrics2);
}

// ============================================================================
// makeNVLinkRawTx/DataRx/DataTx PerInstanceUpdator factory functions
// ============================================================================

TEST(NsmGpmOemBranch4, MakeNVLinkRawTxPerInstanceUpdator)
{
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/gpm_b4_rawtx");

    std::vector<NVLinkMetricsUpdatorInfo> updatorInfos = {
        {"/xyz/test/gpm_b4_rawtx", nvlinkIntf}};

    auto updator = makeNVLinkRawTxPerInstanceUpdator(updatorInfos);
    EXPECT_NE(updator, nullptr);
    std::vector<double> metrics = {5.0};
    updator->updateMetric(metrics);
}

TEST(NsmGpmOemBranch4, MakeNVLinkDataRxPerInstanceUpdator)
{
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/gpm_b4_datarx");

    std::vector<NVLinkMetricsUpdatorInfo> updatorInfos = {
        {"/xyz/test/gpm_b4_datarx", nvlinkIntf}};

    auto updator = makeNVLinkDataRxPerInstanceUpdator(updatorInfos);
    EXPECT_NE(updator, nullptr);
    std::vector<double> metrics = {7.0};
    updator->updateMetric(metrics);
}

TEST(NsmGpmOemBranch4, MakeNVLinkDataTxPerInstanceUpdator)
{
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/gpm_b4_datatx");

    std::vector<NVLinkMetricsUpdatorInfo> updatorInfos = {
        {"/xyz/test/gpm_b4_datatx", nvlinkIntf}};

    auto updator = makeNVLinkDataTxPerInstanceUpdator(updatorInfos);
    EXPECT_NE(updator, nullptr);
    std::vector<double> metrics = {3.0};
    updator->updateMetric(metrics);
}

// ============================================================================
// GPMMetricInstanceUpdator: null gpmIntf
// ============================================================================

TEST(NsmGpmOemBranch4, GPMMetricInstanceUpdator_NullGpmIntf)
{
    auto updator = makeGPMPerInstanceUpdator("NullProp", "/xyz/test/null_gpm",
                                             nullptr);

    std::vector<double> metrics = {1.0, 2.0};
    updator->updateMetric(metrics);
}

// ============================================================================
// NsmGetSupportedGPMMetrics: handleResponseMsg - empty chunks
// ============================================================================

TEST(NsmGpmOemBranch4, GetSupportedGPMMetrics_HandleResponse_EmptyChunks)
{
    static boost::asio::io_context ioEmptyChunk;
    auto systemBus =
        std::make_shared<sdbusplus::asio::connection>(ioEmptyChunk);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, "/xyz/test/gpm_b4_empty_chunk", "com.nvidia.GPMMetrics");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 30, "NSM_DEVICE_INSTANCE_NUMBER", "30", 0);

    NsmGetSupportedGPMMetrics sensor{"TestB4EmptyChunk",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_b4_empty_chunk",
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
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics: handleResponseMsg - empty chunks
// ============================================================================

TEST(NsmGpmOemBranch4,
     GetSupportedPerInstanceGPMMetrics_HandleResponse_EmptyChunks)
{
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 31, "NSM_DEVICE_INSTANCE_NUMBER", "31", 0);
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorB4>();

    std::vector<bitfield8_t> instBf{{.byte = 0x00}};
    NsmGetSupportedPerInstanceGPMMetrics sensor{
        "TestB4PerInstEmpty",       "TestType", nsmDevice, 2, 0, 0, 10, instBf,
        GPMMetricsUnit::PERCENTAGE, updator};

    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t bitmask = 0xFF;
    encode_get_supported_gpm_metrics_resp(1, NSM_SUCCESS, 0, 1, 32, &bitmask,
                                          msg);

    auto rc = sensor.handleResponseMsg(msg, bufSize);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
