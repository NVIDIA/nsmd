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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <cmath>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmGpmOem.hpp"

namespace nsm
{
requester::Coroutine createNsmGPMMetrics(SensorManager& manager,
                                         const std::string& interface,
                                         const std::string& objPath);
} // namespace nsm

class MockMetricPerInstanceUpdator : public nsm::MetricPerInstanceUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const std::vector<double>& metrics),
                (override));
};

class MockMetricUpdator : public nsm::MetricUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const double val), (override));
};

// NaN-aware vector matcher for GPM metrics (NaN == NaN in this context)
MATCHER_P(VectorWithNanEq, expected, "")
{
    if (arg.size() != expected.size())
        return false;
    for (size_t i = 0; i < arg.size(); ++i)
    {
        bool argNan = std::isnan(arg[i]);
        bool expNan = std::isnan(expected[i]);
        if (argNan != expNan)
            return false;
        if (!argNan && arg[i] != expected[i])
            return false;
    }
    return true;
}

TEST(nsmGPMAggregated, GoodGenReq)
{
    const uint8_t retrieval_source = 2;
    const uint8_t gpu_instance = 0x47;
    const uint8_t compute_instance = 90;
    const std::vector<uint8_t> metrics_bitfield{0x89, 0x04, 0x15};

    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/openbmc_project/inventory/gpm"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<nsm::NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/gpm");
    nsm::NsmGPMAggregated gpm{"sensor",
                              "AggregatedGPMMetrics",
                              "/xyz/openbmc_project/inventory/gpm",
                              retrieval_source,
                              gpu_instance,
                              compute_instance,
                              metrics_bitfield,
                              gpmAsioIntf,
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

TEST(nsmGPMAggregated, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    const uint8_t retrieval_source = 2;
    const uint8_t gpu_instance = 0x47;
    const uint8_t compute_instance = 90;
    const std::vector<uint8_t> metrics_bitfield{0x89, 0x04, 0x15};

    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus,
        sdbusplus::object_path{"/xyz/openbmc_project/inventory/gpm_bad"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<nsm::NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/gpm_bad");
    nsm::NsmGPMAggregated gpm{"sensor_bad",
                              "AggregatedGPMMetrics",
                              "/xyz/openbmc_project/inventory/gpm_bad",
                              retrieval_source,
                              gpu_instance,
                              compute_instance,
                              metrics_bitfield,
                              gpmAsioIntf,
                              nvlinkIntf};

    auto request = gpm.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(nsmGPMAggregated, GoodHandleResp)
{
    const uint8_t retrieval_source = 2;
    const uint8_t gpu_instance = 0x47;
    const uint8_t compute_instance = 90;
    const std::vector<uint8_t> metrics_bitfield{0x89, 0x04, 0x15};

    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/openbmc_project/inventory/gpm"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<nsm::NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/gpm");
    nsm::NsmGPMAggregated gpm{"sensor",
                              "AggregatedGPMMetrics",
                              "/xyz/openbmc_project/inventory/gpm",
                              retrieval_source,
                              gpu_instance,
                              compute_instance,
                              metrics_bitfield,
                              gpmAsioIntf,
                              nvlinkIntf};

    auto updator = std::make_unique<MockMetricUpdator>();
    auto percentage_updator = updator.get();
    gpm.metricsTable[3].emplace_back(
        nsm::MetricInfo{nsm::decodePercentage, std::move(updator)});

    updator = std::make_unique<MockMetricUpdator>();
    auto bandwidth_updator = updator.get();
    gpm.metricsTable[8].emplace_back(
        nsm::MetricInfo{nsm::decodeBandwidth, std::move(updator)});

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
                updateMetric(testing::DoubleNear(percentage, 0.01)))
        .Times(1);

    static constexpr uint64_t conversionFactor = 1024 * 1024 * 128;
    EXPECT_CALL(*bandwidth_updator, updateMetric(bandwidth / conversionFactor))
        .Times(1);

    rc = gpm.handleSample({3, static_cast<uint8_t>(percentage_data_len),
                           percentage_data.data(), true});
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = gpm.handleSample({8, static_cast<uint8_t>(bandwidth_data_len),
                           bandwidth_data.data(), true});
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmGPMPerIntance, GoodGenReq)
{
    const uint8_t retrieval_source = 1;
    const uint8_t gpu_instance = 0xFF;
    const uint8_t compute_instance = 38;
    const uint8_t metric_id = 34;
    const std::vector<bitfield8_t> instance_bitmask{{.byte = 38}};

    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/openbmc_project/inventory/gpm"},
        "com.nvidia.GPMMetrics");

    nsm::NsmGPMPerInstance gpm{
        "sensor",
        "AggregatedGPMMetrics",
        retrieval_source,
        gpu_instance,
        compute_instance,
        metric_id,
        instance_bitmask,
        nsm::GPMMetricsUnit::PERCENTAGE,
        nsm::makeGPMPerInstanceUpdator(
            "PropertyName", "/xyz/openbmc_project/inventory/gpm", gpmAsioIntf)};

    const uint8_t eid{12};
    const uint8_t instance_id{30};
    auto request = gpm.genRequestMsg(eid, instance_id);

    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_query_per_instance_gpm_metrics_v2_req*>(
            msg->payload);

    EXPECT_EQ(NSM_QUERY_PER_INSTANCE_GPM_METRICS_V2, command->hdr.command);
    EXPECT_EQ(retrieval_source, command->retrieval_source);
    EXPECT_EQ(gpu_instance, command->gpu_instance);
    EXPECT_EQ(compute_instance, command->compute_instance);
    EXPECT_EQ(metric_id, command->metric_id);
    EXPECT_EQ(instance_bitmask[0].byte, command->instance_bitmask[0].byte);
}

