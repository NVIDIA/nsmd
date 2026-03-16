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

#include "base.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmMemory.hpp"

namespace nsm
{
requester::Coroutine createNsmMemorySensor(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

using namespace nsm;

auto bus = sdbusplus::bus::new_default();
std::string sensorName("dummy_sensor");
std::string sensorType("dummy_type");
std::string inventoryObjPath("/xyz/openbmc_project/inventory/dummy_device");

TEST(nsmRowRemapState, GoodGenReq)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRowRemapState sensor(sensorName, sensorType, rowRemapIntf,
                                 inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_ROW_REMAP_STATE_FLAGS);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmRowRemapState, GoodHandleResp)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRowRemapState sensor(sensorName, sensorType, rowRemapIntf,
                                 inventoryObjPath);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remap_state_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 13;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, reason_code,
                                                 &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmRowRemapState, BadHandleResp)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRowRemapState sensor(sensorName, sensorType, rowRemapIntf,
                                 inventoryObjPath);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remap_state_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 13;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, reason_code,
                                                 &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor.handleResponseMsg(response, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(nsmRowRemappingCounts, GoodGenReq)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRowRemappingCounts sensor(sensorName, sensorType, rowRemapIntf,
                                      inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_ROW_REMAPPING_COUNTS);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmRowRemappingCounts, GoodHandleResp)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRowRemappingCounts sensor(sensorName, sensorType, rowRemapIntf,
                                      inventoryObjPath);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remapping_counts_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reason_code = ERR_NULL;
    uint32_t correctable_error = 4987;
    uint32_t uncorrectable_error = 2564;

    uint8_t rc = encode_get_row_remapping_counts_resp(
        0, NSM_SUCCESS, reason_code, correctable_error, uncorrectable_error,
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmRowRemappingCounts, BadHandleResp)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRowRemappingCounts sensor(sensorName, sensorType, rowRemapIntf,
                                      inventoryObjPath);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remapping_counts_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reason_code = ERR_NULL;
    uint32_t correctable_error = 4987;
    uint32_t uncorrectable_error = 2564;

    uint8_t rc = encode_get_row_remapping_counts_resp(
        0, NSM_SUCCESS, reason_code, correctable_error, uncorrectable_error,
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor.handleResponseMsg(response, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(nsmRemappingAvailabilityBankCount, GoodGenReq)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRemappingAvailabilityBankCount sensor(
        sensorName, sensorType, rowRemapIntf, inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_ROW_REMAP_AVAILABILITY);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmRemappingAvailabilityBankCount, GoodHandleResp)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRemappingAvailabilityBankCount sensor(
        sensorName, sensorType, rowRemapIntf, inventoryObjPath);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(struct nsm_get_row_remap_availability_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reason_code = ERR_NULL;
    struct nsm_row_remap_availability data{};
    data.high_remapping = 100;
    data.low_remapping = 200;
    data.max_remapping = 300;
    data.no_remapping = 400;
    data.partial_remapping = 500;

    uint8_t rc = encode_get_row_remap_availability_resp(
        0, NSM_SUCCESS, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmRemappingAvailabilityBankCount, BadHandleResp)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRemappingAvailabilityBankCount sensor(
        sensorName, sensorType, rowRemapIntf, inventoryObjPath);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(struct nsm_get_row_remap_availability_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reason_code = ERR_NULL;
    struct nsm_row_remap_availability data{};
    data.high_remapping = 100;
    data.low_remapping = 200;
    data.max_remapping = 300;
    data.no_remapping = 400;
    data.partial_remapping = 500;

    uint8_t rc = encode_get_row_remap_availability_resp(
        0, NSM_SUCCESS, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor.handleResponseMsg(response, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(nsmEccErrorCountsDram, GoodGenReq)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus,
                                                     inventoryObjPath.c_str());
    nsm::NsmEccErrorCountsDram eccErrorCntSensor(sensorName, sensorType,
                                                 eccIntf, inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = eccErrorCntSensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_ECC_ERROR_COUNTS);
    EXPECT_EQ(command->data_size, 0);
}

TEST(nsmEccErrorCountsDram, GoodHandleResp)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus,
                                                     inventoryObjPath.c_str());
    nsm::NsmEccErrorCountsDram sensor(sensorName, sensorType, eccIntf,
                                      inventoryObjPath);

    struct nsm_ECC_error_counts errorCounts{};
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

TEST(nsmEccErrorCountsDram, GoodUpdateReading)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus,
                                                     inventoryObjPath.c_str());
    nsm::NsmEccErrorCountsDram sensor(sensorName, sensorType, eccIntf,
                                      inventoryObjPath);
    struct nsm_ECC_error_counts errorCounts{};
    errorCounts.flags.byte = 132;
    errorCounts.sram_corrected = 1234;
    errorCounts.sram_uncorrected_secded = 4532;
    errorCounts.sram_uncorrected_parity = 6567;
    errorCounts.dram_corrected = 9876;
    errorCounts.dram_uncorrected = 9654;
    sensor.updateReading(errorCounts);
    EXPECT_EQ(sensor.eccIntf->ceCount(), errorCounts.dram_corrected);
    EXPECT_EQ(sensor.eccIntf->ueCount(), errorCounts.dram_uncorrected);
}

