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

#include "nsmAssetIntf.hpp"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmErrorInjectionCommon.hpp"
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
                              nullptr, false);

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
                              nullptr, false);
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
                              nullptr, false);
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
                              nullptr, false);
    bitfield8_t flags;
    flags.byte = 1;
    migSensor.updateReading(flags);
    EXPECT_EQ(migSensor.migModeIntf->migModeEnabled(), flags.bits.bit0);
}

TEST(nsmEccErrorCounts, GoodGenReq)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
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
    auto eccIntf = std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
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
    auto eccIntf = std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
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
    auto eccIntf = std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
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
    auto eDPpIntf = std::make_shared<EDPpLocal>(bus, inventoryObjPath);
    auto resetEdppAsyncIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus, inventoryObjPath.c_str(), nullptr);
    nsm::NsmEDPpScalingFactor sensor(sensorName, sensorType, inventoryObjPath,
                                     eDPpIntf, resetEdppAsyncIntf);
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
    auto eDPpIntf = std::make_shared<EDPpLocal>(bus, inventoryObjPath);
    auto resetEdppAsyncIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus, inventoryObjPath.c_str(), nullptr);
    nsm::NsmEDPpScalingFactor sensor(sensorName, sensorType, inventoryObjPath,
                                     eDPpIntf, resetEdppAsyncIntf);

    struct nsm_EDPp_scaling_factors scaling_factors;
    scaling_factors.persistent_scaling_factor = 70;
    scaling_factors.oneshot_scaling_factor = 90;
    scaling_factors.enforced_scaling_factor = 60;

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
    auto eDPpIntf = std::make_shared<EDPpLocal>(bus, inventoryObjPath);
    auto resetEdppAsyncIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus, inventoryObjPath.c_str(), nullptr);
    nsm::NsmEDPpScalingFactor sensor(sensorName, sensorType, inventoryObjPath,
                                     eDPpIntf, resetEdppAsyncIntf);

    struct nsm_EDPp_scaling_factors scaling_factors;
    scaling_factors.persistent_scaling_factor = 70;
    scaling_factors.oneshot_scaling_factor = 90;
    scaling_factors.enforced_scaling_factor = 60;
    sensor.persistence = true;
    sensor.updateReading(scaling_factors);
    EXPECT_EQ(sensor.eDPpIntf->setPoint(),
              std::make_tuple(scaling_factors.enforced_scaling_factor,
                              sensor.persistence));
}

TEST(nsmEDPpScalingFactor, BadHandleResp)
{
    auto eDPpIntf = std::make_shared<EDPpLocal>(bus, inventoryObjPath);
    auto resetEdppAsyncIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus, inventoryObjPath.c_str(), nullptr);
    nsm::NsmEDPpScalingFactor sensor(sensorName, sensorType, inventoryObjPath,
                                     eDPpIntf, resetEdppAsyncIntf);

    struct nsm_EDPp_scaling_factors scaling_factors;
    scaling_factors.persistent_scaling_factor = 70;
    scaling_factors.oneshot_scaling_factor = 90;
    scaling_factors.enforced_scaling_factor = 60;

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
    auto cpuOperatingConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
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
    auto cpuOperatingConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
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
    auto cpuOperatingConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
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
    auto cpuOperatingConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
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
    auto cpuOperatingConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
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
    auto cpuOperatingConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
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