TEST(nsmGPMPerIntance, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    const uint8_t retrieval_source = 1;
    const uint8_t gpu_instance = 0xFF;
    const uint8_t compute_instance = 38;
    const uint8_t metric_id = 34;
    const std::vector<bitfield8_t> instance_bitmask{{.byte = 38}};

    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus,
        sdbusplus::object_path{"/xyz/openbmc_project/inventory/gpm_bad2"},
        "com.nvidia.GPMMetrics");

    nsm::NsmGPMPerInstance gpm{"sensor_bad",
                               "AggregatedGPMMetrics",
                               retrieval_source,
                               gpu_instance,
                               compute_instance,
                               metric_id,
                               instance_bitmask,
                               nsm::GPMMetricsUnit::PERCENTAGE,
                               nsm::makeGPMPerInstanceUpdator(
                                   "PropertyName",
                                   "/xyz/openbmc_project/inventory/gpm_bad2",
                                   gpmAsioIntf)};

    auto request = gpm.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(nsmGPMPerIntance, GoodHandleResp)
{
    const uint8_t retrieval_source = 1;
    const uint8_t gpu_instance = 0xFF;
    const uint8_t compute_instance = 38;
    const uint8_t metric_id = 34;
    const std::vector<bitfield8_t> instance_bitmask{{.byte = 38}};

    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/xyz/openbmc_project/inventory/gpm"},
        "com.nvidia.GPMMetrics");

    nsm::NsmGPMPerInstance gpm{
        "sensor",
        "AggregatedGPMMetrics",
        retrieval_source,
        gpu_instance,
        compute_instance,
        metric_id,
        instance_bitmask,
        nsm::GPMMetricsUnit::BANDWIDTH,
        nsm::makeGPMPerInstanceUpdator(
            "PropertyName", "/xyz/openbmc_project/inventory/gpm", gpmAsioIntf)};

    // Replace the metricUpdator with mock
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
    const double nanV = std::numeric_limits<double>::quiet_NaN();
    const std::vector<double> metrics{nanV, nanV,
                                      bandwidth[0] / conversionFactor, nanV,
                                      bandwidth[1] / conversionFactor};
    EXPECT_CALL(*updator, updateMetric(VectorWithNanEq(metrics))).Times(1);

    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        2 * (sizeof(nsm_aggregate_resp_sample) - 1 + data_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    responseMsg->hdr.instance_id = 0;
    responseMsg->hdr.datagram = 0;

    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 2;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    auto sample1 = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample1->tag = 2;
    sample1->valid = 1;
    sample1->length = std::log2(data_len);
    memcpy(sample1->data, data[0].data(), data_len);

    sample_ptr += sizeof(nsm_aggregate_resp_sample) - 1 + data_len;

    auto sample2 = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample2->tag = 4;
    sample2->valid = 1;
    sample2->length = std::log2(data_len);
    memcpy(sample2->data, data[1].data(), data_len);

    rc = gpm.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

struct NsmGPMMetricsTestFixture :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_GPMMetrics";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/gpm_test";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:10";

    boost::asio::io_context io;
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmGPMMetricsTestFixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
    }

    ~NsmGPMMetricsTestFixture()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmGPMMetricsTestFixture, goodTestCreateGPMMetrics)
{
    using namespace nsm;

    dbus::PropertyMap properties = {
        {"Name", std::string("GPM_Metrics")},
        {"UUID", gpuUuid},
        {"RetrievalSource", uint64_t(2)},
        {"GpuInstance", uint64_t(0)},
        {"ComputeInstance", uint64_t(0)},
        {"MetricsBitfield", std::vector<uint64_t>{0x89, 0x04, 0x15}},
        {"InventoryObjPath", std::string("/xyz/openbmc_project/inventory/gpm")},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = properties;

    EXPECT_CALL(mockManager, getObjServer())
        .WillOnce(testing::ReturnRef(*objServer));

    createNsmGPMMetrics(mockManager, basicIntfName, objPath);

    EXPECT_GE(gpu->deviceSensors.size(), 1);
}

TEST_F(NsmGPMMetricsTestFixture, badTestMissingUUID)
{
    using namespace nsm;

    dbus::PropertyMap properties = {
        {"Name", std::string("GPM_Metrics")},
        {"RetrievalSource", uint64_t(2)},
        {"GpuInstance", uint64_t(0)},
        {"ComputeInstance", uint64_t(0)},
        {"MetricsBitfield", std::vector<uint64_t>{0x89}},
        {"InventoryObjPath", std::string("/xyz/openbmc_project/inventory/gpm")},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = properties;

    EXPECT_THROW_COROUTINE(
        createNsmGPMMetrics(mockManager, basicIntfName, objPath),
        std::bad_optional_access);
}

TEST_F(NsmGPMMetricsTestFixture, testGPMMetricsWithMemoryBandwidth)
{
    using namespace nsm;

    dbus::PropertyMap properties = {
        {"Name", std::string("GPM_Metrics_Memory")},
        {"UUID", gpuUuid},
        {"RetrievalSource", uint64_t(2)},
        {"GpuInstance", uint64_t(0)},
        {"ComputeInstance", uint64_t(0)},
        {"MetricsBitfield", std::vector<uint64_t>{0x10}}, // DRAMUsage bit
        {"InventoryObjPath", std::string("/xyz/openbmc_project/inventory/gpm")},
        {"MemoryBandwidth", true},
        {"MemoryInventoryObjPath",
         std::string("/xyz/openbmc_project/inventory/memory")},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath + "_memory",
                                                          basicIntfName);
    propertyMap = properties;

    EXPECT_CALL(mockManager, getObjServer())
        .WillOnce(testing::ReturnRef(*objServer));

    createNsmGPMMetrics(mockManager, basicIntfName, objPath + "_memory");

    EXPECT_GE(gpu->deviceSensors.size(), 1);
}

TEST_F(NsmGPMMetricsTestFixture, testGPMAggregatedUpdate)
{
    using namespace nsm;

    const uint8_t retrieval_source = 2;
    const uint8_t gpu_instance = 0;
    const uint8_t compute_instance = 0;
    const std::vector<uint8_t> metrics_bitfield{0x89, 0x04, 0x15};

    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus,
        sdbusplus::object_path{"/xyz/openbmc_project/inventory/gpm_update"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/gpm_update");

    auto gpmSensor = std::make_shared<NsmGPMAggregated>(
        "sensor_update", "AggregatedGPMMetrics",
        "/xyz/openbmc_project/inventory/gpm_update", retrieval_source,
        gpu_instance, compute_instance, metrics_bitfield, gpmAsioIntf,
        nvlinkIntf);

    // Create mock response with sample data
    const size_t data_len = 8;
    std::array<std::array<uint8_t, data_len>, 3> data = {{
        {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80}, // Sample 0
        {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}, // Sample 2
        {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22}, // Sample 4
    }};

    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        3 * (sizeof(nsm_aggregate_resp_sample) - 1 + data_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    responseMsg->hdr.instance_id = 0;
    responseMsg->hdr.datagram = 0;

    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 3;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    // Sample 0
    auto sample0 = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample0->tag = 0;
    sample0->valid = 1;
    sample0->length = std::log2(data_len);
    memcpy(sample0->data, data[0].data(), data_len);
    sample_ptr += sizeof(nsm_aggregate_resp_sample) - 1 + data_len;

    // Sample 2
    auto sample2 = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample2->tag = 2;
    sample2->valid = 1;
    sample2->length = std::log2(data_len);
    memcpy(sample2->data, data[1].data(), data_len);
    sample_ptr += sizeof(nsm_aggregate_resp_sample) - 1 + data_len;

    // Sample 4
    auto sample4 = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample4->tag = 4;
    sample4->valid = 1;
    sample4->length = std::log2(data_len);
    memcpy(sample4->data, data[2].data(), data_len);

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(responseBuffer, Response{}));

    gpmSensor->update(gpu);
}

TEST_F(NsmGPMMetricsTestFixture, testGPMAggregatedUpdateError)
{
    using namespace nsm;

    const uint8_t retrieval_source = 2;
    const uint8_t gpu_instance = 0;
    const uint8_t compute_instance = 0;
    const std::vector<uint8_t> metrics_bitfield{0x01};

    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus,
        sdbusplus::object_path{"/xyz/openbmc_project/inventory/gpm_error"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<NVLinkMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/gpm_error");

    auto gpmSensor = std::make_shared<NsmGPMAggregated>(
        "sensor_error", "AggregatedGPMMetrics",
        "/xyz/openbmc_project/inventory/gpm_error", retrieval_source,
        gpu_instance, compute_instance, metrics_bitfield, gpmAsioIntf,
        nvlinkIntf);

    // Mock error response
    Response errorResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(errorResp.data());

    auto payload = reinterpret_cast<nsm_aggregate_resp*>(msg->payload);
    payload->completion_code = NSM_ERROR;

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(errorResp, Response{}));

    auto rc = gpmSensor->update(gpu);
}

TEST_F(NsmGPMMetricsTestFixture, testGPMPerInstanceRequest)
{
    using namespace nsm;

    auto bus = sdbusplus::bus::new_default();
    auto gpmIntf = std::make_shared<GPMMetricsIntf>(
        bus, "/xyz/openbmc_project/inventory/gpm_per_inst");

    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();

    const std::vector<bitfield8_t> instanceBitfield{
        {.byte = 0xFF}, {.byte = 0xFF}, {.byte = 0xFF}, {.byte = 0xFF}};

    NsmGPMPerInstance perInstanceSensor("GPM_PerInst", "GPMPerInstance", 2, 0,
                                        0, 10, instanceBitfield,
                                        GPMMetricsUnit::PERCENTAGE, updator);

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto request = perInstanceSensor.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_per_instance_gpm_metrics_req));
}

TEST_F(NsmGPMMetricsTestFixture, badTestCreateGPMMetricsMissingFields)
{
    using namespace nsm;

    dbus::PropertyMap properties = {
        {"Name", std::string("GPM_Incomplete")},
        {"UUID", gpuUuid},
        // Missing required fields
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath + "_incomplete", basicIntfName);
    propertyMap = properties;

    EXPECT_THROW_COROUTINE(createNsmGPMMetrics(mockManager, basicIntfName,
                                               objPath + "_incomplete"),
                           std::bad_optional_access);
}

// ========== New Branch Coverage Tests ==========

// Internal updator classes (GPMMetricUpdator, NVLinkMetricUpdator, etc.)
// are not directly testable - coverage comes from integration tests below

// DRAMUsageMetricUpdator, GPMMetricInstanceUpdator,
// PortMetricPerInstanceUpdator are internal classes - coverage comes from
// integration tests

// NsmGPMInterfaceCreator addGpmIntfProperty - null gpmIntf (bitfield version)
TEST(NsmGPMInterfaceCreator, testAddGpmIntfPropertyNullInterface)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    nsm::NsmGPMInterfaceCreator creator(objServer, "/test/path");
    creator.gpmIntf.reset();

    std::vector<uint8_t> metricsBitfield{0x01};
    creator.addGpmIntfProperty(metricsBitfield);

    EXPECT_FALSE(creator.gpmIntf);
}