TEST(nsmEccErrorCountsDram, BadHandleResp)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus,
                                                     inventoryObjPath.c_str());
    nsm::NsmEccErrorCountsDram sensor(sensorName, sensorType, eccIntf,
                                      inventoryObjPath);

    struct nsm_ECC_error_counts errorCounts{};
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
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

    rc = sensor.handleResponseMsg(responseMsg, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(nsmMemCurrClockFreq, GoodGenReq)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmMemCurrClockFreq sensor(sensorName, sensorType, dimmIntf,
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

TEST(nsmMemCurrClockFreq, GoodHandleResp)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmMemCurrClockFreq sensor(sensorName, sensorType, dimmIntf,
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

TEST(nsmMemCurrClockFreq, BadHandleResp)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmMemCurrClockFreq sensor(sensorName, sensorType, dimmIntf,
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

    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor.handleResponseMsg(responseMsg, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(nsmMemCapacity, GoodGenReq)
{
    std::shared_ptr<DimmIntf> dimmIntf =
        std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmMemCapacity sensor(sensorName, sensorType, dimmIntf);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_get_inventory_information_req*>(
        msg->payload);

    EXPECT_EQ(command->hdr.command, NSM_GET_INVENTORY_INFORMATION);
    EXPECT_EQ(command->property_identifier, MAXIMUM_MEMORY_CAPACITY);
}

TEST(nsmMemCapacity, GoodHandleResponse)
{
    std::shared_ptr<DimmIntf> dimmIntf =
        std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmMemCapacity sensor(sensorName, sensorType, dimmIntf);
    std::vector<uint8_t> data{0, 0, 1, 2};
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

TEST(nsmMemCapacity, BadHandleResponse)
{
    std::shared_ptr<DimmIntf> dimmIntf =
        std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmMemCapacity sensor(sensorName, sensorType, dimmIntf);
    std::vector<uint8_t> data{0, 0, 1, 2};
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
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor.handleResponseMsg(response, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

struct NsmMemorySensorTestFixture :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Memory";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/memory";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:90";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmMemorySensorTestFixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmMemorySensorTestFixture()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmMemorySensorTestFixture, goodTestCreateMemorySensor)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("Memory_Test")},
        {"UUID", gpuUuid},
        {"MemoryType", std::string("HBM")},
        {"Count", uint64_t(8)},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = properties;

    createNsmMemorySensor(mockManager, basicIntfName, objPath);

    // Should create multiple sensors (RowRemapState, RowRemappingCounts, etc.)
    EXPECT_GE(gpu->deviceSensors.size(), 1);
}

TEST_F(NsmMemorySensorTestFixture, badTestMissingUUID)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("Memory_NoUUID")},
        {"MemoryType", std::string("HBM")},
        {"Count", uint64_t(8)},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath + "_nouuid",
                                                          basicIntfName);
    propertyMap = properties;

    EXPECT_THROW_COROUTINE(
        createNsmMemorySensor(mockManager, basicIntfName, objPath + "_nouuid"),
        std::runtime_error);
}

TEST_F(NsmMemorySensorTestFixture, testRowRemapStateUpdate)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    NsmRowRemapState sensor(sensorName, sensorType, rowRemapIntf,
                            inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remap_state_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 0x0F; // Set some flags
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, reason_code,
                                                 &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(responseMsg, Response{}));

    sensor.update(gpu);
}

TEST_F(NsmMemorySensorTestFixture, testRowRemappingCountsUpdate)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    NsmRowRemappingCounts sensor(sensorName, sensorType, rowRemapIntf,
                                 inventoryObjPath);

    std::vector<uint8_t> data(sizeof(nsm_row_remap_availability));
    auto remapData = reinterpret_cast<nsm_row_remap_availability*>(data.data());
    remapData->no_remapping = 1;
    remapData->low_remapping = 2;
    remapData->partial_remapping = 3;
    remapData->high_remapping = 4;
    remapData->max_remapping = 10;

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + data.size(), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, data.size(), data.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(responseMsg, Response{}));

    sensor.update(gpu);
}

TEST_F(NsmMemorySensorTestFixture, testMemCapacityUpdate)
{
    std::shared_ptr<DimmIntf> dimmIntf =
        std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
    NsmMemCapacity sensor(sensorName, sensorType, dimmIntf);

    // Capacity in bytes (16 GB = 16 * 1024 * 1024 * 1024)
    uint64_t capacity = 17179869184ULL;
    std::vector<uint8_t> data(sizeof(uint64_t));
    memcpy(data.data(), &capacity, sizeof(uint64_t));

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + data.size(), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, data.size(), data.data(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(responseMsg, Response{}));

    sensor.update(gpu);
}

TEST_F(NsmMemorySensorTestFixture, badTestRowRemapStateError)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    NsmRowRemapState sensor(sensorName, sensorType, rowRemapIntf,
                            inventoryObjPath);

    // Error response with reason code
    Response badResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + sizeof(uint16_t), 0);
    auto badMsg = reinterpret_cast<nsm_msg*>(badResp.data());
    auto rc = encode_reason_code(NSM_ERROR, 0x1234,
                                 NSM_GET_ROW_REMAP_STATE_FLAGS, badMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(badResp, Response{}));

    sensor.update(gpu);
}

