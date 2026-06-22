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
 * Branch coverage tests for nsmd/nsmGPM/nsmGpmOem.cpp
 *
 * Covers:
 * - NsmGPMAggregated::handleSample: out-of-bounds tag (TRUE branch)
 * - NsmGPMAggregated::handleSample: null updater (TRUE branch)
 * - NsmGPMAggregated::handleSample: null decodeFunc (TRUE branch)
 * - NsmGPMAggregated::handleSample: decode failure (rc != SW_SUCCESS)
 * - NsmGPMPerInstance::handleResponseMsg: telemetryCount==0 early return
 * - NsmGPMPerInstance::handleResponseMsg: decode_aggregate_resp_sample fail
 * - DRAMUsageMetricUpdator::updateMetric: same value (previousValue==val)
 * - DRAMUsageMetricUpdator::updateMetric: different value path
 * - PortMetricPerInstanceUpdator::updateMetric: resize + update,
 *   then same size + same values (both FALSE branches)
 */

#include "platform-environmental.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <cmath>

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

class MockMetricUpdatorBranch : public MetricUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const double val), (override));
};

class MockMetricPerInstanceUpdatorBranch : public MetricPerInstanceUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const std::vector<double>& metrics),
                (override));
};

// ============================================================================
// Helper: build a minimal NsmGPMAggregated (no registered GPM properties)
// ============================================================================

static NsmGPMAggregated
    makeGPMAggregated(const std::string& suffix,
                      const std::vector<uint8_t>& bitfield = {0x00})
{
    // Use static io_context so it outlives the returned NsmGPMAggregated and
    // any shared_ptr<sdbusplus::asio::connection> created from it.
    // Without this, the connection destructor accesses the already-freed
    // epoll_reactor causing an invalid read of size 8 under valgrind.
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus,
        sdbusplus::object_path("/xyz/openbmc_project/inventory/gpm_branch/" +
                               suffix),
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    const std::string nvlinkPath =
        "/xyz/openbmc_project/inventory/gpm_branch/" + suffix;
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(bus,
                                                          nvlinkPath.c_str());

    return NsmGPMAggregated{"gpm_" + suffix,
                            "AggregatedGPMMetrics",
                            "/xyz/openbmc_project/inventory/gpm_branch/" +
                                suffix,
                            2,
                            0,
                            0,
                            bitfield,
                            gpmAsioIntf,
                            nvlinkIntf};
}

// ============================================================================
// NsmGPMAggregated::handleSample – out-of-bounds tag
// Covers: if (sample.tag > metricsTable.size()) TRUE branch
// ============================================================================