// NsmGPMInterfaceCreator addGpmIntfProperty - empty bitfield
TEST(NsmGPMInterfaceCreator, testAddGpmIntfPropertyEmptyBitfield)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    nsm::NsmGPMInterfaceCreator creator(objServer, "/test/path");

    std::vector<uint8_t> metricsBitfield{};
    creator.addGpmIntfProperty(metricsBitfield);

    EXPECT_TRUE(creator.gpmIntf != nullptr);
}

// NsmGPMInterfaceCreator addGpmIntfProperty - unsupported metric ID
TEST(NsmGPMInterfaceCreator, testAddGpmIntfPropertyUnsupportedMetric)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    nsm::NsmGPMInterfaceCreator creator(objServer, "/test/path");

    // Bit 4 is not in GPMIntfMetricsTable
    std::vector<uint8_t> metricsBitfield{0x10};
    creator.addGpmIntfProperty(metricsBitfield);

    EXPECT_TRUE(creator.gpmIntf != nullptr);
}

// NsmGPMInterfaceCreator addGpmIntfProperty - VectorDouble type
TEST(NsmGPMInterfaceCreator, testAddGpmIntfPropertyVectorDouble)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    nsm::NsmGPMInterfaceCreator creator(objServer, "/test/path");

    creator.addGpmIntfProperty("VectorProperty", nsm::DataType::VectorDouble);

    EXPECT_TRUE(creator.gpmIntf != nullptr);
}

