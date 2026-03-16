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

#include "../../test/commonMock.hpp"
#include "../../test/mockSensorManager.hpp"
#include "../nsmCommon.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();
std::string sensorName("dummy_sensor");
std::string sensorType("dummy_type");
std::string inventoryObjPath("/xyz/openbmc_project/inventory/dummy_device");

TEST(nsmMemCapacityUtil, GoodGenReq)
{
    const uuid_t gpuUuid = "STATIC:0:0:MCTP_EID:28";
    std::shared_ptr<MockNsmDevice> gpuPtr =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);
    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(sensorName,
                                                              sensorType);

    auto provider = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        sensorName, sensorType, dbus::Interfaces{inventoryObjPath});
    NsmMemoryCapacityUtil sensor(provider, totalMemorySensor, false, gpuPtr);

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
    const uuid_t gpuUuid = "STATIC:0:0:MCTP_EID:28";
    std::shared_ptr<MockNsmDevice> gpuPtr =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);
    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(sensorName,
                                                              sensorType);

    // First, set total memory capacity so updateReading can compute percentage
    std::vector<uint8_t> memResponseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto memResponse = reinterpret_cast<nsm_msg*>(memResponseMsg.data());
    uint16_t reason_code = ERR_NULL;
    uint32_t testCapacity = 1000; // 1000 MiB
    std::vector<uint8_t> memData(4, 0);
    std::memcpy(memData.data(), &testCapacity, sizeof(testCapacity));
    uint8_t rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, reason_code, sizeof(testCapacity), memData.data(),
        memResponse);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    totalMemorySensor->handleResponseMsg(memResponse, memResponseMsg.size());

    auto provider = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        sensorName, sensorType, dbus::Interfaces{inventoryObjPath});
    NsmMemoryCapacityUtil sensor(provider, totalMemorySensor, false, gpuPtr);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_memory_capacity_util_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    struct nsm_memory_capacity_utilization data{};
    data.reserved_memory = 100;
    data.used_memory = 50;

    rc = encode_get_memory_capacity_util_resp(0, NSM_SUCCESS, reason_code,
                                              &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmMemCapacityUtil, BadHandleResp)
{
    const uuid_t gpuUuid = "STATIC:0:0:MCTP_EID:28";
    std::shared_ptr<MockNsmDevice> gpuPtr =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);
    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(sensorName,
                                                              sensorType);

    auto provider = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        sensorName, sensorType, dbus::Interfaces{inventoryObjPath});
    NsmMemoryCapacityUtil sensor(provider, totalMemorySensor, false, gpuPtr);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_memory_capacity_util_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint16_t reason_code = ERR_NULL;
    struct nsm_memory_capacity_utilization data{};
    data.reserved_memory = 100;
    data.used_memory = 50;

    uint8_t rc = encode_get_memory_capacity_util_resp(
        0, NSM_SUCCESS, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor.handleResponseMsg(response, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(NsmTotalMemory, GoodHandleResponseAndGetReading)
{
    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(sensorName,
                                                              sensorType);

    // Create a response message with valid memory capacity
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reason_code = ERR_NULL;
    uint32_t testCapacity = 16384; // 16 GB in MiB
    std::vector<uint8_t> data(4, 0);
    std::memcpy(data.data(), &testCapacity, sizeof(testCapacity));

    uint8_t rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, reason_code, sizeof(testCapacity), data.data(),
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Handle the response to trigger updateReading
    rc = totalMemorySensor->handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Verify getReading returns the set value
    auto reading = totalMemorySensor->getReading();
    EXPECT_TRUE(reading.has_value());
    EXPECT_EQ(reading.value(), testCapacity);
}

TEST(NsmTotalMemory, BadHandleResponseReturnsNullopt)
{
    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(sensorName,
                                                              sensorType);

    // Create a response message with error status
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reason_code = ERR_NULL;
    uint32_t testCapacity = 16384;
    std::vector<uint8_t> data(4, 0);
    std::memcpy(data.data(), &testCapacity, sizeof(testCapacity));

    // Encode with error completion code
    uint8_t rc = encode_get_inventory_information_resp(
        0, NSM_ERROR, reason_code, sizeof(testCapacity), data.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Handle the error response
    rc = totalMemorySensor->handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);

    // Verify getReading returns nullopt after error
    auto reading = totalMemorySensor->getReading();
    EXPECT_FALSE(reading.has_value());
}

TEST(NsmMemoryCapacityUtil, EdgeCaseZeroTotalMemory)
{
    const uuid_t gpuUuid = "STATIC:0:0:MCTP_EID:28";
    std::shared_ptr<MockNsmDevice> gpuPtr =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);
    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(sensorName,
                                                              sensorType);

    // Set total memory capacity to 0 by handling response with 0 value
    std::vector<uint8_t> memResponseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto memResponse = reinterpret_cast<nsm_msg*>(memResponseMsg.data());
    uint16_t reason_code = ERR_NULL;
    uint32_t zeroCapacity = 0;
    std::vector<uint8_t> memData(4, 0);
    std::memcpy(memData.data(), &zeroCapacity, sizeof(zeroCapacity));
    uint8_t rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, reason_code, sizeof(zeroCapacity), memData.data(),
        memResponse);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    totalMemorySensor->handleResponseMsg(memResponse, memResponseMsg.size());

    auto provider = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        sensorName, sensorType, dbus::Interfaces{inventoryObjPath});
    NsmMemoryCapacityUtil sensor(provider, totalMemorySensor, false, gpuPtr);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_memory_capacity_util_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    struct nsm_memory_capacity_utilization data{};
    data.reserved_memory = 100;
    data.used_memory = 50;

    rc = encode_get_memory_capacity_util_resp(0, NSM_SUCCESS, reason_code,
                                              &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // When total memory is 0, updateReading should return early without
    // updating the metric (covers lines 146-148)
}

