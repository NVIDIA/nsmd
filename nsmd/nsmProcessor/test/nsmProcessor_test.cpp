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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "device-configuration.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmProcessor.hpp"
#include "nsmReconfigPermissions.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();
std::string sensorName("dummy_sensor");
std::string sensorType("dummy_type");
std::string inventoryObjPath("/xyz/openbmc_project/inventory/dummy_device");

TEST(nsmMigMode, GoodGenReq)
{
    nsm::NsmMigMode migSensor(bus, sensorName, sensorType, inventoryObjPath,
                              nullptr);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = migSensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_MIG_MODE);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmMigMode, GoodHandleResp)
{
    nsm::NsmMigMode migSensor(bus, sensorName, sensorType, inventoryObjPath,
                              nullptr);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_MIG_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 1;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_MIG_mode_resp(0, NSM_SUCCESS, reason_code, &flags,
                                          response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = migSensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmMigMode, BadHandleResp)
{
    nsm::NsmMigMode migSensor(bus, sensorName, sensorType, inventoryObjPath,
                              nullptr);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_MIG_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 1;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_MIG_mode_resp(0, NSM_SUCCESS, reason_code, &flags,
                                          response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = migSensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = migSensor.handleResponseMsg(response, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmMigMode, GoodUpdateReading)
{
    nsm::NsmMigMode migSensor(bus, sensorName, sensorType, inventoryObjPath,
                              nullptr);
    bitfield8_t flags;
    flags.byte = 1;
    migSensor.updateReading(flags);
    EXPECT_EQ(migSensor.migModeIntf->migModeEnabled(), flags.bits.bit0);
}

TEST(nsmEccErrorCounts, GoodGenReq)
{
    auto eccIntf = std::make_shared<NsmEccModeIntf>(
        bus, inventoryObjPath.c_str(), nullptr);
    nsm::NsmEccErrorCounts eccErrorCntSensor(sensorName, sensorType, eccIntf,
                                             inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = eccErrorCntSensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_ECC_ERROR_COUNTS);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmEccErrorCounts, GoodHandleResp)
{
    auto eccIntf = std::make_shared<NsmEccModeIntf>(
        bus, inventoryObjPath.c_str(), nullptr);
    nsm::NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf,
                                  inventoryObjPath);

    struct nsm_ECC_error_counts errorCounts;
    errorCounts.flags.byte = 132;
    errorCounts.sram_corrected = 1234;
    errorCounts.sram_uncorrected_secded = 4532;
    errorCounts.sram_uncorrected_parity = 6567;
    errorCounts.dram_corrected = 9876;
    errorCounts.dram_uncorrected = 9654;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_error_counts_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_ECC_error_counts_resp(0, NSM_SUCCESS, reason_code,
                                                  &errorCounts, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmEccErrorCounts, GoodUpdateReading)
{
    auto eccIntf = std::make_shared<NsmEccModeIntf>(
        bus, inventoryObjPath.c_str(), nullptr);
    nsm::NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf,
                                  inventoryObjPath);
    struct nsm_ECC_error_counts errorCounts;
    errorCounts.flags.byte = 132;
    errorCounts.sram_corrected = 1234;
    errorCounts.sram_uncorrected_secded = 4532;
    errorCounts.sram_uncorrected_parity = 6567;
    errorCounts.dram_corrected = 9876;
    errorCounts.dram_uncorrected = 9654;
    sensor.updateReading(errorCounts);
    EXPECT_EQ(sensor.eccErrorCountIntf->ceCount(), errorCounts.sram_corrected);
    EXPECT_EQ(sensor.eccErrorCountIntf->ueCount(),
              errorCounts.sram_uncorrected_secded +
                  errorCounts.sram_uncorrected_parity);
}

TEST(nsmEccErrorCounts, BadHandleResp)
{
    auto eccIntf = std::make_shared<NsmEccModeIntf>(
        bus, inventoryObjPath.c_str(), nullptr);
    nsm::NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf,
                                  inventoryObjPath);

    struct nsm_ECC_error_counts errorCounts;
    errorCounts.flags.byte = 132;
    errorCounts.sram_corrected = 1234;
    errorCounts.sram_uncorrected_secded = 4532;
    errorCounts.sram_uncorrected_parity = 6567;
    errorCounts.dram_corrected = 9876;
    errorCounts.dram_uncorrected = 9654;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_error_counts_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_ECC_error_counts_resp(0, NSM_SUCCESS, reason_code,
                                                  &errorCounts, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = response.size();

    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = sensor.handleResponseMsg(responseMsg, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(NsmPCIeGroup2, GoodGenReq)
{
    auto pCieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pCiePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup2 sensor(sensorName, sensorType, pCieECCIntf, pCiePortIntf,
                             deviceId, inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const struct nsm_query_scalar_group_telemetry_v1_req*>(
            msg->payload);
    EXPECT_EQ(command->hdr.command, NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1);
    EXPECT_EQ(command->hdr.data_size, 2);
    EXPECT_EQ(command->device_id, deviceId);
    EXPECT_EQ(command->group_index, 2);
}

TEST(NsmPCIeGroup2, GoodHandleResp)
{
    auto pCieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pCiePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup2 sensor(sensorName, sensorType, pCieECCIntf, pCiePortIntf,
                             deviceId, inventoryObjPath);

    struct nsm_query_scalar_group_telemetry_group_2 data;
    data.non_fatal_errors = 1111;
    data.fatal_errors = 2222;
    data.unsupported_request_count = 3333;
    data.correctable_errors = 4444;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_query_scalar_group_telemetry_v1_group2_resp(
        0, NSM_SUCCESS, reason_code, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmPCIeGroup2, BadHandleResp)
{
    auto pCieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pCiePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup2 sensor(sensorName, sensorType, pCieECCIntf, pCiePortIntf,
                             deviceId, inventoryObjPath);

    struct nsm_query_scalar_group_telemetry_group_2 data;
    data.non_fatal_errors = 1111;
    data.fatal_errors = 2222;
    data.unsupported_request_count = 3333;
    data.correctable_errors = 4444;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_query_scalar_group_telemetry_v1_group2_resp(
        0, NSM_SUCCESS, reason_code, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
    rc = sensor.handleResponseMsg(responseMsg, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(NsmPCIeGroup3, GoodGenReq)
{
    auto pCieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pCiePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;

    nsm::NsmPciGroup3 sensor(sensorName, sensorType, pCieECCIntf, pCiePortIntf,
                             deviceId, inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const struct nsm_query_scalar_group_telemetry_v1_req*>(
            msg->payload);
    EXPECT_EQ(command->hdr.command, NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1);
    EXPECT_EQ(command->hdr.data_size, 2);
    EXPECT_EQ(command->device_id, deviceId);
    EXPECT_EQ(command->group_index, 3);
}

TEST(NsmPCIeGroup3, GoodHandleResp)
{
    auto pCieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pCiePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup3 sensor(sensorName, sensorType, pCieECCIntf, pCiePortIntf,
                             deviceId, inventoryObjPath);

    struct nsm_query_scalar_group_telemetry_group_3 data;
    data.L0ToRecoveryCount = 8769;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_query_scalar_group_telemetry_v1_group3_resp(
        0, NSM_SUCCESS, reason_code, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmPCIeGroup3, BadHandleResp)
{
    auto pCieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pCiePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup3 sensor(sensorName, sensorType, pCieECCIntf, pCiePortIntf,
                             deviceId, inventoryObjPath);

    struct nsm_query_scalar_group_telemetry_group_3 data;
    data.L0ToRecoveryCount = 8769;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_query_scalar_group_telemetry_v1_group3_resp(
        0, NSM_SUCCESS, reason_code, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
    rc = sensor.handleResponseMsg(responseMsg, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(NsmPCIeGroup4, GoodGenReq)
{
    auto pCieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pCiePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup4 sensor(sensorName, sensorType, pCieECCIntf, pCiePortIntf,
                             deviceId, inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const struct nsm_query_scalar_group_telemetry_v1_req*>(
            msg->payload);
    EXPECT_EQ(command->hdr.command, NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1);
    EXPECT_EQ(command->hdr.data_size, 2);
    EXPECT_EQ(command->device_id, deviceId);
    EXPECT_EQ(command->group_index, 4);
}

TEST(NsmPCIeGroup4, GoodHandleResp)
{
    auto pCieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pCiePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup4 sensor(sensorName, sensorType, pCieECCIntf, pCiePortIntf,
                             deviceId, inventoryObjPath);

    struct nsm_query_scalar_group_telemetry_group_4 data;
    data.recv_err_cnt = 100;
    data.NAK_recv_cnt = 200;
    data.NAK_sent_cnt = 300;
    data.bad_TLP_cnt = 400;
    data.replay_rollover_cnt = 500;
    data.FC_timeout_err_cnt = 600;
    data.replay_cnt = 700;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_query_scalar_group_telemetry_v1_group4_resp(
        0, NSM_SUCCESS, reason_code, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmPCIeGroup4, BadHandleResp)
{
    auto pCieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pCiePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup4 sensor(sensorName, sensorType, pCieECCIntf, pCiePortIntf,
                             deviceId, inventoryObjPath);

    struct nsm_query_scalar_group_telemetry_group_4 data;
    data.recv_err_cnt = 100;
    data.NAK_recv_cnt = 200;
    data.NAK_sent_cnt = 300;
    data.bad_TLP_cnt = 400;
    data.replay_rollover_cnt = 500;
    data.FC_timeout_err_cnt = 600;
    data.replay_cnt = 700;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_query_scalar_group_telemetry_v1_group4_resp(
        0, NSM_SUCCESS, reason_code, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
    rc = sensor.handleResponseMsg(responseMsg, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(NsmPCIeGroup5, GoodGenReq)
{
    auto processorPerformanceIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup5 sensor(sensorName, sensorType, processorPerformanceIntf,
                             deviceId, inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const struct nsm_query_scalar_group_telemetry_v1_req*>(
            msg->payload);
    EXPECT_EQ(command->hdr.command, NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1);
    EXPECT_EQ(command->hdr.data_size, 2);
    EXPECT_EQ(command->device_id, deviceId);
    EXPECT_EQ(command->group_index, 5);
}

TEST(NsmPCIeGroup5, GoodHandleResp)
{
    auto processorPerformanceIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup5 sensor(sensorName, sensorType, processorPerformanceIntf,
                             deviceId, inventoryObjPath);

    struct nsm_query_scalar_group_telemetry_group_5 data;
    data.PCIeRXBytes = 100;
    data.PCIeTXBytes = 200;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_query_scalar_group_telemetry_v1_group5_resp(
        0, NSM_SUCCESS, reason_code, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmPCIeGroup5, BadHandleResp)
{
    auto processorPerformanceIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup5 sensor(sensorName, sensorType, processorPerformanceIntf,
                             deviceId, inventoryObjPath);

    struct nsm_query_scalar_group_telemetry_group_5 data;
    data.PCIeRXBytes = 100;
    data.PCIeTXBytes = 200;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_query_scalar_group_telemetry_v1_group5_resp(
        0, NSM_SUCCESS, reason_code, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
    rc = sensor.handleResponseMsg(responseMsg, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmEDPpScalingFactor, GoodGenReq)
{
    nsm::NsmEDPpScalingFactor sensor(bus, sensorName, sensorType,
                                     inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmEDPpScalingFactor, GoodHandleResp)
{
    nsm::NsmEDPpScalingFactor sensor(bus, sensorName, sensorType,
                                     inventoryObjPath);

    struct nsm_EDPp_scaling_factors scaling_factors;
    scaling_factors.default_scaling_factor = 70;
    scaling_factors.maximum_scaling_factor = 90;
    scaling_factors.minimum_scaling_factor = 60;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_programmable_EDPp_scaling_factor_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_programmable_EDPp_scaling_factor_resp(
        0, NSM_SUCCESS, reason_code, &scaling_factors, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmEDPpScalingFactor, GoodUpdateReading)
{
    nsm::NsmEDPpScalingFactor sensor(bus, sensorName, sensorType,
                                     inventoryObjPath);

    struct nsm_EDPp_scaling_factors scaling_factors;
    scaling_factors.default_scaling_factor = 70;
    scaling_factors.maximum_scaling_factor = 90;
    scaling_factors.minimum_scaling_factor = 60;
    sensor.updateReading(scaling_factors);
    EXPECT_EQ(sensor.eDPpIntf->allowableMax(),
              scaling_factors.maximum_scaling_factor);
    EXPECT_EQ(sensor.eDPpIntf->allowableMin(),
              scaling_factors.minimum_scaling_factor);
}

TEST(nsmEDPpScalingFactor, BadHandleResp)
{
    nsm::NsmEDPpScalingFactor sensor(bus, sensorName, sensorType,
                                     inventoryObjPath);

    struct nsm_EDPp_scaling_factors scaling_factors;
    scaling_factors.default_scaling_factor = 70;
    scaling_factors.maximum_scaling_factor = 90;
    scaling_factors.minimum_scaling_factor = 60;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_programmable_EDPp_scaling_factor_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_programmable_EDPp_scaling_factor_resp(
        0, NSM_SUCCESS, reason_code, &scaling_factors, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();

    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = sensor.handleResponseMsg(responseMsg, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(NsmClockLimitGraphics, GoodGenReq)
{
    auto cpuOperatingConfigIntf = std::make_shared<NsmCpuOperatingConfigIntf>(
        bus, inventoryObjPath.c_str(), nullptr, GRAPHICS_CLOCK);
    nsm::NsmClockLimitGraphics sensor(sensorName, sensorType,
                                      cpuOperatingConfigIntf, inventoryObjPath);
    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_CLOCK_LIMIT);
    EXPECT_EQ(command->data_size, 1);
}

TEST(NsmClockLimitGraphics, GoodHandleResp)
{
    auto cpuOperatingConfigIntf = std::make_shared<NsmCpuOperatingConfigIntf>(
        bus, inventoryObjPath.c_str(), nullptr, GRAPHICS_CLOCK);
    nsm::NsmClockLimitGraphics sensor(sensorName, sensorType,
                                      cpuOperatingConfigIntf, inventoryObjPath);

    struct nsm_clock_limit clockLimit;
    clockLimit.requested_limit_min = 800;
    clockLimit.requested_limit_max = 1800;
    clockLimit.present_limit_min = 200;
    clockLimit.present_limit_max = 2000;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_clock_limit_resp(0, NSM_SUCCESS, reason_code,
                                             &clockLimit, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmClockLimitGraphics, BadHandleResp)
{
    auto cpuOperatingConfigIntf = std::make_shared<NsmCpuOperatingConfigIntf>(
        bus, inventoryObjPath.c_str(), nullptr, GRAPHICS_CLOCK);
    nsm::NsmClockLimitGraphics sensor(sensorName, sensorType,
                                      cpuOperatingConfigIntf, inventoryObjPath);

    struct nsm_clock_limit clockLimit;
    clockLimit.requested_limit_min = 800;
    clockLimit.requested_limit_max = 1800;
    clockLimit.present_limit_min = 200;
    clockLimit.present_limit_max = 2000;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_clock_limit_resp(0, NSM_SUCCESS, reason_code,
                                             &clockLimit, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
    rc = sensor.handleResponseMsg(responseMsg, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(NsmCurrClockFreq, GoodGenReq)
{
    auto cpuOperatingConfigIntf = std::make_shared<NsmCpuOperatingConfigIntf>(
        bus, inventoryObjPath.c_str(), nullptr, GRAPHICS_CLOCK);
    nsm::NsmCurrClockFreq sensor(sensorName, sensorType, cpuOperatingConfigIntf,
                                 inventoryObjPath);
    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_get_curr_clock_freq_req*>(msg->payload);
    EXPECT_EQ(command->hdr.command, NSM_GET_CURRENT_CLOCK_FREQUENCY);
    EXPECT_EQ(command->hdr.data_size, 1);
}

TEST(NsmCurrClockFreq, GoodHandleResp)
{
    auto cpuOperatingConfigIntf = std::make_shared<NsmCpuOperatingConfigIntf>(
        bus, inventoryObjPath.c_str(), nullptr, GRAPHICS_CLOCK);
    nsm::NsmCurrClockFreq sensor(sensorName, sensorType, cpuOperatingConfigIntf,
                                 inventoryObjPath);

    uint32_t clockFreq = 3000;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, reason_code,
                                                 &clockFreq, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmCurrClockFreq, BadHandleResp)
{
    auto cpuOperatingConfigIntf = std::make_shared<NsmCpuOperatingConfigIntf>(
        bus, inventoryObjPath.c_str(), nullptr, GRAPHICS_CLOCK);
    nsm::NsmCurrClockFreq sensor(sensorName, sensorType, cpuOperatingConfigIntf,
                                 inventoryObjPath);

    uint32_t clockFreq = 3000;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, reason_code,
                                                 &clockFreq, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);

    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
    rc = sensor.handleResponseMsg(responseMsg, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmProcessorThrottleReason, GoodGenReq)
{
    auto processorPerformanceIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());
    nsm::NsmProcessorThrottleReason sensor(
        sensorName, sensorType, processorPerformanceIntf, inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_CLOCK_EVENT_REASON_CODES);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmProcessorThrottleReason, GoodHandleResp)
{
    auto processorPerformanceIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());
    nsm::NsmProcessorThrottleReason sensor(
        sensorName, sensorType, processorPerformanceIntf, inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(struct nsm_get_current_clock_event_reason_code_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield32_t flags;
    flags.byte = 1;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_current_clock_event_reason_code_resp(
        0, NSM_SUCCESS, reason_code, &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmProcessorThrottleReason, BadHandleResp)
{
    auto processorPerformanceIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());
    nsm::NsmProcessorThrottleReason sensor(
        sensorName, sensorType, processorPerformanceIntf, inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(struct nsm_get_current_clock_event_reason_code_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield32_t flags;
    flags.byte = 1;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_current_clock_event_reason_code_resp(
        0, NSM_SUCCESS, reason_code, &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = sensor.handleResponseMsg(response, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(NsmAccumGpuUtilTime, GoodGenReq)
{
    auto processorPerformanceIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());
    nsm::NsmAccumGpuUtilTime sensor(sensorName, sensorType,
                                    processorPerformanceIntf, inventoryObjPath);
    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME);
    EXPECT_EQ(command->data_size, 0);
}

TEST(NsmAccumGpuUtilTime, GoodHandleResp)
{
    auto processorPerformanceIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());
    nsm::NsmAccumGpuUtilTime sensor(sensorName, sensorType,
                                    processorPerformanceIntf, inventoryObjPath);

    uint32_t context_util_time = 100;
    uint32_t SM_util_time = 200;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_accum_GPU_util_time_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_accum_GPU_util_time_resp(
        0, NSM_SUCCESS, reason_code, &context_util_time, &SM_util_time,
        responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmAccumGpuUtilTime, BadHandleResp)
{
    auto processorPerformanceIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());
    nsm::NsmAccumGpuUtilTime sensor(sensorName, sensorType,
                                    processorPerformanceIntf, inventoryObjPath);

    uint32_t context_util_time = 100;
    uint32_t SM_util_time = 200;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_accum_GPU_util_time_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_accum_GPU_util_time_resp(
        0, NSM_SUCCESS, reason_code, &context_util_time, &SM_util_time,
        responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);

    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
    rc = sensor.handleResponseMsg(responseMsg, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmProcessorRevision, GoodGenReq)
{
    nsm::NsmProcessorRevision sensor(bus, sensorName, sensorType,
                                     inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_get_inventory_information_req*>(
        msg->payload);

    EXPECT_EQ(command->hdr.command, NSM_GET_INVENTORY_INFORMATION);
    EXPECT_EQ(command->property_identifier, DEVICE_PART_NUMBER);
}

TEST(nsmProcessorRevision, GoodHandleResp)
{
    nsm::NsmProcessorRevision sensor(bus, sensorName, sensorType,
                                     inventoryObjPath);

    std::vector<uint8_t> data{'N', 'V', 'I', 'D', 'I', 'A', '\0'};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + data.size(), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, reason_code, data.size(), (uint8_t*)data.data(),
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmProcessorRevision, BadHandleResp)
{
    nsm::NsmProcessorRevision sensor(bus, sensorName, sensorType,
                                     inventoryObjPath);

    std::vector<uint8_t> data{'N', 'V', 'I', 'D', 'I', 'A', '\0'};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + data.size(), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, reason_code, data.size(), (uint8_t*)data.data(),
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
    rc = sensor.handleResponseMsg(response, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

struct NsmProcessorTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Processor";
    const std::string name = "GPU_SXM_1";
    const std::string objPath = processorsInventoryBasePath / name;

    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    const uuid_t badUuid = "092b3ec1-e468-f145-8686-409009062aa8";

    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>(gpuUuid)},
    };
    NsmDevice& gpu = *devices[0];

    NiceMock<MockSensorManager> mockManager{devices};

    const PropertyValuesCollection error = {
        {"Type", "NSM_processor"},
        {"UUID", badUuid},
        {"Features",
         std::vector<uint64_t>{
             (uint64_t)ReconfigSettingsIntf::FeatureType::InSystemTest,
             (uint64_t)ReconfigSettingsIntf::FeatureType::FusingMode,
             (uint64_t)ReconfigSettingsIntf::FeatureType::CCMode,
             (uint64_t)ReconfigSettingsIntf::FeatureType::Unknown,
         }},
    };
    const PropertyValuesCollection basic = {
        {"Name", name},
        {"Type", "NSM_Processor"},
        {"UUID", gpuUuid},
        {"InventoryObjPath", objPath},
    };
    const PropertyValuesCollection prcKnobs = {
        {"Type", "NSM_InbandReconfigPermissions"},
        {"Priority", false},
        {"Features", // features are not propertly sorted and some are
                     // duplicated
         std::vector<uint64_t>{
             (uint64_t)ReconfigSettingsIntf::FeatureType::InSystemTest,
             (uint64_t)ReconfigSettingsIntf::FeatureType::FusingMode,
             (uint64_t)ReconfigSettingsIntf::FeatureType::CCMode,
             (uint64_t)ReconfigSettingsIntf::FeatureType::
                 PowerSmoothingPrivilegeLevel1,
             (uint64_t)ReconfigSettingsIntf::FeatureType::BAR0Firewall,
             (uint64_t)ReconfigSettingsIntf::FeatureType::CCDevMode,
             (uint64_t)ReconfigSettingsIntf::FeatureType::TGPCurrentLimit,
             (uint64_t)ReconfigSettingsIntf::FeatureType::TGPRatedLimit,
             (uint64_t)ReconfigSettingsIntf::FeatureType::HBMFrequencyChange,
             (uint64_t)ReconfigSettingsIntf::FeatureType::TGPMaxLimit,
             (uint64_t)ReconfigSettingsIntf::FeatureType::TGPMinLimit,
             (uint64_t)ReconfigSettingsIntf::FeatureType::ClockLimit,
             (uint64_t)ReconfigSettingsIntf::FeatureType::NVLinkDisable,
             (uint64_t)ReconfigSettingsIntf::FeatureType::ECCEnable,
             (uint64_t)ReconfigSettingsIntf::FeatureType::PCIeVFConfiguration,
             (uint64_t)ReconfigSettingsIntf::FeatureType::RowRemappingAllowed,
             (uint64_t)ReconfigSettingsIntf::FeatureType::RowRemappingFeature,
             (uint64_t)ReconfigSettingsIntf::FeatureType::HULKLicenseUpdate,
             (uint64_t)ReconfigSettingsIntf::FeatureType::ForceTestCoupling,
             (uint64_t)ReconfigSettingsIntf::FeatureType::BAR0TypeConfig,
             (uint64_t)ReconfigSettingsIntf::FeatureType::EDPpScalingFactor,
             (uint64_t)ReconfigSettingsIntf::FeatureType::PowerSmoothing,
             (uint64_t)ReconfigSettingsIntf::FeatureType::
                 PowerSmoothingPrivilegeLevel0,
             (uint64_t)ReconfigSettingsIntf::FeatureType::
                 PowerSmoothingPrivilegeLevel2,
             (uint64_t)ReconfigSettingsIntf::FeatureType::
                 PowerSmoothingPrivilegeLevel1,
         }},
    };
};

namespace nsm
{
void createNsmProcessorSensor(SensorManager& manager,
                              const std::string& interface,
                              const std::string& objPath);
};

TEST_F(NsmProcessorTest, badTestTypeError)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(error, "Type")))
        .WillOnce(Return(get(basic, "InventoryObjPath")));
    EXPECT_NO_THROW(
        createNsmProcessorSensor(mockManager, basicIntfName, objPath));
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}

TEST_F(NsmProcessorTest, badTestNoDevideFound)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(error, "UUID")))
        .WillOnce(Return(get(basic, "Type")))
        .WillOnce(Return(get(basic, "InventoryObjPath")));
    EXPECT_THROW(createNsmProcessorSensor(mockManager, basicIntfName, objPath),
                 std::runtime_error);
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}

TEST_F(NsmProcessorTest, goodTestCreateInbandReconfigPermissionsSensors)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(prcKnobs, "Type")))
        .WillOnce(Return(get(basic, "InventoryObjPath")))
        .WillOnce(Return(get(prcKnobs, "Priority")))
        .WillOnce(Return(get(prcKnobs, "Features")));
    createNsmProcessorSensor(
        mockManager, basicIntfName + ".InbandReconfigPermissions", objPath);

    const size_t expectedSensorsCount = 24;
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(expectedSensorsCount, gpu.roundRobinSensors.size());
    EXPECT_EQ(expectedSensorsCount, gpu.deviceSensors.size());

    nsm_reconfiguration_permissions_v1 data = {0, 1, 1, 0};
    Response response(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_reconfiguration_permissions_v1_resp),
                      0);
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_reconfiguration_permissions_v1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, msg);
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .Times(expectedSensorsCount)
        .WillRepeatedly(mockSendRecvNsmMsg(response));
    for (size_t i = 0; i < expectedSensorsCount; i++)
    {
        auto reconfigPermissions =
            dynamic_pointer_cast<NsmReconfigPermissions>(gpu.deviceSensors[i]);
        EXPECT_NE(nullptr, reconfigPermissions);

        // Test if added permissions are sorted and unique
        EXPECT_EQ((ReconfigSettingsIntf::FeatureType)i, reconfigPermissions->feature);
        reconfigPermissions->update(mockManager, eid).detach();
        EXPECT_EQ(data.oneshot,
                  reconfigPermissions->pdi().allowOneShotConfig());
        EXPECT_EQ(data.persistent,
                  reconfigPermissions->pdi().allowPersistentConfig());
        EXPECT_EQ(data.flr_persistent,
                  reconfigPermissions->pdi().allowFLRPersistentConfig());
        EXPECT_EQ(reconfigPermissions->feature,
                  reconfigPermissions->pdi().type());
    }
}

TEST_F(NsmProcessorTest, badTestCreateInbandReconfigPermissionsSensors)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(prcKnobs, "Type")))
        .WillOnce(Return(get(basic, "InventoryObjPath")))
        .WillOnce(Return(get(prcKnobs, "Priority")))
        .WillOnce(Return(get(error, "Features")));
    EXPECT_THROW(
        createNsmProcessorSensor(
            mockManager, basicIntfName + ".InbandReconfigPermissions", objPath),
        std::invalid_argument);
}