// NsmGPMInterfaceCreator addGpmIntfProperty - null gpmIntf (name version)
TEST(NsmGPMInterfaceCreator, testAddGpmIntfPropertyByNameNullInterface)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    nsm::NsmGPMInterfaceCreator creator(objServer, "/test/path");
    creator.gpmIntf.reset();

    creator.addGpmIntfProperty("TestProperty", nsm::DataType::Double);

    EXPECT_FALSE(creator.gpmIntf);
}

// NsmGPMAggregated constructor - multiple metrics in bitfield
TEST(NsmGPMAggregated, testConstructorMultipleMetrics)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<nsm::NVLinkMetricsIntf>(bus,
                                                               "/test/gpm");

    // Multiple bits set for different metric IDs
    std::vector<uint8_t> metricsBitfield{0xFF, 0xFF};

    nsm::NsmGPMAggregated gpm("test", "AggregatedGPMMetrics", "/test/gpm", 1, 0,
                              0, metricsBitfield, gpmIntf, nvlinkIntf);

    EXPECT_FALSE(gpm.metricsTable.empty());
}

// NsmGPMAggregated constructor - single metric
TEST(NsmGPMAggregated, testConstructorSingleMetric)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<nsm::NVLinkMetricsIntf>(bus,
                                                               "/test/gpm");

    std::vector<uint8_t> metricsBitfield{0x01};

    nsm::NsmGPMAggregated gpm("test", "AggregatedGPMMetrics", "/test/gpm", 1, 0,
                              0, metricsBitfield, gpmIntf, nvlinkIntf);

    EXPECT_FALSE(gpm.metricsTable.empty());
}