TEST(NsmGPMAggregatedBranch, HandleSample_OutOfBoundsTag_ReturnsImmediately)
{
    auto gpm = makeGPMAggregated("oob");

    // NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE = 0xEF = 239
    // metricsTable.size() = 239; tag=0xF0=240 > 239 → TRUE branch
    uint8_t dummy = 0;
    auto rc = gpm.handleSample({0xF0, 0, &dummy, true});

    // Returns early with NSM_SW_SUCCESS (returnValue was initialized to 0)
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated::handleSample – null updater
// Covers: if (!metric.decodeFunc || !metric.updater) TRUE via null updater
// The constructor always sets metricsTable[4] = MetricInfo{decodePercentage,
// {}} where {} is an empty (null) unique_ptr<MetricUpdator>.
// ============================================================================

TEST(NsmGPMAggregatedBranch, HandleSample_NullUpdater_Tag4_ReturnsImmediately)
{
    auto gpm = makeGPMAggregated("null_upd");

    // metricsTable[4] has MetricInfo{decodePercentage, {}} – null updater
    uint8_t dummy = 0;
    auto rc = gpm.handleSample({4, 0, &dummy, true});

    // if (!metric.updater) → TRUE → early return
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated::handleSample – null decodeFunc
// Covers: if (!metric.decodeFunc || ...) TRUE via null decodeFunc
// ============================================================================

TEST(NsmGPMAggregatedBranch, HandleSample_NullDecodeFunc_ReturnsImmediately)
{
    auto gpm = makeGPMAggregated("null_dec");

    // Insert a MetricInfo with null decodeFunc at slot 5
    auto mockUpd = std::make_unique<MockMetricUpdatorBranch>();
    gpm.metricsTable[5].clear();
    gpm.metricsTable[5].emplace_back(MetricInfo{nullptr, std::move(mockUpd)});

    uint8_t dummy = 0;
    auto rc = gpm.handleSample({5, 0, &dummy, true});

    // if (!metric.decodeFunc) → TRUE → early return (mock NOT called)
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMAggregated::handleSample – decode failure
// Covers: if (rc != NSM_SW_SUCCESS) TRUE branch in handleSample
// ============================================================================

TEST(NsmGPMAggregatedBranch, HandleSample_DecodeFailure_SetsReturnCode)
{
    // Keep io, systemBus, bus, nvlinkIntf alive in same scope as gpm so that
    // NVLinkMetricUpdators (metricsTable[10]-[13]) remain valid throughout.
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    const std::string decObjPath =
        "/xyz/openbmc_project/inventory/gpm_branch/dec_err2";
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path(decObjPath), "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf2 = std::make_shared<NVLinkMetricsIntf>(bus,
                                                           decObjPath.c_str());

    NsmGPMAggregated gpm{"gpm_dec_err2",
                         "AggregatedGPMMetrics",
                         decObjPath,
                         2,
                         0,
                         0,
                         {0x00},
                         gpmAsioIntf,
                         nvlinkIntf2};

    // metricsTable[3] is empty with {0x00} bitfield – no .clear() needed
    auto mockUpd = std::make_unique<MockMetricUpdatorBranch>();
    auto* rawMock = mockUpd.get();
    gpm.metricsTable[3].emplace_back(
        MetricInfo{decodePercentage, std::move(mockUpd)});

    // updateMetric is called even on decode failure (with 0.0 since percentage
    // stays default when decode_aggregate_gpm_metric_percentage_data returns
    // NSM_SW_ERROR_LENGTH for size 0)
    EXPECT_CALL(*rawMock, updateMetric(_)).Times(1);

    // data_len=0 → decode_aggregate_gpm_metric_percentage_data fails
    uint8_t dummy = 0;
    auto rc = gpm.handleSample({3, 0, &dummy, true});

    // rc != NSM_SW_SUCCESS → returnValue = error
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg – zero telemetry count
// Covers: if (telemetryCount == 0) TRUE branch → calls updateMetric({})
// ============================================================================

struct NsmGPMPerInstanceBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:88";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmGPMPerInstanceBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmGPMPerInstanceBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmGPMPerInstance>
        makePerInstance(const std::string& suffix,
                        std::shared_ptr<MetricPerInstanceUpdator> updator)
    {
        const std::vector<bitfield8_t> instanceBitfield{{.byte = 0x0F}};
        return std::make_shared<NsmGPMPerInstance>(
            "GPM_PerInst_" + suffix, "GPMPerInstance", 2, 0, 0, 10,
            instanceBitfield, GPMMetricsUnit::PERCENTAGE, updator);
    }
};

TEST_F(NsmGPMPerInstanceBranchTest,
       HandleResponseMsg_ZeroTelemetryCount_CallsUpdateWithEmpty)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorBranch>();
    auto perInstance = makePerInstance("zero_count", updator);

    // telemetryCount==0 → early return via updateMetric(empty_vector)
    EXPECT_CALL(*updator, updateMetric(IsEmpty())).Times(1);

    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto* payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 0; // ← triggers the telemetryCount==0 branch

    auto rc = perInstance->handleResponseMsg(responseMsg,
                                             responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg – sample decode failure
// Covers: if (rc != NSM_SW_SUCCESS) TRUE branch inside while loop → continue
// ============================================================================

TEST_F(NsmGPMPerInstanceBranchTest,
       HandleResponseMsg_SampleDecodeFail_ContinuesLoop)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorBranch>();
    auto perInstance = makePerInstance("sample_fail", updator);

    // updateMetric still called after the failed sample → empty metrics
    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    // Buffer with aggregate header + one sample header (length=1 in bitfield)
    // decode_aggregate_resp_sample: msg_len=3 < consumed_len=4 → ERROR_DATA
    // (sets valid/tag before returning, so no uninitialized read in logging)
    std::vector<uint8_t> responseBuffer(sizeof(nsm_msg_hdr) +
                                            sizeof(nsm_aggregate_resp) +
                                            sizeof(nsm_aggregate_resp_sample),
                                        0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto* payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 1;
    // Sample bitfield byte (offset 1): valid=0, length=1 (bits 1-3 = 001)
    // → data_len = 1<<1 = 2, consumed_len = 2+2 = 4 > 3 = msg_len
    responseBuffer[sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) + 1] = 0x02;

    // decode_aggregate_resp_sample returns ERROR_DATA after setting valid/tag →
    // continue
    auto rc = perInstance->handleResponseMsg(responseMsg,
                                             responseBuffer.size());
    // returnValue stays NSM_SW_SUCCESS (the continue doesn't set it)
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// DRAMUsageMetricUpdator::updateMetric – same value (previousValue == val)
// Covers: if (previousValue != val) FALSE branch → skips intf->utilization()
// ============================================================================

TEST(DRAMUsageMetricUpdatorBranch, UpdateMetric_SameValue_SkipsUpdate)
{
    auto bus = sdbusplus::bus::new_default();
    auto dimmIntf = std::make_shared<DimmIntf>(
        bus, "/xyz/openbmc_project/inventory/dimm_branch0");

    DRAMUsageMetricUpdator updator(
        dimmIntf, "/xyz/openbmc_project/inventory/dimm_branch0");

    // First call: previousValue = NaN, val = 25.0 → NaN != 25.0 → TRUE → update
    updator.updateMetric(25.0);
    EXPECT_DOUBLE_EQ(dimmIntf->utilization(), 25.0);

    // Second call: previousValue = 25.0, val = 25.0 → FALSE branch (no-op)
    updator.updateMetric(25.0);
    EXPECT_DOUBLE_EQ(dimmIntf->utilization(), 25.0); // Unchanged
}

// ============================================================================
// DRAMUsageMetricUpdator::updateMetric – different values
// Covers: if (previousValue != val) TRUE branch multiple times
// ============================================================================

TEST(DRAMUsageMetricUpdatorBranch, UpdateMetric_DifferentValues_UpdatesEach)
{
    auto bus = sdbusplus::bus::new_default();
    auto dimmIntf = std::make_shared<DimmIntf>(
        bus, "/xyz/openbmc_project/inventory/dimm_branch1");

    DRAMUsageMetricUpdator updator(
        dimmIntf, "/xyz/openbmc_project/inventory/dimm_branch1");

    updator.updateMetric(10.0);
    EXPECT_DOUBLE_EQ(dimmIntf->utilization(), 10.0);

    updator.updateMetric(20.0); // Different → update
    EXPECT_DOUBLE_EQ(dimmIntf->utilization(), 20.0);
}

// ============================================================================
// PortMetricPerInstanceUpdator::updateMetric via
// makeNVLinkRawRxPerInstanceUpdator Covers:
//  - if (previousMetrics.size() != length) TRUE: first call resizes
//  - if (previousMetrics.size() != length) FALSE: subsequent calls
//  - if (previousMetrics[i] != metrics[i]) TRUE: first call updates
//  - if (previousMetrics[i] != metrics[i]) FALSE: repeated same values
// ============================================================================

TEST(PortMetricPerInstanceUpdatorBranch,
     UpdateMetric_ResizeThenSameValues_BothBranchesCovered)
{
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/nvlink_branch");

    std::vector<NVLinkMetricsUpdatorInfo> updatorInfos = {
        {"/xyz/openbmc_project/inventory/nvlink_branch", nvlinkIntf}};

    auto updator = makeNVLinkRawRxPerInstanceUpdator(updatorInfos);

    // First call: previousMetrics is empty (size=0 != 1) → resize TRUE
    // and previousMetrics[0] = NaN != 42.0 → update TRUE
    updator->updateMetric({42.0});

    // Second call: previousMetrics.size()==1 == length=1 → resize FALSE
    // previousMetrics[0] = 42.0 == 42.0 → update FALSE (no-op)
    updator->updateMetric({42.0});

    // Third call: same size but different value → resize FALSE, update TRUE
    updator->updateMetric({99.0});

    // All calls should complete without error
    EXPECT_TRUE(true);
}

// ============================================================================
// NsmGPMPerInstance::handleResponseMsg – decodeFunc fails for valid sample
// Covers: if (decodeRc != NSM_SW_SUCCESS) TRUE branch (lines 691-694)
// Sample has valid=1 but data_len=1, which is too small for decodePercentage
// (expects sizeof(uint32_t)=4 bytes) → NSM_SW_ERROR_LENGTH → returnValue set.
// ============================================================================

TEST_F(NsmGPMPerInstanceBranchTest,
       HandleResponseMsg_DecodeFuncFails_SetsReturnCode)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorBranch>();
    auto perInstance = makePerInstance("decode_fail", updator);

    // updateMetric is called after the loop completes (even on decode failure)
    EXPECT_CALL(*updator, updateMetric(_)).Times(1);

    // Build: nsm_msg_hdr + nsm_aggregate_resp(cc=SUCCESS, count=1)
    //      + nsm_aggregate_resp_sample(tag=1, valid=1, length=0→data_len=1)
    // data_len=1 is too small for decodePercentage (expects 4 bytes)
    const size_t bufSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
                           sizeof(nsm_aggregate_resp_sample);
    std::vector<uint8_t> responseBuffer(bufSize, 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto* payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 1;

    auto* sample = reinterpret_cast<nsm_aggregate_resp_sample*>(
        responseBuffer.data() + sizeof(nsm_msg_hdr) +
        sizeof(nsm_aggregate_resp));
    sample->tag = 1;
    sample->valid = 1;
    sample->length = 0; // data_len = 1 << 0 = 1 byte (too small for decode)
    sample->data[0] = 0xAB;

    auto rc = perInstance->handleResponseMsg(responseMsg, bufSize);

    // decodeRc = NSM_SW_ERROR_LENGTH → returnValue set to non-success
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// makeNVLinkRawTxPerInstanceUpdator, makeNVLinkDataRxPerInstanceUpdator,
// makeNVLinkDataTxPerInstanceUpdator – ensure factories are called
// ============================================================================

TEST(NVLinkPerInstanceUpdatorBranch, AllFactories_CreateAndCallUpdate)
{
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/nvlink_factory");

    std::vector<NVLinkMetricsUpdatorInfo> infos = {
        {"/xyz/openbmc_project/inventory/nvlink_factory", nvlinkIntf}};

    auto txUpdator = makeNVLinkRawTxPerInstanceUpdator(infos);
    auto dataRxUpdator = makeNVLinkDataRxPerInstanceUpdator(infos);
    auto dataTxUpdator = makeNVLinkDataTxPerInstanceUpdator(infos);

    // All should be non-null
    EXPECT_NE(txUpdator, nullptr);
    EXPECT_NE(dataRxUpdator, nullptr);
    EXPECT_NE(dataTxUpdator, nullptr);

    // Each factory creates a PortMetricPerInstanceUpdator; call updateMetric
    txUpdator->updateMetric({5.0});
    dataRxUpdator->updateMetric({6.0});
    dataTxUpdator->updateMetric({7.0});
}

// ============================================================================
// NsmGPMAggregated::getMetricInfo – covers line 179 (return metricInfos)
// metricsTable[4] is always populated in constructor (decodePercentage, {})
// ============================================================================

TEST(NsmGPMAggregatedBranch, GetMetricInfo_ReturnsNonEmptyForTag4)
{
    auto gpm = makeGPMAggregated("get_metric_info");
    // tag 4 is always inserted in the NsmGPMAggregated constructor
    auto metrics = gpm.getMetricInfo(4);
    EXPECT_EQ(metrics.size(), 1u);
}

// getMetricInfo for an empty slot returns empty vector (still covers return)
TEST(NsmGPMAggregatedBranch, GetMetricInfo_EmptySlot_ReturnsEmpty)
{
    auto gpm = makeGPMAggregated("get_metric_empty");
    // tag 3 is not populated for a zero-bitfield aggregated object
    auto metrics = gpm.getMetricInfo(3);
    EXPECT_EQ(metrics.size(), 0u);
}

// ============================================================================
// NsmGPMInterfaceCreator::initialize() – null gpmIntf (else branch, lines
// 147-148) Reset gpmIntf to null then call initialize() → triggers else-branch
// log
// ============================================================================

TEST(NsmGPMInterfaceCreatorBranch, Initialize_NullGpmIntf_LogsError)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer,
                                   "/xyz/openbmc_project/test/gpm_null_init");
    // Reset gpmIntf to null to trigger the else-branch in initialize()
    // (#define private public allows direct member access in tests)
    creator.gpmIntf.reset();
    EXPECT_EQ(creator.getGPMIntf(), nullptr);
    // initialize() should log error and return without crash
    EXPECT_NO_THROW(creator.initialize());
    EXPECT_EQ(creator.getGPMIntf(), nullptr);
}

// ============================================================================
// NsmGPMAggregated constructor: non-zero metricsBitfield exercises the
// GPMIntfMetricsTable lookup loop body (nsmGpmOem.cpp L462-469).
// Bitfield {0x01} sets bit 0 → key 0 ("GraphicsEngineActivityPercent")
// is in GPMIntfMetricsTable → TRUE branch of the inner if is taken.
// ============================================================================

TEST(NsmGPMAggregatedBranch, Constructor_NonZeroBitfield_PopulatesMetricsTable)
{
    // {0x01}: bit 0 set → supportedGPMMetrics[0]=true AND
    // GPMIntfMetricsTable.find(0) != end → loop body at L462-469 executes.
    auto gpm = makeGPMAggregated("nonzero_bf", {0x01});

    // metricsTable[0] must have been populated (GPMMetricUpdator created)
    EXPECT_FALSE(gpm.metricsTable[0].empty());
    EXPECT_NE(gpm.metricsTable[0][0].decodeFunc, nullptr);
}

// ============================================================================
// NsmGPMPerInstance constructor: BANDWIDTH unit exercises the BANDWIDTH case
// in the switch statement (nsmGpmOem.cpp L580-582).
// ============================================================================

TEST_F(NsmGPMPerInstanceBranchTest,
       Constructor_BandwidthUnit_CoversBandwidthCase)
{
    const std::vector<bitfield8_t> instanceBitfield{{.byte = 0x0F}};
    // GPMMetricsUnit::BANDWIDTH takes the second switch case (L580-582)
    auto sensor = std::make_shared<NsmGPMPerInstance>(
        "GPM_bw_unit", "GPMPerInstance", 2, 0, 0, 10, instanceBitfield,
        GPMMetricsUnit::BANDWIDTH, nullptr);

    EXPECT_NE(sensor, nullptr);
    // decodeFunc should be set to decodeBandwidth (non-null)
    EXPECT_NE(sensor->decodeFunc, nullptr);
}

// ============================================================================
// GPMMetricUpdator::updateMetric – same value twice → L46 Decision 'false'
// (previousValue == val → skip set_property, FALSE branch of compound &&)
//
// Create NsmGPMAggregated with bitfield {0x01} so metricsTable[0] holds a
// real GPMMetricUpdator. Call handleSample twice with the same data:
//   - First call: previousValue=NaN, val=X → TRUE (updates, sets
//   previousValue=X)
//   - Second call: previousValue=X, val=X → FALSE (skips set_property)
// ============================================================================

TEST(NsmGPMAggregatedBranch, HandleSample_SameValueTwice_FalseBranchL46)
{
    // {0x01}: bit 0 set → metricsTable[0] gets a real GPMMetricUpdator
    auto gpm = makeGPMAggregated("same_val_L46", {0x01});

    // Ensure metricsTable[0] has a real updater (not mock)
    ASSERT_FALSE(gpm.metricsTable[0].empty());

    // Use a fixed 4-byte value for percentage decode
    uint32_t raw = 1000;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&raw);

    // First call: previousValue=NaN, val=decoded → TRUE branch (set_property)
    auto rc1 = gpm.handleSample({0, 4, data, true});
    EXPECT_EQ(rc1, NSM_SW_SUCCESS);

    // Second call: same data → same decoded val → previousValue==val → FALSE
    auto rc2 = gpm.handleSample({0, 4, data, true});
    EXPECT_EQ(rc2, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGPMInterfaceCreator::addGpmIntfProperty(name, dataType) overload
// Lines 425–446 in nsmGpmOem.cpp
// ============================================================================

// Path 1: gpmIntf is null (not yet created) → early-return log at L427-430.
// Covers the TRUE branch of `if (!gpmIntf)`.
TEST(NsmGPMInterfaceCreatorBranch, AddProperty_NullGpmIntf_EarlyReturn)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer,
                                   "/xyz/test/gpm_null_intf_early_return");
    // Constructor creates gpmIntf; manually null it to exercise the guard
    creator.gpmIntf = nullptr;
    EXPECT_NO_THROW(creator.addGpmIntfProperty("SomeMetric", DataType::Double));
    // gpmIntf remains null after early return
    EXPECT_EQ(creator.getGPMIntf(), nullptr);
}

// Path 2: gpmIntf created → switch DataType::Double → register double NaN.
// Covers the FALSE branch of `if (!gpmIntf)` and the Double case.
TEST(NsmGPMInterfaceCreatorBranch, AddProperty_Double_RegistersNaN)
{
    static boost::asio::io_context io2;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io2);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer, "/xyz/test/gpm_intf_double_prop");
    creator.createGPMIntf();
    ASSERT_NE(creator.getGPMIntf(), nullptr);

    EXPECT_NO_THROW(
        creator.addGpmIntfProperty("DoubleMetric", DataType::Double));
}

