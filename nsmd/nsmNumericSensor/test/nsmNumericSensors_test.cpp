/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
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



#include "base.h"
#include "platform-environmental.h"

#include "utils.hpp"

#include <sdbusplus/bus.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmEnergy.hpp"
#include "nsmNumericSensorValue_mock.hpp"
#include "nsmPower.hpp"
#include "nsmTemp.hpp"
#include "nsmVoltage.hpp"

static auto& bus = utils::DBusHandler::getBus();
static const std::string sensorName("dummy_sensor");
static const std::string sensorType("dummy_type");
static const std::vector<utils::Association>
    associations({{"chassis", "all_sensors",
                   "/xyz/openbmc_project/inventory/dummy_device"}});

TEST(nsmTemp, GoodGenReq)
{
    nsm::NsmTemp sensor{bus, sensorName,   sensorType,
                        1,   associations, associations[0].absolutePath};

    EXPECT_EQ(sensor.sensorId, 1);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_get_temperature_reading_req*>(msg->payload);
    EXPECT_EQ(command->hdr.command, NSM_GET_TEMPERATURE_READING);
    EXPECT_EQ(command->hdr.data_size, 1);
    EXPECT_EQ(command->sensor_id, 1);
}

TEST(nsmTemp, GoodHandleResp)
{
    nsm::NsmTemp sensor{bus, sensorName,   sensorType,
                        1,   associations, associations[0].absolutePath};

    auto value = std::make_shared<MockNsmNumericSensorValue>();

    sensor.sensorValue = value;

    static constexpr size_t msg_size =
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp);
    std::array<char, msg_size> request;
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    const uint8_t instance_id{30};
    const uint8_t cc = NSM_SUCCESS;
    const uint16_t reason_code = ERR_NULL;
    const double reading{3843.358};

    auto rc = encode_get_temperature_reading_resp(instance_id, cc, reason_code,
                                                  reading, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*value, updateReading(testing::DoubleNear(reading, 0.01), 0))
        .Times(1);

    sensor.handleResponseMsg(msg, msg_size);
}

TEST(nsmTemp, BadHandleResp)
{
    nsm::NsmTemp sensor{bus, sensorName,   sensorType,
                        1,   associations, associations[0].absolutePath};

    auto value = std::make_shared<MockNsmNumericSensorValue>();

    sensor.sensorValue = value;

    static constexpr size_t msg_size =
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp);
    std::array<char, msg_size> request;
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    const uint8_t instance_id{30};
    const uint8_t cc = NSM_ERROR;
    const uint16_t reason_code = ERR_NOT_SUPPORTED;
    const double reading{3843.348};
    uint8_t rc = NSM_SW_SUCCESS;

    rc = sensor.handleResponseMsg(nullptr, msg_size);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = sensor.handleResponseMsg(msg, msg_size - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = encode_get_temperature_reading_resp(instance_id, cc, reason_code,
                                             reading, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(msg, msg_size);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmPower, GoodGenReq)
{
    nsm::NsmPower sensor{bus,
                         sensorName,
                         sensorType,
                         1,
                         1,
                         associations,
                         associations[0].absolutePath};

    EXPECT_EQ(sensor.sensorId, 1);
    EXPECT_EQ(sensor.averagingInterval, 1);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_get_current_power_draw_req*>(msg->payload);

    EXPECT_EQ(command->hdr.command, NSM_GET_POWER);
    EXPECT_EQ(command->hdr.data_size, 2);
    EXPECT_EQ(command->sensor_id, 1);
    EXPECT_EQ(command->averaging_interval, 1);
}

TEST(nsmPower, GoodHandleResp)
{
    nsm::NsmPower sensor{bus,
                         sensorName,
                         sensorType,
                         1,
                         1,
                         associations,
                         associations[0].absolutePath};

    auto value = std::make_shared<MockNsmNumericSensorValue>();

    sensor.sensorValue = value;

    static constexpr size_t msg_size =
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp);
    std::array<char, msg_size> request;
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    const uint8_t instance_id{30};
    const uint8_t cc = NSM_SUCCESS;
    const uint16_t reason_code = ERR_NULL;
    const uint32_t reading{34320};

    auto rc = encode_get_current_power_draw_resp(instance_id, cc, reason_code,
                                                 reading, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*value, updateReading(reading / 1000.0, 0)).Times(1);

    sensor.handleResponseMsg(msg, msg_size);
}