// NsmGPMAggregated handleSample - tag > metricsTable.size()
TEST(NsmGPMAggregated, testHandleSampleInvalidTag)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<nsm::NVLinkMetricsIntf>(bus,
                                                               "/test/gpm");

    std::vector<uint8_t> metricsBitfield{0x01};

    nsm::NsmGPMAggregated gpm("test", "AggregatedGPMMetrics", "/test/gpm", 1, 0,
                              0, metricsBitfield, gpmIntf, nvlinkIntf);

    uint8_t data[8] = {0};
    nsm::NsmSensorAggregator::TelemetrySample sample{255, 8, data, true};

    auto rc = gpm.handleSample(sample);

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// NsmGPMAggregated handleSample - null decodeFunc
TEST(NsmGPMAggregated, testHandleSampleNullDecodeFunc)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<nsm::NVLinkMetricsIntf>(bus,
                                                               "/test/gpm");

    std::vector<uint8_t> metricsBitfield{0x01};

    nsm::NsmGPMAggregated gpm("test", "AggregatedGPMMetrics", "/test/gpm", 1, 0,
                              0, metricsBitfield, gpmIntf, nvlinkIntf);

    gpm.metricsTable[5].emplace_back(nsm::MetricInfo{nullptr, nullptr});

    uint8_t data[8] = {0};
    nsm::NsmSensorAggregator::TelemetrySample sample{5, 8, data, true};

    auto rc = gpm.handleSample(sample);

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// NsmGPMAggregated handleSample - null updater
TEST(NsmGPMAggregated, testHandleSampleNullUpdater)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<nsm::NVLinkMetricsIntf>(bus,
                                                               "/test/gpm");

    std::vector<uint8_t> metricsBitfield{0x01};

    nsm::NsmGPMAggregated gpm("test", "AggregatedGPMMetrics", "/test/gpm", 1, 0,
                              0, metricsBitfield, gpmIntf, nvlinkIntf);

    gpm.metricsTable[5].emplace_back(
        nsm::MetricInfo{nsm::decodePercentage, nullptr});

    uint8_t data[8] = {0};
    nsm::NsmSensorAggregator::TelemetrySample sample{5, 8, data, true};

    auto rc = gpm.handleSample(sample);

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// NsmGPMPerInstance constructor tests are covered by existing
// nsmGPMPerIntance GoodGenReq and GoodHandleResp tests

// NsmGPMPerInstance handleResponseMsg - completion code error
TEST(NsmGPMPerInstance, testHandleResponseMsgCompletionCodeError)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm"},
        "com.nvidia.GPMMetrics");

    std::vector<bitfield8_t> instanceBitfield{{.byte = 0x01}};

    nsm::NsmGPMPerInstance gpm(
        "test", "PerInstanceMetrics", 1, 0, 0, 10, instanceBitfield,
        nsm::GPMMetricsUnit::PERCENTAGE,
        nsm::makeGPMPerInstanceUpdator("Property", "/test/gpm", gpmIntf));

    std::vector<uint8_t> responseBuffer(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_aggregate_resp));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_ERROR;
    payload->telemetry_count = 0;

    auto rc = gpm.handleResponseMsg(responseMsg, responseBuffer.size());

    EXPECT_EQ(rc, NSM_ERROR);
}

// NsmGPMPerInstance handleResponseMsg - decode failure (too-short buffer)
TEST(NsmGPMPerInstance, HandleResponseMsg_DecodeFail_ReturnsError)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm/decode_fail"},
        "com.nvidia.GPMMetrics");

    std::vector<bitfield8_t> instanceBitfield{{.byte = 0x01}};

    nsm::NsmGPMPerInstance gpm(
        "test", "PerInstanceMetrics", 1, 0, 0, 10, instanceBitfield,
        nsm::GPMMetricsUnit::PERCENTAGE,
        nsm::makeGPMPerInstanceUpdator("Property", "/test/gpm/decode_fail",
                                       gpmIntf));

    // 7-byte buffer: decode_aggregate_resp requires sizeof(nsm_msg_hdr)
    // + sizeof(nsm_aggregate_resp) = 9 bytes. With 7 bytes it returns
    // NSM_SW_ERROR_LENGTH before accessing any payload fields.
    std::vector<uint8_t> responseBuffer(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());

    auto rc = gpm.handleResponseMsg(responseMsg, responseBuffer.size());

    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmGPMPerInstance handleResponseMsg - telemetryCount == 0
