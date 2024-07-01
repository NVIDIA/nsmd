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

#include "config.h"

#include "nsmProcessor.hpp"

#include "pci-links.h"
#include "platform-environmental.h"

#include "deviceManager.hpp"
#include "nsmDevice.hpp"
#include "nsmInterface.hpp"
#include "nsmObjectFactory.hpp"

#include <stdint.h>

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>

#include <cstdint>
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

NsmProcessorAssociation::NsmProcessorAssociation(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::string& inventoryObjPath,
    const std::vector<utils::Association>& associations) :
    NsmObject(name, type)
{
    associationDef = std::make_unique<AssociationDefinitionsIntf>(
        bus, inventoryObjPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDef->associations(associations_list);
}
NsmUuidIntf::NsmUuidIntf(sdbusplus::bus::bus& bus, std::string& name,
                         std::string& type, std::string& inventoryObjPath,
                         uuid_t uuid) :
    NsmObject(name, type)
{
    uuidIntf = std::make_unique<UuidIntf>(bus, inventoryObjPath.c_str());
    uuidIntf->uuid(uuid);
}

requester::Coroutine NsmUuidIntf::update(SensorManager& manager, eid_t eid)
{
    DeviceManager& deviceManager = DeviceManager::getInstance();
    auto uuid = utils::getUUIDFromEID(deviceManager.getEidTable(), eid);
    if (uuid)
    {
        auto nsmDevice = manager.getNsmDevice(*uuid);
        if (nsmDevice)
        {
            uuidIntf->uuid(nsmDevice->deviceUuid);
        }
    }
    co_return NSM_SW_SUCCESS;
}

NsmLocationIntfProcessor::NsmLocationIntfProcessor(
    sdbusplus::bus::bus& bus, std::string& name, std::string& type,
    std::string& inventoryObjPath, std::string& locationType) :
    NsmObject(name, type)
{
    locationIntf =
        std::make_unique<LocationIntfProcessor>(bus, inventoryObjPath.c_str());
    locationIntf->locationType(
        LocationIntfProcessor::convertLocationTypesFromString(locationType));
}

NsmLocationCodeIntfProcessor::NsmLocationCodeIntfProcessor(
    sdbusplus::bus::bus& bus, std::string& name, std::string& type,
    std::string& inventoryObjPath, std::string& locationCode) :
    NsmObject(name, type)
{
    locationCodeIntf = std::make_unique<LocationCodeIntfProcessor>(
        bus, inventoryObjPath.c_str());
    locationCodeIntf->locationCode(locationCode);
}

NsmMigMode::NsmMigMode(sdbusplus::bus::bus& bus, std::string& name,
                       std::string& type, std::string& inventoryObjPath,
                       std::shared_ptr<NsmDevice> device) :
    NsmSensor(name, type)

{
    lg2::info("NsmMigMode: create sensor:{NAME}", "NAME", name.c_str());
    migModeIntf =
        std::make_unique<NsmMigModeIntf>(bus, inventoryObjPath.c_str(), device);
}

void NsmMigMode::updateReading(bitfield8_t flags)
{
    migModeIntf->MigModeIntf::migModeEnabled(flags.bits.bit0);
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
                       std::shared_ptr<NsmEccModeIntf> eccIntf) :
    NsmObject(name, type)

{
    eccModeIntf = eccIntf;
}

requester::Coroutine NsmEccMode::update(SensorManager& manager, eid_t eid)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_ECC_mode_req(0, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_ECC_mode_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        co_return rc;
    }
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error("NsmEccMode SendRecvNsmMsg failed with RC={RC}, eid={EID}",
                   "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    bitfield8_t flags;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;

    rc = decode_get_ECC_mode_resp(responseMsg.get(), responseLen, &cc,
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

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return cc;
}

void NsmEccMode::updateReading(bitfield8_t flags)
{
    eccModeIntf->EccModeIntf::eccModeEnabled(flags.bits.bit0);
    eccModeIntf->EccModeIntf::pendingECCState(flags.bits.bit1);
}

NsmEccErrorCounts::NsmEccErrorCounts(std::string& name, std::string& type,
                                     std::shared_ptr<NsmEccModeIntf> eccIntf) :
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

    eccErrorCountIntf->isThresholdExceeded(errorCounts.flags.bits.bit0);
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

NsmPciePortIntf::NsmPciePortIntf(sdbusplus::bus::bus& bus,
                                 const std::string& name,
                                 const std::string& type,
                                 std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    lg2::info("NsmPciePortIntf: create sensor:{NAME}", "NAME", name.c_str());
    pciePortIntf = std::make_shared<PciePortIntf>(bus,
                                                  inventoryObjPath.c_str());
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
                           std::shared_ptr<PCieEccIntf> pCiePortIntf,
                           uint8_t deviceId) :
    NsmPcieGroup(name, type, deviceId, GROUP_ID_2),
    pCiePortIntf(pCiePortIntf), pCieEccIntf(pCieECCIntf)

{
    lg2::info("NsmPciGroup2: create sensor:{NAME}", "NAME", name.c_str());
}

void NsmPciGroup2::updateReading(
    const nsm_query_scalar_group_telemetry_group_2& data)
{
    pCieEccIntf->nonfeCount(data.non_fatal_errors);
    pCieEccIntf->feCount(data.fatal_errors);
    pCieEccIntf->ceCount(data.correctable_errors);
    // pcie port metrics
    pCiePortIntf->nonfeCount(data.non_fatal_errors);
    pCiePortIntf->feCount(data.fatal_errors);
    pCiePortIntf->ceCount(data.correctable_errors);
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
            "NsmPciGroup2 :: handleResponseMsg:  decode_query_scalar_group_telemetry_v1_group2_resp"
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmPciGroup3::NsmPciGroup3(const std::string& name, const std::string& type,
                           std::shared_ptr<PCieEccIntf> pCieECCIntf,
                           std::shared_ptr<PCieEccIntf> pCiePortIntf,
                           uint8_t deviceId) :
    NsmPcieGroup(name, type, deviceId, GROUP_ID_3),
    pCiePortIntf(pCiePortIntf), pCieEccIntf(pCieECCIntf)

{
    lg2::info("NsmPciGroup2: create sensor:{NAME}", "NAME", name.c_str());
}

void NsmPciGroup3::updateReading(
    const nsm_query_scalar_group_telemetry_group_3& data)
{
    pCieEccIntf->l0ToRecoveryCount(data.L0ToRecoveryCount);
    // pcie port metrics
    pCiePortIntf->l0ToRecoveryCount(data.L0ToRecoveryCount);
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
                           std::shared_ptr<PCieEccIntf> pCiePortIntf,
                           uint8_t deviceId) :
    NsmPcieGroup(name, type, deviceId, GROUP_ID_4),
    pCiePortIntf(pCiePortIntf), pCieEccIntf(pCieECCIntf)

{
    lg2::info("NsmPciGroup4: create sensor:{NAME}", "NAME", name.c_str());
}

void NsmPciGroup4::updateReading(
    const nsm_query_scalar_group_telemetry_group_4& data)
{
    pCieEccIntf->replayCount(data.replay_cnt);
    pCieEccIntf->replayRolloverCount(data.replay_rollover_cnt);
    pCieEccIntf->nakSentCount(data.NAK_sent_cnt);
    pCieEccIntf->nakReceivedCount(data.NAK_recv_cnt);
    // pcie port metrics
    pCiePortIntf->replayCount(data.replay_cnt);
    pCiePortIntf->replayRolloverCount(data.replay_rollover_cnt);
    pCiePortIntf->nakSentCount(data.NAK_sent_cnt);
    pCiePortIntf->nakReceivedCount(data.NAK_recv_cnt);
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

NsmPciGroup5::NsmPciGroup5(
    const std::string& name, const std::string& type,
    std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
    uint8_t deviceId) :
    NsmPcieGroup(name, type, deviceId, GROUP_ID_5)

{
    lg2::info("NsmPciGroup5: create sensor:{NAME}", "NAME", name.c_str());
    processorPerformanceIntf = processorPerfIntf;
    ;
}

void NsmPciGroup5::updateReading(
    const nsm_query_scalar_group_telemetry_group_5& data)
{
    processorPerformanceIntf->pcIeRXBytes(data.PCIeRXBytes);
    processorPerformanceIntf->pcIeTXBytes(data.PCIeTXBytes);
}

uint8_t NsmPciGroup5::handleResponseMsg(const struct nsm_msg* responseMsg,
                                        size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    struct nsm_query_scalar_group_telemetry_group_5 data;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_query_scalar_group_telemetry_v1_group5_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(data);
    }
    else
    {
        lg2::error(
            "handleResponseMsg:  decode_query_scalar_group_telemetry_v1_group5_resp"
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
    auto rc = encode_get_programmable_EDPp_scaling_factor_req(instanceId,
                                                              requestPtr);
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

NsmClockLimitGraphics::NsmClockLimitGraphics(
    const std::string& name, const std::string& type,
    std::shared_ptr<NsmCpuOperatingConfigIntf> cpuConfigIntf) :
    NsmSensor(name, type)

{
    lg2::info("NsmClockLimitGraphics: create sensor:{NAME}", "NAME",
              name.c_str());
    cpuOperatingConfigIntf = cpuConfigIntf;
}

void NsmClockLimitGraphics::updateReading(
    const struct nsm_clock_limit& clockLimit)
{
    cpuOperatingConfigIntf->CpuOperatingConfigIntf::speedLimit(
        clockLimit.present_limit_max);
    if (clockLimit.present_limit_max == clockLimit.present_limit_min)
    {
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::speedLocked(true);
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::speedConfig(
            std::make_tuple(true, (uint32_t)clockLimit.present_limit_max));
    }
    else
    {
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::speedLocked(false);
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::speedConfig(
            std::make_tuple(false, (uint32_t)clockLimit.present_limit_max),
            true);
    }
}

std::optional<std::vector<uint8_t>>
    NsmClockLimitGraphics::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_clock_limit_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    uint8_t clock_id = GRAPHICS_CLOCK;
    auto rc = encode_get_clock_limit_req(instanceId, clock_id, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_clock_limit_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmClockLimitGraphics::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    struct nsm_clock_limit clockLimit;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_clock_limit_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &clockLimit);
    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(clockLimit);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_clock_limit_resp  "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmCurrClockFreq::NsmCurrClockFreq(
    const std::string& name, const std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf) :
    NsmSensor(name, type)

{
    lg2::info("NsmCurrClockFreq: create sensor:{NAME}", "NAME", name.c_str());
    cpuOperatingConfigIntf = cpuConfigIntf;
}

void NsmCurrClockFreq::updateReading(const uint32_t& clockFreq)
{
    cpuOperatingConfigIntf->CpuOperatingConfigIntf::operatingSpeed(clockFreq);
}

std::optional<std::vector<uint8_t>>
    NsmCurrClockFreq::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_curr_clock_freq_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    uint8_t clock_id = GRAPHICS_CLOCK;
    auto rc = encode_get_curr_clock_freq_req(instanceId, clock_id, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_curr_clock_freq_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmCurrClockFreq::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint32_t clockFreq = 1;
    uint16_t data_size;
    uint16_t reason_code;

    auto rc = decode_get_curr_clock_freq_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &clockFreq);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(clockFreq);
    }
    else
    {
        lg2::error(
            "handleResponseMsg:  decode_get_curr_clock_freq_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmMinGraphicsClockLimit::NsmMinGraphicsClockLimit(
    std::string& name, std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf) :
    NsmObject(name, type),
    cpuOperatingConfigIntf(cpuConfigIntf)
{
    lg2::info("NsmMinGraphicsClockLimit: create sensor:{NAME}", "NAME",
              name.c_str());
}

requester::Coroutine NsmMinGraphicsClockLimit::update(SensorManager& manager,
                                                      eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MINIMUM_GRAPHICS_CLOCK_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmMinGraphicsClockLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "NsmMinGraphicsClockLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::minSpeed(value);
    }
    else
    {
        lg2::error(
            "NsmMinGraphicsClockLimit decode_get_inventory_information_resp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
}

NsmMaxGraphicsClockLimit::NsmMaxGraphicsClockLimit(
    std::string& name, std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf) :
    NsmObject(name, type),
    cpuOperatingConfigIntf(cpuConfigIntf)
{
    lg2::info("NsmMaxGraphicsClockLimit: create sensor:{NAME}", "NAME",
              name.c_str());
}

requester::Coroutine NsmMaxGraphicsClockLimit::update(SensorManager& manager,
                                                      eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MAXIMUM_GRAPHICS_CLOCK_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmMaxGraphicsClockLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "NsmMaxGraphicsClockLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::maxSpeed(value);
    }
    else
    {
        lg2::error(
            "NsmMaxGraphicsClockLimit decode_get_inventory_information_resp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
}

const std::string NsmCurrentUtilization::dBusIntf{
    "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig"};

const std::string NsmCurrentUtilization::dBusProperty{"Utilization"};

NsmCurrentUtilization::NsmCurrentUtilization(
    const std::string& name, const std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    const std::string& objPath) :
    NsmSensor(name, type), cpuOperatingConfigIntf(cpuConfigIntf),
    objPath(objPath)
{}

std::optional<std::vector<uint8_t>>
    NsmCurrentUtilization::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_get_current_utilization_req(instanceId, requestPtr);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_current_utilization_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmCurrentUtilization::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint32_t gpu_utilization;
    uint32_t memory_utilization;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_current_utilization_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code,
        &gpu_utilization, &memory_utilization);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        cpuOperatingConfigIntf->utilization(gpu_utilization);

#ifdef NVIDIA_SHMEM
        auto timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());

        DbusVariantType valueVariant{gpu_utilization};

        nv::shmem::AggregationService::updateTelemetry(
            objPath, dBusIntf, dBusProperty, valueVariant, timestamp, 0);
#endif
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_current_utilization_resp  "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);

        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmProcessorThrottleReason::NsmProcessorThrottleReason(
    std::string& name, std::string& type,
    std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf) :
    NsmSensor(name, type)

{
    lg2::info("NsmProcessorThrottleReason: create sensor:{NAME}", "NAME",
              name.c_str());
    processorPerformanceIntf = processorPerfIntf;
}

void NsmProcessorThrottleReason::updateReading(bitfield32_t flags)
{
    std::vector<ThrottleReasons> throttleReasons;

    if (flags.bits.bit0)
    {
        throttleReasons.push_back(ThrottleReasons::ClockOptimizedForPower);
    }
    if (flags.bits.bit1)
    {
        throttleReasons.push_back(ThrottleReasons::HWSlowdown);
    }
    if (flags.bits.bit2)
    {
        throttleReasons.push_back(ThrottleReasons::HWThermalSlowdown);
    }
    if (flags.bits.bit3)
    {
        throttleReasons.push_back(ThrottleReasons::HWPowerBrakeSlowdown);
    }
    if (flags.bits.bit4)
    {
        throttleReasons.push_back(ThrottleReasons::SyncBoost);
    }
    if (flags.bits.bit5)
    {
        throttleReasons.push_back(
            ThrottleReasons::ClockOptimizedForThermalEngage);
    }
    processorPerformanceIntf->throttleReason(throttleReasons);
}

std::optional<std::vector<uint8_t>>
    NsmProcessorThrottleReason::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_current_clock_event_reason_code_req(instanceId,
                                                             requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_current_clock_event_reason_code_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmProcessorThrottleReason::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    bitfield32_t data;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_get_current_clock_event_reason_code_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(data);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_current_clock_event_reason_code_resp  "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmAccumGpuUtilTime::NsmAccumGpuUtilTime(
    const std::string& name, const std::string& type,
    std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf) :
    NsmSensor(name, type)

{
    lg2::info("NsmAccumGpuUtilTime: create sensor:{NAME}", "NAME",
              name.c_str());
    processorPerformanceIntf = processorPerfIntf;
}

void NsmAccumGpuUtilTime::updateReading(const uint32_t& context_util_time,
                                        const uint32_t& SM_util_time)
{
    processorPerformanceIntf->accumulatedGPUContextUtilizationDuration(
        context_util_time);
    processorPerformanceIntf->accumulatedSMUtilizationDuration(SM_util_time);
}

std::optional<std::vector<uint8_t>>
    NsmAccumGpuUtilTime::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_accum_GPU_util_time_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_accum_GPU_util_time_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmAccumGpuUtilTime::handleResponseMsg(const struct nsm_msg* responseMsg,
                                           size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint32_t context_util_time;
    uint32_t SM_util_time;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_get_accum_GPU_util_time_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code,
        &context_util_time, &SM_util_time);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(context_util_time, SM_util_time);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_accum_GPU_util_time_resp  "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}


NsmPowerCap::NsmPowerCap(std::string& name, std::string& type,
                         std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
                         const std::vector<std::string>& parents) :
    NsmSensor(name, type),
    powerCapIntf(powerCapIntf), parents(parents)
{}

NsmTotalCacheMemory::NsmTotalCacheMemory(
    const std::string& name, const std::string& type,
    std::shared_ptr<PersistentMemoryInterface> persistentMemoryInterface) :
    NsmMemoryCapacity(name, type),
    persistentMemoryInterface(persistentMemoryInterface)
{
    lg2::info("NsmTotalCacheMemory : create sensor:{NAME}", "NAME",
              name.c_str());
}

void NsmTotalCacheMemory::updateReading(uint32_t* maximumMemoryCapacity)
{
    if (maximumMemoryCapacity == NULL)
    {
        lg2::error(
            "NsmTotalCacheMemory::updateReading unable to fetch Maximum Memory Capacity");
        return;
    }
    uint64_t cacheSize = *maximumMemoryCapacity;
    persistentMemoryInterface->cacheSizeInKiB(cacheSize * 1024);
}

NsmProcessorRevision::NsmProcessorRevision(sdbusplus::bus::bus& bus,
                                           const std::string& name,
                                           const std::string& type,
                                           std::string& inventoryObjPath) :
    NsmSensor(name, type)

{
    lg2::info("NsmProcessorRevision: create sensor:{NAME}", "NAME",
              name.c_str());
    revisionIntf = std::make_unique<RevisionIntf>(bus,
                                                  inventoryObjPath.c_str());
}

std::optional<std::vector<uint8_t>>
    NsmProcessorRevision::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_inventory_information_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(
        instanceId, DEVICE_PART_NUMBER, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_inventory_information_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmProcessorRevision::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    std::vector<uint8_t> data(65535, 0);
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_inventory_information_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        std::string revision(data.begin(), data.end());
        revisionIntf->version(revision);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_inventory_information_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmGpuHealth::NsmGpuHealth(sdbusplus::bus::bus& bus, std::string& name,
                           std::string& type, std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    healthIntf = std::make_unique<GpuHealthIntf>(bus, inventoryObjPath.c_str());
    healthIntf->health(GpuHealthType::OK);
}

void NsmPowerCap::updateReading(uint32_t value)
{
    // calling parent powercap to update the value on dbus
    powerCapIntf->PowerCapIntf::powerCap(value);
    powerCapIntf->PowerCapIntf::powerCapEnable(true);
    SensorManager& manager = SensorManager::getInstance();
    for (auto it = parents.begin(); it != parents.end();)
    {
        auto sensorIt = manager.objectPathToSensorMap.find(*it);
        if (sensorIt != manager.objectPathToSensorMap.end())
        {
            auto sensor = sensorIt->second;
            if (sensor)
            {
                sensorCache.emplace_back(
                    std::dynamic_pointer_cast<NsmPowerControl>(sensor));
                it = parents.erase(it);
                continue;
            }
        }
        ++it;
    }
    // update each cached sensor
    for (const auto& sensor : sensorCache)
    {
        sensor->updatePowerCapValue(getName(), value);
    }
}

std::optional<std::vector<uint8_t>>
    NsmPowerCap::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(struct nsm_get_power_limit_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_device_power_limit_req(instanceId, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_device_power_limit_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmPowerCap::handleResponseMsg(const struct nsm_msg* responseMsg,
                                       size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t requested_persistent_limit_in_miliwatts = 0;
    uint32_t requested_oneshot_limit_in_miliwatts = 0;
    uint32_t enforced_limit_in_miliwatts = 0;

    auto rc = decode_get_power_limit_resp(
        responseMsg, responseLen, &cc, &dataSize, &reason_code,
        &requested_persistent_limit_in_miliwatts,
        &requested_oneshot_limit_in_miliwatts, &enforced_limit_in_miliwatts);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(enforced_limit_in_miliwatts / 1000);
    }
    else
    {
        lg2::error(
            "decode_get_power_limit_resp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmMaxPowerCap::NsmMaxPowerCap(
    std::string& name, std::string& type,
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
    std::shared_ptr<PowerLimitIface> powerLimitIntf) :
    NsmObject(name, type),
    powerCapIntf(powerCapIntf), powerLimitIntf(powerLimitIntf)
{}

void NsmMaxPowerCap::updateValue(uint32_t value)
{
    powerCapIntf->maxPowerCapValue(value);
    powerLimitIntf->maxPowerWatts(value);
    lg2::info("NsmMaxPowerCap::updateValue {VALUE}", "VALUE", value);
}

requester::Coroutine NsmMaxPowerCap::update(SensorManager& manager, eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MAXIMUM_DEVICE_POWER_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "NsmMaxPowerCap SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        // miliwatts to Watts
        updateValue(value / 1000);
    }
    else
    {
        lg2::error(
            "decode_get_inventory_information_resp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
}

NsmMinPowerCap::NsmMinPowerCap(
    std::string& name, std::string& type,
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
    std::shared_ptr<PowerLimitIface> powerLimitIntf) :
    NsmObject(name, type),
    powerCapIntf(powerCapIntf), powerLimitIntf(powerLimitIntf)
{}

void NsmMinPowerCap::updateValue(uint32_t value)
{
    powerCapIntf->minPowerCapValue(value);
    powerLimitIntf->minPowerWatts(value);
    lg2::info("NsmMinPowerCap::updateValue {VALUE}", "VALUE", value);
}

requester::Coroutine NsmMinPowerCap::update(SensorManager& manager, eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MINIMUM_DEVICE_POWER_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "NsmMinPowerCap SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        // miliwatts to Watts
        updateValue(value / 1000);
    }
    else
    {
        lg2::error(
            "decode_get_inventory_information_resp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
}

NsmDefaultPowerCap::NsmDefaultPowerCap(
    std::string& name, std::string& type,
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf) :
    NsmObject(name, type),
    clearPowerCapIntf(clearPowerCapIntf)
{}

void NsmDefaultPowerCap::updateValue(uint32_t value)
{
    clearPowerCapIntf->defaultPowerCap(value);
    lg2::info("NsmDefaultPowerCap::updateValue {VALUE}", "VALUE", value);
}

requester::Coroutine NsmDefaultPowerCap::update(SensorManager& manager,
                                                eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = RATED_DEVICE_POWER_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "NsmDefaultPowerCap SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        // miliwatts to Watts
        updateValue(value / 1000);
    }
    else
    {
        lg2::error(
            "decode_get_inventory_information_resp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
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
            auto associations =
                utils::getAssociations(objPath, interface + ".Associations");
            auto associationSensor = std::make_shared<NsmProcessorAssociation>(
                bus, name, type, inventoryObjPath, associations);
            nsmDevice->deviceSensors.push_back(associationSensor);

            auto sensor = std::make_shared<NsmAcceleratorIntf>(
                bus, name, type, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(sensor);

            auto deviceUuid = utils::DBusHandler().getDbusProperty<uuid_t>(
                objPath.c_str(), "DEVICE_UUID", PROCESSOR_INTERFACE);

            auto uuidSensor = std::make_shared<NsmUuidIntf>(
                bus, name, type, inventoryObjPath, deviceUuid);
            nsmDevice->deviceSensors.push_back(uuidSensor);
            auto gpuRevisionSensor = std::make_shared<NsmProcessorRevision>(
                bus, name, type, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(gpuRevisionSensor);
            gpuRevisionSensor->update(manager, manager.getEid(nsmDevice))
                .detach();

            auto persistentMemoryIntf =
                std::make_shared<PersistentMemoryInterface>(
                    bus, inventoryObjPath.c_str());
            auto cacheMemorySensor = std::make_shared<NsmTotalCacheMemory>(
                name, type, persistentMemoryIntf);
            nsmDevice->deviceSensors.push_back(cacheMemorySensor);
            cacheMemorySensor->update(manager, manager.getEid(nsmDevice))
                .detach();

            auto healthSensor = std::make_shared<NsmGpuHealth>(
                bus, name, type, inventoryObjPath);
            nsmDevice->deviceSensors.push_back(healthSensor);
        }
        else if (type == "NSM_Location")
        {
            auto locationType =
                utils::DBusHandler().getDbusProperty<std::string>(
                    objPath.c_str(), "LocationType", interface.c_str());
            auto sensor = std::make_shared<NsmLocationIntfProcessor>(
                bus, name, type, inventoryObjPath, locationType);
            nsmDevice->deviceSensors.push_back(sensor);
        }
        else if (type == "NSM_LocationCode")
        {
            auto locationCode =
                utils::DBusHandler().getDbusProperty<std::string>(
                    objPath.c_str(), "LocationCode", interface.c_str());
            auto sensor = std::make_shared<NsmLocationCodeIntfProcessor>(
                bus, name, type, inventoryObjPath, locationCode);
            nsmDevice->deviceSensors.push_back(sensor);
        }
        else if (type == "NSM_Asset")
        {
            auto assetIntf = std::make_shared<AssetIntfProcessor>(
                bus, inventoryObjPath.c_str());
            auto manufacturer =
                utils::DBusHandler().getDbusProperty<std::string>(
                    objPath.c_str(), "Manufacturer", interface.c_str());

            auto assetObject = NsmAssetIntfProcessor<AssetIntfProcessor>(
                name, type, assetIntf);
            assetObject.pdi().manufacturer(manufacturer);
            auto eid = manager.getEid(nsmDevice);
            // create sensor
            nsmDevice
                ->addStaticSensor(
                    std::make_shared<NsmInventoryProperty<AssetIntfProcessor>>(
                        assetObject, DEVICE_PART_NUMBER))
                .update(manager, eid)
                .detach();
            nsmDevice
                ->addStaticSensor(
                    std::make_shared<NsmInventoryProperty<AssetIntfProcessor>>(
                        assetObject, SERIAL_NUMBER))
                .update(manager, eid)
                .detach();
            nsmDevice
                ->addStaticSensor(
                    std::make_shared<NsmInventoryProperty<AssetIntfProcessor>>(
                        assetObject, MARKETING_NAME))
                .update(manager, eid)
                .detach();
        }
        else if (type == "NSM_MIG")
        {
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());

            auto sensor = std::make_shared<NsmMigMode>(
                bus, name, type, inventoryObjPath, nsmDevice);
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
            int count = utils::DBusHandler().getDbusProperty<uint64_t>(
                objPath.c_str(), "Count", interface.c_str());
            for (auto idx = 0; idx < count; idx++)
            {
                auto pcieObjPath =
                    inventoryObjPath + "/Ports/PCIe_" +
                    std::to_string(idx); // port metrics dbus path
                auto pCieECCIntf = std::make_shared<PCieEccIntf>(
                    bus, inventoryObjPath.c_str());
                auto pCiePortIntf =
                    std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
                auto pciPortSensor = std::make_shared<NsmPciePortIntf>(
                    bus, name, type, pcieObjPath);
                auto sensorGroup2 = std::make_shared<NsmPciGroup2>(
                    name, type, pCieECCIntf, pCiePortIntf, deviceId);
                auto sensorGroup3 = std::make_shared<NsmPciGroup3>(
                    name, type, pCieECCIntf, pCiePortIntf, deviceId);
                auto sensorGroup4 = std::make_shared<NsmPciGroup4>(
                    name, type, pCieECCIntf, pCiePortIntf, deviceId);
                nsmDevice->deviceSensors.push_back(pciPortSensor);
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
        }
        else if (type == "NSM_ECC")
        {
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());

            auto eccIntf = std::make_shared<NsmEccModeIntf>(
                bus, inventoryObjPath.c_str(), nsmDevice);

            auto eccModeSensor = std::make_shared<NsmEccMode>(name, type,
                                                              eccIntf);

            eccModeSensor->update(manager, manager.getEid(nsmDevice)).detach();

            nsmDevice->deviceSensors.push_back(eccModeSensor);

            auto eccErrorCntSensor =
                std::make_shared<NsmEccErrorCounts>(name, type, eccIntf);

            nsmDevice->deviceSensors.push_back(eccErrorCntSensor);

            if (priority)
            {
                nsmDevice->prioritySensors.push_back(eccErrorCntSensor);
            }
            else
            {
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
        else if (type == "NSM_CpuOperatingConfig")
        {
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());
            auto cpuOperatingConfigIntf =
                std::make_shared<NsmCpuOperatingConfigIntf>(
                    bus, inventoryObjPath.c_str(), nsmDevice, GRAPHICS_CLOCK);

            auto clockFreqSensor = std::make_shared<NsmCurrClockFreq>(
                name, type, cpuOperatingConfigIntf);
            auto clockLimitSensor = std::make_shared<NsmClockLimitGraphics>(
                name, type, cpuOperatingConfigIntf);
            auto minGraphicsClockFreq =
                std::make_shared<NsmMinGraphicsClockLimit>(
                    name, type, cpuOperatingConfigIntf);
            auto maxGraphicsClockFreq =
                std::make_shared<NsmMaxGraphicsClockLimit>(
                    name, type, cpuOperatingConfigIntf);
            auto currentUtilization = std::make_shared<NsmCurrentUtilization>(
                name + "_CurrentUtilization", type, cpuOperatingConfigIntf,
                inventoryObjPath);

            nsmDevice->deviceSensors.emplace_back(minGraphicsClockFreq);
            nsmDevice->deviceSensors.emplace_back(maxGraphicsClockFreq);
            nsmDevice->deviceSensors.emplace_back(currentUtilization);

            if (priority)
            {
                nsmDevice->prioritySensors.push_back(clockFreqSensor);
                nsmDevice->prioritySensors.push_back(clockLimitSensor);
                nsmDevice->prioritySensors.push_back(currentUtilization);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(clockFreqSensor);
                nsmDevice->roundRobinSensors.push_back(clockLimitSensor);
                nsmDevice->roundRobinSensors.push_back(currentUtilization);
            }
            minGraphicsClockFreq->update(manager, manager.getEid(nsmDevice))
                .detach();
            maxGraphicsClockFreq->update(manager, manager.getEid(nsmDevice))
                .detach();
        }
        else if (type == "NSM_ProcessorPerformance")
        {
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());
            auto deviceId = utils::DBusHandler().getDbusProperty<uint64_t>(
                objPath.c_str(), "DeviceId", interface.c_str());

            auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
                bus, inventoryObjPath.c_str());

            auto throttleReasonSensor =
                std::make_shared<NsmProcessorThrottleReason>(name, type,
                                                             processorPerfIntf);

            auto gpuUtilSensor = std::make_shared<NsmAccumGpuUtilTime>(
                name, type, processorPerfIntf);
            auto pciRxTxSensor = std::make_shared<NsmPciGroup5>(
                name, type, processorPerfIntf, deviceId);

            if (priority)
            {
                nsmDevice->prioritySensors.push_back(gpuUtilSensor);
                nsmDevice->prioritySensors.push_back(pciRxTxSensor);
                nsmDevice->prioritySensors.push_back(throttleReasonSensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(gpuUtilSensor);
                nsmDevice->roundRobinSensors.push_back(pciRxTxSensor);
                nsmDevice->roundRobinSensors.push_back(throttleReasonSensor);
            }
        }
        else if (type == "NSM_MemCapacityUtil")
        {
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());
            auto totalMemorySensor = std::make_shared<NsmTotalMemory>(name,
                                                                      type);
            nsmDevice->deviceSensors.push_back(totalMemorySensor);
            totalMemorySensor->update(manager, manager.getEid(nsmDevice))
                .detach();
            auto sensor = std::make_shared<NsmMemoryCapacityUtil>(
                bus, name, type, inventoryObjPath, totalMemorySensor);
            nsmDevice->deviceSensors.push_back(sensor);
            if (priority)
            {
                nsmDevice->prioritySensors.push_back(sensor);
                nsmDevice->prioritySensors.push_back(totalMemorySensor);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(sensor);
                nsmDevice->roundRobinSensors.push_back(totalMemorySensor);
            }
        }
        else if (type == "NSM_PowerCap")
        {
            std::vector<std::string> candidateForList;
            try
            {
                candidateForList =
                    utils::DBusHandler()
                        .getDbusProperty<std::vector<std::string>>(
                            objPath.c_str(), "CompositeNumericSensors",
                            interface.c_str());
            }
            catch (const sdbusplus::exception::SdBusError& e)
            {}
            auto priority = utils::DBusHandler().getDbusProperty<bool>(
                objPath.c_str(), "Priority", interface.c_str());
            // create power cap , clear power cap and power limit interface
            auto powerCapIntf = std::make_shared<NsmPowerCapIntf>(
                bus, inventoryObjPath.c_str(), name, candidateForList,
                nsmDevice);

            auto clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(
                bus, inventoryObjPath.c_str(), nsmDevice, powerCapIntf);

            auto powerLimitIntf = std::make_shared<PowerLimitIface>(
                bus, inventoryObjPath.c_str());

            // create sensors for power cap properties
            auto powerCap = std::make_shared<NsmPowerCap>(
                name, type, powerCapIntf, candidateForList);
            nsmDevice->deviceSensors.emplace_back(powerCap);
            nsmDevice->capabilityRefreshSensors.emplace_back(powerCap);
            manager.powerCapList.emplace_back(powerCap);

            auto defaultPowerCap = std::make_shared<NsmDefaultPowerCap>(
                name, type, clearPowerCapIntf);
            nsmDevice->deviceSensors.emplace_back(defaultPowerCap);
            manager.defaultPowerCapList.emplace_back(defaultPowerCap);

            auto maxPowerCap = std::make_shared<NsmMaxPowerCap>(
                name, type, powerCapIntf, powerLimitIntf);
            nsmDevice->deviceSensors.emplace_back(maxPowerCap);
            manager.maxPowerCapList.emplace_back(maxPowerCap);

            auto minPowerCap = std::make_shared<NsmMinPowerCap>(
                name, type, powerCapIntf, powerLimitIntf);
            nsmDevice->deviceSensors.emplace_back(minPowerCap);
            manager.minPowerCapList.emplace_back(minPowerCap);

            if (priority)
            {
                nsmDevice->prioritySensors.push_back(powerCap);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(powerCap);
            }

            defaultPowerCap->update(manager, manager.getEid(nsmDevice))
                .detach();
            maxPowerCap->update(manager, manager.getEid(nsmDevice)).detach();
            minPowerCap->update(manager, manager.getEid(nsmDevice)).detach();
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
REGISTER_NSM_CREATION_FUNCTION(
    createNsmProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_Processor.ProcessorPerformance")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_Processor.CpuOperatingConfig")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_Processor.MemCapacityUtil")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_Processor.Location")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_Processor.LocationCode")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_Processor.Asset")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmProcessorSensor,
    "xyz.openbmc_project.Configuration.NSM_Processor.PowerCap")

} // namespace nsm
