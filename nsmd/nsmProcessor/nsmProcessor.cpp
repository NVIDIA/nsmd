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



#include "nsmProcessor.hpp"

#include "pci-links.h"
#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

#define PROCESSOR_INTERFACE "xyz.openbmc_project.Configuration.NSM_Processor"

namespace nsm
{

NsmAcceleratorIntf::NsmAcceleratorIntf(sdbusplus::bus::bus& bus,
                                       std::string& name, std::string& type,
                                       std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    acceleratorIntf =
        std::make_unique<AcceleratorIntf>(bus, inventoryObjPath.c_str());
    acceleratorIntf->type(accelaratorType::GPU);
}

NsmUuidIntf::NsmUuidIntf(sdbusplus::bus::bus& bus, std::string& name,
                         std::string& type, std::string& inventoryObjPath,
                         uuid_t uuid) :
    NsmObject(name, type)
{
    uuidIntf = std::make_unique<UuidIntf>(bus, inventoryObjPath.c_str());
    uuidIntf->uuid(uuid);
}

NsmMigMode::NsmMigMode(sdbusplus::bus::bus& bus, std::string& name,
                       std::string& type, std::string& inventoryObjPath) :
    NsmSensor(name, type)

{
    lg2::info("NsmMigMode: create sensor:{NAME}", "NAME", name.c_str());
    migModeIntf = std::make_unique<MigModeIntf>(bus, inventoryObjPath.c_str());
}

void NsmMigMode::updateReading(bitfield8_t flags)
{
    migModeIntf->migModeEnabled(flags.bits.bit0);
}

std::optional<std::vector<uint8_t>>
    NsmMigMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_MIG_mode_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_MIG_mode_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmMigMode::handleResponseMsg(const struct nsm_msg* responseMsg,
                                      size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    bitfield8_t flags;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_MIG_mode_resp(responseMsg, responseLen, &cc,
                                       &data_size, &reason_code, &flags);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(flags);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_temperature_reading_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmEccMode::NsmEccMode(std::string& name, std::string& type,
                       std::shared_ptr<EccModeIntf> eccIntf) :
    NsmSensor(name, type)

{
    lg2::info("NsmEccMode: create sensor:{NAME}", "NAME", name.c_str());
    eccModeIntf = eccIntf;
}

void NsmEccMode::updateReading(bitfield8_t flags)
{
    eccModeIntf->eccModeEnabled(flags.bits.bit0);
    eccModeIntf->pendingECCState(flags.bits.bit1);
}

std::optional<std::vector<uint8_t>>
    NsmEccMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_ECC_mode_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_ECC_mode_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmEccMode::handleResponseMsg(const struct nsm_msg* responseMsg,
                                      size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    bitfield8_t flags;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_ECC_mode_resp(responseMsg, responseLen, &cc,
                                       &data_size, &reason_code, &flags);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(flags);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_ECC_mode_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);

        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmEccErrorCounts::NsmEccErrorCounts(std::string& name, std::string& type,
                                     std::shared_ptr<EccModeIntf> eccIntf) :
    NsmSensor(name, type)

{
    lg2::info("NsmEccErrorCounts: create sensor:{NAME}", "NAME", name.c_str());
    eccErrorCountIntf = eccIntf;
}

void NsmEccErrorCounts::updateReading(struct nsm_ECC_error_counts errorCounts)
{
    eccErrorCountIntf->ceCount(errorCounts.sram_corrected);
    int64_t ueCount = errorCounts.sram_uncorrected_secded +
                      errorCounts.sram_uncorrected_parity;
    eccErrorCountIntf->ueCount(ueCount);
}

std::optional<std::vector<uint8_t>>
    NsmEccErrorCounts::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_ECC_error_counts_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_ECC_error_counts_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmEccErrorCounts::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    struct nsm_ECC_error_counts errorCounts;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_get_ECC_error_counts_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &errorCounts);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(errorCounts);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_ECC_error_counts_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);

        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmPcieGroup::NsmPcieGroup(const std::string& name, const std::string& type,
                           uint8_t deviceId, uint8_t groupId) :
    NsmSensor(name, type),
    deviceId(deviceId), groupId(groupId)
{}

