#include <gmock/gmock.h>
#include <gtest/gtest.h>
using namespace ::testing;

#define private public
#define protected public

#include "pci-links.h"

#include "nsmGpuChassisPCIeDevice.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmPCIeDevice.hpp"
#include "nsmPCIeFunction.hpp"
#include "nsmPCIeLTSSMState.hpp"

using namespace nsm;

struct NsmGpuChassisPCIeDeviceTest : public testing::Test
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_GPU_ChassisPCIeDevice";
    const std::string chassisName = "HGX_GPU_SXM_1";
    const std::string name = "PCIeDevice1";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/" + chassisName + "/" +
        name;
};

struct NsmPCIeDeviceTest : public NsmGpuChassisPCIeDeviceTest
{
  protected:
    uint8_t deviceId = 1;
    NsmGpuChassisPCIeDevice<PCIeDeviceIntf> pcieDevice{chassisName, name};

  private:
    std::shared_ptr<NsmPCIeDevice> sensor =
        std::make_shared<NsmPCIeDevice>(pcieDevice, deviceId);

  protected:
    void SetUp() override
    {
        EXPECT_EQ(pcieDevice.getName(), name);
        EXPECT_EQ(pcieDevice.getType(), "NSM_GPU_ChassisPCIeDevice");
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->deviceId, deviceId);
    }
};

TEST_F(NsmPCIeDeviceTest, goodTestRequest)
{
    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    uint8_t groupIndex = 0;
    uint8_t deviceIndex = 0;
    auto rc = decode_query_scalar_group_telemetry_v1_req(
        requestPtr, request.value().size(), &deviceIndex, &groupIndex);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(1, groupIndex);
    EXPECT_EQ(deviceId, deviceIndex);
}
TEST_F(NsmPCIeDeviceTest, badTestRequest)
{
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmPCIeDeviceTest, goodTestResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_1 data{3, 3, 3, 3, 3};
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
TEST_F(NsmPCIeDeviceTest, badTestResponseSize)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp) - 1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, nullptr, responseMsg);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmPCIeDeviceTest, badTestCompletionErrorResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_1 data{3, 3, 3, 3, 3};
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    struct nsm_query_scalar_group_telemetry_v1_resp* resp =
        (struct nsm_query_scalar_group_telemetry_v1_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

struct NsmPCIeFunctionTest : public NsmPCIeDeviceTest
{
    std::shared_ptr<NsmPCIeFunction> sensor;
    void init(uint8_t functionId)
    {
        sensor =
            std::make_shared<NsmPCIeFunction>(pcieDevice, deviceId, functionId);
        EXPECT_EQ(functionId, sensor->functionId);
    }
};

TEST_F(NsmPCIeFunctionTest, goodTestRequest)
{
    init(0);
    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    uint8_t groupIndex = 0;
    uint8_t deviceIndex = 0;
    auto rc = decode_query_scalar_group_telemetry_v1_req(
        requestPtr, request.value().size(), &deviceIndex, &groupIndex);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(0, groupIndex);
    EXPECT_EQ(deviceId, deviceIndex);
}
TEST_F(NsmPCIeFunctionTest, badTestRequest)
{
    init(0);
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmPCIeFunctionTest, goodTestResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_0 data{3, 3, 3, 3};
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
}
TEST_F(NsmPCIeFunctionTest, badTestResponseSize)
{
    init(0);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp) - 1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, nullptr, responseMsg);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmPCIeFunctionTest, badTestCompletionErrorResponse)
{
    init(0);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_0 data{3, 3, 3, 3};
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    struct nsm_query_scalar_group_telemetry_v1_resp* resp =
        (struct nsm_query_scalar_group_telemetry_v1_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

struct NsmPCIeLTSSMStateTest : public NsmPCIeDeviceTest
{
    NsmGpuChassisPCIeDevice<LTSSMStateIntf> ltssmDevice{chassisName, name};
    std::shared_ptr<NsmPCIeLTSSMState> sensor =
        std::make_shared<NsmPCIeLTSSMState>(ltssmDevice, deviceId);
};

TEST_F(NsmPCIeLTSSMStateTest, goodTestRequest)
{
    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    uint8_t groupIndex = 0;
    uint8_t deviceIndex = 0;
    auto rc = decode_query_scalar_group_telemetry_v1_req(
        requestPtr, request.value().size(), &deviceIndex, &groupIndex);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(6, groupIndex);
    EXPECT_EQ(deviceId, deviceIndex);
}
TEST_F(NsmPCIeLTSSMStateTest, badTestRequest)
{
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmPCIeLTSSMStateTest, goodTestResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_6 data{3, 3};
    auto rc = encode_query_scalar_group_telemetry_v1_group6_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
TEST_F(NsmPCIeLTSSMStateTest, badTestResponseSize)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp) - 1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group6_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, nullptr, responseMsg);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmPCIeLTSSMStateTest, badTestCompletionErrorResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_query_scalar_group_telemetry_group_6 data{3, 3};
    auto rc = encode_query_scalar_group_telemetry_v1_group6_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    struct nsm_query_scalar_group_telemetry_v1_resp* resp =
        (struct nsm_query_scalar_group_telemetry_v1_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}