TEST(NsmGPMPerInstance, testHandleResponseMsgZeroTelemetryCount)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm"},
        "com.nvidia.GPMMetrics");

    std::vector<bitfield8_t> instanceBitfield{{.byte = 0x01}};

    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();

    nsm::NsmGPMPerInstance gpm("test", "PerInstanceMetrics", 1, 0, 0, 10,
                               instanceBitfield,
                               nsm::GPMMetricsUnit::PERCENTAGE, updator);

    EXPECT_CALL(*updator, updateMetric(std::vector<double>{})).Times(1);

    std::vector<uint8_t> responseBuffer(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_aggregate_resp));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 0;

    auto rc = gpm.handleResponseMsg(responseMsg, responseBuffer.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// NsmGPMPerInstance handleResponseMsg - invalid sample (valid bit false)
TEST(NsmGPMPerInstance, testHandleResponseMsgInvalidSample)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm"},
        "com.nvidia.GPMMetrics");

    std::vector<bitfield8_t> instanceBitfield{{.byte = 0x01}};

    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();

    nsm::NsmGPMPerInstance gpm("test", "PerInstanceMetrics", 1, 0, 0, 10,
                               instanceBitfield,
                               nsm::GPMMetricsUnit::PERCENTAGE, updator);

    const size_t data_len = 8;
    std::array<uint8_t, data_len> data = {0};

    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        sizeof(nsm_aggregate_resp_sample) - 1 + data_len);

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 1;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);
    auto sample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample->tag = 1;
    sample->valid = 0; // Invalid sample
    sample->length = std::log2(data_len);
    memcpy(sample->data, data.data(), data_len);

    EXPECT_CALL(*updator, updateMetric(testing::_)).Times(1);

    auto rc = gpm.handleResponseMsg(responseMsg, responseBuffer.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// NsmGPMPerInstance handleResponseMsg - tag > MAX
TEST(NsmGPMPerInstance, testHandleResponseMsgTagTooLarge)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm"},
        "com.nvidia.GPMMetrics");

    std::vector<bitfield8_t> instanceBitfield{{.byte = 0x01}};

    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();

    nsm::NsmGPMPerInstance gpm("test", "PerInstanceMetrics", 1, 0, 0, 10,
                               instanceBitfield,
                               nsm::GPMMetricsUnit::PERCENTAGE, updator);

    const size_t data_len = 8;
    std::array<uint8_t, data_len> data = {0};

    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        sizeof(nsm_aggregate_resp_sample) - 1 + data_len);

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 1;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);
    auto sample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample->tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample->valid = 1;
    sample->length = std::log2(data_len);
    memcpy(sample->data, data.data(), data_len);

    EXPECT_CALL(*updator, updateMetric(testing::_)).Times(1);

    auto rc = gpm.handleResponseMsg(responseMsg, responseBuffer.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// NsmGPMPerInstance handleResponseMsg - tag >= metrics.size() (resize)
TEST(NsmGPMPerInstance, testHandleResponseMsgResizeMetrics)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm"},
        "com.nvidia.GPMMetrics");

    std::vector<bitfield8_t> instanceBitfield{{.byte = 0xFF}};

    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();

    nsm::NsmGPMPerInstance gpm("test", "PerInstanceMetrics", 1, 0, 0, 10,
                               instanceBitfield, nsm::GPMMetricsUnit::BANDWIDTH,
                               updator);

    const double bandwidth = 500000000;
    const size_t data_len = 8;
    std::array<uint8_t, data_len> data = {};
    size_t encoded_len = 0;

    auto rc_encode = encode_aggregate_gpm_metric_bandwidth_data(
        bandwidth, data.data(), &encoded_len);
    EXPECT_EQ(rc_encode, NSM_SW_SUCCESS);

    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        sizeof(nsm_aggregate_resp_sample) - 1 + data_len);

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 1;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);
    auto sample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample->tag = 10; // Large tag to trigger resize
    sample->valid = 1;
    sample->length = std::log2(data_len);
    memcpy(sample->data, data.data(), data_len);

    EXPECT_CALL(*updator, updateMetric(testing::_)).Times(1);

    auto rc = gpm.handleResponseMsg(responseMsg, responseBuffer.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// makeGPMPerInstanceUpdator test
TEST(MakeUpdators, testMakeGPMPerInstanceUpdator)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm"},
        "com.nvidia.GPMMetrics");

    auto updator = nsm::makeGPMPerInstanceUpdator("TestProperty", "/test/gpm",
                                                  gpmIntf);

    EXPECT_NE(updator, nullptr);
}

// makeNVLinkRawRxPerInstanceUpdator test
TEST(MakeUpdators, testMakeNVLinkRawRxPerInstanceUpdator)
{
    auto bus = sdbusplus::bus::new_default();
    std::vector<nsm::NVLinkMetricsUpdatorInfo> updatorInfos;

    for (int i = 0; i < 2; ++i)
    {
        auto intf = std::make_shared<nsm::NVLinkMetricsIntf>(
            bus, ("/test/nvlink" + std::to_string(i)).c_str());
        updatorInfos.push_back({"/test/nvlink" + std::to_string(i), intf});
    }

    auto updator = nsm::makeNVLinkRawRxPerInstanceUpdator(updatorInfos);

    EXPECT_NE(updator, nullptr);
}