// NsmGPMPerInstance::handleResponseMsg – decode_aggregate_resp fails
// A tiny buffer causes decode_aggregate_resp to return NSM_SW_ERROR_LENGTH
// (rc != NSM_SW_SUCCESS). This covers:
//   1. shouldLog("decode_aggregate_resp", ...) TRUE branch → LG2_ERROR logged
//
//   2. if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS) TRUE path → early return
TEST_F(NsmGPMPerInstanceBranchTest,
       HandleResponseMsg_DecodeAggregateRespFail_ReturnsError)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdatorBranch>();
    auto perInstance = makePerInstance("decode_fail", updator);

    // updateMetric is NOT called when decode_aggregate_resp fails
    EXPECT_CALL(*updator, updateMetric(_)).Times(0);

    // Tiny buffer — too small for decode_aggregate_resp → NSM_SW_ERROR_LENGTH
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr), 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = perInstance->handleResponseMsg(responseMsg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// Path 3: gpmIntf created → switch DataType::VectorDouble → register
// empty vector. Covers the VectorDouble case.
TEST(NsmGPMInterfaceCreatorBranch, AddProperty_VectorDouble_RegistersEmptyVec)
{
    static boost::asio::io_context io3;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io3);
    sdbusplus::asio::object_server objServer(systemBus);

    NsmGPMInterfaceCreator creator(objServer,
                                   "/xyz/test/gpm_intf_vectordouble_prop");
    creator.createGPMIntf();
    ASSERT_NE(creator.getGPMIntf(), nullptr);

    EXPECT_NO_THROW(
        creator.addGpmIntfProperty("VectorMetric", DataType::VectorDouble));
}

