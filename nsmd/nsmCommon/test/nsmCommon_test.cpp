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

TEST(nsmMemCapacityUtil, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    const uuid_t gpuUuid = "STATIC:0:0:MCTP_EID:28";
    std::shared_ptr<MockNsmDevice> gpuPtr =
        std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid, 1);
    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(sensorName,
                                                              sensorType);

    auto provider = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        sensorName, sensorType, dbus::Interfaces{inventoryObjPath});
    NsmMemoryCapacityUtil sensor(provider, totalMemorySensor, false, gpuPtr);

    // instanceId > NSM_INSTANCE_MAX causes encode to fail → return nullopt
    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
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

// NsmMemoryCapacityUtil::handleResponseMsg with isLongRunning=true exercises
// the decode_get_memory_capacity_util_event_resp branch (line 187 in
// nsmCommon.cpp).
TEST(nsmMemCapacityUtil, GoodHandleResp_LongRunning)
{
    const uuid_t gpuUuid = "STATIC:0:0:MCTP_EID:28";
    auto gpuPtr = std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid,
                                                  1);
    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(sensorName,
                                                              sensorType);

    // Set total memory capacity to 1000 MiB
    {
        std::vector<uint8_t> memResponseMsg(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
                4,
            0);
        auto memResponse = reinterpret_cast<nsm_msg*>(memResponseMsg.data());
        uint16_t reason_code = ERR_NULL;
        uint32_t testCapacity = 1000;
        std::vector<uint8_t> memData(4, 0);
        std::memcpy(memData.data(), &testCapacity, sizeof(testCapacity));
        encode_get_inventory_information_resp(0, NSM_SUCCESS, reason_code,
                                              sizeof(testCapacity),
                                              memData.data(), memResponse);
        totalMemorySensor->handleResponseMsg(memResponse,
                                             memResponseMsg.size());
    }

    // isLongRunning=true → uses decode_get_memory_capacity_util_event_resp
    auto provider = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        sensorName, sensorType, dbus::Interfaces{inventoryObjPath});
    NsmMemoryCapacityUtil sensor(provider, totalMemorySensor, true, gpuPtr);

    struct nsm_memory_capacity_utilization data{};
    data.reserved_memory = 100;
    data.used_memory = 50;

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
            sizeof(struct nsm_long_running_resp) +
            sizeof(struct nsm_memory_capacity_utilization),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_memory_capacity_util_event_resp(
        0, NSM_SUCCESS, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// NsmMemoryCapacityUtil::equals: two sensors sharing the same totalMemory
// compare equal because genRequestMsg output is identical; comparing with a
// different sensor type returns false (dynamic_cast to NsmMemoryCapacityUtil
// fails).
TEST(NsmMemoryCapacityUtil, Equals_SameTotalMem_ReturnsTrue)
{
    const uuid_t gpuUuid = "STATIC:0:0:MCTP_EID:28";
    auto gpuPtr = std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid,
                                                  1);
    auto totalMem = std::make_shared<NsmTotalMemory>(sensorName, sensorType);

    std::string path1 = inventoryObjPath + "/eq1";
    std::string path2 = inventoryObjPath + "/eq2";
    auto provider1 = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        sensorName, sensorType, dbus::Interfaces{path1});
    auto provider2 = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        sensorName, sensorType, dbus::Interfaces{path2});
    NsmMemoryCapacityUtil sensor1(provider1, totalMem, false, gpuPtr);
    NsmMemoryCapacityUtil sensor2(provider2, totalMem, false, gpuPtr);

    // Both sensors send identical requests → equals() == true
    EXPECT_TRUE(sensor1.equals(sensor2));
}

TEST(NsmMemoryCapacityUtil, Equals_DifferentType_ReturnsFalse)
{
    const uuid_t gpuUuid = "STATIC:0:0:MCTP_EID:28";
    auto gpuPtr = std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid,
                                                  1);
    auto totalMem = std::make_shared<NsmTotalMemory>(sensorName, sensorType);

    std::string path1 = inventoryObjPath + "/eq3";
    auto provider = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        sensorName, sensorType, dbus::Interfaces{path1});
    NsmMemoryCapacityUtil sensor(provider, totalMem, false, gpuPtr);

    // NsmTotalMemory is not NsmMemoryCapacityUtil → equals() == false
    EXPECT_FALSE(sensor.equals(*totalMem));
}

