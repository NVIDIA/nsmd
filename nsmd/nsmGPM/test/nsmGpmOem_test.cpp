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

#include "platform-environmental.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmGpmOem.hpp"

class MockMetricPerInstanceUpdator : public nsm::MetricPerInstanceUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const std::vector<double>& metrics),
                (override));
};

class MockMetricUpdator : public nsm::MetricUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const std::string& name, const double val),
                (override));
};

TEST(nsmGPMAggregated, GoodGenReq)
{
    const uint8_t retrieval_source = 2;
    const uint8_t gpu_instance = 0x47;
    const uint8_t compute_instance = 90;
    const std::vector<uint8_t> metrics_bitfield{0x89, 0x04, 0x15};

    auto bus = sdbusplus::bus::new_default();
    auto gpmIntf = std::make_shared<nsm::GPMMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/gpm");
    auto nvlinkIntf = std::make_shared<nsm::NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/gpm");
    nsm::NsmGPMAggregated gpm{"sensor",
                              "AggregatedGPMMetrics",
                              "/xyz/openbmc_project/inventory/gpm",
                              retrieval_source,
                              gpu_instance,
                              compute_instance,
                              metrics_bitfield,
                              gpmIntf,
                              nvlinkIntf};

    const uint8_t eid{12};
    const uint8_t instance_id{30};
    auto request = gpm.genRequestMsg(eid, instance_id);

    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_query_aggregate_gpm_metrics_req*>(
        msg->payload);

    EXPECT_EQ(NSM_QUERY_AGGREGATE_GPM_METRICS, command->hdr.command);
    EXPECT_EQ(retrieval_source, command->retrieval_source);
    EXPECT_EQ(gpu_instance, command->gpu_instance);
    EXPECT_EQ(compute_instance, command->compute_instance);
    EXPECT_EQ(0x89, command->metrics_bitfield[0]);
    EXPECT_EQ(0x04, command->metrics_bitfield[1]);
    EXPECT_EQ(0x15, command->metrics_bitfield[2]);
}