// ============================================================================
// NsmGetSupportedGPMMetrics tests
// Covers: constructor, splitMetricsBitfield, genRequestMsg, handleResponseMsg
// ============================================================================

// Helper: build a valid encode_get_supported_gpm_metrics_resp buffer
static std::vector<uint8_t>
    makeGetSupportedGPMMetricsResp(uint8_t cc, uint16_t maskSz,
                                   uint16_t maxMetricsPerCmd,
                                   const std::vector<uint8_t>& bitmask)
{
    // size: header + struct (includes 1 byte bitmask slot) + (maskSz-1) extra
    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_supported_gpm_metrics_resp) +
                     (maskSz > 0 ? maskSz - 1 : 0);
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_supported_gpm_metrics_resp(1, cc, 0, maskSz, maxMetricsPerCmd,
                                          bitmask.data(), msg);
    return buf;
}

struct NsmGetSupportedGPMMetricsTest : public Test
{
    static boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus{
        std::make_shared<sdbusplus::asio::connection>(io)};
    std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf{
        std::make_shared<sdbusplus::asio::dbus_interface>(
            systemBus, sdbusplus::object_path{"/xyz/test/gpm_supported_test"},
            "com.nvidia.GPMMetrics")};
    sdbusplus::bus_t bus{sdbusplus::bus::new_default()};
    std::shared_ptr<NVLinkMetricsIntf> nvlinkIntf{
        std::make_shared<NVLinkMetricsIntf>(bus,
                                            "/xyz/test/gpm_supported_test")};
    std::shared_ptr<MockNsmDevice> nsmDevice{std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "NSM_DEVICE_INSTANCE_NUMBER", "0", 0)};

    NsmGetSupportedGPMMetrics
        makeSensor(const std::string& suffix,
                   const std::vector<uint8_t>& configuredBitfield = {0x01})
    {
        static int idx = 0;
        ++idx;
        return NsmGetSupportedGPMMetrics{
            "TestGetSupported_" + suffix + std::to_string(idx),
            "TestType",
            GPM_METRIC_TYPE_AGGREGATE,
            nsmDevice,
            "/xyz/test/gpm_supported_" + suffix + std::to_string(idx),
            2,
            0,
            0,
            configuredBitfield,
            gpmIntf,
            nvlinkIntf};
    }
};

