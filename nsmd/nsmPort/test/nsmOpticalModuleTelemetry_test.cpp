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

// Unit tests for NsmOpticalModuleTelemetry (NSM Type-1 cmd 0x14 group 0x09
// Optical Module Metrics) and, via it, the shared
// NsmSensorAggregatorPaginated::update() pagination loop:
//   - Constructor: D-Bus properties initialize to 8-element zero vectors
//     (regression test for the brace-vs-paren vector-init bug: brace-init
//     with an enum-converts-to-double count selected the
//     initializer_list<double> ctor and produced a 2-element vector).
//   - genRequestMsg / handleSample tag routing (TX power, RX power, bias
//     current, SNR raw-to-dB scaling, unknown tag) / resetState / postUpdate.
//   - update(): single-page and multi-page happy paths, the kMaxPages guard,
//     the stuck-sequence-token guard, and the decode-failure break (not
//     continue) fix.

#include "network-ports.h"
#include "platform-environmental.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <any>
#include <sstream>

using namespace ::testing;

#define private public
#define protected public

#include "nsmPort.hpp"

#undef protected
#undef private

using namespace nsm;

namespace
{

constexpr uint8_t kNumberOfLanes = 8;

// Build one page of a group 0x09 Optical Module aggregate response: all 32
// per-lane samples (TX power, RX power, bias current, SNR), the per-module
// tx_bias_scaling_factor record (tag 0x20), plus a terminating Sequence
// Token Record with the given next_sequence_token.
Response buildOpticalModulePage(uint32_t nextSequenceToken,
                                bool includeSamples = true)
{
    std::vector<uint8_t> body;
    uint16_t sampleCount = 0;

    auto appendSample = [&](uint8_t tag, const uint8_t* data, size_t dataLen) {
        std::array<uint8_t, 256> sampleBuf{};
        auto sample =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf.data());
        size_t sampleLen = 0;
        auto rc = encode_aggregate_resp_sample(tag, true, data, dataLen, sample,
                                               &sampleLen, 0);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        body.insert(body.end(), sampleBuf.begin(),
                    std::next(sampleBuf.begin(), static_cast<long>(sampleLen)));
        ++sampleCount;
    };

    if (includeSamples)
    {
        static const uint8_t bases[] = {
            NSM_OPTICAL_MODULE_TAG_TX_POWER_BASE,
            NSM_OPTICAL_MODULE_TAG_RX_POWER_BASE,
            NSM_OPTICAL_MODULE_TAG_BIAS_CURRENT_BASE};
        for (uint8_t base : bases)
        {
            for (uint8_t lane = 0; lane < kNumberOfLanes; ++lane)
            {
                uint16_t value =
                    htole16(static_cast<uint16_t>(1000 + base + lane));
                appendSample(static_cast<uint8_t>(base + lane),
                             reinterpret_cast<uint8_t*>(&value), sizeof(value));
            }
        }
        for (uint8_t lane = 0; lane < kNumberOfLanes; ++lane)
        {
            uint32_t raw = htole32(static_cast<uint32_t>(5665 + lane * 100));
            appendSample(
                static_cast<uint8_t>(NSM_OPTICAL_MODULE_TAG_SNR_BASE + lane),
                reinterpret_cast<uint8_t*>(&raw), sizeof(raw));
        }
        // Tag 0x20 trails the lane blocks, as it does on the wire.
        uint8_t biasScale = NSM_OPTICAL_MODULE_BIAS_SCALE_2X;
        appendSample(NSM_OPTICAL_MODULE_TAG_BIAS_SCALING_FACTOR, &biasScale,
                     sizeof(biasScale));
    }

    uint32_t tokenLe = htole32(nextSequenceToken);
    appendSample(0xFD, reinterpret_cast<uint8_t*>(&tokenLe), sizeof(tokenLe));