// makeNVLinkRawTxPerInstanceUpdator test
TEST(MakeUpdators, testMakeNVLinkRawTxPerInstanceUpdator)
{
    auto bus = sdbusplus::bus::new_default();
    std::vector<nsm::NVLinkMetricsUpdatorInfo> updatorInfos;

    for (int i = 0; i < 2; ++i)
    {
        auto intf = std::make_shared<nsm::NVLinkMetricsIntf>(
            bus, ("/test/nvlink" + std::to_string(i)).c_str());
        updatorInfos.push_back({"/test/nvlink" + std::to_string(i), intf});
    }

    auto updator = nsm::makeNVLinkRawTxPerInstanceUpdator(updatorInfos);

    EXPECT_NE(updator, nullptr);
}

// makeNVLinkDataRxPerInstanceUpdator test
TEST(MakeUpdators, testMakeNVLinkDataRxPerInstanceUpdator)
{
    auto bus = sdbusplus::bus::new_default();
    std::vector<nsm::NVLinkMetricsUpdatorInfo> updatorInfos;

    for (int i = 0; i < 2; ++i)
    {
        auto intf = std::make_shared<nsm::NVLinkMetricsIntf>(
            bus, ("/test/nvlink" + std::to_string(i)).c_str());
        updatorInfos.push_back({"/test/nvlink" + std::to_string(i), intf});
    }

    auto updator = nsm::makeNVLinkDataRxPerInstanceUpdator(updatorInfos);

    EXPECT_NE(updator, nullptr);
}

// makeNVLinkDataTxPerInstanceUpdator test
TEST(MakeUpdators, testMakeNVLinkDataTxPerInstanceUpdator)
{
    auto bus = sdbusplus::bus::new_default();
    std::vector<nsm::NVLinkMetricsUpdatorInfo> updatorInfos;

    for (int i = 0; i < 2; ++i)
    {
        auto intf = std::make_shared<nsm::NVLinkMetricsIntf>(
            bus, ("/test/nvlink" + std::to_string(i)).c_str());
        updatorInfos.push_back({"/test/nvlink" + std::to_string(i), intf});
    }

    auto updator = nsm::makeNVLinkDataTxPerInstanceUpdator(updatorInfos);

    EXPECT_NE(updator, nullptr);
}

// decodePercentage test
TEST(DecodeFunctions, testDecodePercentage)
{
    const double percentage = 87.654;
    std::array<uint8_t, sizeof(double)> data = {};
    size_t data_len = 0;

    auto rc_encode = encode_aggregate_gpm_metric_percentage_data(
        percentage, data.data(), &data_len);
    EXPECT_EQ(rc_encode, NSM_SW_SUCCESS);

    auto [rc, decoded_val] = nsm::decodePercentage(data.data(), data_len);

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_NEAR(decoded_val, percentage, 0.01);
}

// decodeBandwidth test
TEST(DecodeFunctions, testDecodeBandwidth)
{
    const uint64_t bandwidth = 12345678900;
    std::array<uint8_t, sizeof(uint64_t)> data = {};
    size_t data_len = 0;

    auto rc_encode = encode_aggregate_gpm_metric_bandwidth_data(
        bandwidth, data.data(), &data_len);
    EXPECT_EQ(rc_encode, NSM_SW_SUCCESS);

    auto [rc, decoded_val] = nsm::decodeBandwidth(data.data(), data_len);

    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    static constexpr uint64_t conversionFactor = 1024 * 1024 * 128;
    double expected = bandwidth / static_cast<double>(conversionFactor);
    EXPECT_NEAR(decoded_val, expected, 0.001);
}

// ========== Coverage Tests for Private Updator updateMetric() ==========