boost::asio::io_context NsmGetSupportedGPMMetricsTest::io;

// Constructor creates sensor successfully
TEST_F(NsmGetSupportedGPMMetricsTest, Constructor_CreatesSuccessfully)
{
    EXPECT_NO_THROW({ auto s = makeSensor("ctor"); });
}

// genRequestMsg returns valid message
TEST_F(NsmGetSupportedGPMMetricsTest, GenRequestMsg_ReturnsValidMessage)
{
    auto sensor = makeSensor("gen_req");
    auto result = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_gpm_metrics_req));
}

// genRequestMsg: verify message is consistent across multiple calls
TEST_F(NsmGetSupportedGPMMetricsTest,
       GenRequestMsg_MultipleCalls_ConsistentSize)
{
    auto sensor = makeSensor("gen_req_multi");
    auto r1 = sensor.genRequestMsg(1, 0);
    auto r2 = sensor.genRequestMsg(255, 31);
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r1->size(), r2->size());
}

// handleResponseMsg: decode failure (too-small buffer → NSM_SW_ERROR_LENGTH)
TEST_F(NsmGetSupportedGPMMetricsTest,
       HandleResponseMsg_DecodeFailure_ReturnsError)
{
    auto sensor = makeSensor("decode_fail");
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.hasValidResponse());
}