std::optional<std::vector<uint8_t>>
    NsmPcieGroup::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(instanceId, deviceId,
                                                         groupId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmPciGroup :: encode_query_scalar_group_telemetry_v1_req failed for"
            "group = {GROUPID} eid={EID} rc={RC}",
            "GROUPID", groupId, "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

NsmPciGroup2::NsmPciGroup2(const std::string& name, const std::string& type,
                           std::shared_ptr<PCieEccIntf> pCieECCIntf,
                           uint8_t deviceId) :
    NsmPcieGroup(name, type, deviceId, 2)

{
    lg2::info("NsmPciGroup2: create sensor:{NAME}", "NAME", name.c_str());
    pCieEccIntf = pCieECCIntf;
}

void NsmPciGroup2::updateReading(
    const nsm_query_scalar_group_telemetry_group_2& data)
{
    pCieEccIntf->nonfeCount(data.non_fatal_errors);
    pCieEccIntf->feCount(data.fatal_errors);
    pCieEccIntf->ceCount(data.correctable_errors);
}

uint8_t NsmPciGroup2::handleResponseMsg(const struct nsm_msg* responseMsg,
                                        size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    struct nsm_query_scalar_group_telemetry_group_2 data;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_query_scalar_group_telemetry_v1_group2_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(data);
    }
    else
    {
        lg2::error(
            "NsmPciGroup4 :: handleResponseMsg:  decode_query_scalar_group_telemetry_v1_group2_resp"
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmPciGroup3::NsmPciGroup3(const std::string& name, const std::string& type,
                           std::shared_ptr<PCieEccIntf> pCieECCIntf,
                           uint8_t deviceId) :
    NsmPcieGroup(name, type, deviceId, 3)

{
    lg2::info("NsmPciGroup2: create sensor:{NAME}", "NAME", name.c_str());
    pCieEccIntf = pCieECCIntf;
}

void NsmPciGroup3::updateReading(
    const nsm_query_scalar_group_telemetry_group_3& data)
{
    pCieEccIntf->l0ToRecoveryCount(data.L0ToRecoveryCount);
}

uint8_t NsmPciGroup3::handleResponseMsg(const struct nsm_msg* responseMsg,
                                        size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    struct nsm_query_scalar_group_telemetry_group_3 data;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_query_scalar_group_telemetry_v1_group3_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(data);
    }
    else
    {
        lg2::error(
            "NsmPciGroup3 :: handleResponseMsg:  decode_query_scalar_group_telemetry_v1_group3_resp"
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmPciGroup4::NsmPciGroup4(const std::string& name, const std::string& type,
                           std::shared_ptr<PCieEccIntf> pCieECCIntf,
                           uint8_t deviceId) :
    NsmPcieGroup(name, type, deviceId, 4)

{
    lg2::info("NsmPciGroup4: create sensor:{NAME}", "NAME", name.c_str());
    pCieEccIntf = pCieECCIntf;
}

void NsmPciGroup4::updateReading(
    const nsm_query_scalar_group_telemetry_group_4& data)
{
    pCieEccIntf->replayCount(data.replay_cnt);
    pCieEccIntf->replayRolloverCount(data.replay_rollover_cnt);
    pCieEccIntf->nakSentCount(data.NAK_sent_cnt);
    pCieEccIntf->nakRecievedCount(data.NAK_recv_cnt);
}

uint8_t NsmPciGroup4::handleResponseMsg(const struct nsm_msg* responseMsg,
                                        size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    struct nsm_query_scalar_group_telemetry_group_4 data;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_query_scalar_group_telemetry_v1_group4_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(data);
    }
    else
    {
        lg2::error(
            "handleResponseMsg:  decode_query_scalar_group_telemetry_v1_group4_resp"
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmEDPpScalingFactor::NsmEDPpScalingFactor(sdbusplus::bus::bus& bus,
                                           std::string& name, std::string& type,
                                           std::string& inventoryObjPath) :
    NsmSensor(name, type)

{
    lg2::info("NsmEDPpScalingFactor: create sensor:{NAME}", "NAME",
              name.c_str());
    eDPpIntf = std::make_shared<EDPpLocal>(bus, inventoryObjPath.c_str());
}

void NsmEDPpScalingFactor::updateReading(
    struct nsm_EDPp_scaling_factors scaling_factors)
{
    eDPpIntf->allowableMax(scaling_factors.maximum_scaling_factor);
    eDPpIntf->allowableMin(scaling_factors.minimum_scaling_factor);
}

std::optional<std::vector<uint8_t>>
    NsmEDPpScalingFactor::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc =
        encode_get_programmable_EDPp_scaling_factor_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_programmable_EDPp_scaling_factor_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmEDPpScalingFactor::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{

    uint8_t cc = NSM_ERROR;
    struct nsm_EDPp_scaling_factors scaling_factors;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_get_programmable_EDPp_scaling_factor_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code,
        &scaling_factors);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(scaling_factors);
    }
    else
    {
        lg2::error(
            "handleResponseMsg:  decode_get_programmable_EDPp_scaling_factor_resp"
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

static void createNsmProcessorSensor(SensorManager& manager,
                                     const std::string& interface,
                                     const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        auto name = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name", PROCESSOR_INTERFACE);

        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", PROCESSOR_INTERFACE);

        auto type = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Type", interface.c_str());

        auto inventoryObjPath =
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "InventoryObjPath", PROCESSOR_INTERFACE);

        auto nsmDevice = manager.getNsmDevice(uuid);
        if (!nsmDevice)
        {
            // cannot found a nsmDevice for the sensor
            lg2::error(
                "The UUID of NSM_Processor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);
            return;
        }

        if (type == "NSM_Processor")
        {
            auto sensor = std::make_shared<NsmAcceleratorIntf>(
                bus, name, type, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(sensor);

            auto uuidSensor = std::make_shared<NsmUuidIntf>(
                bus, name, type, inventoryObjPath, uuid);
            nsmDevice->deviceSensors.push_back(uuidSensor);
        }

        if (type == "NSM_MIG")
        {
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());

            auto sensor =
                std::make_shared<NsmMigMode>(bus, name, type, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(sensor);
            if (priority)
            {
                nsmDevice->prioritySensors.push_back(sensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(sensor);
            }
        }
        if (type == "NSM_PCIe")
        {
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());
            auto deviceId = utils::DBusHandler().getDbusProperty<uint64_t>(
                objPath.c_str(), "DeviceId", interface.c_str());
            auto pCieECCIntf =
                std::make_shared<PCieEccIntf>(bus, inventoryObjPath.c_str());
            auto sensorGroup2 = std::make_shared<NsmPciGroup2>(
                name, type, pCieECCIntf, deviceId);
            auto sensorGroup3 = std::make_shared<NsmPciGroup3>(
                name, type, pCieECCIntf, deviceId);
            auto sensorGroup4 = std::make_shared<NsmPciGroup4>(
                name, type, pCieECCIntf, deviceId);

            nsmDevice->deviceSensors.push_back(sensorGroup2);
            nsmDevice->deviceSensors.push_back(sensorGroup3);
            nsmDevice->deviceSensors.push_back(sensorGroup4);

            if (priority)
            {
                nsmDevice->prioritySensors.push_back(sensorGroup2);
                nsmDevice->prioritySensors.push_back(sensorGroup3);
                nsmDevice->prioritySensors.push_back(sensorGroup4);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(sensorGroup2);
                nsmDevice->roundRobinSensors.push_back(sensorGroup3);
                nsmDevice->roundRobinSensors.push_back(sensorGroup4);
            }
        }
        else if (type == "NSM_ECC")
        {
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());

            auto eccIntf =
                std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
            auto eccModeSensor =
                std::make_shared<NsmEccMode>(name, type, eccIntf);

            nsmDevice->deviceSensors.push_back(eccModeSensor);

            auto eccErrorCntSensor =
                std::make_shared<NsmEccErrorCounts>(name, type, eccIntf);

            nsmDevice->deviceSensors.push_back(eccErrorCntSensor);

            if (priority)
            {
                nsmDevice->prioritySensors.push_back(eccModeSensor);
                nsmDevice->prioritySensors.push_back(eccErrorCntSensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(eccModeSensor);
                nsmDevice->roundRobinSensors.push_back(eccErrorCntSensor);
            }
        }
        else if (type == "NSM_EDPp")
        {
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());
            auto sensor = std::make_shared<NsmEDPpScalingFactor>(
                bus, name, type, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(sensor);
            if (priority)
            {
                nsmDevice->prioritySensors.push_back(sensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(sensor);
            }
        }
    }

    catch (const std::exception& e)
    {
        lg2::error(
            "Error while addSensor for path {PATH} and interface {INTF}, {ERROR}",
            "PATH", objPath, "INTF", interface, "ERROR", e);
        return;
    }
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmProcessorSensor, "xyz.openbmc_project.Configuration.NSM_Processor")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_Processor.MIGMode")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_Processor.ECCMode")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_Processor.PCIe")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_Processor.EDPpScalingFactor")

} // namespace nsm