// Error-CC path: decode succeeds but cc = NSM_ERROR (non-zero)
// → return cc ? cc : rc; takes the true branch (return cc).
TEST(nsmMemCapacityUtil, HandleResponseMsg_ErrorCC_CoversBranch)
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
    struct nsm_memory_capacity_utilization data{};
    uint8_t rc = encode_get_memory_capacity_util_resp(0, NSM_ERROR, ERR_NULL,
                                                      &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
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

// Fixture for testing the update() coroutines of NsmMinGraphicsClockLimit and
// NsmMaxGraphicsClockLimit.  Inherits SensorManagerTest to get mockSensorIO.
struct NsmClockLimitUpdateTest : public testing::Test, public SensorManagerTest
{
    NsmDeviceTable devices;
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    std::shared_ptr<MockNsmDevice> nsmDevice;

    NsmClockLimitUpdateTest() : SensorManagerTest(devices) {}

    ~NsmClockLimitUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        nsmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        ASSERT_NE(nsmDevice, nullptr);
    }
};

// Error-CC path: update coroutine – sensorIO succeeds but cc = NSM_ERROR
// → co_return cc ? cc : rc; takes the true branch.
TEST_F(NsmClockLimitUpdateTest,
       MinGraphicsClockLimit_Update_ErrorCC_CoversBranch)
{
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
    NsmMinGraphicsClockLimit sensor(sensorName, sensorType, cpuConfigIntf,
                                    inventoryObjPath);

    uint32_t dummy = 0;
    uint16_t dataSize = sizeof(dummy);
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + dataSize, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_inventory_information_resp(
        0, NSM_ERROR, ERR_NULL, dataSize,
        reinterpret_cast<const uint8_t*>(&dummy), response);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    sensor.update(nsmDevice);
    // cc = NSM_ERROR → true branch of co_return cc ? cc : rc //
}

// Error-CC path: update coroutine – sensorIO succeeds but cc = NSM_ERROR
// → co_return cc ? cc : rc; takes the true branch.
TEST_F(NsmClockLimitUpdateTest,
       MaxGraphicsClockLimit_Update_ErrorCC_CoversBranch)
{
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
    NsmMaxGraphicsClockLimit sensor(sensorName, sensorType, cpuConfigIntf,
                                    inventoryObjPath);

    uint32_t dummy = 0;
    uint16_t dataSize = sizeof(dummy);
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + dataSize, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_inventory_information_resp(
        0, NSM_ERROR, ERR_NULL, dataSize,
        reinterpret_cast<const uint8_t*>(&dummy), response);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    sensor.update(nsmDevice);
    // cc = NSM_ERROR → true branch of co_return cc ? cc : rc //
}

// Success path: update coroutine – sensorIO delivers cc = NSM_SUCCESS
// → co_return cc ? cc : rc; takes the false branch (returns rc). //

TEST_F(NsmClockLimitUpdateTest,
       MinGraphicsClockLimit_Update_Success_CoversFalseBranch)
{
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
    NsmMinGraphicsClockLimit sensor(sensorName, sensorType, cpuConfigIntf,
                                    inventoryObjPath);

    uint32_t dummy = 0;
    uint16_t dataSize = sizeof(dummy);
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + dataSize, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, dataSize,
        reinterpret_cast<const uint8_t*>(&dummy), response);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    sensor.update(nsmDevice);
    // cc = NSM_SUCCESS → false branch of co_return cc ? cc : rc //
}

// Success path: update coroutine – sensorIO delivers cc = NSM_SUCCESS
// → co_return cc ? cc : rc; takes the false branch (returns rc). //

TEST_F(NsmClockLimitUpdateTest,
       MaxGraphicsClockLimit_Update_Success_CoversFalseBranch)
{
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
    NsmMaxGraphicsClockLimit sensor(sensorName, sensorType, cpuConfigIntf,
                                    inventoryObjPath);

    uint32_t dummy = 0;
    uint16_t dataSize = sizeof(dummy);
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + dataSize, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, dataSize,
        reinterpret_cast<const uint8_t*>(&dummy), response);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseData));

    sensor.update(nsmDevice);
    // cc = NSM_SUCCESS → false branch of co_return cc ? cc : rc //
}