TEST(nsmGPMAggregated, GoodHandleResp)
{
    const uint8_t retrieval_source = 2;
    const uint8_t gpu_instance = 0x47;
    const uint8_t compute_instance = 90;
    const std::vector<uint8_t> metrics_bitfield{0x89, 0x04, 0x15};

    auto bus = sdbusplus::bus::new_default();
    auto gpmIntf = std::make_shared<nsm::GPMMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/gpm");
    auto nvlinkIntf = std::make_shared<nsm::NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/gpm");
    nsm::NsmGPMAggregated gpm{"sensor",
                              "AggregatedGPMMetrics",
                              "/xyz/openbmc_project/inventory/gpm",
                              retrieval_source,
                              gpu_instance,
                              compute_instance,
                              metrics_bitfield,
                              gpmIntf,
                              nvlinkIntf};

    auto updator = std::make_unique<MockMetricUpdator>();
    auto percentage_updator = updator.get();
    gpm.metricsTable[3] = {"TensorCoreActivityPercent", nsm::decodePercentage,
                           std::move(updator)};

    updator = std::make_unique<MockMetricUpdator>();
    auto bandwidth_updator = updator.get();
    gpm.metricsTable[9] = {"PCIeRawTxBandwidthGbps", nsm::decodeBandwidth,
                           std::move(updator)};

    const double percentage{34.5633};
    const double bandwidth{689535402};
    std::array<uint8_t, sizeof(double)> percentage_data{};
    std::array<uint8_t, sizeof(double)> bandwidth_data{};
    size_t percentage_data_len{};
    size_t bandwidth_data_len{};

    auto rc = encode_aggregate_gpm_metric_percentage_data(
        percentage, percentage_data.data(), &percentage_data_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = encode_aggregate_gpm_metric_bandwidth_data(
        bandwidth, bandwidth_data.data(), &bandwidth_data_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*percentage_updator,
                updateMetric("TensorCoreActivityPercent",
                             testing::DoubleNear(percentage, 0.01)))
        .Times(1);

    static constexpr uint64_t conversionFactor = 1024 * 1024 * 128;
    EXPECT_CALL(*bandwidth_updator, updateMetric("PCIeRawTxBandwidthGbps",
                                                 bandwidth / conversionFactor))
        .Times(1);

    rc = gpm.handleSamples(
        {{3, static_cast<uint8_t>(percentage_data_len), percentage_data.data()},
         {9, static_cast<uint8_t>(bandwidth_data_len), bandwidth_data.data()}});
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmGPMPerIntance, GoodHandleResp)
{
    const uint8_t retrieval_source = 1;
    const uint8_t gpu_instance = 0xFF;
    const uint8_t compute_instance = 38;
    const uint8_t metric_id = 34;
    const uint32_t instance_bitmask = 38;

    auto bus = sdbusplus::bus::new_default();
    auto gpmIntf = std::make_shared<nsm::GPMMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/gpm");

    nsm::NsmGPMPerInstance gpm{
        "sensor",
        "AggregatedGPMMetrics",
        retrieval_source,
        gpu_instance,
        compute_instance,
        metric_id,
        instance_bitmask,
        nsm::GPMMetricsUnit::PERCENTAGE,
        nsm::makeNVDecPerInstanceUpdator("/xyz/openbmc_project/inventory/gpm",
                                         gpmIntf)};

    const uint8_t eid{12};
    const uint8_t instance_id{30};
    auto request = gpm.genRequestMsg(eid, instance_id);

    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_query_per_instance_gpm_metrics_req*>(
            msg->payload);

    EXPECT_EQ(NSM_QUERY_PER_INSTANCE_GPM_METRICS, command->hdr.command);
    EXPECT_EQ(retrieval_source, command->retrieval_source);
    EXPECT_EQ(gpu_instance, command->gpu_instance);
    EXPECT_EQ(compute_instance, command->compute_instance);
    EXPECT_EQ(metric_id, command->metric_id);
    EXPECT_EQ(instance_bitmask, command->instance_bitmask);
}

TEST(nsmGPMPerIntance, GoodGenReq)
{
    const uint8_t retrieval_source = 1;
    const uint8_t gpu_instance = 0xFF;
    const uint8_t compute_instance = 38;
    const uint8_t metric_id = 34;
    const uint32_t instance_bitmask = 38;

    auto bus = sdbusplus::bus::new_default();
    auto gpmIntf = std::make_shared<nsm::GPMMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/gpm");

    nsm::NsmGPMPerInstance gpm{
        "sensor",
        "AggregatedGPMMetrics",
        retrieval_source,
        gpu_instance,
        compute_instance,
        metric_id,
        instance_bitmask,
        nsm::GPMMetricsUnit::BANDWIDTH,
        nsm::makeNVDecPerInstanceUpdator("/xyz/openbmc_project/inventory/gpm",
                                         gpmIntf)};

    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();
    const_cast<std::shared_ptr<nsm::MetricPerInstanceUpdator>&>(
        gpm.metricUpdator) = updator;

    const std::vector<double> bandwidth{345455633, 89144532};
    std::array<std::array<uint8_t, sizeof(double)>, 2> data{};
    size_t data_len{};

    auto rc = encode_aggregate_gpm_metric_bandwidth_data(
        bandwidth[0], data[0].data(), &data_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = encode_aggregate_gpm_metric_bandwidth_data(bandwidth[1],
                                                    data[1].data(), &data_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    static constexpr uint64_t conversionFactor = 1024 * 1024 * 128;
    const std::vector<double> metrics{0, 0, bandwidth[0] / conversionFactor, 0,
                                      bandwidth[1] / conversionFactor};
    EXPECT_CALL(*updator, updateMetric(metrics)).Times(1);

    rc = gpm.handleSamples(
        {{2, static_cast<uint8_t>(data_len), data[0].data()},
         {4, static_cast<uint8_t>(data_len), data[1].data()}});
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