TEST(NsmMemoryCapacityUtil, EdgeCaseNulloptTotalMemory)
{
    const uuid_t gpuUuid = "STATIC:0:0:MCTP_EID:28";
    std::shared_ptr<MockNsmDevice> gpuPtr =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);
    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(sensorName,
                                                              sensorType);

    // totalMemorySensor starts with nullopt by default (no handleResponseMsg
    // called)

    auto provider = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        sensorName, sensorType, dbus::Interfaces{inventoryObjPath});
    NsmMemoryCapacityUtil sensor(provider, totalMemorySensor, false, gpuPtr);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_memory_capacity_util_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reason_code = ERR_NULL;
    struct nsm_memory_capacity_utilization data{};
    data.reserved_memory = 100;
    data.used_memory = 50;

    uint8_t rc = encode_get_memory_capacity_util_resp(
        0, NSM_SUCCESS, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // When total memory is nullopt, updateReading should return early without
    // updating the metric (covers lines 142-144)
}

TEST(NsmMinGraphicsClockLimit, ConstructorCreatesObject)
{
    std::string name = sensorName;
    std::string type = sensorType;
    std::string path = inventoryObjPath;

    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(bus,
                                                                  path.c_str());

    NsmMinGraphicsClockLimit sensor(name, type, cpuConfigIntf, path);

    EXPECT_EQ(sensor.getName(), name);
    EXPECT_EQ(sensor.getType(), type);
}

TEST(NsmMaxGraphicsClockLimit, ConstructorCreatesObject)
{
    std::string name = sensorName;
    std::string type = sensorType;
    std::string path = inventoryObjPath;

    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(bus,
                                                                  path.c_str());

    NsmMaxGraphicsClockLimit sensor(name, type, cpuConfigIntf, path);

    EXPECT_EQ(sensor.getName(), name);
    EXPECT_EQ(sensor.getType(), type);
}