// ============================================================================
// sensorIO failure path: rc != 0 → if (rc) TRUE branch in update() coroutine.
// Previously uncovered for NsmMinGraphicsClockLimit and
// NsmMaxGraphicsClockLimit.
// ============================================================================

TEST_F(NsmClockLimitUpdateTest,
       MinGraphicsClockLimit_Update_SensorIOFail_CoReturnsRc)
{
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
    NsmMinGraphicsClockLimit sensor(sensorName, sensorType, cpuConfigIntf,
                                    inventoryObjPath);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor.update(nsmDevice);
    // rc = NSM_ERROR → if (rc) TRUE branch → co_return rc
}

TEST_F(NsmClockLimitUpdateTest,
       MaxGraphicsClockLimit_Update_SensorIOFail_CoReturnsRc)
{
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
    NsmMaxGraphicsClockLimit sensor(sensorName, sensorType, cpuConfigIntf,
                                    inventoryObjPath);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor.update(nsmDevice);
    // rc = NSM_ERROR → if (rc) TRUE branch → co_return rc
}

// ============================================================================
// getDeviceUUID branch coverage tests
// mctpDiscovery is [[maybe_unused]] in getDeviceUUID - use aligned placeholder
// ============================================================================

// Fake MctpDiscovery reference helper (parameter is never accessed)
static mctp::MctpDiscovery& fakeMctpDiscovery()
{
    alignas(alignof(void*)) static char buf[8192] = {};
    return *reinterpret_cast<mctp::MctpDiscovery*>(buf);
}

static std::shared_ptr<MockNsmDevice>
    makeDeviceWithMctpUuid(eid_t eid, const uuid_t& mctpUuid)
{
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_UUID", mctpUuid, 0);
    dev->eid = eid;
    return dev;
}

static std::vector<uint8_t> makeInventoryInfoNonSuccessResp(uint8_t cc)
{
    std::vector<uint8_t> respBuf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto resp = reinterpret_cast<nsm_msg*>(respBuf.data());
    auto rc = encode_get_inventory_information_resp(0, cc, ERR_NULL, 0, nullptr,
                                                    resp);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    return respBuf;
}

// null nsmDevice → if (nsmDevice) false → co_return NSM_SW_ERROR_COMMAND_FAIL
//
TEST(GetDeviceUUID, NullDevice_ReturnsCommandFail)
{
    uuid_t uuid;
    nsm::getDeviceUUID(nullptr, fakeMctpDiscovery(), uuid);
    // Does not throw; hits the co_return at line 527
}

// Baseboard device (deviceType == NSM_DEV_ID_BASEBOARD) →
// uses mctp uuid directly, co_return NSM_SW_SUCCESS
TEST_F(NsmClockLimitUpdateTest, BaseboardDevice_AssignsMctpUuid)
{
    // STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0 →
    // deviceType=3=NSM_DEV_ID_BASEBOARD
    const uuid_t baseboardUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto baseboard = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(baseboardUuid));
    ASSERT_NE(baseboard, nullptr);
    EXPECT_EQ(baseboard->getDeviceType(), NSM_DEV_ID_BASEBOARD);

    uuid_t outUuid;
    nsm::getDeviceUUID(baseboard, fakeMctpDiscovery(), outUuid);
    // Hits lines 413-418; assigns mctp uuid
    EXPECT_EQ(outUuid, baseboard->getUuid());
}

// sensorIO fails with NSM_ERR_UNSUPPORTED_COMMAND_CODE →
// fallback to mctp uuid, co_return NSM_SW_SUCCESS
TEST_F(NsmClockLimitUpdateTest,
       GetDeviceUUID_SensorIO_Unsupported_FallsBackToMctpUuid)
{
    // Use EID 200 (unique to avoid static logger state interference)
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "200", 0);
    EXPECT_CALL(*dev, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    uuid_t outUuid;
    nsm::getDeviceUUID(dev, fakeMctpDiscovery(), outUuid);
    // Hits lines 446-455; assigns mctp uuid
    EXPECT_EQ(outUuid, dev->getUuid());
}

