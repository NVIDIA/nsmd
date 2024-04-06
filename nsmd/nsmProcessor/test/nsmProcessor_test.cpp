#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::_;
using ::testing::Args;
using ::testing::ElementsAreArray;

#include "base.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "../nsmProcessor.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();
std::string sensorName("dummy_sensor");
std::string sensorType("dummy_type");
std::string inventoryObjPath("/xyz/openbmc_project/inventory/dummy_device");

TEST(nsmMigMode, GoodGenReq)
{
    nsm::NsmMigMode migSensor(bus, sensorName, sensorType, inventoryObjPath);

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
    nsm::NsmMigMode migSensor(bus, sensorName, sensorType, inventoryObjPath);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_MIG_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 1;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc =
        encode_get_MIG_mode_resp(0, NSM_SUCCESS, reason_code, &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = migSensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmMigMode, BadHandleResp)
{
    nsm::NsmMigMode migSensor(bus, sensorName, sensorType, inventoryObjPath);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_MIG_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 1;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc =
        encode_get_MIG_mode_resp(0, NSM_SUCCESS, reason_code, &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = migSensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = migSensor.handleResponseMsg(response, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmMigMode, GoodUpdateReading)
{
    nsm::NsmMigMode migSensor(bus, sensorName, sensorType, inventoryObjPath);
    bitfield8_t flags;
    flags.byte = 1;
    migSensor.updateReading(flags);
    EXPECT_EQ(migSensor.migModeIntf->migModeEnabled(), flags.bits.bit0);
}

TEST(nsmEccMode, GoodGenReq)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccMode eccModeSensor(sensorName, sensorType, eccIntf);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = eccModeSensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_ECC_MODE);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmEccMode, GoodHandleResp)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccMode sensor(sensorName, sensorType, eccIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_ECC_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 1;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc =
        encode_get_ECC_mode_resp(0, NSM_SUCCESS, reason_code, &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmEccMode, GoodUpdateReading)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccMode sensor(sensorName, sensorType, eccIntf);
    bitfield8_t flags;
    flags.byte = 1;
    sensor.updateReading(flags);
    EXPECT_EQ(sensor.eccModeIntf->eccModeEnabled(), flags.bits.bit0);
}

TEST(nsmEccMode, BadHandleResp)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccMode sensor(sensorName, sensorType, eccIntf);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_ECC_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 1;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc =
        encode_get_ECC_mode_resp(0, NSM_SUCCESS, reason_code, &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();

    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = sensor.handleResponseMsg(response, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmEccErrorCounts, GoodGenReq)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccErrorCounts eccErrorCntSensor(sensorName, sensorType, eccIntf);

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
    nsm::NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf);

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
    nsm::NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf);
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
    nsm::NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf);

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
    auto pCieECCIntf =
        std::make_shared<PCieEccIntf>(bus, inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup2 sensor(sensorName, sensorType, pCieECCIntf, deviceId);

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
    auto pCieECCIntf =
        std::make_shared<PCieEccIntf>(bus, inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup2 sensor(sensorName, sensorType, pCieECCIntf, deviceId);

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
    auto pCieECCIntf =
        std::make_shared<PCieEccIntf>(bus, inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup2 sensor(sensorName, sensorType, pCieECCIntf, deviceId);

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
    auto pCieECCIntf =
        std::make_shared<PCieEccIntf>(bus, inventoryObjPath.c_str());
    uint8_t deviceId = 0;

    nsm::NsmPciGroup3 sensor(sensorName, sensorType, pCieECCIntf, deviceId);

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
    auto pCieECCIntf =
        std::make_shared<PCieEccIntf>(bus, inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup3 sensor(sensorName, sensorType, pCieECCIntf, deviceId);

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
    auto pCieECCIntf =
        std::make_shared<PCieEccIntf>(bus, inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup3 sensor(sensorName, sensorType, pCieECCIntf, deviceId);

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
    auto pCieECCIntf =
        std::make_shared<PCieEccIntf>(bus, inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup4 sensor(sensorName, sensorType, pCieECCIntf, deviceId);

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
    auto pCieECCIntf =
        std::make_shared<PCieEccIntf>(bus, inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup4 sensor(sensorName, sensorType, pCieECCIntf, deviceId);

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
    auto pCieECCIntf =
        std::make_shared<PCieEccIntf>(bus, inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup4 sensor(sensorName, sensorType, pCieECCIntf, deviceId);

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