    std::vector<uint8_t> header(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    auto headerMsg = reinterpret_cast<nsm_msg*>(header.data());
    auto rc = encode_aggregate_resp(0, NSM_QUERY_PORT_TELEMETRY_COUNTER_V2,
                                    NSM_SUCCESS, sampleCount, headerMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    Response full = header;
    full.insert(full.end(), body.begin(), body.end());
    return full;
}

} // namespace

// ===========================================================================
// Constructor / resetState — 8-element zero-vector regression coverage
// ===========================================================================

TEST(NsmOpticalModuleTelemetryTest, ConstructorInitializesEightZeroLanes)
{
    auto bus = sdbusplus::bus::new_default();
    std::string objPath = "/xyz/openbmc_project/test/optical_ctor";

    NsmOpticalModuleTelemetry sensor(bus, "CX_0", "NSM_NVLinkWithNetworkPort",
                                     objPath, /*portNumber=*/1);

    ASSERT_EQ(sensor.rxPowerMW_.size(), kNumberOfLanes);
    ASSERT_EQ(sensor.txPowerMW_.size(), kNumberOfLanes);
    ASSERT_EQ(sensor.txBiasmA_.size(), kNumberOfLanes);
    ASSERT_EQ(sensor.snrDB_.size(), kNumberOfLanes);
    for (uint8_t lane = 0; lane < kNumberOfLanes; ++lane)
    {
        EXPECT_DOUBLE_EQ(sensor.rxPowerMW_[lane], 0.0);
        EXPECT_DOUBLE_EQ(sensor.txPowerMW_[lane], 0.0);
        EXPECT_DOUBLE_EQ(sensor.txBiasmA_[lane], 0.0);
        EXPECT_DOUBLE_EQ(sensor.snrDB_[lane], 0.0);
    }

    // D-Bus properties are seeded from the same (correctly sized) vectors.
    EXPECT_EQ(sensor.opticalMetricsIntf_->rxInputPowerMilliWatts().size(),
              kNumberOfLanes);
    EXPECT_EQ(sensor.opticalMetricsIntf_->txOutputPowerMilliWatts().size(),
              kNumberOfLanes);
    EXPECT_EQ(sensor.opticalMetricsIntf_->txBiasCurrentMilliAmps().size(),
              kNumberOfLanes);
    EXPECT_EQ(sensor.opticalMetricsIntf_->signalToNoiseRatioPerLane().size(),
              kNumberOfLanes);
}

TEST(NsmOpticalModuleTelemetryTest, ResetStateRestoresEightZeroLanes)
{
    auto bus = sdbusplus::bus::new_default();
    std::string objPath = "/xyz/openbmc_project/test/optical_reset";

    NsmOpticalModuleTelemetry sensor(bus, "CX_1", "NSM_NVLinkWithNetworkPort",
                                     objPath, /*portNumber=*/2);

    // Populate lane 0 via handleSample, then reset.
    uint16_t value = htole16(4242);
    NsmSensorAggregatorPaginated::TelemetrySample sample{
        NSM_OPTICAL_MODULE_TAG_TX_POWER_BASE, sizeof(value),
        reinterpret_cast<const uint8_t*>(&value), true};
    ASSERT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
    ASSERT_EQ(sensor.rawTxPower_[0], 4242);

    // Also dirty the bias scaling state so resetState() is covered for it.
    uint8_t encoded = NSM_OPTICAL_MODULE_BIAS_SCALE_4X;
    NsmSensorAggregatorPaginated::TelemetrySample scaleSample{
        NSM_OPTICAL_MODULE_TAG_BIAS_SCALING_FACTOR, sizeof(encoded), &encoded,
        true};
    ASSERT_EQ(sensor.handleSample(scaleSample), NSM_SW_SUCCESS);
    ASSERT_EQ(sensor.encodedBiasScale_, NSM_OPTICAL_MODULE_BIAS_SCALE_4X);

    sensor.resetState();

    // Zero is Multiply_1x, which is also the spec's fallback when the tag
    // is absent, so the reset value needs no special default.
    EXPECT_EQ(sensor.encodedBiasScale_, NSM_OPTICAL_MODULE_BIAS_SCALE_1X);
    ASSERT_EQ(sensor.rawBiasCurrent_.size(), kNumberOfLanes);
    for (uint8_t lane = 0; lane < kNumberOfLanes; ++lane)
    {
        EXPECT_EQ(sensor.rawTxPower_[lane], 0);
        EXPECT_EQ(sensor.rawRxPower_[lane], 0);
        EXPECT_EQ(sensor.rawBiasCurrent_[lane], 0);
        EXPECT_EQ(sensor.rawSnr_[lane], 0u);
    }

    ASSERT_EQ(sensor.txPowerMW_.size(), kNumberOfLanes);
    for (uint8_t lane = 0; lane < kNumberOfLanes; ++lane)
    {
        EXPECT_DOUBLE_EQ(sensor.rxPowerMW_[lane], 0.0);
        EXPECT_DOUBLE_EQ(sensor.txPowerMW_[lane], 0.0);
        EXPECT_DOUBLE_EQ(sensor.txBiasmA_[lane], 0.0);
        EXPECT_DOUBLE_EQ(sensor.snrDB_[lane], 0.0);
    }
}

// ===========================================================================
// genRequestMsg
// ===========================================================================

TEST(NsmOpticalModuleTelemetryTest, GenRequestMsg_Success)
{
    auto bus = sdbusplus::bus::new_default();
    std::string objPath = "/xyz/openbmc_project/test/optical_genreq";
    NsmOpticalModuleTelemetry sensor(bus, "CX_2", "NSM_NVLinkWithNetworkPort",
                                     objPath, /*portNumber=*/3);

    auto request = sensor.genRequestMsg(/*eid=*/10, /*instanceId=*/1);
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_v2_req));
}