// sensorIO fails with generic error (not UNSUPPORTED_COMMAND_CODE) →
// fallback to mctp uuid, co_return NSM_SW_SUCCESS.
TEST_F(NsmClockLimitUpdateTest,
       GetDeviceUUID_SensorIO_OtherError_FallsBackToMctpUuid)
{
    const uuid_t mctpUuid = "22222222-3333-4444-5555-666666666666";
    auto dev = makeDeviceWithMctpUuid(201, mctpUuid);
    EXPECT_CALL(*dev, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    uuid_t outUuid;
    auto rc = nsm::getDeviceUUID(dev, fakeMctpDiscovery(), outUuid);

    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(outUuid, mctpUuid);
    EXPECT_EQ(dev->deviceUuid, mctpUuid);
}

// sensorIO succeeds; decode response with cc=NSM_ERROR → fallback to mctp uuid.
TEST_F(NsmClockLimitUpdateTest,
       GetDeviceUUID_DecodeResp_ErrorCC_FallsBackToMctpUuid)
{
    const uuid_t mctpUuid = "33333333-4444-5555-6666-777777777777";
    auto dev = makeDeviceWithMctpUuid(202, mctpUuid);
    auto respBuf = makeInventoryInfoNonSuccessResp(NSM_ERROR);

    EXPECT_CALL(*dev, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(respBuf));

    uuid_t outUuid;
    auto rc = nsm::getDeviceUUID(dev, fakeMctpDiscovery(), outUuid);

    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(outUuid, mctpUuid);
    EXPECT_EQ(dev->deviceUuid, mctpUuid);
}

// sensorIO succeeds but decode fails due to an empty response → fallback to
// mctp uuid, co_return NSM_SW_SUCCESS.
TEST_F(NsmClockLimitUpdateTest,
       GetDeviceUUID_DecodeResp_Failure_FallsBackToMctpUuid)
{
    const uuid_t mctpUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    auto dev = makeDeviceWithMctpUuid(212, mctpUuid);

    EXPECT_CALL(*dev, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(Response()));

    uuid_t outUuid;
    auto rc = nsm::getDeviceUUID(dev, fakeMctpDiscovery(), outUuid);

    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(outUuid, mctpUuid);
    EXPECT_EQ(dev->deviceUuid, mctpUuid);
}

// sensorIO succeeds; decode response with cc=NSM_SUCCESS, dataSize < 16 →
// fallback to mctp uuid.
TEST_F(NsmClockLimitUpdateTest,
       GetDeviceUUID_CcSuccess_DataSizeTooSmall_FallsBackToMctpUuid)
{
    const uuid_t mctpUuid = "44444444-5555-6666-7777-888888888888";
    auto dev = makeDeviceWithMctpUuid(203, mctpUuid);

    // Encode response with cc=NSM_SUCCESS but only 4 bytes of data (<
    // UUID_INT_SIZE=16)
    std::vector<uint8_t> data(4, 0xAA);
    std::vector<uint8_t> respBuf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto resp = reinterpret_cast<nsm_msg*>(respBuf.data());
    auto encodeRc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, 4, data.data(), resp);
    EXPECT_EQ(encodeRc, NSM_SW_SUCCESS);

    EXPECT_CALL(*dev, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(respBuf));

    uuid_t outUuid;
    auto rc = nsm::getDeviceUUID(dev, fakeMctpDiscovery(), outUuid);

    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(outUuid, mctpUuid);
    EXPECT_EQ(dev->deviceUuid, mctpUuid);
}

