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
#include "device-configuration.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmPowerLimit.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();
std::string sensorName("dummy_sensor");
std::string sensorType("dummy_type");
std::string inventoryObjPath("/xyz/openbmc_project/inventory/dummy_device");

// NsmPersistentPowerLimit Tests
TEST(NsmPersistentPowerLimit, HandleOfflineState_GPU_BASE)
{
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());
    auto clearPowerLimitIntf =
        std::make_shared<NsmClearPowerLimitIntf>(bus, inventoryObjPath);
    auto associationDefIntf = std::make_shared<AssociationDefinitionsIntf>(
        bus, inventoryObjPath.c_str());

    NsmPersistentPowerLimit sensor(sensorName, sensorType, powerLimitsIntf,
                                   clearPowerLimitIntf, associationDefIntf,
                                   nullptr, GPU_BASE, nullptr);

    uint32_t initialPowerCap = 300;
    powerLimitsIntf->powerCap(initialPowerCap);

    sensor.handleOfflineState();

    EXPECT_EQ(powerLimitsIntf->powerCap(), initialPowerCap);
}

TEST(NsmPersistentPowerLimit, HandleOfflineState_CPU_LIMIT)
{
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());
    auto clearPowerLimitIntf =
        std::make_shared<NsmClearPowerLimitIntf>(bus, inventoryObjPath);
    auto associationDefIntf = std::make_shared<AssociationDefinitionsIntf>(
        bus, inventoryObjPath.c_str());

    NsmPersistentPowerLimit sensor(sensorName, sensorType, powerLimitsIntf,
                                   clearPowerLimitIntf, associationDefIntf,
                                   nullptr, CPU_LIMIT_GPU_COPY, nullptr);

    uint32_t initialPowerCap = 300;
    powerLimitsIntf->powerCap(initialPowerCap);

    sensor.handleOfflineState();

    EXPECT_EQ(powerLimitsIntf->powerCap(), INVALID_POWER_LIMIT);
}

TEST(NsmPersistentPowerLimit, GoodGenReq)
{
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());
    auto clearPowerLimitIntf =
        std::make_shared<NsmClearPowerLimitIntf>(bus, inventoryObjPath);
    auto associationDefIntf = std::make_shared<AssociationDefinitionsIntf>(
        bus, inventoryObjPath.c_str());

    NsmPersistentPowerLimit sensor(sensorName, sensorType, powerLimitsIntf,
                                   clearPowerLimitIntf, associationDefIntf,
                                   nullptr, GPU_BASE, nullptr);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_get_device_mode_settings_v2_req*>(
        msg->payload);
    EXPECT_EQ(command->hdr.command, NSM_GET_DEVICE_MODE_SETTINGS_V2);
}

TEST(NsmPersistentPowerLimit, GoodHandleResp)
{
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());
    auto clearPowerLimitIntf =
        std::make_shared<NsmClearPowerLimitIntf>(bus, inventoryObjPath);
    auto associationDefIntf = std::make_shared<AssociationDefinitionsIntf>(
        bus, inventoryObjPath.c_str());
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus, inventoryObjPath.c_str());

    NsmPersistentPowerLimit sensor(sensorName, sensorType, powerLimitsIntf,
                                   clearPowerLimitIntf, associationDefIntf,
                                   nullptr, GPU_BASE, persistencyIntf);

    uint32_t powerLimitValue = htole32(300000);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(powerLimitValue) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, reason_code,
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue),
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue), responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(powerLimitsIntf->powerCap(), 300);
}

TEST(NsmPersistentPowerLimit, BadHandleResp)
{
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());
    auto clearPowerLimitIntf =
        std::make_shared<NsmClearPowerLimitIntf>(bus, inventoryObjPath);
    auto associationDefIntf = std::make_shared<AssociationDefinitionsIntf>(
        bus, inventoryObjPath.c_str());

    NsmPersistentPowerLimit sensor(sensorName, sensorType, powerLimitsIntf,
                                   clearPowerLimitIntf, associationDefIntf,
                                   nullptr, GPU_BASE, nullptr);

    uint32_t powerLimitValue = htole32(300000);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(powerLimitValue) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, reason_code,
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue),
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue), responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(responseMsg, 0);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmPersistentPowerLimit, HandleOfflineState_GPU_COPY_SWITCH)
{
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());
    auto associationDefIntf = std::make_shared<AssociationDefinitionsIntf>(
        bus, inventoryObjPath.c_str());

    NsmPersistentPowerLimit sensor(sensorName, sensorType, powerLimitsIntf,
                                   nullptr, associationDefIntf, nullptr,
                                   GPU_COPY_SWITCH, nullptr);

    uint32_t initialPowerCap = 300;
    powerLimitsIntf->powerCap(initialPowerCap);

    sensor.handleOfflineState();

    EXPECT_EQ(powerLimitsIntf->powerCap(), initialPowerCap);
}

TEST(NsmPersistentPowerLimit, GoodHandleResp_GPU_COPY_SWITCH)
{
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());
    auto associationDefIntf = std::make_shared<AssociationDefinitionsIntf>(
        bus, inventoryObjPath.c_str());

    NsmPersistentPowerLimit sensor(sensorName, sensorType, powerLimitsIntf,
                                   nullptr, associationDefIntf, nullptr,
                                   GPU_COPY_SWITCH, nullptr);

    uint32_t powerLimitValue = htole32(300000);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(powerLimitValue) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, reason_code,
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue),
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue), responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(powerLimitsIntf->powerCap(), 300);
}