TEST(NsmOpticalModuleTelemetryTest, GenRequestMsg_BadInstanceId_ReturnsNullopt)
{
    auto bus = sdbusplus::bus::new_default();
    std::string objPath = "/xyz/openbmc_project/test/optical_genreq_bad";
    NsmOpticalModuleTelemetry sensor(bus, "CX_3", "NSM_NVLinkWithNetworkPort",
                                     objPath, /*portNumber=*/4);

    auto request = sensor.genRequestMsg(/*eid=*/10,
                                        /*instanceId=*/NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ===========================================================================
// handleSample — tag routing across all four metric blocks
// ===========================================================================

class NsmOpticalModuleTelemetryHandleSampleTest : public ::testing::Test
{
  protected:
    NsmOpticalModuleTelemetryHandleSampleTest() :
        bus(sdbusplus::bus::new_default()),
        sensor(bus, "CX_4", "NSM_NVLinkWithNetworkPort",
               "/xyz/openbmc_project/test/optical_handlesample",
               /*portNumber=*/5)
    {}

    static NsmSensorAggregatorPaginated::TelemetrySample
        makeSample(uint8_t tag, const uint8_t* data, size_t dataLen)
    {
        return {tag, static_cast<uint8_t>(dataLen), data, true};
    }

    sdbusplus::bus_t bus;
    NsmOpticalModuleTelemetry sensor;
};

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest, TxPower_Lane0_Stored)
{
    uint16_t value = htole16(1234);
    auto sample = makeSample(NSM_OPTICAL_MODULE_TAG_TX_POWER_BASE,
                             reinterpret_cast<uint8_t*>(&value), sizeof(value));
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
    // handleSample() only decodes; the raw value is held as received.
    EXPECT_EQ(sensor.rawTxPower_[0], 1234);

    sensor.convertSamples();
    // 1 uW LSB: raw 1234 -> 1.234 mW.
    EXPECT_DOUBLE_EQ(sensor.txPowerMW_[0], 1.234);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest, TxPower_Lane7_Boundary)
{
    uint16_t value = htole16(777);
    auto sample =
        makeSample(static_cast<uint8_t>(NSM_OPTICAL_MODULE_TAG_TX_POWER_BASE +
                                        (kNumberOfLanes - 1)),
                   reinterpret_cast<uint8_t*>(&value), sizeof(value));
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
    sensor.convertSamples();
    EXPECT_DOUBLE_EQ(sensor.txPowerMW_[kNumberOfLanes - 1], 0.777);
    // Adjacent lanes untouched.
    EXPECT_DOUBLE_EQ(sensor.txPowerMW_[0], 0.0);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest, RxPower_Lane3_Stored)
{
    uint16_t value = htole16(555);
    auto sample = makeSample(
        static_cast<uint8_t>(NSM_OPTICAL_MODULE_TAG_RX_POWER_BASE + 3),
        reinterpret_cast<uint8_t*>(&value), sizeof(value));
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
    sensor.convertSamples();
    EXPECT_DOUBLE_EQ(sensor.rxPowerMW_[3], 0.555);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest,
       BiasCurrent_Lane5_BufferedRawUntilPostUpdate)
{
    uint16_t value = htole16(88);
    auto sample = makeSample(
        static_cast<uint8_t>(NSM_OPTICAL_MODULE_TAG_BIAS_CURRENT_BASE + 5),
        reinterpret_cast<uint8_t*>(&value), sizeof(value));
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
    // Held raw: handleSample() decodes only, and the tag 0x20 scaling
    // factor is not known yet.
    EXPECT_EQ(sensor.rawBiasCurrent_[5], 88);
    EXPECT_DOUBLE_EQ(sensor.txBiasmA_[5], 0.0);

    sensor.postUpdate();
    // No tag 0x20 seen -> 1x per spec: 88 * 2 * 1 / 1000 = 0.176 mA.
    EXPECT_DOUBLE_EQ(sensor.txBiasmA_[5], 0.176);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest,
       BiasScalingFactor_2x_AppliedInPostUpdate)
{
    // Values captured on hardware: raw 11004 on a 2x module reads
    // 44.016 mA in-band.
    uint16_t bias = htole16(11004);
    auto biasSample = makeSample(NSM_OPTICAL_MODULE_TAG_BIAS_CURRENT_BASE,
                                 reinterpret_cast<uint8_t*>(&bias),
                                 sizeof(bias));
    EXPECT_EQ(sensor.handleSample(biasSample), NSM_SW_SUCCESS);

    // Tag 0x20 follows the bias tags in tag order.
    uint8_t encoded = NSM_OPTICAL_MODULE_BIAS_SCALE_2X;
    auto scaleSample = makeSample(NSM_OPTICAL_MODULE_TAG_BIAS_SCALING_FACTOR,
                                  &encoded, sizeof(encoded));
    EXPECT_EQ(sensor.handleSample(scaleSample), NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.encodedBiasScale_, NSM_OPTICAL_MODULE_BIAS_SCALE_2X);

    sensor.postUpdate();
    EXPECT_DOUBLE_EQ(sensor.txBiasmA_[0], 44.016);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest,
       BiasScalingFactor_4x_AppliedInPostUpdate)
{
    // A 4x module captured on hardware: raw 17338 -> 138.704 mA.
    uint16_t bias = htole16(17338);
    auto biasSample = makeSample(NSM_OPTICAL_MODULE_TAG_BIAS_CURRENT_BASE,
                                 reinterpret_cast<uint8_t*>(&bias),
                                 sizeof(bias));
    EXPECT_EQ(sensor.handleSample(biasSample), NSM_SW_SUCCESS);

    uint8_t encoded = NSM_OPTICAL_MODULE_BIAS_SCALE_4X;
    auto scaleSample = makeSample(NSM_OPTICAL_MODULE_TAG_BIAS_SCALING_FACTOR,
                                  &encoded, sizeof(encoded));
    EXPECT_EQ(sensor.handleSample(scaleSample), NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.encodedBiasScale_, NSM_OPTICAL_MODULE_BIAS_SCALE_4X);

    sensor.postUpdate();
    EXPECT_DOUBLE_EQ(sensor.txBiasmA_[0], 138.704);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest,
       BiasScalingFactor_DoesNotCarryIntoNextCollection)
{
    uint16_t bias = htole16(11004);
    auto biasSample = makeSample(NSM_OPTICAL_MODULE_TAG_BIAS_CURRENT_BASE,
                                 reinterpret_cast<uint8_t*>(&bias),
                                 sizeof(bias));
    uint8_t encoded = NSM_OPTICAL_MODULE_BIAS_SCALE_2X;
    auto scaleSample = makeSample(NSM_OPTICAL_MODULE_TAG_BIAS_SCALING_FACTOR,
                                  &encoded, sizeof(encoded));

    ASSERT_EQ(sensor.handleSample(biasSample), NSM_SW_SUCCESS);
    ASSERT_EQ(sensor.handleSample(scaleSample), NSM_SW_SUCCESS);
    sensor.postUpdate();
    ASSERT_DOUBLE_EQ(sensor.txBiasmA_[0], 44.016);

    // The next collection carries no tag 0x20. Availability is determined
    // per response, and the spec directs Multiply_1x when the factor is
    // unavailable, so the previous collection's factor must not be reused.
    ASSERT_EQ(sensor.handleSample(biasSample), NSM_SW_SUCCESS);
    sensor.postUpdate();
    EXPECT_DOUBLE_EQ(sensor.txBiasmA_[0], 22.008);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest,
       BiasScalingFactor_UnknownEncoding_FallsBackTo1xWithoutFailing)
{
    // An encoding this build does not know cannot be converted -- the
    // multiplier is defined by the spec table, not carried on the wire --
    // so it falls back to 1x. It must not discard the power and SNR samples
    // that arrived in the same response.
    uint8_t encoded = 3;
    auto sample = makeSample(NSM_OPTICAL_MODULE_TAG_BIAS_SCALING_FACTOR,
                             &encoded, sizeof(encoded));
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.encodedBiasScale_, encoded);
    // Mapping happens at conversion time and falls back to 1x.
    EXPECT_EQ(sensor.biasScaleFromEncoding(sensor.encodedBiasScale_), 1);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest,
       BiasScaleFromEncoding_MapsEachEncodingExplicitly)
{
    EXPECT_EQ(sensor.biasScaleFromEncoding(NSM_OPTICAL_MODULE_BIAS_SCALE_1X),
              1);
    EXPECT_EQ(sensor.biasScaleFromEncoding(NSM_OPTICAL_MODULE_BIAS_SCALE_2X),
              2);
    EXPECT_EQ(sensor.biasScaleFromEncoding(NSM_OPTICAL_MODULE_BIAS_SCALE_4X),
              4);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest,
       BiasScalingFactor_WrongLength_KeepsDefault)
{
    uint8_t tooLong[2] = {NSM_OPTICAL_MODULE_BIAS_SCALE_2X, 0};
    auto sample = makeSample(NSM_OPTICAL_MODULE_TAG_BIAS_SCALING_FACTOR,
                             tooLong, sizeof(tooLong));
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.encodedBiasScale_, NSM_OPTICAL_MODULE_BIAS_SCALE_1X);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest, TxPower_HardwareCapturedValue)
{
    // Captured on hardware: raw 1572 -> 1.572 mW (in-band ~1.96 dBm).
    uint16_t value = htole16(1572);
    auto sample = makeSample(NSM_OPTICAL_MODULE_TAG_TX_POWER_BASE,
                             reinterpret_cast<uint8_t*>(&value), sizeof(value));
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
    sensor.convertSamples();
    EXPECT_DOUBLE_EQ(sensor.txPowerMW_[0], 1.572);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest, Snr_Lane2_RawDividedBy256)
{
    // raw PRM value 5665 -> 22.13 dB.
    uint32_t raw = htole32(5665);
    auto sample =
        makeSample(static_cast<uint8_t>(NSM_OPTICAL_MODULE_TAG_SNR_BASE + 2),
                   reinterpret_cast<uint8_t*>(&raw), sizeof(raw));
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.rawSnr_[2], 5665);

    sensor.convertSamples();
    EXPECT_NEAR(sensor.snrDB_[2], 22.13, 0.01);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest, UnknownTag_ReturnsErrorData)
{
    // 0x28 is outside the defined tags (0x00-0x1F lane blocks plus 0x20).
    uint16_t value = htole16(1);
    auto sample = makeSample(0x28, reinterpret_cast<uint8_t*>(&value),
                             sizeof(value));
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_ERROR_DATA);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest,
       PowerBiasTag_WrongLength_DecodeFails)
{
    uint8_t tooShort[1] = {0};
    auto sample = makeSample(NSM_OPTICAL_MODULE_TAG_RX_POWER_BASE, tooShort,
                             sizeof(tooShort));
    EXPECT_NE(sensor.handleSample(sample), NSM_SW_SUCCESS);
    // Value must remain untouched on decode failure.
    EXPECT_EQ(sensor.rawRxPower_[0], 0);
}

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest,
       SnrTag_WrongLength_DecodeFails)
{
    uint8_t tooShort[2] = {0, 0};
    auto sample = makeSample(NSM_OPTICAL_MODULE_TAG_SNR_BASE, tooShort,
                             sizeof(tooShort));
    EXPECT_NE(sensor.handleSample(sample), NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.rawSnr_[0], 0u);
}