// sensorIO succeeds; decode response with cc=NSM_SUCCESS, dataSize > 16 →
// fallback to mctp uuid.
TEST_F(NsmClockLimitUpdateTest,
       GetDeviceUUID_CcSuccess_DataSizeTooLarge_FallsBackToMctpUuid)
{
    const uuid_t mctpUuid = "55555555-6666-7777-8888-999999999999";
    auto dev = makeDeviceWithMctpUuid(211, mctpUuid);

    std::vector<uint8_t> data(UUID_INT_SIZE + 1, 0xAA);
    std::vector<uint8_t> respBuf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            data.size(),
        0);
    auto resp = reinterpret_cast<nsm_msg*>(respBuf.data());
    auto encodeRc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, static_cast<uint16_t>(data.size()),
        data.data(), resp);
    EXPECT_EQ(encodeRc, NSM_SW_SUCCESS);

    EXPECT_CALL(*dev, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(respBuf));

    uuid_t outUuid;
    auto rc = nsm::getDeviceUUID(dev, fakeMctpDiscovery(), outUuid);

    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(outUuid, mctpUuid);
    EXPECT_EQ(dev->deviceUuid, mctpUuid);
}

// sensorIO succeeds; cc=NSM_SUCCESS, dataSize == 16, valid UUID bytes →
// assigns uuid and co_return NSM_SW_SUCCESS
TEST_F(NsmClockLimitUpdateTest, GetDeviceUUID_CcSuccess_ValidUuid_AssignsUuid)
{
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "204", 0);

    // Encode 16 bytes of UUID data with NSM_SUCCESS
    std::vector<uint8_t> uuidData = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                     0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
                                     0x0d, 0x0e, 0x0f, 0x10};
    std::vector<uint8_t> respBuf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 16,
        0);
    auto resp = reinterpret_cast<nsm_msg*>(respBuf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, 16,
                                          uuidData.data(), resp);

    EXPECT_CALL(*dev, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(respBuf));

    uuid_t outUuid;
    nsm::getDeviceUUID(dev, fakeMctpDiscovery(), outUuid);
    // shouldLog=false, cc=NSM_SUCCESS, dataSize=16 → lines 483-507
    EXPECT_FALSE(outUuid.empty());
    EXPECT_EQ(dev->deviceUuid, outUuid);
}

// cc=NSM_ERR_INVALID_DATA means DEVICE_GUID is unavailable → fallback to mctp
// uuid on the first call.
TEST_F(NsmClockLimitUpdateTest, GetDeviceUUID_CcInvalidData_FallsBackToMctpUuid)
{
    const uuid_t mctpUuid = "77777777-8888-9999-aaaa-bbbbbbbbbbbb";
    auto dev = makeDeviceWithMctpUuid(205, mctpUuid);
    auto respBuf = makeInventoryInfoNonSuccessResp(NSM_ERR_INVALID_DATA);

    EXPECT_CALL(*dev, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(respBuf));

    uuid_t outUuid;
    auto rc = nsm::getDeviceUUID(dev, fakeMctpDiscovery(), outUuid);

    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(outUuid, mctpUuid);
    EXPECT_EQ(dev->deviceUuid, mctpUuid);
}