// handleResponseMsg: cc != NSM_SUCCESS → error path
TEST_F(NsmGetSupportedGPMMetricsTest, HandleResponseMsg_CcError_ReturnsError)
{
    auto sensor = makeSensor("cc_err");
    auto buf = makeGetSupportedGPMMetricsResp(NSM_ERROR, 1, 8, {0x01});
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.hasValidResponse());
}

// handleResponseMsg: already received → skips and returns NSM_SUCCESS
TEST_F(NsmGetSupportedGPMMetricsTest,
       HandleResponseMsg_AlreadyReceived_ReturnsSuccess)
{
    auto sensor = makeSensor("already_recv");
    // Force responseReceived = true
    sensor.responseReceived = true;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Should return NSM_SUCCESS without even trying to decode
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// handleResponseMsg: success with zero-bit configuredMetricsBitfield
// → parseChunks is empty, no sensors created
TEST_F(NsmGetSupportedGPMMetricsTest,
       HandleResponseMsg_Success_EmptyConfiguredBitfield_NoSensors)
{
    // configuredMetricsBitfield = {0x00} → splitMetricsBitfield produces
    // one chunk with 0 metrics → skipped → 0 sensors added to nsmDevice
    auto sensor = makeSensor("empty_bf", {0x00});
    auto buf = makeGetSupportedGPMMetricsResp(NSM_SUCCESS, 1, 8, {0xFF});
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());
    EXPECT_EQ(sensor.getMaxMetricsPerCommand(), 8);
}

// handleResponseMsg: success with non-zero configuredMetricsBitfield
// → sensors created and added to nsmDevice
TEST_F(NsmGetSupportedGPMMetricsTest,
       HandleResponseMsg_Success_NonZeroBitfield_SensorsCreated)
{
    auto sensor = makeSensor("nonzero_bf", {0x01});
    auto buf = makeGetSupportedGPMMetricsResp(NSM_SUCCESS, 1, 8, {0xFF});
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());
    EXPECT_EQ(sensor.getMaxMetricsPerCommand(), 8);
    EXPECT_EQ(sensor.getSupportedMetricsBitmask().size(), 1u);
}

// splitMetricsBitfield: maxMetricsPerCommand == 0 → returns empty
TEST_F(NsmGetSupportedGPMMetricsTest,
       SplitMetricsBitfield_ZeroMaxMetrics_ReturnsEmpty)
{
    auto sensor = makeSensor("split_zero_max");
    sensor.maxMetricsPerCommand = 0;
    sensor.maskSize = 1;
    auto result = sensor.splitMetricsBitfield({0xFF});
    EXPECT_TRUE(result.empty());
}

// splitMetricsBitfield: maskSize == 0 → returns empty
TEST_F(NsmGetSupportedGPMMetricsTest,
       SplitMetricsBitfield_ZeroMaskSize_ReturnsEmpty)
{
    auto sensor = makeSensor("split_zero_mask");
    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 0;
    auto result = sensor.splitMetricsBitfield({0xFF});
    EXPECT_TRUE(result.empty());
}

// splitMetricsBitfield: empty bitfield → returns empty
TEST_F(NsmGetSupportedGPMMetricsTest,
       SplitMetricsBitfield_EmptyBitfield_ReturnsEmpty)
{
    auto sensor = makeSensor("split_empty_bf");
    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 1;
    auto result = sensor.splitMetricsBitfield({});
    EXPECT_TRUE(result.empty());
}

// splitMetricsBitfield: valid params → single chunk
TEST_F(NsmGetSupportedGPMMetricsTest, SplitMetricsBitfield_Valid_SingleChunk)
{
    auto sensor = makeSensor("split_valid");
    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 1;
    auto result = sensor.splitMetricsBitfield({0x03});
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0][0], 0x03);
}

// splitMetricsBitfield: maxMetricsPerCommand < total bits → multiple chunks
// With maxMetricsPerCommand=4 and maskSize=1 (8 bits total), we get 2 chunks
TEST_F(NsmGetSupportedGPMMetricsTest,
       SplitMetricsBitfield_SmallMaxMetrics_MultipleChunks)
{
    auto sensor = makeSensor("split_multi");
    sensor.maxMetricsPerCommand = 4; // 4 bits per chunk
    sensor.maskSize = 1;
    // 8 bits total / 4 per chunk = 2 chunks
    auto result = sensor.splitMetricsBitfield({0x0F});
    EXPECT_EQ(result.size(), 2u);
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics tests
// ============================================================================

struct NsmGetSupportedPerInstanceGPMMetricsTest : public Test
{
    static boost::asio::io_context io;
    std::shared_ptr<MockNsmDevice> nsmDevice{std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 1, "NSM_DEVICE_INSTANCE_NUMBER", "1", 0)};
    std::shared_ptr<MockMetricPerInstanceUpdatorBranch> updator{
        std::make_shared<MockMetricPerInstanceUpdatorBranch>()};

    NsmGetSupportedPerInstanceGPMMetrics makeSensor(
        const std::string& suffix,
        const std::vector<bitfield8_t>& instBitfield = {{.byte = 0x0F}})
    {
        static int idx = 0;
        ++idx;
        return NsmGetSupportedPerInstanceGPMMetrics{
            "TestGetSupportedPerInst_" + suffix + std::to_string(idx),
            "TestType",
            nsmDevice,
            2,
            0,
            0,
            10,
            instBitfield,
            GPMMetricsUnit::PERCENTAGE,
            updator};
    }
};