TEST(nsmPower, BadHandleResp)
{
    nsm::NsmPower sensor{bus,
                         sensorName,
                         sensorType,
                         1,
                         1,
                         associations,
                         associations[0].absolutePath};

    auto value = std::make_shared<MockNsmNumericSensorValue>();

    sensor.sensorValue = value;

    static constexpr size_t msg_size =
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp);
    std::array<char, msg_size> request;
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    const uint8_t instance_id{30};
    const uint8_t cc = NSM_ERR_NOT_READY;
    const uint16_t reason_code = ERR_TIMEOUT;
    const uint32_t reading{34320};
    uint8_t rc = NSM_SW_SUCCESS;

    rc = sensor.handleResponseMsg(nullptr, msg_size);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = sensor.handleResponseMsg(msg, msg_size - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = encode_get_current_power_draw_resp(instance_id, cc, reason_code,
                                            reading, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(msg, msg_size);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmEnergy, GoodGenReq)
{
    nsm::NsmEnergy sensor{bus, sensorName,   sensorType,
                          1,   associations, associations[0].absolutePath};

    EXPECT_EQ(sensor.sensorId, 1);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_get_current_energy_count_req*>(msg->payload);

    EXPECT_EQ(command->hdr.command, NSM_GET_ENERGY_COUNT);
    EXPECT_EQ(command->hdr.data_size, 1);
    EXPECT_EQ(command->sensor_id, 1);
}

TEST(nsmEnergy, GoodHandleResp)
{
    nsm::NsmEnergy sensor{bus, sensorName,   sensorType,
                          1,   associations, associations[0].absolutePath};

    auto value = std::make_shared<MockNsmNumericSensorValue>();

    const_cast<std::shared_ptr<nsm::NsmNumericSensorValue>&>(
        sensor.sensorValue) = value;

    static constexpr size_t msg_size =
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_energy_count_resp);
    std::array<char, msg_size> request;
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    const uint8_t instance_id{30};
    const uint8_t cc = NSM_SUCCESS;
    const uint16_t reason_code = ERR_NULL;
    const uint64_t reading{34320907};

    auto rc = encode_get_current_energy_count_resp(instance_id, cc, reason_code,
                                                   reading, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*value, updateReading(reading, 0)).Times(1);

    sensor.handleResponseMsg(msg, msg_size);
}

TEST(nsmEnergy, BadHandleResp)
{
    nsm::NsmEnergy sensor{bus, sensorName,   sensorType,
                          1,   associations, associations[0].absolutePath};

    auto value = std::make_shared<MockNsmNumericSensorValue>();

    const_cast<std::shared_ptr<nsm::NsmNumericSensorValue>&>(
        sensor.sensorValue) = value;

    static constexpr size_t msg_size =
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_energy_count_resp);
    std::array<char, msg_size> request;
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    const uint8_t instance_id{30};
    const uint8_t cc = NSM_ERROR;
    const uint16_t reason_code = ERR_NOT_SUPPORTED;
    const uint64_t reading{34320907};
    uint8_t rc = NSM_SW_SUCCESS;

    rc = sensor.handleResponseMsg(nullptr, msg_size);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = sensor.handleResponseMsg(msg, msg_size - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = encode_get_current_energy_count_resp(instance_id, cc, reason_code,
                                              reading, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(msg, msg_size);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmVoltage, GoodGenReq)
{
    nsm::NsmVoltage sensor{bus, sensorName, sensorType, 1, associations};

    EXPECT_EQ(sensor.sensorId, 1);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_get_voltage_req*>(msg->payload);

    EXPECT_EQ(command->hdr.command, NSM_GET_VOLTAGE);
    EXPECT_EQ(command->hdr.data_size, 1);
    EXPECT_EQ(command->sensor_id, 1);
}

TEST(nsmVoltage, GoodHandleResp)
{
    nsm::NsmVoltage sensor{bus, sensorName, sensorType, 1, associations};

    auto value = std::make_shared<MockNsmNumericSensorValue>();

    const_cast<std::shared_ptr<nsm::NsmNumericSensorValue>&>(
        sensor.sensorValue) = value;

    static constexpr size_t msg_size =
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_voltage_resp);
    std::array<char, msg_size> request;
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    const uint8_t instance_id{30};
    const uint8_t cc = NSM_SUCCESS;
    const uint16_t reason_code = ERR_NULL;
    const uint32_t reading{4345787};

    auto rc =
        encode_get_voltage_resp(instance_id, cc, reason_code, reading, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*value, updateReading(reading / 1'000'000.0, 0)).Times(1);

    sensor.handleResponseMsg(msg, msg_size);
}

TEST(nsmVoltage, BadHandleResp)
{
    nsm::NsmVoltage sensor{bus, sensorName, sensorType, 1, associations};

    auto value = std::make_shared<MockNsmNumericSensorValue>();

    const_cast<std::shared_ptr<nsm::NsmNumericSensorValue>&>(
        sensor.sensorValue) = value;

    static constexpr size_t msg_size =
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_voltage_resp);
    std::array<char, msg_size> request;
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    const uint8_t instance_id{30};
    const uint8_t cc = NSM_ERR_NOT_READY;
    const uint16_t reason_code = ERR_TIMEOUT;
    const uint32_t reading{4345787};
    uint8_t rc = NSM_SW_SUCCESS;

    rc = sensor.handleResponseMsg(nullptr, msg_size);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = sensor.handleResponseMsg(msg, msg_size - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = encode_get_voltage_resp(instance_id, cc, reason_code, reading, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(msg, msg_size);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}