TEST(NsmPersistentPowerLimit, BadHandleResp_GPU_COPY_SWITCH)
{
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());
    auto associationDefIntf = std::make_shared<AssociationDefinitionsIntf>(
        bus, inventoryObjPath.c_str());

    NsmPersistentPowerLimit sensor(sensorName, sensorType, powerLimitsIntf,
                                   nullptr, associationDefIntf, nullptr,
                                   GPU_COPY_SWITCH, nullptr);

    uint32_t powerLimitValue = htole32(300000);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(powerLimitValue) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, reason_code,
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue),
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue), responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(responseMsg, 0);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// NsmOneShotPowerLimit Tests
TEST(NsmOneShotPowerLimit, GoodGenReq)
{
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus, inventoryObjPath.c_str());
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());

    NsmOneShotPowerLimit sensor(sensorName, sensorType, GPU_BASE,
                                persistencyIntf, powerLimitsIntf);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_get_device_mode_settings_v2_req*>(
        msg->payload);
    EXPECT_EQ(command->hdr.command, NSM_GET_DEVICE_MODE_SETTINGS_V2);
}

TEST(NsmOneShotPowerLimit, GoodHandleResp)
{
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus, inventoryObjPath.c_str());
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());

    persistencyIntf->persistentPowerLimit(300.0);
    powerLimitsIntf->powerCap(300);

    NsmOneShotPowerLimit sensor(sensorName, sensorType, GPU_BASE,
                                persistencyIntf, powerLimitsIntf);

    uint32_t powerLimitValue = htole32(300000);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(powerLimitValue) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, reason_code,
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue),
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue), responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(persistencyIntf->oneShotPowerLimit(), 300.0);
    EXPECT_EQ(persistencyIntf->persistency(), true);
}

TEST(NsmOneShotPowerLimit, BadHandleResp)
{
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus, inventoryObjPath.c_str());
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());

    NsmOneShotPowerLimit sensor(sensorName, sensorType, GPU_BASE,
                                persistencyIntf, powerLimitsIntf);

    uint32_t powerLimitValue = htole32(300000);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
            sizeof(powerLimitValue) * 2,
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;

    uint8_t rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, reason_code,
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue),
        reinterpret_cast<const uint8_t*>(&powerLimitValue),
        sizeof(powerLimitValue), responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(responseMsg, 0);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// NsmPowerLimitRange Tests
TEST(NsmPowerLimitRange, Constructor_MaxPowerLimit)
{
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());

    NsmPowerLimitRange sensor(sensorName, sensorType,
                              MAXIMUM_GPU_BASE_POWER_LIMIT, powerLimitsIntf);

    EXPECT_EQ(sensor.propertyName, "MAXIMUM_GPU_BASE_POWER_LIMIT");
    EXPECT_EQ(sensor.propertyId, MAXIMUM_GPU_BASE_POWER_LIMIT);
}

TEST(NsmPowerLimitRange, Constructor_MinPowerLimit)
{
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());

    NsmPowerLimitRange sensor(sensorName, sensorType,
                              MINIMUM_GPU_BASE_POWER_LIMIT, powerLimitsIntf);

    EXPECT_EQ(sensor.propertyName, "MINIMUM_GPU_BASE_POWER_LIMIT");
    EXPECT_EQ(sensor.propertyId, MINIMUM_GPU_BASE_POWER_LIMIT);
}

TEST(NsmPowerLimitRange, Constructor_UnknownPropertyId)
{
    auto powerLimitsIntf =
        std::make_shared<PowerLimitsIntf>(bus, inventoryObjPath.c_str());

    NsmPowerLimitRange sensor(sensorName, sensorType, 255, powerLimitsIntf);

    EXPECT_EQ(sensor.propertyName, "");
    EXPECT_EQ(sensor.propertyId, 255);
}

// NsmDefaultPowerLimit Tests
TEST(NsmDefaultPowerLimit, Constructor_RatedPowerLimit)
{
    auto clearPowerLimitIntf =
        std::make_shared<NsmClearPowerLimitIntf>(bus, inventoryObjPath);

    NsmDefaultPowerLimit sensor(sensorName, sensorType,
                                RATED_GPU_BASE_POWER_LIMIT,
                                clearPowerLimitIntf);

    EXPECT_EQ(sensor.propertyName, "RATED_GPU_BASE_POWER_LIMIT");
    EXPECT_EQ(sensor.propertyId, RATED_GPU_BASE_POWER_LIMIT);
}

TEST(NsmDefaultPowerLimit, Constructor_UnknownPropertyId)
{
    auto clearPowerLimitIntf =
        std::make_shared<NsmClearPowerLimitIntf>(bus, inventoryObjPath);

    NsmDefaultPowerLimit sensor(sensorName, sensorType, 255,
                                clearPowerLimitIntf);

    EXPECT_EQ(sensor.propertyName, "UNKNOWN");
    EXPECT_EQ(sensor.propertyId, 255);
}

// NsmClearPowerLimitIntf Tests
TEST(NsmClearPowerLimitIntf, ClearPowerCap)
{
    NsmClearPowerLimitIntf clearIntf(bus, inventoryObjPath);

    int32_t result = clearIntf.clearPowerCap();
    EXPECT_EQ(result, 0);
}