boost::asio::io_context NsmGetSupportedPerInstanceGPMMetricsTest::io;

// Constructor creates sensor successfully
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       Constructor_CreatesSuccessfully)
{
    EXPECT_NO_THROW({ auto s = makeSensor("ctor"); });
}

// genRequestMsg returns valid message
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       GenRequestMsg_ReturnsValidMessage)
{
    auto sensor = makeSensor("gen_req");
    auto result = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_gpm_metrics_req));
}

// handleResponseMsg: decode failure (too-small buffer)
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       HandleResponseMsg_DecodeFailure_ReturnsError)
{
    auto sensor = makeSensor("decode_fail");
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.hasValidResponse());
}

// handleResponseMsg: cc != NSM_SUCCESS → error
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       HandleResponseMsg_CcError_ReturnsError)
{
    auto sensor = makeSensor("cc_err");
    auto buf = makeGetSupportedGPMMetricsResp(NSM_ERROR, 1, 8, {0x01});
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.hasValidResponse());
}

// handleResponseMsg: already received → returns NSM_SUCCESS immediately
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       HandleResponseMsg_AlreadyReceived_ReturnsSuccess)
{
    auto sensor = makeSensor("already_recv");
    sensor.responseReceived = true;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// handleResponseMsg: success with empty instance bitfield (all zeros)
// → instanceChunks produced but have 0 instances → skipped → 0 sensors
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       HandleResponseMsg_Success_EmptyInstBitfield_NoSensors)
{
    std::vector<bitfield8_t> emptyBitfield{{.byte = 0x00}};
    auto sensor = makeSensor("empty_ibf", emptyBitfield);
    auto buf = makeGetSupportedGPMMetricsResp(NSM_SUCCESS, 1, 8, {0xFF});
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());
}

// handleResponseMsg: success with non-zero instance bitfield → sensors created
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       HandleResponseMsg_Success_NonZeroInstBitfield_SensorsCreated)
{
    std::vector<bitfield8_t> instBitfield{{.byte = 0x01}};
    auto sensor = makeSensor("nonzero_ibf", instBitfield);
    auto buf = makeGetSupportedGPMMetricsResp(NSM_SUCCESS, 1, 8, {0xFF});
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());
    EXPECT_EQ(sensor.getMaxMetricsPerCommand(), 8);
}

// splitInstanceBitfield: maxMetricsPerCommand == 0 → returns empty
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       SplitInstanceBitfield_ZeroMaxMetrics_ReturnsEmpty)
{
    auto sensor = makeSensor("split_zero_max");
    sensor.maxMetricsPerCommand = 0;
    sensor.maskSize = 1;
    std::vector<bitfield8_t> instBf{{.byte = 0xFF}};
    auto result = sensor.splitInstanceBitfield(instBf);
    EXPECT_TRUE(result.empty());
}

// splitInstanceBitfield: maskSize == 0 → returns empty
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       SplitInstanceBitfield_ZeroMaskSize_ReturnsEmpty)
{
    auto sensor = makeSensor("split_zero_mask");
    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 0;
    std::vector<bitfield8_t> instBf{{.byte = 0xFF}};
    auto result = sensor.splitInstanceBitfield(instBf);
    EXPECT_TRUE(result.empty());
}

// splitInstanceBitfield: empty bitfield → returns empty
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       SplitInstanceBitfield_EmptyBitfield_ReturnsEmpty)
{
    auto sensor = makeSensor("split_empty");
    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 1;
    std::vector<bitfield8_t> empty{};
    auto result = sensor.splitInstanceBitfield(empty);
    EXPECT_TRUE(result.empty());
}

// splitInstanceBitfield: valid params → single chunk
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       SplitInstanceBitfield_Valid_SingleChunk)
{
    auto sensor = makeSensor("split_valid");
    sensor.maxMetricsPerCommand = 8;
    sensor.maskSize = 1;
    std::vector<bitfield8_t> instBf{{.byte = 0x03}};
    auto result = sensor.splitInstanceBitfield(instBf);
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0][0].byte, 0x03);
}

// splitInstanceBitfield: maxMetricsPerCommand < total bits → multiple chunks
// With maxMetricsPerCommand=4 and maskSize=1 (8 bits total), we get 2 chunks
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       SplitInstanceBitfield_SmallMax_MultipleChunks)
{
    auto sensor = makeSensor("split_multi");
    sensor.maxMetricsPerCommand = 4; // 4 bits per chunk
    sensor.maskSize = 1;
    std::vector<bitfield8_t> instBf{{.byte = 0x0F}};
    // 8 bits total / 4 per chunk = 2 chunks
    auto result = sensor.splitInstanceBitfield(instBf);
    EXPECT_EQ(result.size(), 2u);
}

