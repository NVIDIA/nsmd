#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::_;
using ::testing::Args;
using ::testing::ElementsAreArray;

#include "base.h"
#include "pci-links.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmCommon/nsmPcieGroup.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();
std::string sensorName("dummy_sensor");
std::string sensorType("dummy_type");
std::string inventoryObjPath("/xyz/openbmc_project/inventory/dummy_device");

TEST(NsmPCIeGroup2, GoodGenReq)
{
    auto pcieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pciePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup2 sensor(sensorName, sensorType, pcieECCIntf, pciePortIntf,
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
    auto pcieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pciePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup2 sensor(sensorName, sensorType, pcieECCIntf, pciePortIntf,
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

TEST(NsmPCIeGroup3, GoodGenReq)
{
    auto pcieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pciePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;

    nsm::NsmPciGroup3 sensor(sensorName, sensorType, pcieECCIntf, pciePortIntf,
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
    auto pcieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pciePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup3 sensor(sensorName, sensorType, pcieECCIntf, pciePortIntf,
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

TEST(NsmPCIeGroup4, GoodGenReq)
{
    auto pcieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pciePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup4 sensor(sensorName, sensorType, pcieECCIntf, pciePortIntf,
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
    auto pcieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_0";
    auto pciePortIntf = std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
    uint8_t deviceId = 0;
    nsm::NsmPciGroup4 sensor(sensorName, sensorType, pcieECCIntf, pciePortIntf,
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