// ===========================================================================
// postUpdate — D-Bus properties reflect the accumulation vectors
// ===========================================================================

TEST_F(NsmOpticalModuleTelemetryHandleSampleTest,
       PostUpdatePublishesAccumulatedValues)
{
    uint16_t txValue = htole16(111);
    uint32_t snrRaw = htole32(2560); // 2560 / 256 = 10.0 dB exactly
    ASSERT_EQ(sensor.handleSample(makeSample(
                  NSM_OPTICAL_MODULE_TAG_TX_POWER_BASE,
                  reinterpret_cast<uint8_t*>(&txValue), sizeof(txValue))),
              NSM_SW_SUCCESS);
    ASSERT_EQ(sensor.handleSample(makeSample(
                  NSM_OPTICAL_MODULE_TAG_SNR_BASE,
                  reinterpret_cast<uint8_t*>(&snrRaw), sizeof(snrRaw))),
              NSM_SW_SUCCESS);

    sensor.postUpdate();

    auto publishedTx = sensor.opticalMetricsIntf_->txOutputPowerMilliWatts();
    auto publishedSnr = sensor.opticalMetricsIntf_->signalToNoiseRatioPerLane();
    ASSERT_EQ(publishedTx.size(), kNumberOfLanes);
    ASSERT_EQ(publishedSnr.size(), kNumberOfLanes);
    // 1 uW LSB: raw 111 -> 0.111 mW. SNR is unaffected by this change.
    EXPECT_DOUBLE_EQ(publishedTx[0], 0.111);
    EXPECT_DOUBLE_EQ(publishedSnr[0], 10.0);
}

