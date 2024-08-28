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

#include "../nsmCommon.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();
std::string sensorName("dummy_sensor");
std::string sensorType("dummy_type");
std::string inventoryObjPath("/xyz/openbmc_project/inventory/dummy_device");

TEST(nsmMemCapacityUtil, GoodGenReq)
{
    auto totalMemorySensor =
        std::make_shared<NsmTotalMemory>(sensorName, sensorType);
    nsm::NsmMemoryCapacityUtil sensor(bus, sensorName, sensorType,
                                      inventoryObjPath, totalMemorySensor,
                                      false);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_MEMORY_CAPACITY_UTILIZATION);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmMemCapacityUtil, GoodHandleResp)
{
    auto totalMemorySensor =
        std::make_shared<NsmTotalMemory>(sensorName, sensorType);
    nsm::NsmMemoryCapacityUtil sensor(bus, sensorName, sensorType,
                                      inventoryObjPath, totalMemorySensor,
                                      false);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_memory_capacity_util_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reason_code = ERR_NULL;
    struct nsm_memory_capacity_utilization data;
    data.reserved_memory = 100;
    data.used_memory = 50;

    uint8_t rc = encode_get_memory_capacity_util_resp(
        0, NSM_SUCCESS, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmMemCapacityUtil, BadHandleResp)
{
    auto totalMemorySensor =
        std::make_shared<NsmTotalMemory>(sensorName, sensorType);
    nsm::NsmMemoryCapacityUtil sensor(bus, sensorName, sensorType,
                                      inventoryObjPath, totalMemorySensor,
                                      false);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_memory_capacity_util_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint16_t reason_code = ERR_NULL;
    struct nsm_memory_capacity_utilization data;
    data.reserved_memory = 100;
    data.used_memory = 50;

    uint8_t rc = encode_get_memory_capacity_util_resp(
        0, NSM_SUCCESS, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
    rc = sensor.handleResponseMsg(response, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}