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

#include "base.h"
#include "network-ports.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmPort.hpp"

using namespace nsm;

struct EthPortTelemetryAggregatorTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:50";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> switchDevice;

    EthPortTelemetryAggregatorTest() : SensorManagerTest(devices)
    {
        switchDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_NE(switchDevice, nullptr);
    }

    ~EthPortTelemetryAggregatorTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(EthPortTelemetryAggregatorTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "EthPort_Test";
    uint16_t portNumber = 1;
    std::string type = "NSM_EthPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ethport_test";

    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    EthPortTelemetryAggregator ethPort(bus, portName, portNumber, type,
                                       inventoryObjPath, portMetricsOem2Intf,
                                       portPacketCountersIntf);

    EXPECT_EQ(ethPort.getName(), portName);
    EXPECT_EQ(ethPort.portNumber, portNumber);
    EXPECT_NE(ethPort.ethPortIntf, nullptr);
    EXPECT_NE(ethPort.portMetricsOem2Intf, nullptr);
    EXPECT_NE(ethPort.portPacketCountersIntf, nullptr);
    EXPECT_EQ(ethPort.tagToPropertyMap.size(), 21);
}

TEST_F(EthPortTelemetryAggregatorTest, testGenRequest)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "EthPort_Req";
    uint16_t portNumber = 2;
    std::string type = "NSM_EthPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ethport_req";

    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    EthPortTelemetryAggregator ethPort(bus, portName, portNumber, type,
                                       inventoryObjPath, portMetricsOem2Intf,
                                       portPacketCountersIntf);

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto request = ethPort.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_ethernet_port_telemetry_counter_req));
}

TEST_F(EthPortTelemetryAggregatorTest, testHandleSampleRXBytes)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "EthPort_RX";
    uint16_t portNumber = 3;
    std::string type = "NSM_EthPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ethport_rx";

    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    EthPortTelemetryAggregator ethPort(bus, portName, portNumber, type,
                                       inventoryObjPath, portMetricsOem2Intf,
                                       portPacketCountersIntf);

    // Test RXBytes (tag 0)
    NsmSensorAggregator::TelemetrySample sample;
    sample.tag = 0; // RXBytes
    sample.valid = true;
    sample.data_len = 8;
    uint64_t rxBytes = 123456789;
    sample.data = reinterpret_cast<const uint8_t*>(&rxBytes);

    auto rc = ethPort.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(EthPortTelemetryAggregatorTest, testHandleSampleAllCounters)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "EthPort_All";
    uint16_t portNumber = 4;
    std::string type = "NSM_EthPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ethport_all";

    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    EthPortTelemetryAggregator ethPort(bus, portName, portNumber, type,
                                       inventoryObjPath, portMetricsOem2Intf,
                                       portPacketCountersIntf);

    // Test various tags
    std::vector<uint8_t> tags = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
                                 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

    for (auto tag : tags)
    {
        uint64_t value64 = 1000 * (tag + 1);
        uint32_t value32 = 100 * (tag + 1);

        NsmSensorAggregator::TelemetrySample sample;
        sample.tag = tag;
        sample.valid = true;
        sample.data_len = (tag <= 7) ? 8 : 4; // 64-bit for 0-7, 32-bit for 8-20

        if (sample.data_len == 8)
        {
            sample.data = reinterpret_cast<const uint8_t*>(&value64);
        }
        else
        {
            sample.data = reinterpret_cast<const uint8_t*>(&value32);
        }

        auto rc = ethPort.handleSample(sample);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
}

TEST_F(EthPortTelemetryAggregatorTest, badTestInvalidSample)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "EthPort_Invalid";
    uint16_t portNumber = 5;
    std::string type = "NSM_EthPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ethport_invalid";

    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    EthPortTelemetryAggregator ethPort(bus, portName, portNumber, type,
                                       inventoryObjPath, portMetricsOem2Intf,
                                       portPacketCountersIntf);

    // Test with invalid sample (valid = false)
    NsmSensorAggregator::TelemetrySample sample;
    sample.tag = 0;
    sample.valid = false; // Invalid sample
    sample.data_len = 8;
    uint64_t value = 123;
    sample.data = reinterpret_cast<const uint8_t*>(&value);

    auto rc = ethPort.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS); // Should succeed but not update
}

TEST_F(EthPortTelemetryAggregatorTest, badTestReservedTag)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "EthPort_Reserved";
    uint16_t portNumber = 6;
    std::string type = "NSM_EthPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ethport_reserved";

    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    EthPortTelemetryAggregator ethPort(bus, portName, portNumber, type,
                                       inventoryObjPath, portMetricsOem2Intf,
                                       portPacketCountersIntf);

    // Test with reserved tag (> max unreserved)
    NsmSensorAggregator::TelemetrySample sample;
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample.valid = true;
    sample.data_len = 8;
    uint64_t value = 999;
    sample.data = reinterpret_cast<const uint8_t*>(&value);

    auto rc = ethPort.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS); // Should succeed but not process
}

struct NsmGetPortECCCountersTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t bridgeUuid = "STATIC:2:0:NSM_DEVICE_INSTANCE_NUMBER:60";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> bridge;

    NsmGetPortECCCountersTest() : SensorManagerTest(devices)
    {
        bridge = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(bridgeUuid));
        EXPECT_NE(bridge, nullptr);
    }

    ~NsmGetPortECCCountersTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmGetPortECCCountersTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_ECC_Test";
    std::string type = "NSM_Port";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/port_ecc_test";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters eccCounters(bus, portName, type, inventoryObjPath,
                                      portNumber);

    EXPECT_EQ(eccCounters.getName(), portName);
    EXPECT_EQ(eccCounters.portNumber, portNumber);
    EXPECT_NE(eccCounters.portECCIntf, nullptr);
}

TEST_F(NsmGetPortECCCountersTest, testGenRequest)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_ECC_Req";
    std::string type = "NSM_Port";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/port_ecc_req";
    uint8_t portNumber = 2;

    NsmGetPortECCCounters eccCounters(bus, portName, type, inventoryObjPath,
                                      portNumber);

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto request = eccCounters.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_ecc_counters_req));
}

TEST_F(NsmGetPortECCCountersTest, testHandleResponseAllTags)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_ECC_All";
    std::string type = "NSM_Port";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/port_ecc_all";
    uint8_t portNumber = 3;

    NsmGetPortECCCounters eccCounters(bus, portName, type, inventoryObjPath,
                                      portNumber);

    // Create mock aggregate response with all ECC counter tags
    const size_t data_len = 8;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        6 * (sizeof(nsm_aggregate_resp_sample) - 1 + data_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 6;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    // Symbol errors
    auto sample0 = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample0->tag = NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES;
    sample0->valid = 1;
    sample0->length = std::log2(data_len);
    uint64_t symbolErrors = 100;
    memcpy(sample0->data, &symbolErrors, data_len);
    sample_ptr += sizeof(nsm_aggregate_resp_sample) - 1 + data_len;

    // Corrected bits
    auto sample1 = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample1->tag = NSM_TAG_ECC_CORRECTED_BITS;
    sample1->valid = 1;
    sample1->length = std::log2(data_len);
    uint64_t correctedBits = 50;
    memcpy(sample1->data, &correctedBits, data_len);
    sample_ptr += sizeof(nsm_aggregate_resp_sample) - 1 + data_len;

    // Raw errors per lane
    for (uint8_t lane = 0; lane < 4; lane++)
    {
        auto sample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
        sample->tag = NSM_TAG_ECC_RAW_ERRORS_LANE_0 + lane;
        sample->valid = 1;
        sample->length = std::log2(data_len);
        uint64_t rawErrors = (lane + 1) * 10;
        memcpy(sample->data, &rawErrors, data_len);
        sample_ptr += sizeof(nsm_aggregate_resp_sample) - 1 + data_len;
    }

    auto rc = eccCounters.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(eccCounters.portECCIntf->symbolErrorRXBytes(), 100);
    EXPECT_EQ(eccCounters.portECCIntf->correctedBits(), 50);

    auto rawErrorsPerLane = eccCounters.portECCIntf->rawErrorsPerLane();
    EXPECT_EQ(rawErrorsPerLane.size(), RAW_ERRORS_PER_LANE_COUNT);
    EXPECT_EQ(rawErrorsPerLane[0], 10);
    EXPECT_EQ(rawErrorsPerLane[1], 20);
    EXPECT_EQ(rawErrorsPerLane[2], 30);
    EXPECT_EQ(rawErrorsPerLane[3], 40);
}

TEST_F(NsmGetPortECCCountersTest, badTestCompletionCodeError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_ECC_Error";
    std::string type = "NSM_Port";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/port_ecc_err";
    uint8_t portNumber = 4;

    NsmGetPortECCCounters eccCounters(bus, portName, type, inventoryObjPath,
                                      portNumber);

    // Create mock response with completion code error
    std::vector<uint8_t> responseBuffer(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_aggregate_resp));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_ERROR;
    payload->telemetry_count = 0;

    auto rc = eccCounters.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmGetPortECCCountersTest, badTestInvalidSampleDecode)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_ECC_Invalid";
    std::string type = "NSM_Port";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/port_ecc_invalid";
    uint8_t portNumber = 5;

    NsmGetPortECCCounters eccCounters(bus, portName, type, inventoryObjPath,
                                      portNumber);

    // Create mock response with invalid sample
    const size_t data_len = 8;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        sizeof(nsm_aggregate_resp_sample) - 1 + data_len);

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 1;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);
    auto sample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample->tag = 255; // Invalid tag
    sample->valid = 1;
    sample->length = std::log2(data_len);

    auto rc = eccCounters.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmGetPortECCCountersTest, testHandleResponseInvalidTag)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_ECC_BadTag";
    std::string type = "NSM_Port";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/port_ecc_badtag";
    uint8_t portNumber = 6;

    NsmGetPortECCCounters eccCounters(bus, portName, type, inventoryObjPath,
                                      portNumber);

    // Create response with reserved tag
    const size_t data_len = 8;
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
    uint64_t value = 999;
    memcpy(sample->data, &value, data_len);

    auto rc = eccCounters.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS); // Should skip reserved tags
}