// NVLinkMetricUpdator::updateMetric() is triggered via
// NsmGPMAggregated::handleSample() with NVLink tags (10-13).
// The constructor always populates metricsTable[10..13] regardless of
// metricsBitfield.
TEST(NsmGPMAggregated, testHandleSampleNVLinkTag10TriggersNVLinkUpdator)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm_nvlink10"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf =
        std::make_shared<nsm::NVLinkMetricsIntf>(bus, "/test/gpm_nvlink10");

    std::vector<uint8_t> metricsBitfield{0x01};
    nsm::NsmGPMAggregated gpm("test_nvlink10", "AggregatedGPMMetrics",
                              "/test/gpm_nvlink10", 1, 0, 0, metricsBitfield,
                              gpmIntf, nvlinkIntf);

    // Encode bandwidth for tag 10 (NVLinkRawTxBandwidthGbps)
    const uint64_t bandwidth = 500000000ULL;
    std::array<uint8_t, sizeof(uint64_t)> data = {};
    size_t data_len = 0;
    auto rc_encode = encode_aggregate_gpm_metric_bandwidth_data(
        bandwidth, data.data(), &data_len);
    EXPECT_EQ(rc_encode, NSM_SW_SUCCESS);

    // First call: previousValue(NaN) != val → calls nvLinkRawTxBandwidthGbps
    nsm::NsmSensorAggregator::TelemetrySample sample{
        10, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Second call with same data: previousValue == val → no-op branch
    rc = gpm.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// GPMMetricInstanceUpdator::updateMetric() - covered via factory + direct call
// with null gpmIntf (covers the if-false branch without needing set_property)
TEST(MakeUpdators, testGPMPerInstanceUpdatorUpdateMetricNullIntf)
{
    // Null gpmIntf: condition (previousMetrics != metrics && gpmIntf) → false
    auto updator = nsm::makeGPMPerInstanceUpdator("TestProp", "/test/gpm_upd",
                                                  nullptr);
    EXPECT_NE(updator, nullptr);

    // First call: previousMetrics({}) != metrics({1,2,3}) && nullptr → false
    updator->updateMetric({1.0, 2.0, 3.0});

    // Second call with same values: same evaluation path
    updator->updateMetric({1.0, 2.0, 3.0});

    // Call with empty vector: previousMetrics({}) == metrics({}) → false
    auto updator2 = nsm::makeGPMPerInstanceUpdator("TestProp2",
                                                   "/test/gpm_upd2", nullptr);
    updator2->updateMetric({});
    updator2->updateMetric({});
}

// PortMetricPerInstanceUpdator::updateMetric() - triggered by calling
// makeNVLinkRawTxPerInstanceUpdator then updateMetric with real NVLink intfs
TEST(MakeUpdators, testPortMetricPerInstanceUpdatorUpdateMetric)
{
    auto bus = sdbusplus::bus::new_default();
    std::vector<nsm::NVLinkMetricsUpdatorInfo> updatorInfos;
    for (int i = 0; i < 3; ++i)
    {
        auto intf = std::make_shared<nsm::NVLinkMetricsIntf>(
            bus, ("/test/nvlink_ppu_" + std::to_string(i)).c_str());
        updatorInfos.push_back({"/test/nvlink_ppu_" + std::to_string(i), intf});
    }

    auto updator = nsm::makeNVLinkRawTxPerInstanceUpdator(updatorInfos);
    EXPECT_NE(updator, nullptr);

    // First call: previousMetrics empty → resize → update each interface
    updator->updateMetric({1.0, 2.0, 3.0});

    // Second call same values: previousMetrics[i] == metrics[i] → no update
    updator->updateMetric({1.0, 2.0, 3.0});

    // Third call with different values → update each interface again
    updator->updateMetric({4.0, 5.0, 6.0});

    // Call with fewer values than updatorInfos: min(1, 3) = 1 update
    updator->updateMetric({7.0});

    // Call with empty vector: length = 0, loop not entered
    updator->updateMetric({});
}

// NsmGPMAggregated::handleSample - decode failure path (line 552)
// Non-null decodeFunc + non-null updater, but decodeFunc returns non-zero rc
// because the data is too short (2 bytes < sizeof(double)=8 bytes needed)
TEST(NsmGPMAggregated, testHandleSampleDecodeFail)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm_df"},
        "com.nvidia.GPMMetrics");
    auto bus = sdbusplus::bus::new_default();
    auto nvlinkIntf = std::make_shared<nsm::NVLinkMetricsIntf>(bus,
                                                               "/test/gpm_df");

    std::vector<uint8_t> metricsBitfield{0x01};
    nsm::NsmGPMAggregated gpm("test_df", "AggregatedGPMMetrics", "/test/gpm_df",
                              1, 0, 0, metricsBitfield, gpmIntf, nvlinkIntf);

    // Use decodePercentage which needs sizeof(double)=8 bytes
    // Providing only 2 bytes causes decode to return non-zero rc → line 552
    // updater->updateMetric is still called (line 558) even on decode failure
    auto mockUpd = std::make_unique<MockMetricUpdator>();
    EXPECT_CALL(*mockUpd, updateMetric(testing::_)).Times(1);

    gpm.metricsTable[5].emplace_back(
        nsm::MetricInfo{nsm::decodePercentage, std::move(mockUpd)});

    uint8_t shortData[2] = {0, 0};
    nsm::NsmSensorAggregator::TelemetrySample sample{5, 2, shortData, true};

    auto rc = gpm.handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// GPMMetricInstanceUpdator::updateMetric() with non-null gpmIntf
// Covers lines 257-258: gpmIntf->set_property + previousMetrics = metrics
TEST(MakeUpdators, testGPMPerInstanceUpdatorUpdateMetricValidIntf)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto gpmIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus, sdbusplus::object_path{"/test/gpm_vi"},
        "com.nvidia.GPMMetrics");

    auto updator = nsm::makeGPMPerInstanceUpdator("TestPropVI", "/test/gpm_vi",
                                                  gpmIntf);
    EXPECT_NE(updator, nullptr);

    // First call: previousMetrics({}) != {1,2,3} AND gpmIntf != null → lines
    // 257-258
    updator->updateMetric({1.0, 2.0, 3.0});

    // Second call with same values: condition false → no update
    updator->updateMetric({1.0, 2.0, 3.0});

    // Third call with different values → lines 257-258 again
    updator->updateMetric({4.0, 5.0, 6.0});
}