TEST_F(NsmMemorySensorTestFixture,
       testCreateMemorySensorWithMultipleMemoryDevices)
{
    for (size_t i = 0; i < 4; i++)
    {
        std::string testPath = objPath + "_" + std::to_string(i);

        dbus::PropertyMap properties = {
            {"Name", std::string("Memory_") + std::to_string(i)},
            {"UUID", gpuUuid},
            {"MemoryType", std::string("HBM")},
            {"Count", uint64_t(8)},
        };

        auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                              basicIntfName);
        propertyMap = properties;

        createNsmMemorySensor(mockManager, basicIntfName, testPath);
    }

    // Should have created memory sensors (actual number depends on
    // configuration)
    EXPECT_GE(gpu->deviceSensors.size(), 1);
}

// ============================================================================
// addSensor<T> instantiation coverage
// ============================================================================

TEST_F(NsmMemorySensorTestFixture, AddSensorNsmMemCurrClockFreq)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/inventory/test/mem_freq");
    std::string testName = "MemCurrClockFreq_AS";
    std::string testType = "NSM_Memory";
    auto sensor = std::make_shared<NsmMemCurrClockFreq>(
        testName, testType, dimmIntf,
        "/xyz/openbmc_project/inventory/test/mem_freq");
    size_t before = gpu->deviceSensors.size();
    gpu->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

TEST_F(NsmMemorySensorTestFixture, AddSensorNsmMemCapacity)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/inventory/test/mem_cap");
    std::string testName = "MemCapacity_AS";
    std::string testType = "NSM_Memory";
    auto sensor = std::make_shared<NsmMemCapacity>(testName, testType,
                                                   dimmIntf);
    size_t before = gpu->deviceSensors.size();
    gpu->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

TEST_F(NsmMemorySensorTestFixture, AddSensorNsmMinMemoryClockLimit)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/inventory/test/mem_min_clk");
    std::string testName = "MinMemClock_AS";
    std::string testType = "NSM_Memory";
    auto sensor = std::make_shared<NsmMinMemoryClockLimit>(testName, testType,
                                                           dimmIntf);
    size_t before = gpu->deviceSensors.size();
    gpu->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

TEST_F(NsmMemorySensorTestFixture, AddSensorNsmMaxMemoryClockLimit)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/inventory/test/mem_max_clk");
    std::string testName = "MaxMemClock_AS";
    std::string testType = "NSM_Memory";
    auto sensor = std::make_shared<NsmMaxMemoryClockLimit>(testName, testType,
                                                           dimmIntf);
    size_t before = gpu->deviceSensors.size();
    gpu->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}