// ===========================================================================
// update() — paginated fetch loop (NsmSensorAggregatorPaginated), exercised
// through the concrete NsmOpticalModuleTelemetry sensor.
// ===========================================================================

struct NsmOpticalModuleTelemetryUpdateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t bridgeUuid = "STATIC:2:0:NSM_DEVICE_INSTANCE_NUMBER:70";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> bridge;

    NsmOpticalModuleTelemetryUpdateTest() : SensorManagerTest(devices)
    {
        bridge = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(bridgeUuid));
        EXPECT_NE(bridge, nullptr);
    }

    ~NsmOpticalModuleTelemetryUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmOpticalModuleTelemetryUpdateTest, SinglePage_PublishesAllLanes)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmOpticalModuleTelemetry sensor(
        bus, "CX_Single", "NSM_NVLinkWithNetworkPort",
        "/xyz/openbmc_project/test/optical_update_single",
        /*portNumber=*/1);

    auto page = buildOpticalModulePage(/*nextSequenceToken=*/0);
    EXPECT_CALL(*bridge, sensorIO).WillOnce(mockSensorIO(page));

    auto rc = sensor.update(bridge).data();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto txPower = sensor.opticalMetricsIntf_->txOutputPowerMilliWatts();
    ASSERT_EQ(txPower.size(), kNumberOfLanes);
    EXPECT_DOUBLE_EQ(
        txPower[0],
        (1000.0 + static_cast<double>(NSM_OPTICAL_MODULE_TAG_TX_POWER_BASE)) /
            1000.0);

    // Page carries tag 0x20 = 2x, so bias lane 0 is raw * 2 * 2 / 1000.
    auto txBias = sensor.opticalMetricsIntf_->txBiasCurrentMilliAmps();
    ASSERT_EQ(txBias.size(), kNumberOfLanes);
    EXPECT_DOUBLE_EQ(txBias[0],
                     (1000.0 + static_cast<double>(
                                   NSM_OPTICAL_MODULE_TAG_BIAS_CURRENT_BASE)) *
                         2.0 * 2.0 / 1000.0);
}