// NsmMemoryCapacity::genRequestMsg (exercised via NsmTotalMemory which inherits
// it) – instanceId > NSM_INSTANCE_MAX causes
// encode_get_inventory_information_req to fail → returns nullopt.  Covers lines
// 54-60 of nsmCommon.cpp.
TEST(NsmTotalMemory, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    NsmTotalMemory sensor(sensorName, sensorType);
    auto request = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// Repeated non-success CCs should still fallback every time; logger state must
// not affect UUID selection.
TEST_F(NsmClockLimitUpdateTest,
       GetDeviceUUID_RepeatedOtherCC_FallsBackToMctpUuid)
{
    const uuid_t mctpUuid = "88888888-9999-aaaa-bbbb-cccccccccccc";
    auto dev = makeDeviceWithMctpUuid(210, mctpUuid);

    uuid_t outUuid;

    EXPECT_CALL(*dev, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeInventoryInfoNonSuccessResp(NSM_ERROR)));
    auto firstRc = nsm::getDeviceUUID(dev, fakeMctpDiscovery(), outUuid);
    EXPECT_EQ(firstRc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(outUuid, mctpUuid);

    EXPECT_CALL(*dev, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeInventoryInfoNonSuccessResp(NSM_ERROR)));
    auto secondRc = nsm::getDeviceUUID(dev, fakeMctpDiscovery(), outUuid);
    EXPECT_EQ(secondRc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(outUuid, mctpUuid);
    EXPECT_EQ(dev->deviceUuid, mctpUuid);
}

// NsmMemoryCapacityUtil::update() – lines 204-211 of nsmCommon.cpp.
// Delegates to totalMemory->update() (1st sensorIO) then
// NsmLongRunningSensor::update() (2nd sensorIO). Covers the co_await chain. //

TEST_F(NsmClockLimitUpdateTest,
       MemoryCapacityUtil_Update_CallsBothSubCoroutines)
{
    // Inventory-information response for totalMemory->update()
    uint32_t testCapacity = 1000;
    std::vector<uint8_t> memData(4, 0);
    std::memcpy(memData.data(), &testCapacity, sizeof(testCapacity));
    std::vector<uint8_t> memRespBuf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) + 4,
        0);
    auto memResp = reinterpret_cast<nsm_msg*>(memRespBuf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          sizeof(testCapacity), memData.data(),
                                          memResp);

    // Memory-capacity-util response for NsmLongRunningSensor::update()
    struct nsm_memory_capacity_utilization utilData{};
    utilData.reserved_memory = 100;
    utilData.used_memory = 50;
    std::vector<uint8_t> utilRespBuf(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_memory_capacity_util_resp),
        0);
    auto utilResp = reinterpret_cast<nsm_msg*>(utilRespBuf.data());
    encode_get_memory_capacity_util_resp(0, NSM_SUCCESS, ERR_NULL, &utilData,
                                         utilResp);

    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(sensorName,
                                                              sensorType);
    auto provider = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        sensorName, sensorType, dbus::Interfaces{inventoryObjPath});
    // Sensor must be a shared_ptr because update() calls shared_from_this()
    auto sensor = std::make_shared<NsmMemoryCapacityUtil>(
        provider, totalMemorySensor, false, nsmDevice);

    EXPECT_CALL(*nsmDevice, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(memRespBuf))   // totalMemory->update()
        .WillOnce(mockSensorIO(utilRespBuf)); // NsmLongRunningSensor::update()

    // update() is private in NsmMemoryCapacityUtil; call through public base
    static_cast<NsmSensor*>(sensor.get())->update(nsmDevice);
}

// NsmTotalMemory::handleResponseMsg: rc==NSM_SW_SUCCESS, cc!=NSM_SUCCESS.
// 9-byte buffer: decode_reason_code_and_cc returns NSM_SW_SUCCESS with
// cc=NSM_ERROR → "rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS" is FALSE →
// updateReading(nullopt) is called.
TEST(NsmTotalMemory, HandleResponseMsg_DecodeSuccessNonZeroCC_ReturnsNullopt)
{
    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(sensorName,
                                                              sensorType);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code
    auto response = reinterpret_cast<nsm_msg*>(buf.data());

    uint8_t rc = totalMemorySensor->handleResponseMsg(response, buf.size());

    EXPECT_NE(rc, NSM_SUCCESS);
    EXPECT_FALSE(totalMemorySensor->getReading().has_value());
}

// NsmMemoryCapacityUtil::handleResponseMsg: rc==NSM_SW_SUCCESS,
// cc!=NSM_SUCCESS. 9-byte buffer: decode_reason_code_and_cc returns
// NSM_SW_SUCCESS with cc=NSM_ERROR → "rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS" is
// FALSE.
TEST(NsmMemoryCapacityUtil,
     HandleResponseMsg_DecodeSuccessNonZeroCC_ReturnsError)
{
    const uuid_t gpuUuid = "STATIC:0:0:MCTP_EID:28";
    auto gpuPtr = std::make_shared<MockNsmDevice>(1, 1, "MCTP_UUID", gpuUuid,
                                                  1);
    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(sensorName,
                                                              sensorType);
    auto provider = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        sensorName, sensorType, dbus::Interfaces{inventoryObjPath});
    NsmMemoryCapacityUtil sensor(provider, totalMemorySensor, false, gpuPtr);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code
    auto response = reinterpret_cast<nsm_msg*>(buf.data());

    uint8_t rc = sensor.handleResponseMsg(response, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}