TEST(nsmProcessorThrottleDuration, GoodGenReq)
{
    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    std::shared_ptr<NsmDevice> gpuPtr = std::make_shared<NsmDevice>(gpuUuid);
    auto processorPerformanceIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());
    nsm::NsmProcessorThrottleDuration sensor(sensorName, sensorType,
                                             processorPerformanceIntf,
                                             inventoryObjPath, false, gpuPtr);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_VIOLATION_DURATION);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmProcessorThrottleDuration, GoodHandleResp)
{
    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    std::shared_ptr<NsmDevice> gpuPtr = std::make_shared<NsmDevice>(gpuUuid);
    auto processorPerformanceIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());
    nsm::NsmProcessorThrottleDuration sensor(sensorName, sensorType,
                                             processorPerformanceIntf,
                                             inventoryObjPath, false, gpuPtr);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_violation_duration_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    struct nsm_violation_duration data;
    data.supported_counter.byte = 255;
    data.hw_violation_duration = 2000000;
    data.global_sw_violation_duration = 3000000;
    data.power_violation_duration = 4000000;
    data.thermal_violation_duration = 5000000;
    data.counter4 = 6000000;
    data.counter5 = 7000000;
    data.counter6 = 8000000;
    data.counter7 = 9000000;

    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_violation_duration_resp(0, NSM_SUCCESS, reason_code,
                                                    &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmProcessorThrottleDuration, BadHandleResp)
{
    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    std::shared_ptr<NsmDevice> gpuPtr = std::make_shared<NsmDevice>(gpuUuid);
    auto processorPerformanceIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());
    nsm::NsmProcessorThrottleDuration sensor(sensorName, sensorType,
                                             processorPerformanceIntf,
                                             inventoryObjPath, false, gpuPtr);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_violation_duration_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_violation_duration data;
    data.supported_counter.byte = 255;
    data.hw_violation_duration = 2000000;
    data.global_sw_violation_duration = 3000000;
    data.power_violation_duration = 4000000;
    data.thermal_violation_duration = 5000000;
    data.counter4 = 6000000;
    data.counter5 = 7000000;
    data.counter6 = 8000000;
    data.counter7 = 9000000;

    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_violation_duration_resp(0, NSM_SUCCESS, reason_code,
                                                    &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = sensor.handleResponseMsg(response, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmPowerSmoothing, GoodGenReq)
{
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(
        bus, inventoryObjPath, nullptr);
    NsmPowerSmoothing controlSensor(sensorName, sensorType, inventoryObjPath,
                                    featIntf);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = controlSensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_PWR_SMOOTHING_GET_FEATURE_INFO);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmPowerSmoothing, GoodHandleResp)
{
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(
        bus, inventoryObjPath, nullptr);
    NsmPowerSmoothing controlSensor(sensorName, sensorType, inventoryObjPath,
                                    featIntf);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_power_smoothing_feat_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_pwr_smoothing_featureinfo_data feat;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_powersmoothing_featinfo_resp(
        0, NSM_SUCCESS, reason_code, &feat, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = controlSensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmPowerSmoothing, BadHandleResp)
{
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(
        bus, inventoryObjPath, nullptr);
    NsmPowerSmoothing controlSensor(sensorName, sensorType, inventoryObjPath,
                                    featIntf);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_power_smoothing_feat_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_pwr_smoothing_featureinfo_data feat;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_powersmoothing_featinfo_resp(
        0, NSM_SUCCESS, reason_code, &feat, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = controlSensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = controlSensor.handleResponseMsg(response, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmPowerSmoothing, GoodUpdateReading)
{
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(
        bus, inventoryObjPath, nullptr);
    NsmPowerSmoothing controlSensor(sensorName, sensorType, inventoryObjPath,
                                    featIntf);
    struct nsm_pwr_smoothing_featureinfo_data feat;
    feat.feature_flag = 7;
    controlSensor.updateReading(&feat);
    EXPECT_EQ(
        controlSensor.pwrSmoothingIntf->PowerSmoothingIntf::featureSupported(),
        true);
}

TEST(nsmHwCircuitryTelemetry, GoodGenReq)
{
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(
        bus, inventoryObjPath, nullptr);
    NsmHwCircuitryTelemetry lifetimeCicuitrySensor(sensorName, sensorType,
                                                   inventoryObjPath, featIntf);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = lifetimeCicuitrySensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command,
              NSM_PWR_SMOOTHING_GET_HARDWARE_CIRCUITRY_LIFETIME_USAGE);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmHwCircuitryTelemetry, GoodHandleResp)
{
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(
        bus, inventoryObjPath, nullptr);
    NsmHwCircuitryTelemetry lifetimeCicuitrySensor(sensorName, sensorType,
                                                   inventoryObjPath, featIntf);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_hardwareciruitry_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_hardwarecircuitry_data lifetimeusage;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_hardware_lifetime_cricuitry_resp(
        0, NSM_SUCCESS, reason_code, &lifetimeusage, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = lifetimeCicuitrySensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmHwCircuitryTelemetry, BadHandleResp)
{
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(
        bus, inventoryObjPath, nullptr);
    NsmHwCircuitryTelemetry lifetimeCicuitrySensor(sensorName, sensorType,
                                                   inventoryObjPath, featIntf);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_hardwareciruitry_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_hardwarecircuitry_data lifetimeusage;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_hardware_lifetime_cricuitry_resp(
        0, NSM_SUCCESS, reason_code, &lifetimeusage, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = lifetimeCicuitrySensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = lifetimeCicuitrySensor.handleResponseMsg(response, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmHwCircuitryTelemetry, GoodUpdateReading)
{
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(
        bus, inventoryObjPath, nullptr);
    NsmHwCircuitryTelemetry lifetimeCicuitrySensor(sensorName, sensorType,
                                                   inventoryObjPath, featIntf);
    struct nsm_hardwarecircuitry_data data;
    data.reading = 0;
    lifetimeCicuitrySensor.updateReading(&data);
    EXPECT_EQ(lifetimeCicuitrySensor.pwrSmoothingIntf
                  ->PowerSmoothingIntf::lifeTimeRemaining(),
              0);
}

TEST(nsmPowerSmoothingAdminOverride, GoodGenReq)
{
    auto featIntf = std::make_shared<OemAdminProfileIntf>(bus, inventoryObjPath,
                                                          nullptr);
    NsmPowerSmoothingAdminOverride adminProfileSensor(
        sensorName, sensorType, featIntf, inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = adminProfileSensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmPowerSmoothingAdminOverride, GoodHandleResp)
{
    auto featIntf = std::make_shared<OemAdminProfileIntf>(bus, inventoryObjPath,
                                                          nullptr);
    NsmPowerSmoothingAdminOverride adminProfileSensor(
        sensorName, sensorType, featIntf, inventoryObjPath);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_query_admin_override_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_admin_override_data adminProfileData;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_query_admin_override_resp(0, NSM_SUCCESS, reason_code,
                                                  &adminProfileData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = adminProfileSensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmPowerSmoothingAdminOverride, BadHandleResp)
{
    auto featIntf = std::make_shared<OemAdminProfileIntf>(bus, inventoryObjPath,
                                                          nullptr);
    NsmPowerSmoothingAdminOverride adminProfileSensor(
        sensorName, sensorType, featIntf, inventoryObjPath);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_query_admin_override_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_admin_override_data adminProfileData;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_query_admin_override_resp(0, NSM_SUCCESS, reason_code,
                                                  &adminProfileData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = adminProfileSensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = adminProfileSensor.handleResponseMsg(response, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmPowerSmoothingAdminOverride, GoodUpdateReading)
{
    auto featIntf = std::make_shared<OemAdminProfileIntf>(bus, inventoryObjPath,
                                                          nullptr);
    NsmPowerSmoothingAdminOverride adminProfileSensor(
        sensorName, sensorType, featIntf, inventoryObjPath);
    struct nsm_admin_override_data adminProfileData;
    adminProfileData.admin_override_percent_tmp_floor = 0;
    adminProfileSensor.updateReading(&adminProfileData);
    EXPECT_EQ(adminProfileSensor.adminProfileIntf
                  ->AdminPowerProfileIntf::tmpFloorPercent(),
              0);
}

TEST(nsmPowerProfileCollection, GoodGenReq)
{
    NsmPowerProfileCollection getAllPowerProfileSensor(
        sensorName, sensorType, inventoryObjPath, nullptr);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = getAllPowerProfileSensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command,
              NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmPowerProfileCollection, GoodHandleResp)
{
    NsmPowerProfileCollection getAllPowerProfileSensor(
        sensorName, sensorType, inventoryObjPath, nullptr);
    struct nsm_get_all_preset_profile_meta_data profile_meta_data;
    int number_of_profiles = 1;
    profile_meta_data.max_profiles_supported = number_of_profiles;
    struct nsm_preset_profile_data profiles[1];
    uint16_t meta_data_size =
        sizeof(struct nsm_get_all_preset_profile_meta_data);
    uint16_t profile_data_size = sizeof(struct nsm_preset_profile_data);
    uint16_t data_size = meta_data_size +
                         number_of_profiles * profile_data_size;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp) + data_size, 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_preset_profile_resp(
        0, NSM_SUCCESS, reason_code, &profile_meta_data, profiles,
        profile_meta_data.max_profiles_supported, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = getAllPowerProfileSensor.handleResponseMsg(responseMsg, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmPowerProfileCollection, BadHandleResp)
{
    NsmPowerProfileCollection getAllPowerProfileSensor(
        sensorName, sensorType, inventoryObjPath, nullptr);
    struct nsm_get_all_preset_profile_meta_data profile_meta_data;
    int number_of_profiles = 1;
    profile_meta_data.max_profiles_supported = number_of_profiles;
    struct nsm_preset_profile_data profiles[1];
    uint16_t meta_data_size =
        sizeof(struct nsm_get_all_preset_profile_meta_data);
    uint16_t profile_data_size = sizeof(struct nsm_preset_profile_data);
    uint16_t data_size = meta_data_size +
                         number_of_profiles * profile_data_size;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp) + data_size, 0);

    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_preset_profile_resp(
        0, NSM_SUCCESS, reason_code, &profile_meta_data, profiles,
        profile_meta_data.max_profiles_supported, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = getAllPowerProfileSensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = getAllPowerProfileSensor.handleResponseMsg(responseMsg, 2);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmCurrentPowerSmoothingProfile, GoodGenReq)
{
    std::shared_ptr<OemAdminProfileIntf> adminProfileIntf =
        std::make_shared<OemAdminProfileIntf>(bus, inventoryObjPath, nullptr);
    auto adminProfileSensor = std::make_shared<NsmPowerSmoothingAdminOverride>(
        sensorName, sensorType, adminProfileIntf, inventoryObjPath);
    std::shared_ptr<OemCurrentPowerProfileIntf> pwrSmoothingCurProfileIntf =
        std::make_shared<OemCurrentPowerProfileIntf>(
            bus, inventoryObjPath, adminProfileIntf->getInventoryObjPath(),
            nullptr);
    auto getAllPowerProfileSensor = std::make_shared<NsmPowerProfileCollection>(
        sensorName, sensorType, inventoryObjPath, nullptr);
    NsmCurrentPowerSmoothingProfile currentProfileSensor(
        sensorName, sensorType, inventoryObjPath, pwrSmoothingCurProfileIntf,
        getAllPowerProfileSensor, adminProfileSensor);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = currentProfileSensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command,
              NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmCurrentPowerSmoothingProfile, GoodHandleResp)
{
    std::shared_ptr<OemAdminProfileIntf> adminProfileIntf =
        std::make_shared<OemAdminProfileIntf>(bus, inventoryObjPath, nullptr);
    auto adminProfileSensor = std::make_shared<NsmPowerSmoothingAdminOverride>(
        sensorName, sensorType, adminProfileIntf, inventoryObjPath);
    std::shared_ptr<OemCurrentPowerProfileIntf> pwrSmoothingCurProfileIntf =
        std::make_shared<OemCurrentPowerProfileIntf>(
            bus, inventoryObjPath, adminProfileIntf->getInventoryObjPath(),
            nullptr);
    auto getAllPowerProfileSensor = std::make_shared<NsmPowerProfileCollection>(
        sensorName, sensorType, inventoryObjPath, nullptr);
    NsmCurrentPowerSmoothingProfile currentProfileSensor(
        sensorName, sensorType, inventoryObjPath, pwrSmoothingCurProfileIntf,
        getAllPowerProfileSensor, adminProfileSensor);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_current_profile_info_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_get_current_profile_data profileData;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_current_profile_info_resp(
        0, NSM_SUCCESS, reason_code, &profileData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = currentProfileSensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmCurrentPowerSmoothingProfile, BadHandleResp)
{
    std::shared_ptr<OemAdminProfileIntf> adminProfileIntf =
        std::make_shared<OemAdminProfileIntf>(bus, inventoryObjPath, nullptr);
    auto adminProfileSensor = std::make_shared<NsmPowerSmoothingAdminOverride>(
        sensorName, sensorType, adminProfileIntf, inventoryObjPath);
    std::shared_ptr<OemCurrentPowerProfileIntf> pwrSmoothingCurProfileIntf =
        std::make_shared<OemCurrentPowerProfileIntf>(
            bus, inventoryObjPath, adminProfileIntf->getInventoryObjPath(),
            nullptr);
    auto getAllPowerProfileSensor = std::make_shared<NsmPowerProfileCollection>(
        sensorName, sensorType, inventoryObjPath, nullptr);
    NsmCurrentPowerSmoothingProfile currentProfileSensor(
        sensorName, sensorType, inventoryObjPath, pwrSmoothingCurProfileIntf,
        getAllPowerProfileSensor, adminProfileSensor);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_current_profile_info_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_get_current_profile_data profileData;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_current_profile_info_resp(
        0, NSM_SUCCESS, reason_code, &profileData, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = currentProfileSensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = currentProfileSensor.handleResponseMsg(response, msg_len - 1);
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

    std::shared_ptr<NsmDevice> gpuPtr = std::make_shared<NsmDevice>(gpuUuid);
    NsmDeviceTable devices{{gpuPtr}};
    NsmDevice& gpu = *gpuPtr;

    NsmProcessorTest() : SensorManagerTest(devices)
    {
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    const PropertyValuesCollection error = {
        {"Type", "NSM_processor"},
        {"UUID", badUuid},
        {"UnknownFeature",
         std::vector<std::string>{
             "Unknown",
         }},
        {"InvalidFeatures",
         std::vector<std::string>{
             "InSystemTestt",
         }},
    };
    const PropertyValuesCollection basic = {
        {"Name", name},
        {"Type", "NSM_Processor"},
        {"UUID", gpuUuid},
        {"DEVICE_UUID", gpuUuid},
        {"InventoryObjPath", objPath},
    };
    const PropertyValuesCollection prcKnobs = {
        {"Type", "NSM_ReconfigPermissions"},
        {"Priority", false},
        {"Features", // features are not propertly sorted and some are
                     // duplicated
         std::vector<std::string>{
             "InSystemTest",
             "FusingMode",
             "CCMode",
             "PowerSmoothingPrivilegeLevel1",
             "BAR0Firewall",
             "CCDevMode",
             "TGPCurrentLimit",
             "TGPRatedLimit",
             "HBMFrequencyChange",
             "TGPMaxLimit",
             "TGPMinLimit",
             "ClockLimit",
             "NVLinkDisable",
             "ECCEnable",
             "PCIeVFConfiguration",
             "RowRemappingAllowed",
             "RowRemappingFeature",
             "HULKLicenseUpdate",
             "ForceTestCoupling",
             "BAR0TypeConfig",
             "EDPpScalingFactor",
             "PowerSmoothingPrivilegeLevel2",
             "PowerSmoothingPrivilegeLevel1",
         }},
    };
};

namespace nsm
{
requester::Coroutine createNsmProcessorSensor(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath);
};

TEST_F(NsmProcessorTest, badTestTypeError)
{
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(error, "Type"));
    values.push(objPath, get(basic, "InventoryObjPath"));
    createNsmProcessorSensor(mockManager, basicIntfName, objPath);
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}

TEST_F(NsmProcessorTest, badTestNoDevideFound)
{
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(error, "UUID"));
    values.push(objPath, get(basic, "Type"));
    values.push(objPath, get(basic, "InventoryObjPath"));
    createNsmProcessorSensor(mockManager, basicIntfName, objPath);
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}

TEST_F(NsmProcessorTest, goodTestCreateInbandReconfigPermissionsSensors)
{
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(prcKnobs, "Type"));
    values.push(objPath, get(basic, "InventoryObjPath"));
    values.push(objPath, get(prcKnobs, "Priority"));
    values.push(objPath, get(prcKnobs, "Features"));

    createNsmProcessorSensor(mockManager,
                             basicIntfName + ".ReconfigPermissions", objPath);

    const size_t expectedSensorsCount = 22;
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(22, gpu.roundRobinSensors.size());
    EXPECT_EQ(expectedSensorsCount, gpu.deviceSensors.size());

    nsm_reconfiguration_permissions_v1 data = {0, 1, 1, 0, 0, 1, 1};
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
        EXPECT_EQ(
            reconfiguration_permissions_v1_index(i),
            NsmReconfigPermissions::getIndex(reconfigPermissions->feature));
        reconfigPermissions->update(mockManager, eid).detach();
        EXPECT_EQ(data.host_oneshot,
                  reconfigPermissions->hostConfigIntf->allowOneShotConfig());
        EXPECT_EQ(data.host_persistent,
                  reconfigPermissions->hostConfigIntf->allowPersistentConfig());
        EXPECT_EQ(
            data.host_flr_persistent,
            reconfigPermissions->hostConfigIntf->allowFLRPersistentConfig());
        EXPECT_EQ(reconfigPermissions->feature,
                  reconfigPermissions->hostConfigIntf->type());
    }
}

TEST_F(NsmProcessorTest, goodTestCreateErrorInjectionSensors)
{
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(basic, "Type"));
    values.push(objPath, get(basic, "InventoryObjPath"));
    values.push(objPath, get(basic, "DEVICE_UUID"));
    createNsmProcessorSensor(mockManager, basicIntfName, objPath);

    auto capabilitiesCount =
        size_t(ErrorInjectionCapabilityIntf::Type::Unknown);
    EXPECT_EQ(0, gpu.prioritySensors.size());
    // Total 10 RR sensor for type = NSM_Processor are added.
    // 8 are added as part of createNsmProcessorSensor() and 2 are added as part
    // of createNsmErrorInjectionSensors()
    EXPECT_EQ(10, gpu.roundRobinSensors.size());
    // Total 18 device sensor for type = NSM_Processor are added.
    // 10 are added as part of createNsmProcessorSensor() (NOTE:
    // NVIDIA_RESET_METRICS & ENABLE_SYSTEM_GUID are disabled during this test
    // run) and 8 are added as part of createNsmErrorInjectionSensors()
    EXPECT_EQ(14 + capabilitiesCount, gpu.deviceSensors.size());

    int si = 9;

    auto expectedInterfaces = int(ErrorInjectionCapabilityIntf::Type::Unknown);
    auto setErrorInjection =
        dynamic_pointer_cast<NsmSetErrorInjection>(gpu.deviceSensors[si++]);
    EXPECT_NE(nullptr, setErrorInjection);
    auto errorInjectionSensor =
        dynamic_pointer_cast<NsmErrorInjection>(gpu.deviceSensors[si++]);
    EXPECT_NE(nullptr, errorInjectionSensor);
    auto errorInjectionSupported =
        dynamic_pointer_cast<NsmErrorInjectionSupported>(
            gpu.deviceSensors[si++]);
    EXPECT_NE(nullptr, errorInjectionSupported);
    EXPECT_EQ(expectedInterfaces, errorInjectionSupported->interfaces.size());
    EXPECT_TRUE(errorInjectionSupported->isStatic);
    auto errorInjectionEnabled =
        dynamic_pointer_cast<NsmErrorInjectionEnabled>(gpu.deviceSensors[si++]);
    EXPECT_NE(nullptr, errorInjectionEnabled);
    EXPECT_EQ(expectedInterfaces, errorInjectionEnabled->interfaces.size());

    std::vector<std::shared_ptr<NsmSetErrorInjectionEnabled>>
        setErrorInjectionEnabled(capabilitiesCount, nullptr);
    for (size_t i = 0; i < capabilitiesCount; i++)
    {
        setErrorInjectionEnabled[i] =
            dynamic_pointer_cast<NsmSetErrorInjectionEnabled>(
                gpu.deviceSensors[si++]);
        EXPECT_NE(nullptr, setErrorInjectionEnabled[i]);
        EXPECT_EQ(expectedInterfaces,
                  setErrorInjectionEnabled[i]->interfaces.size());
    }

    nsm_error_injection_types_mask supportedData = {0b00001001, 0, 0, 0,
                                                    0,          0, 0, 0};

    Response supportedResponse(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_types_mask_resp),
        0);
    auto supportedResponseMsg =
        reinterpret_cast<nsm_msg*>(supportedResponse.data());
    auto rc = encode_get_supported_error_injection_types_v1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &supportedData,
        supportedResponseMsg);
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    Response setModeResponse(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto setModeResponseMsg =
        reinterpret_cast<nsm_msg*>(setModeResponse.data());
    rc = encode_set_error_injection_mode_v1_resp(instanceId, NSM_SUCCESS,
                                                 ERR_NULL, setModeResponseMsg);
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    nsm_error_injection_mode_v1 mode = {1, 1};
    Response modeResponse(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_mode_v1_resp), 0);
    auto modeResponseMsg = reinterpret_cast<nsm_msg*>(modeResponse.data());
    rc = encode_get_error_injection_mode_v1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &mode, modeResponseMsg);
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    Response setEnableResponse(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                               0);
    auto setEnableResponseMsg =
        reinterpret_cast<nsm_msg*>(setEnableResponse.data());
    rc = encode_set_current_error_injection_types_v1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, setEnableResponseMsg);
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .WillOnce(mockSendRecvNsmMsg(supportedResponse))
        .WillOnce(mockSendRecvNsmMsg(setModeResponse))
        .WillOnce(mockSendRecvNsmMsg(modeResponse));

    errorInjectionSupported->update(mockManager, eid).detach();

    const auto errorInjectionBasePath = processorsInventoryBasePath / name /
                                        "ErrorInjection";
    const auto memoryErrorsPath = errorInjectionBasePath / "MemoryErrors";
    const auto nvLinkErrorsPath = errorInjectionBasePath / "NVLinkErrors";
    const auto pcieErrorsPath = errorInjectionBasePath / "PCIeErrors";
    const auto thermalErrorsPath = errorInjectionBasePath / "ThermalErrors";
    EXPECT_TRUE(
        errorInjectionSupported->interfaces[memoryErrorsPath]->supported());
    EXPECT_FALSE(
        errorInjectionSupported->interfaces[nvLinkErrorsPath]->supported());
    EXPECT_FALSE(
        errorInjectionSupported->interfaces[pcieErrorsPath]->supported());
    EXPECT_TRUE(
        errorInjectionSupported->interfaces[thermalErrorsPath]->supported());

    auto status = AsyncOperationStatusType::Success;
    setErrorInjection->errorInjectionModeEnabled(true, &status, gpuPtr);
    EXPECT_EQ(AsyncOperationStatusType::Success, status);
    errorInjectionSensor->update(mockManager, eid);

    EXPECT_TRUE(errorInjectionSensor->pdi().errorInjectionModeEnabled());
    EXPECT_TRUE(errorInjectionSensor->pdi().persistentDataModified());

    // test set async
    nsm_error_injection_types_mask enabledData = {0b00001000, 0, 0, 0,
                                                  0,          0, 0, 0};
    Response enableResponse(sizeof(nsm_msg_hdr) +
                                sizeof(nsm_get_error_injection_types_mask_resp),
                            0);
    auto enableResponseMsg = reinterpret_cast<nsm_msg*>(enableResponse.data());
    rc = encode_get_current_error_injection_types_v1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &enabledData, enableResponseMsg);
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    gpu.isDeviceActive = true;
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .Times(2)
        .WillOnce(mockSendRecvNsmMsg(setEnableResponse))
        .WillOnce(mockSendRecvNsmMsg(enableResponse));
    const AsyncSetOperationValueType value{};
    auto dispatcher =
        AsyncOperationManager::getInstance()->getDispatcher(thermalErrorsPath);
    auto result = AsyncOperationManager::getInstance()->getNewStatusInterface();
    dispatcher->setImpl("com.nvidia.ErrorInjection.ErrorInjectionCapability",
                        "Enabled", true, result.second);

    EXPECT_FALSE(
        errorInjectionEnabled->interfaces[memoryErrorsPath]->enabled());
    EXPECT_FALSE(
        errorInjectionEnabled->interfaces[nvLinkErrorsPath]->enabled());
    EXPECT_FALSE(errorInjectionEnabled->interfaces[pcieErrorsPath]->enabled());
    EXPECT_TRUE(
        errorInjectionEnabled->interfaces[thermalErrorsPath]->enabled());
}

TEST(nsmTotalNvLinks, GoodGenReq)
{
    auto totalNvLinkInterface =
        std::make_shared<TotalNvLinkInterface>(bus, inventoryObjPath.c_str());
    nsm::NsmTotalNvLinks sensor(sensorName, sensorType, totalNvLinkInterface,
                                inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_query_ports_available_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_QUERY_PORTS_AVAILABLE);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmTotalNvLinks, GoodHandleResp)
{
    auto totalNvLinkInterface =
        std::make_shared<TotalNvLinkInterface>(bus, inventoryObjPath.c_str());
    nsm::NsmTotalNvLinks sensor(sensorName, sensorType, totalNvLinkInterface,
                                inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_query_ports_available_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t nvLinkCount = 144;
    uint16_t reason_code = ERR_NULL;
    uint8_t rc = encode_query_ports_available_resp(0, NSM_SUCCESS, reason_code,
                                                   nvLinkCount, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmTotalNvLinks, BadHandleResp)
{
    auto totalNvLinkInterface =
        std::make_shared<TotalNvLinkInterface>(bus, inventoryObjPath.c_str());
    nsm::NsmTotalNvLinks sensor(sensorName, sensorType, totalNvLinkInterface,
                                inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_query_ports_available_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t nvLinkCount = 144;
    uint16_t reason_code = ERR_NULL;
    uint8_t rc = encode_query_ports_available_resp(0, NSM_SUCCESS, reason_code,
                                                   nvLinkCount, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = sensor.handleResponseMsg(response, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}