TEST_F(NsmOpticalModuleTelemetryUpdateTest, TwoPages_SecondPageTerminates)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmOpticalModuleTelemetry sensor(
        bus, "CX_TwoPage", "NSM_NVLinkWithNetworkPort",
        "/xyz/openbmc_project/test/optical_update_twopage",
        /*portNumber=*/2);

    auto page1 = buildOpticalModulePage(/*nextSequenceToken=*/0xAAAA0001,
                                        /*includeSamples=*/false);
    auto page2 = buildOpticalModulePage(/*nextSequenceToken=*/0);

    EXPECT_CALL(*bridge, sensorIO)
        .WillOnce(mockSensorIO(page1))
        .WillOnce(mockSensorIO(page2));

    auto rc = sensor.update(bridge).data();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto snr = sensor.opticalMetricsIntf_->signalToNoiseRatioPerLane();
    ASSERT_EQ(snr.size(), kNumberOfLanes);
    EXPECT_NEAR(snr[0], 5665.0 / 256.0, 0.01);
}

TEST_F(NsmOpticalModuleTelemetryUpdateTest, StuckSequenceToken_AbortsWithError)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmOpticalModuleTelemetry sensor(
        bus, "CX_Stuck", "NSM_NVLinkWithNetworkPort",
        "/xyz/openbmc_project/test/optical_update_stuck",
        /*portNumber=*/3);

    // Firmware echoes back the same non-zero token twice in a row.
    auto page1 = buildOpticalModulePage(/*nextSequenceToken=*/0x1234,
                                        /*includeSamples=*/false);
    auto page2 = buildOpticalModulePage(/*nextSequenceToken=*/0x1234,
                                        /*includeSamples=*/false);

    EXPECT_CALL(*bridge, sensorIO)
        .WillOnce(mockSensorIO(page1))
        .WillOnce(mockSensorIO(page2));

    auto rc = sensor.update(bridge).data();
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

