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

#include "interfaceWrapper.hpp"
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

TEST(nsmRowRemapState, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRowRemapState sensor(sensorName, sensorType, rowRemapIntf,
                                 inventoryObjPath);
    // instanceId > NSM_INSTANCE_MAX causes encode to fail → return nullopt
    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
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

TEST(nsmRowRemappingCounts, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRowRemappingCounts sensor(sensorName, sensorType, rowRemapIntf,
                                      inventoryObjPath);
    // instanceId > NSM_INSTANCE_MAX causes encode to fail → return nullopt
    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
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

TEST(nsmRemappingAvailabilityBankCount,
     BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRemappingAvailabilityBankCount sensor(
        sensorName, sensorType, rowRemapIntf, inventoryObjPath);
    // instanceId > NSM_INSTANCE_MAX causes encode to fail → return nullopt
    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
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

TEST(nsmEccErrorCountsDram, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus,
                                                     inventoryObjPath.c_str());
    nsm::NsmEccErrorCountsDram sensor(sensorName, sensorType, eccIntf,
                                      inventoryObjPath);
    // instanceId > NSM_INSTANCE_MAX causes encode to fail → return nullopt
    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
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

TEST(nsmMemCurrClockFreq, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmMemCurrClockFreq sensor(sensorName, sensorType, dimmIntf,
                                    inventoryObjPath);
    // instanceId > NSM_INSTANCE_MAX causes encode to fail → return nullopt
    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
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

// Error-CC paths: handleResponseMsg – encode response with cc=NSM_ERROR so
// decode returns NSM_SW_SUCCESS but cc != 0, covering the true branch of
// return cc ? cc : rc.
// ============================================================================

TEST(nsmRowRemapState, HandleResponseMsg_ErrorCC_CoversBranch)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRowRemapState sensor(sensorName, sensorType, rowRemapIntf,
                                 inventoryObjPath);

    std::vector<uint8_t> response(256, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    bitfield8_t flags = {};
    auto encRc = encode_get_row_remap_state_resp(0, NSM_ERROR, ERR_NULL, &flags,
                                                 responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(nsmRowRemappingCounts, HandleResponseMsg_ErrorCC_CoversBranch)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRowRemappingCounts sensor(sensorName, sensorType, rowRemapIntf,
                                      inventoryObjPath);

    std::vector<uint8_t> response(256, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto encRc = encode_get_row_remapping_counts_resp(0, NSM_ERROR, ERR_NULL, 0,
                                                      0, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(nsmRemappingAvailabilityBankCount, HandleResponseMsg_ErrorCC_CoversBranch)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRemappingAvailabilityBankCount sensor(
        sensorName, sensorType, rowRemapIntf, inventoryObjPath);

    std::vector<uint8_t> response(256, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    struct nsm_row_remap_availability data = {};
    auto encRc = encode_get_row_remap_availability_resp(0, NSM_ERROR, ERR_NULL,
                                                        &data, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(nsmEccErrorCountsDram, HandleResponseMsg_ErrorCC_CoversBranch)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus,
                                                     inventoryObjPath.c_str());
    nsm::NsmEccErrorCountsDram sensor(sensorName, sensorType, eccIntf,
                                      inventoryObjPath);

    std::vector<uint8_t> response(256, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    struct nsm_ECC_error_counts errorCounts = {};
    auto encRc = encode_get_ECC_error_counts_resp(0, NSM_ERROR, ERR_NULL,
                                                  &errorCounts, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(nsmMemCurrClockFreq, HandleResponseMsg_ErrorCC_CoversBranch)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmMemCurrClockFreq sensor(sensorName, sensorType, dimmIntf,
                                    inventoryObjPath);

    std::vector<uint8_t> response(256, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint32_t clockFreq = 0;
    auto encRc = encode_get_curr_clock_freq_resp(0, NSM_ERROR, ERR_NULL,
                                                 &clockFreq, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// Test createNsmMemorySensor with type "NSM_Memory" → calls createNSMMemory
TEST_F(NsmMemorySensorTestFixture, testCreateMemorySensorTypeNSMMemory)
{
    const std::string testPath = objPath + "_nsm_memory_type";

    auto& pm = utils::MockDbusAsync::propertyMap(testPath, basicIntfName);
    pm["Name"] = std::string("Memory_NsmMemory");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/nsm_test_mem_type1");
    pm["Type"] = std::string("NSM_Memory");
    pm["ErrorCorrection"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.Ecc.NoECC");
    pm["DeviceType"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DRAM");

    size_t initialCount = gpu->deviceSensors.size() + gpu->staticSensors.size();
    createNsmMemorySensor(mockManager, basicIntfName, testPath);
    EXPECT_GT(gpu->deviceSensors.size() + gpu->staticSensors.size(),
              initialCount);
}

// Test createNsmMemorySensor with type "NSM_Memory_Attributes" →
// calls createMemoryRowRemapping, createMemoryECCMode,
// createMemoryMemCapacityUtil
TEST_F(NsmMemorySensorTestFixture,
       testCreateMemorySensorTypeNSMMemoryAttributes)
{
    const std::string testPath = objPath + "_nsm_memory_attrs";

    auto& pm = utils::MockDbusAsync::propertyMap(testPath, basicIntfName);
    pm["Name"] = std::string("Memory_NsmMemoryAttrs");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/nsm_test_mem_attrs1");
    pm["Type"] = std::string("NSM_Memory_Attributes");

    size_t initialCount = gpu->deviceSensors.size();
    createNsmMemorySensor(mockManager, basicIntfName, testPath);
    EXPECT_GT(gpu->deviceSensors.size(), initialCount);
}

// Test createNsmMemorySensor type "NSM_Memory" with Priority=true branch
TEST_F(NsmMemorySensorTestFixture, testCreateMemorySensorTypeNSMMemoryPriority)
{
    const std::string testPath = objPath + "_nsm_memory_prio";

    auto& pm = utils::MockDbusAsync::propertyMap(testPath, basicIntfName);
    pm["Name"] = std::string("Memory_NsmMemoryPrio");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/nsm_test_mem_prio1");
    pm["Type"] = std::string("NSM_Memory");
    pm["ErrorCorrection"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.Ecc.NoECC");
    pm["DeviceType"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DRAM");
    pm["Priority"] = bool(true);

    size_t initialCount = gpu->deviceSensors.size() + gpu->staticSensors.size();
    createNsmMemorySensor(mockManager, basicIntfName, testPath);
    EXPECT_GT(gpu->deviceSensors.size() + gpu->staticSensors.size(),
              initialCount);
}

// Test createNsmMemorySensor coGetCachedBaseProperties fail → early return
TEST_F(NsmMemorySensorTestFixture, testCreateMemorySensorBasePropertiesFail)
{
    const std::string testPath = objPath + "_no_base";
    // No property map registered for this path → coGetCachedBaseProperties
    // returns error → factory returns early without adding sensors
    size_t initialCount = gpu->deviceSensors.size() + gpu->staticSensors.size();
    createNsmMemorySensor(mockManager, basicIntfName, testPath);
    EXPECT_EQ(gpu->deviceSensors.size() + gpu->staticSensors.size(),
              initialCount);
}

// =============================================================================
// NsmMinMemoryClockLimit::update() branch coverage
// =============================================================================

// Success path: sensorIO returns valid 4-byte inventory response → decode
// succeeds → allowedSpeedsMT[0] updated.
TEST_F(NsmMemorySensorTestFixture, NsmMinMemoryClockLimit_Update_Success)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/inventory/test/min_clk_upd");
    // Pre-initialize two slots so index [0] access is safe.
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    std::string testName = "MinClk_UpdateSuccess";
    std::string testType = "NSM_Memory";
    auto sensor = std::make_shared<NsmMinMemoryClockLimit>(testName, testType,
                                                           dimmIntf);

    uint32_t clockValue = 2000;
    std::vector<uint8_t> data(sizeof(uint32_t));
    memcpy(data.data(), &clockValue, sizeof(clockValue));

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + data.size(), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    ASSERT_EQ(encode_get_inventory_information_resp(
                  0, NSM_SUCCESS, ERR_NULL, data.size(), data.data(), response),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(responseMsg, Response{}));
    sensor->update(gpu);

    EXPECT_EQ(dimmIntf->allowedSpeedsMT()[0],
              static_cast<uint16_t>(clockValue));
}

// sensorIO failure path: rc != 0 → early return (lines 488-492).
TEST_F(NsmMemorySensorTestFixture, NsmMinMemoryClockLimit_Update_SensorIOFails)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/inventory/test/min_clk_iofail");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    std::string testName = "MinClk_IOFail";
    std::string testType = "NSM_Memory";
    auto sensor = std::make_shared<NsmMinMemoryClockLimit>(testName, testType,
                                                           dimmIntf);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(Response{}, NSM_ERROR));
    sensor->update(gpu);
    // allowedSpeedsMT unchanged
    EXPECT_EQ(dimmIntf->allowedSpeedsMT()[0], uint16_t(0));
}

// Bad CC path: sensorIO succeeds but response CC != NSM_SUCCESS →
// line 513 false branch (condition fails → no value update).
TEST_F(NsmMemorySensorTestFixture, NsmMinMemoryClockLimit_Update_BadCC)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/inventory/test/min_clk_badcc");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    std::string testName = "MinClk_BadCC";
    std::string testType = "NSM_Memory";
    auto sensor = std::make_shared<NsmMinMemoryClockLimit>(testName, testType,
                                                           dimmIntf);

    uint32_t clockValue = 3000;
    std::vector<uint8_t> data(sizeof(uint32_t));
    memcpy(data.data(), &clockValue, sizeof(clockValue));

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + data.size(), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    // Use error CC so condition (cc == NSM_SUCCESS) is false.
    ASSERT_EQ(encode_get_inventory_information_resp(
                  0, NSM_ERROR, ERR_NULL, data.size(), data.data(), response),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(responseMsg, Response{}));
    sensor->update(gpu);
    // allowedSpeedsMT[0] should NOT have been updated.
    EXPECT_EQ(dimmIntf->allowedSpeedsMT()[0], uint16_t(0));
}

// =============================================================================
// NsmMaxMemoryClockLimit::update() branch coverage
// =============================================================================

// Success path: sensorIO returns valid response → allowedSpeedsMT[1] updated.
TEST_F(NsmMemorySensorTestFixture, NsmMaxMemoryClockLimit_Update_Success)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/inventory/test/max_clk_upd");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    std::string testName = "MaxClk_UpdateSuccess";
    std::string testType = "NSM_Memory";
    auto sensor = std::make_shared<NsmMaxMemoryClockLimit>(testName, testType,
                                                           dimmIntf);

    uint32_t clockValue = 4000;
    std::vector<uint8_t> data(sizeof(uint32_t));
    memcpy(data.data(), &clockValue, sizeof(clockValue));

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + data.size(), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    ASSERT_EQ(encode_get_inventory_information_resp(
                  0, NSM_SUCCESS, ERR_NULL, data.size(), data.data(), response),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(responseMsg, Response{}));
    sensor->update(gpu);

    EXPECT_EQ(dimmIntf->allowedSpeedsMT()[1],
              static_cast<uint16_t>(clockValue));
}

// sensorIO failure path.
TEST_F(NsmMemorySensorTestFixture, NsmMaxMemoryClockLimit_Update_SensorIOFails)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/inventory/test/max_clk_iofail");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    std::string testName = "MaxClk_IOFail";
    std::string testType = "NSM_Memory";
    auto sensor = std::make_shared<NsmMaxMemoryClockLimit>(testName, testType,
                                                           dimmIntf);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(Response{}, NSM_ERROR));
    sensor->update(gpu);
    EXPECT_EQ(dimmIntf->allowedSpeedsMT()[1], uint16_t(0));
}

// Bad CC path: condition (cc == NSM_SUCCESS) false → no update to slot [1].
TEST_F(NsmMemorySensorTestFixture, NsmMaxMemoryClockLimit_Update_BadCC)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/inventory/test/max_clk_badcc");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    std::string testName = "MaxClk_BadCC";
    std::string testType = "NSM_Memory";
    auto sensor = std::make_shared<NsmMaxMemoryClockLimit>(testName, testType,
                                                           dimmIntf);

    uint32_t clockValue = 5000;
    std::vector<uint8_t> data(sizeof(uint32_t));
    memcpy(data.data(), &clockValue, sizeof(clockValue));

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + data.size(), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    ASSERT_EQ(encode_get_inventory_information_resp(
                  0, NSM_ERROR, ERR_NULL, data.size(), data.data(), response),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(responseMsg, Response{}));
    sensor->update(gpu);
    EXPECT_EQ(dimmIntf->allowedSpeedsMT()[1], uint16_t(0));
}

// =============================================================================
// FALSE-branch coverage: count() checks in createNsmMemorySensor
// =============================================================================

// Type absent → count("Type") FALSE → type="" → none of the type-switch
// branches match → no sensor created, no exception
TEST_F(NsmMemorySensorTestFixture, Factory_MissingType_NoSensor)
{
    const std::string testPath = objPath + "_missing_type";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, basicIntfName);
    pm["Name"] = std::string("Memory_NoType");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/nsm_test_notype");
    // "Type" intentionally omitted → type="" → no type branch matches

    size_t initialCount = gpu->deviceSensors.size() + gpu->staticSensors.size();
    createNsmMemorySensor(mockManager, basicIntfName, testPath);
    EXPECT_EQ(gpu->deviceSensors.size() + gpu->staticSensors.size(),
              initialCount);
}

// InventoryObjPath absent → count("InventoryObjPath") FALSE →
// inventoryObjPath="" → inventoryObjPath = "" + "_DRAM_0" = "_DRAM_0" →
// relative D-Bus path → factory has no try/catch → propagates
TEST_F(NsmMemorySensorTestFixture, Factory_MissingInventoryObjPath_Throws)
{
    const std::string testPath = objPath + "_missing_invpath";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, basicIntfName);
    pm["Name"] = std::string("Memory_NoInvPath");
    pm["UUID"] = gpuUuid;
    // "InventoryObjPath" intentionally omitted → inventoryObjPath="" →
    // path="_DRAM_0" (relative) → getInterfaceOnObjectPath throws
    pm["Type"] = std::string("NSM_Memory");
    pm["ErrorCorrection"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.Ecc.NoECC");
    pm["DeviceType"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DRAM");

    EXPECT_THROW_COROUTINE(
        createNsmMemorySensor(mockManager, basicIntfName, testPath),
        std::exception);
}

// ErrorCorrection absent → count("ErrorCorrection") FALSE → correctionType="" →
// NsmMemoryErrorCorrection ctor converts "" to enum → InvalidEnumString thrown
// → factory has no try/catch → propagates
TEST_F(NsmMemorySensorTestFixture,
       Factory_NSMMemory_MissingErrorCorrection_Throws)
{
    const std::string testPath = objPath + "_missing_errcorr";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, basicIntfName);
    pm["Name"] = std::string("Memory_NoErrCorr");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/nsm_test_no_errcorr");
    pm["Type"] = std::string("NSM_Memory");
    // "ErrorCorrection" intentionally omitted → correctionType="" → throws
    pm["DeviceType"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DRAM");

    EXPECT_THROW_COROUTINE(
        createNsmMemorySensor(mockManager, basicIntfName, testPath),
        std::exception);
}

// DeviceType absent → count("DeviceType") FALSE → deviceType="" →
// NsmMemoryDeviceType ctor converts "" to enum → InvalidEnumString thrown
// → factory has no try/catch → propagates
TEST_F(NsmMemorySensorTestFixture, Factory_NSMMemory_MissingDeviceType_Throws)
{
    const std::string testPath = objPath + "_missing_devtype";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, basicIntfName);
    pm["Name"] = std::string("Memory_NoDevType");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/nsm_test_no_devtype");
    pm["Type"] = std::string("NSM_Memory");
    pm["ErrorCorrection"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.Ecc.NoECC");
    // "DeviceType" intentionally omitted → deviceType="" → throws

    EXPECT_THROW_COROUTINE(
        createNsmMemorySensor(mockManager, basicIntfName, testPath),
        std::exception);
}

// count("Priority") FALSE in createNSMMemory (line 732): "Priority" absent →
// priority stays false (default) → currClockFreqSensor added with
// priority=false.
TEST_F(NsmMemorySensorTestFixture, Factory_NSMMemory_PriorityAbsent_FalseBranch)
{
    const std::string testPath = objPath + "_nsm_memory_no_prio";

    auto& pm = utils::MockDbusAsync::propertyMap(testPath, basicIntfName);
    pm["Name"] = std::string("Memory_NoPriority");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/nsm_test_no_prio");
    pm["Type"] = std::string("NSM_Memory");
    pm["ErrorCorrection"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.Ecc.NoECC");
    pm["DeviceType"] =
        std::string("xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.DRAM");
    // "Priority" intentionally absent → L732 FALSE → priority stays false

    size_t initialCount = gpu->deviceSensors.size() + gpu->staticSensors.size();
    createNsmMemorySensor(mockManager, basicIntfName, testPath);
    EXPECT_GT(gpu->deviceSensors.size() + gpu->staticSensors.size(),
              initialCount);
}

// count("Name") FALSE in outer factory (line 822): "Name" absent → name="" →
// type="NSM_Memory_Attributes" so createMemoryRowRemapping etc. are called
// with empty name → sensors created (inventoryObjPath path is still valid).
TEST_F(NsmMemorySensorTestFixture, Factory_MissingName_MemAttrsCreated)
{
    const std::string testPath = objPath + "_missing_name";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, basicIntfName);
    // "Name" intentionally omitted → name=""
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/nsm_test_no_name");
    pm["Type"] = std::string("NSM_Memory_Attributes");

    size_t initialCount = gpu->deviceSensors.size() + gpu->staticSensors.size();
    createNsmMemorySensor(mockManager, basicIntfName, testPath);
    EXPECT_GT(gpu->deviceSensors.size() + gpu->staticSensors.size(),
              initialCount);
}

// Exercises getInterfaceOnObjectPath<DimmIntf> else-branch
// (interfaceWrapper.hpp L77-79): two calls with the same sensorObjectPath
// re-use the cached DimmIntf. Using getInterfaceOnObjectPath directly avoids
// D-Bus path conflict from the other D-Bus objects that createNSMMemory would
// also register.
TEST_F(NsmMemorySensorTestFixture,
       GetInterfaceOnObjectPath_DimmIntf_ReusesCachedEntry)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string sensorObjPath =
        "/xyz/openbmc_project/inventory/nsm_dimm_reuse_test";
    const std::string dbusPath =
        "/xyz/openbmc_project/inventory/nsm_dimm_reuse_test";

    // First call: inserts InterfaceWrapper<DimmIntf> into objectPathToSensorMap
    auto intf1 = nsm::getInterfaceOnObjectPath<nsm::DimmIntf>(
        sensorObjPath, mockManager, bus, dbusPath.c_str());
    EXPECT_NE(intf1, nullptr);

    // Second call: same sensorObjPath → else branch in getInterfaceOnObjectPath
    // → dynamic_pointer_cast → returns same cached interface
    auto intf2 = nsm::getInterfaceOnObjectPath<nsm::DimmIntf>(
        sensorObjPath, mockManager, bus, dbusPath.c_str());
    EXPECT_NE(intf2, nullptr);
    EXPECT_EQ(intf1, intf2);
}

// handleResponseMsg: rc==NSM_SW_SUCCESS, cc!=NSM_SUCCESS branch tests.
// 9-byte buffer: decode_reason_code_and_cc returns NSM_SW_SUCCESS with
// cc=NSM_ERROR → "rc && cc" condition is FALSE → no update, cc returned.

TEST(nsmRowRemapState, HandleResponseMsg_DecodeSuccessNonZeroCC_ReturnsCC)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRowRemapState sensor(sensorName, sensorType, rowRemapIntf,
                                 inventoryObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(nsmRowRemappingCounts, HandleResponseMsg_DecodeSuccessNonZeroCC_ReturnsCC)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRowRemappingCounts sensor(sensorName, sensorType, rowRemapIntf,
                                      inventoryObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(nsmRemappingAvailabilityBankCount,
     HandleResponseMsg_DecodeSuccessNonZeroCC_ReturnsCC)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmRemappingAvailabilityBankCount sensor(
        sensorName, sensorType, rowRemapIntf, inventoryObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(nsmEccErrorCountsDram, HandleResponseMsg_DecodeSuccessNonZeroCC_ReturnsCC)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus,
                                                     inventoryObjPath.c_str());
    nsm::NsmEccErrorCountsDram sensor(sensorName, sensorType, eccIntf,
                                      inventoryObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(nsmMemCurrClockFreq, HandleResponseMsg_DecodeSuccessNonZeroCC_ReturnsCC)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmMemCurrClockFreq sensor(sensorName, sensorType, dimmIntf,
                                    inventoryObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmMinMemoryClockLimit::update() — wrong dataSize: decode succeeds with
// cc=NSM_SUCCESS but dataSize != sizeof(uint32_t) → 3rd &&-operand FALSE →
// allowedSpeedsMT[0] not updated. Covers L549 third-operand branch.
TEST_F(NsmMemorySensorTestFixture, NsmMinMemoryClockLimit_Update_WrongDataSize)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/inventory/test/min_clk_wrongsize");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    std::string testName = "MinClk_WrongSize";
    std::string testType = "NSM_Memory";
    auto sensor = std::make_shared<NsmMinMemoryClockLimit>(testName, testType,
                                                           dimmIntf);

    // Encode response with 2-byte payload (not 4 bytes) → dataSize=2 ≠ 4
    std::vector<uint8_t> twoByteData(2, 0xAB);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + twoByteData.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    ASSERT_EQ(encode_get_inventory_information_resp(
                  0, NSM_SUCCESS, ERR_NULL, twoByteData.size(),
                  twoByteData.data(), response),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(responseMsg, Response{}));
    sensor->update(gpu);

    // dataSize=2 != sizeof(uint32_t)=4 → no update to allowedSpeedsMT[0]
    EXPECT_EQ(dimmIntf->allowedSpeedsMT()[0], uint16_t(0));
}

// NsmMaxMemoryClockLimit::update() — wrong dataSize: decode succeeds with
// cc=NSM_SUCCESS but dataSize != sizeof(uint32_t) → 3rd &&-operand FALSE →
// allowedSpeedsMT[1] not updated. Covers L622 third-operand branch.
TEST_F(NsmMemorySensorTestFixture, NsmMaxMemoryClockLimit_Update_WrongDataSize)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/inventory/test/max_clk_wrongsize");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    std::string testName = "MaxClk_WrongSize";
    std::string testType = "NSM_Memory";
    auto sensor = std::make_shared<NsmMaxMemoryClockLimit>(testName, testType,
                                                           dimmIntf);

    // Encode response with 2-byte payload (not 4 bytes) → dataSize=2 ≠ 4
    std::vector<uint8_t> twoByteData(2, 0xCD);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + twoByteData.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    ASSERT_EQ(encode_get_inventory_information_resp(
                  0, NSM_SUCCESS, ERR_NULL, twoByteData.size(),
                  twoByteData.data(), response),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(responseMsg, Response{}));
    sensor->update(gpu);

    // dataSize=2 != sizeof(uint32_t)=4 → no update to allowedSpeedsMT[1]
    EXPECT_EQ(dimmIntf->allowedSpeedsMT()[1], uint16_t(0));
}