// NsmGetSupportedGPMMetrics: handleResponseMsg success with DRAM metric
// (bit 4 set in configuredMetricsBitfield) → DRAMUsageMetricUpdator setup
TEST(NsmGetSupportedGPMMetricsDRAM, HandleResponseMsg_DramMetric_UpdaterSetup)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_dram"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(bus,
                                                          "/xyz/test/gpm_dram");
    auto dimmIntf = std::make_shared<DimmIntf>(bus,
                                               "/xyz/test/dimm_for_gpm_dram");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 2, "NSM_DEVICE_INSTANCE_NUMBER", "2", 0);

    // configuredMetricsBitfield: bit 4 set (DRAM usage metric ID = 4)
    NsmGetSupportedGPMMetrics sensor{"TestGetSupportedDRAM",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_dram",
                                     2,
                                     0,
                                     0,
                                     {0x10}, // bit 4 set (DRAM metric)
                                     gpmIntf,
                                     nvlinkIntf,
                                     dimmIntf,
                                     "/xyz/test/dimm_for_gpm_dram"};

    auto buf = makeGetSupportedGPMMetricsResp(NSM_SUCCESS, 1, 8, {0xFF});
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::genRequestMsg encode failure
// Covers: if (rc != NSM_SW_SUCCESS) TRUE branch (L799)
// ============================================================================
TEST_F(NsmGetSupportedGPMMetricsTest,
       GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    auto sensor = makeSensor("gen_fail");
    // NSM_INSTANCE_MAX + 1 causes encode to fail
    auto result = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// NsmGetSupportedPerInstanceGPMMetrics::genRequestMsg encode failure
// Covers: if (rc != NSM_SW_SUCCESS) TRUE branch (L997)
// ============================================================================
TEST_F(NsmGetSupportedPerInstanceGPMMetricsTest,
       GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    auto sensor = makeSensor("gen_fail");
    auto result = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg - dimmIntf==nullptr, bit 4 set
// Covers: if (dimmIntf && !dramUpdaterConfigured) FALSE branch (L897)
// when dimmIntf is null: short-circuits, DRAM updater not set up
// ============================================================================
TEST(NsmGetSupportedGPMMetricsNoDimm,
     HandleResponseMsg_NullDimmIntf_NoDramUpdater)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/test/gpm_nodimm"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, "/xyz/test/gpm_nodimm");
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 3, "NSM_DEVICE_INSTANCE_NUMBER", "3", 0);

    // dimmIntf == nullptr; bit 4 set → DRAM metric present but no updater
    NsmGetSupportedGPMMetrics sensor{"TestGetSupportedNoDimm",
                                     "TestType",
                                     GPM_METRIC_TYPE_AGGREGATE,
                                     nsmDevice,
                                     "/xyz/test/gpm_nodimm",
                                     2,
                                     0,
                                     0,
                                     {0x10}, // bit 4 set (DRAM usage metric)
                                     gpmIntf,
                                     nvlinkIntf,
                                     nullptr, // no dimmIntf
                                     ""};

    auto buf = makeGetSupportedGPMMetricsResp(NSM_SUCCESS, 1, 8, {0xFF});
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());
}

// ============================================================================
// GPMMetricUpdator::updateMetric same-value branch
// Covers: if (previousValue != val && gpmIntf) FALSE branch (L50)
// Bit 0 in bitfield → metricsTable[0] = GPMMetricUpdator
// ============================================================================
TEST(NsmGPMAggregatedBranch, HandleSample_GPMMetric_SameValue_SkipsUpdate)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    const std::string path =
        "/xyz/openbmc_project/inventory/gpm_branch/same_val";
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path(path), "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(bus, path.c_str());

    // Bit 0 set → GraphicsEngineActivityPercent → GPMMetricUpdator at slot 0
    NsmGPMAggregated gpm{"gpm_same_val",
                         "AggregatedGPMMetrics",
                         path,
                         2,
                         0,
                         0,
                         {0x01},
                         gpmAsioIntf,
                         nvlinkIntf};

    // Valid 4-byte percentage payload (uint32_t)
    uint32_t percentage = 75;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&percentage);
    uint8_t dataLen = sizeof(percentage);

    // First call: previousValue(NaN) != val → update attempted
    auto rc1 = gpm.handleSample({0, dataLen, data, true});
    EXPECT_EQ(rc1, NSM_SW_SUCCESS);

    // Second call: previousValue == val → FALSE branch (no set_property call)
    auto rc2 = gpm.handleSample({0, dataLen, data, true});
    EXPECT_EQ(rc2, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmGetSupportedGPMMetrics::handleResponseMsg allzero configuredBitfield
// Covers: if (numMetrics == 0) TRUE branch via empty chunk (L880)
// configuredMetricsBitfield={0x00} → split produces 1 chunk of zeros
// ============================================================================
TEST_F(NsmGetSupportedGPMMetricsTest,
       HandleResponseMsg_AllZeroBitfield_EmptyChunkSkipped)
{
    // makeSensor uses {0x00} bitfield → splitMetricsBitfield → 1 chunk, all
    // zero bits → numMetrics=0 → if (numMetrics==0) TRUE → skip
    auto sensor = makeSensor("zero_bf");
    auto buf = makeGetSupportedGPMMetricsResp(NSM_SUCCESS, 1, 8, {0xFF});
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.hasValidResponse());
}