TEST_F(NsmOpticalModuleTelemetryUpdateTest, ManyPages_MaxPageGuardAborts)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmOpticalModuleTelemetry sensor(
        bus, "CX_MaxPages", "NSM_NVLinkWithNetworkPort",
        "/xyz/openbmc_project/test/optical_update_maxpages",
        /*portNumber=*/4);

    // Firmware advances the token every page (never stuck, never zero) --
    // the only thing that can stop this is the kMaxPages cap.
    auto counter = std::make_shared<uint32_t>(1);
    EXPECT_CALL(*bridge, sensorIO)
        .WillRepeatedly([counter](eid_t, Request&,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t& responseLen,
                                  bool) -> requester::Coroutine {
        auto page = buildOpticalModulePage(/*nextSequenceToken=*/++(*counter),
                                           /*includeSamples=*/false);
        responseLen = page.size();
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
            [](const nsm_msg* ptr) { free((void*)ptr); });
        memcpy((uint8_t*)responseMsg.get(), page.data(), responseLen);
        // coverity[missing_return]
        co_return NSM_SUCCESS;
    });

    auto rc = sensor.update(bridge).data();
    EXPECT_EQ(rc, NSM_SW_ERROR);
    // The cap must actually bound the loop -- if it didn't, this test
    // would not reach this line (mock would be invoked forever).
    EXPECT_GT(*counter, 1u);
}
