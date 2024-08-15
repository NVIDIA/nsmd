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

#include "device-configuration.h"
#include "pci-links.h"
#include "platform-environmental.h"

#include "asyncOperationManager.hpp"
#include "deviceManager.hpp"
#include "nsmCommon/sharedMemCommon.hpp"
#include "nsmDevice.hpp"
#include "nsmInterface.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPort/nsmPortDisableFuture.hpp"
#include "nsmPowerSmoothing.hpp"
#include "nsmReconfigPermissions.hpp"
#include "nsmSetCpuOperatingConfig.hpp"
#include "nsmSetECCMode.hpp"

#include <stdint.h>

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>

#include <cstdint>
#include <optional>
#include <vector>

#define PROCESSOR_INTERFACE "xyz.openbmc_project.Configuration.NSM_Processor"

using std::filesystem::path;

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
    NsmObject(name, type),
    inventoryObjPath(inventoryObjPath)
{
    uuidIntf = std::make_unique<UuidIntf>(bus, inventoryObjPath.c_str());
    uuidIntf->uuid(uuid);
}

void NsmUuidIntf::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(uuidIntf->interface);
    nv::sensor_aggregation::DbusVariantType valueVariant{uuidIntf->uuid()};
    std::vector<uint8_t> smbusData = {};
    std::string propName = "UUID";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, valueVariant);
#endif
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
            updateMetricOnSharedMemory();
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
                       [[maybe_unused]] std::shared_ptr<NsmDevice> device) :
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmMigMode: create sensor:{NAME}", "NAME", name.c_str());
    migModeIntf = std::make_unique<MigModeIntf>(bus, inventoryObjPath.c_str());
    updateMetricOnSharedMemory();
}

void NsmMigMode::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(migModeIntf->interface);
    std::vector<uint8_t> smbusData = {};
    nv::sensor_aggregation::DbusVariantType migModeEnabled{
        migModeIntf->MigModeIntf::migModeEnabled()};
    std::string propName = "MIGModeEnabled";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, migModeEnabled);
#endif
}

void NsmMigMode::updateReading(bitfield8_t flags)
{
    migModeIntf->MigModeIntf::migModeEnabled(flags.bits.bit0);
    updateMetricOnSharedMemory();
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
                       std::shared_ptr<EccModeIntf> eccIntf,
                       std::string& inventoryObjPath) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath)

{
    eccModeIntf = eccIntf;
    updateMetricOnSharedMemory();
}

void NsmEccMode::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(eccModeIntf->interface);
    std::vector<uint8_t> smbusData = {};

    nv::sensor_aggregation::DbusVariantType eccModeEnabled{
        eccModeIntf->EccModeIntf::eccModeEnabled()};
    std::string propName = "ECCModeEnabled";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, eccModeEnabled);

    propName = "PendingECCState";
    nv::sensor_aggregation::DbusVariantType pendingECCState{
        eccModeIntf->EccModeIntf::pendingECCState()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, pendingECCState);
#endif
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

void NsmEccMode::updateReading(bitfield8_t flags)
{
    eccModeIntf->EccModeIntf::eccModeEnabled(flags.bits.bit0);
    eccModeIntf->EccModeIntf::pendingECCState(flags.bits.bit1);
    updateMetricOnSharedMemory();
}

NsmEccErrorCounts::NsmEccErrorCounts(std::string& name, std::string& type,
                                     std::shared_ptr<EccModeIntf> eccIntf,
                                     std::string& inventoryObjPath) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmEccErrorCounts: create sensor:{NAME}", "NAME", name.c_str());
    eccErrorCountIntf = eccIntf;
    updateMetricOnSharedMemory();
}
void NsmEccErrorCounts::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(eccErrorCountIntf->interface);
    std::vector<uint8_t> smbusData = {};

    nv::sensor_aggregation::DbusVariantType ceCountOnSharedMem{
        static_cast<int64_t>(eccErrorCountIntf->ceCount())};
    std::string propName = "ceCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, ceCountOnSharedMem);

    propName = "ueCount";
    nv::sensor_aggregation::DbusVariantType ueCountOnSharedMem{
        static_cast<int64_t>(eccErrorCountIntf->ueCount())};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, ueCountOnSharedMem);
#endif
}

void NsmEccErrorCounts::updateReading(struct nsm_ECC_error_counts errorCounts)
{
    eccErrorCountIntf->ceCount(errorCounts.sram_corrected);
    int64_t ueCount = errorCounts.sram_uncorrected_secded +
                      errorCounts.sram_uncorrected_parity;
    eccErrorCountIntf->ueCount(ueCount);

    eccErrorCountIntf->isThresholdExceeded(errorCounts.flags.bits.bit0);
    updateMetricOnSharedMemory();
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
                           uint8_t deviceId, std::string& inventoryObjPath) :
    NsmPcieGroup(name, type, deviceId, GROUP_ID_2),
    pCiePortIntf(pCiePortIntf), pCieEccIntf(pCieECCIntf),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmPciGroup2: create sensor:{NAME}", "NAME", name.c_str());
    updateMetricOnSharedMemory();
}

void NsmPciGroup2::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(pCieEccIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "nonfeCount";
    nv::sensor_aggregation::DbusVariantType nonfeCountVal{
        pCieEccIntf->nonfeCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, nonfeCountVal);

    propName = "feCount";
    nv::sensor_aggregation::DbusVariantType feCountVal{pCieEccIntf->feCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, feCountVal);

    propName = "ceCount";
    nv::sensor_aggregation::DbusVariantType ceCountVal{pCieEccIntf->ceCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, ceCountVal);

    // pcie port metrics
    ifaceName = std::string(pCiePortIntf->interface);
    propName = "nonfeCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, ceCountVal);

    ifaceName = std::string(pCiePortIntf->interface);
    propName = "feCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, feCountVal);

    ifaceName = std::string(pCiePortIntf->interface);
    propName = "ceCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, ceCountVal);
#endif
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

    updateMetricOnSharedMemory();
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
                           uint8_t deviceId, std::string& inventoryObjPath) :
    NsmPcieGroup(name, type, deviceId, GROUP_ID_3),
    pCiePortIntf(pCiePortIntf), pCieEccIntf(pCieECCIntf),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmPciGroup2: create sensor:{NAME}", "NAME", name.c_str());
    updateMetricOnSharedMemory();
}

void NsmPciGroup3::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(pCieEccIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "L0ToRecoveryCount";
    nv::sensor_aggregation::DbusVariantType l0ToRecoveryCountVal{
        pCieEccIntf->l0ToRecoveryCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, l0ToRecoveryCountVal);

    // pcie port metrics
    ifaceName = std::string(pCiePortIntf->interface);
    propName = "L0ToRecoveryCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, l0ToRecoveryCountVal);
#endif
}

void NsmPciGroup3::updateReading(
    const nsm_query_scalar_group_telemetry_group_3& data)
{
    pCieEccIntf->l0ToRecoveryCount(data.L0ToRecoveryCount);
    // pcie port metrics
    pCiePortIntf->l0ToRecoveryCount(data.L0ToRecoveryCount);
    updateMetricOnSharedMemory();
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
                           uint8_t deviceId, std::string& inventoryObjPath) :
    NsmPcieGroup(name, type, deviceId, GROUP_ID_4),
    pCiePortIntf(pCiePortIntf), pCieEccIntf(pCieECCIntf),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmPciGroup4: create sensor:{NAME}", "NAME", name.c_str());
    updateMetricOnSharedMemory();
}

void NsmPciGroup4::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(pCieEccIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "ReplayCount";
    nv::sensor_aggregation::DbusVariantType replayCountVal{
        pCieEccIntf->replayCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, replayCountVal);

    propName = "ReplayRolloverCount";
    nv::sensor_aggregation::DbusVariantType replayRolloverCountVal{
        pCieEccIntf->replayRolloverCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 replayRolloverCountVal);

    propName = "NAKSentCount";
    nv::sensor_aggregation::DbusVariantType nakSentCountVal{
        pCieEccIntf->nakSentCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, nakSentCountVal);

    propName = "NAKReceivedCount";
    nv::sensor_aggregation::DbusVariantType nakReceivedCountVal{
        pCieEccIntf->nakReceivedCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, nakReceivedCountVal);

    // pcie port metrics
    propName = "ReplayCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, replayCountVal);

    propName = "ReplayRolloverCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 replayRolloverCountVal);

    propName = "NAKSentCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, nakSentCountVal);

    propName = "NAKReceivedCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, nakReceivedCountVal);
#endif
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
    updateMetricOnSharedMemory();
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
    uint8_t deviceId, std::string& inventoryObjPath) :
    NsmPcieGroup(name, type, deviceId, GROUP_ID_5),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmPciGroup5: create sensor:{NAME}", "NAME", name.c_str());
    processorPerformanceIntf = processorPerfIntf;
    updateMetricOnSharedMemory();
}

void NsmPciGroup5::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(processorPerformanceIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "PCIeRXBytes";
    nv::sensor_aggregation::DbusVariantType pcIeRXBytesVal{
        processorPerformanceIntf->pcIeRXBytes()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, pcIeRXBytesVal);

    propName = "PCIeTXBytes";
    nv::sensor_aggregation::DbusVariantType pcIeTXBytesVal{
        processorPerformanceIntf->pcIeTXBytes()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, pcIeTXBytesVal);

#endif
}

void NsmPciGroup5::updateReading(
    const nsm_query_scalar_group_telemetry_group_5& data)
{
    processorPerformanceIntf->pcIeRXBytes(data.PCIeRXBytes);
    processorPerformanceIntf->pcIeTXBytes(data.PCIeTXBytes);
    updateMetricOnSharedMemory();
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
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmEDPpScalingFactor: create sensor:{NAME}", "NAME",
              name.c_str());
    eDPpIntf = std::make_shared<EDPpLocal>(bus, inventoryObjPath.c_str());
    updateMetricOnSharedMemory();
}
void NsmEDPpScalingFactor::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(eDPpIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "AllowableMax";
    nv::sensor_aggregation::DbusVariantType allowableMaxVal{
        eDPpIntf->allowableMax()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, allowableMaxVal);

    propName = "AllowableMin";
    nv::sensor_aggregation::DbusVariantType allowableMinVal{
        eDPpIntf->allowableMin()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, allowableMinVal);
#endif
}

void NsmEDPpScalingFactor::updateReading(
    struct nsm_EDPp_scaling_factors scaling_factors)
{
    eDPpIntf->allowableMax(scaling_factors.maximum_scaling_factor);
    eDPpIntf->allowableMin(scaling_factors.minimum_scaling_factor);
    updateMetricOnSharedMemory();
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
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmClockLimitGraphics: create sensor:{NAME}", "NAME",
              name.c_str());
    cpuOperatingConfigIntf = cpuConfigIntf;
    updateMetricOnSharedMemory();
}

void NsmClockLimitGraphics::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(cpuOperatingConfigIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "SpeedLocked";
    nv::sensor_aggregation::DbusVariantType speedLockedVal{
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::speedLocked()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, speedLockedVal);

    propName = "SpeedConfig";
    nv::sensor_aggregation::DbusVariantType speedConfigVal{
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::speedConfig()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, speedConfigVal);
#endif
}

void NsmClockLimitGraphics::updateReading(
    const struct nsm_clock_limit& clockLimit)
{
    cpuOperatingConfigIntf->speedLimit(clockLimit.present_limit_max);
    if (clockLimit.present_limit_max == clockLimit.present_limit_min)
    {
        cpuOperatingConfigIntf->speedLocked(true);
        cpuOperatingConfigIntf->speedConfig(
            std::make_tuple(true, (uint32_t)clockLimit.present_limit_max));
    }
    else
    {
        cpuOperatingConfigIntf->speedLocked(false);
        cpuOperatingConfigIntf->speedConfig(
            std::make_tuple(false, (uint32_t)clockLimit.present_limit_max),
            true);
    }
    updateMetricOnSharedMemory();
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
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmCurrClockFreq: create sensor:{NAME}", "NAME", name.c_str());
    cpuOperatingConfigIntf = cpuConfigIntf;
    updateMetricOnSharedMemory();
}

void NsmCurrClockFreq::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(cpuOperatingConfigIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "OperatingSpeed";
    nv::sensor_aggregation::DbusVariantType operatingSpeedVal{
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::operatingSpeed()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, operatingSpeedVal);

#endif
}

void NsmCurrClockFreq::updateReading(const uint32_t& clockFreq)
{
    cpuOperatingConfigIntf->CpuOperatingConfigIntf::operatingSpeed(clockFreq);
    updateMetricOnSharedMemory();
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

NsmDefaultBaseClockSpeed::NsmDefaultBaseClockSpeed(
    std::string& name, std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf) :
    NsmObject(name, type), cpuOperatingConfigIntf(cpuConfigIntf)
{
    lg2::info("NsmDefaultBaseClockSpeed: create sensor:{NAME}", "NAME",
              name.c_str());
}

requester::Coroutine NsmDefaultBaseClockSpeed::update(SensorManager& manager,
                                                      eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = DEFAULT_BASE_CLOCKS;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmDefaultBaseClockSpeed: encode_get_inventory_information_req failed. eid={EID} rc={RC}",
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
            "NsmDefaultBaseClockSpeed SendRecvNsmMsg failed with RC={RC}, eid={EID}",
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
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::baseSpeed(value);
    }
    else
    {
        lg2::error(
            "NsmDefaultBaseClockSpeed decode_get_inventory_information_resp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
}

NsmDefaultBoostClockSpeed::NsmDefaultBoostClockSpeed(
    std::string& name, std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf) :
    NsmObject(name, type), cpuOperatingConfigIntf(cpuConfigIntf)
{
    lg2::info("NsmDefaultBoostClockSpeed: create sensor:{NAME}", "NAME",
              name.c_str());
}

requester::Coroutine NsmDefaultBoostClockSpeed::update(SensorManager& manager,
                                                       eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = DEFAULT_BOOST_CLOCKS;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmDefaultBoostClockSpeed: encode_get_inventory_information_req failed. eid={EID} rc={RC}",
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
            "NsmDefaultBoostClockSpeed: SendRecvNsmMsg failed with RC={RC}, eid={EID}",
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
        cpuOperatingConfigIntf
            ->CpuOperatingConfigIntf::defaultBoostClockSpeedMHz(value);
    }
    else
    {
        lg2::error(
            "NsmDefaultBoostClockSpeed: decode_get_inventory_information_resp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
}

NsmCurrentUtilization::NsmCurrentUtilization(
    const std::string& name, const std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type),
    cpuOperatingConfigIntf(cpuConfigIntf), inventoryObjPath(inventoryObjPath)
{
    updateMetricOnSharedMemory();
}

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

void NsmCurrentUtilization::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(cpuOperatingConfigIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "Utilization";
    nv::sensor_aggregation::DbusVariantType utilizationVal{
        cpuOperatingConfigIntf->utilization()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, utilizationVal);
#endif
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
        updateMetricOnSharedMemory();
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
    std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmProcessorThrottleReason: create sensor:{NAME}", "NAME",
              name.c_str());
    processorPerformanceIntf = processorPerfIntf;
    std::vector<ThrottleReasons> throttleReasons;
    throttleReasons.push_back(ThrottleReasons::None);
    processorPerformanceIntf->throttleReason(throttleReasons);
    updateMetricOnSharedMemory();
}
void NsmProcessorThrottleReason::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(processorPerformanceIntf->interface);
    std::vector<uint8_t> smbusData = {};
    std::vector<std::string> throttleReasonsForShmem;

    for (auto tr : processorPerformanceIntf->throttleReason())
    {
        throttleReasonsForShmem.push_back(
            processorPerformanceIntf->convertThrottleReasonsToString(tr));
    }

    std::string propName = "ThrottleReason";
    nv::sensor_aggregation::DbusVariantType throttleReasonVal{
        throttleReasonsForShmem};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, throttleReasonVal);

#endif
}

void NsmProcessorThrottleReason::updateReading(bitfield32_t flags)
{
    std::vector<ThrottleReasons> throttleReasons;

    if (flags.bits.bit0)
    {
        throttleReasons.push_back(ThrottleReasons::SWPowerCap);
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
    if (throttleReasons.size() == 0)
    {
        throttleReasons.push_back(ThrottleReasons::None);
    }
    processorPerformanceIntf->throttleReason(throttleReasons);
    updateMetricOnSharedMemory();
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
    std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmAccumGpuUtilTime: create sensor:{NAME}", "NAME",
              name.c_str());
    processorPerformanceIntf = processorPerfIntf;
    updateMetricOnSharedMemory();
}

void NsmAccumGpuUtilTime::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(processorPerformanceIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "AccumulatedGPUContextUtilizationDuration";
    nv::sensor_aggregation::DbusVariantType
        accumulatedGPUContextUtilizationDurationVal{
            processorPerformanceIntf
                ->accumulatedGPUContextUtilizationDuration()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData,
        accumulatedGPUContextUtilizationDurationVal);

    propName = "AccumulatedSMUtilizationDuration";
    nv::sensor_aggregation::DbusVariantType accumulatedSMUtilizationDurationVal{
        processorPerformanceIntf->accumulatedSMUtilizationDuration()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData,
        accumulatedSMUtilizationDurationVal);

#endif
}

void NsmAccumGpuUtilTime::updateReading(const uint32_t& context_util_time,
                                        const uint32_t& SM_util_time)
{
    processorPerformanceIntf->accumulatedGPUContextUtilizationDuration(
        context_util_time);
    processorPerformanceIntf->accumulatedSMUtilizationDuration(SM_util_time);
    updateMetricOnSharedMemory();
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

NsmTotalMemorySize::NsmTotalMemorySize(
    std::string& name, std::string& type,
    std::shared_ptr<PersistentMemoryInterface> persistentMemoryInterface) :
    NsmObject(name, type), persistentMemoryInterface(persistentMemoryInterface)
{
    lg2::info("NsmTotalMemorySize: create sensor:{NAME}", "NAME", name.c_str());
}

requester::Coroutine NsmTotalMemorySize::update(SensorManager& manager,
                                                eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MAXIMUM_MEMORY_CAPACITY;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmTotalMemorySize: encode_get_inventory_information_req failed. eid={EID} rc={RC}",
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
            "NsmTotalMemorySize: SendRecvNsmMsg failed with RC={RC}, eid={EID}",
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
        persistentMemoryInterface->volatileSizeInKiB(value*1024);
    }
    else
    {
        lg2::error(
            "NsmTotalMemorySize: decode_get_inventory_information_resp failed. cc={CC} reasonCode={RESONCODE} and rc={RC}",
            "CC", cc, "RESONCODE", reason_code, "RC", rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
}

NsmTotalNvLinks::NsmTotalNvLinks(
    const std::string& name, const std::string& type,
    std::shared_ptr<TotalNvLinkInterface> totalNvLinkInterface,
    std::string& inventoryObjPath) :
    NsmSensor(name, type), totalNvLinkInterface(totalNvLinkInterface),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmTotalNvLinks: create sensor:{NAME}", "NAME", name.c_str());
    updateMetricOnSharedMemory();
}

void NsmTotalNvLinks::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(totalNvLinkInterface->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "TotalNvLinksCount";
    nv::sensor_aggregation::DbusVariantType totalNvLinksCount{
        totalNvLinkInterface->totalNumberNVLinks()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, totalNvLinksCount);

#endif
}

std::optional<std::vector<uint8_t>>
    NsmTotalNvLinks::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_query_ports_available_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_ports_available_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_query_ports_available_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmTotalNvLinks::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint8_t totalNvLinks;
    uint16_t data_size;
    uint16_t reason_code;

    auto rc = decode_query_ports_available_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &totalNvLinks);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
       totalNvLinkInterface->totalNumberNVLinks(totalNvLinks);
       updateMetricOnSharedMemory();
    }
    else
    {
        lg2::error(
            "NsmTotalNvLinks::handleResponseMsg  decode_query_ports_available_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmProcessorRevision::NsmProcessorRevision(sdbusplus::bus::bus& bus,
                                           const std::string& name,
                                           const std::string& type,
                                           std::string& inventoryObjPath) :
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmProcessorRevision: create sensor:{NAME}", "NAME",
              name.c_str());
    revisionIntf = std::make_unique<RevisionIntf>(bus,
                                                  inventoryObjPath.c_str());
    updateMetricOnSharedMemory();
}

void NsmProcessorRevision::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(revisionIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "Version";
    nv::sensor_aggregation::DbusVariantType versionVal{revisionIntf->version()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, versionVal);

#endif
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
        updateMetricOnSharedMemory();
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

NsmPowerCap::NsmPowerCap(std::string& name, std::string& type,
                         std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
                         const std::vector<std::string>& parents,
                         std::string& inventoryObjPath) :
    NsmSensor(name, type),
    powerCapIntf(powerCapIntf), parents(parents),
    inventoryObjPath(inventoryObjPath)
{}

void NsmPowerCap::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(powerCapIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "PowerCap";
    nv::sensor_aggregation::DbusVariantType powerCapVal{
        powerCapIntf->PowerCapIntf::powerCap()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, powerCapVal);

    propName = "PowerCapEnable";
    nv::sensor_aggregation::DbusVariantType powerCapEnableVal{
        powerCapIntf->PowerCapIntf::powerCapEnable()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, powerCapEnableVal);

#endif
}

void NsmPowerCap::updateReading(uint32_t value)
{
    // calling parent powercap to update the value on dbus
    powerCapIntf->PowerCapIntf::powerCap(value);
    powerCapIntf->PowerCapIntf::powerCapEnable(true);
    updateMetricOnSharedMemory();

    // updating Total Power
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
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf,
    std::shared_ptr<NsmClearPowerCapAsyncIntf> clearPowerCapAsyncIntf) :
    NsmObject(name, type), defaultPowerCapIntf(clearPowerCapIntf),
    clearPowerCapAsyncIntf(clearPowerCapAsyncIntf)
{}

void NsmDefaultPowerCap::updateValue(uint32_t value)
{
    defaultPowerCapIntf->defaultPowerCap(value);
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

NsmProcessorThrottleDuration::NsmProcessorThrottleDuration(
    std::string& name, std::string& type,
    std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type), processorPerformanceIntf(processorPerfIntf),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmProcessorThrottleDuration: create sensor:{NAME}", "NAME",
              name.c_str());
    updateMetricOnSharedMemory();
}
void NsmProcessorThrottleDuration::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM

    auto ifaceName = std::string(processorPerformanceIntf->interface);
    std::vector<uint8_t> smbusData = {};
    std::string propName = "PowerLimitThrottleDuration";
    nv::sensor_aggregation::DbusVariantType powerLimitThrottleDuration{
        processorPerformanceIntf->powerLimitThrottleDuration()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 powerLimitThrottleDuration);

    propName = "ThermalLimitThrottleDuration";
    nv::sensor_aggregation::DbusVariantType thermalLimitThrottleDuration{
        processorPerformanceIntf->thermalLimitThrottleDuration()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 thermalLimitThrottleDuration);

    propName = "GlobalSoftwareViolationThrottleDuration";
    nv::sensor_aggregation::DbusVariantType globalSoftwareViolationThrottleDuration{
        processorPerformanceIntf->globalSoftwareViolationThrottleDuration()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData,
        globalSoftwareViolationThrottleDuration);

    propName = "HardwareViolationThrottleDuration";
    nv::sensor_aggregation::DbusVariantType hardwareViolationThrottleDuration{
        processorPerformanceIntf->hardwareViolationThrottleDuration()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData,
        hardwareViolationThrottleDuration);

#endif
}

void NsmProcessorThrottleDuration::updateReading(
    const nsm_violation_duration& data)
{
    processorPerformanceIntf->powerLimitThrottleDuration(
        data.power_violation_duration);
    processorPerformanceIntf->thermalLimitThrottleDuration(
        data.thermal_violation_duration);
    processorPerformanceIntf->hardwareViolationThrottleDuration(
        data.hw_violation_duration);
    processorPerformanceIntf->globalSoftwareViolationThrottleDuration(
        data.global_sw_violation_duration);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmProcessorThrottleDuration::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_violation_duration_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_violation_duration_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmProcessorThrottleDuration::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    nsm_violation_duration data;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;
    auto rc = decode_get_violation_duration_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &data);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(data);
    }
    else
    {
        lg2::error(
            "handleResponseMsg: decode_get_violation_duration_resp  "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

void createNsmProcessorSensor(SensorManager& manager,
                              const std::string& interface,
                              const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", PROCESSOR_INTERFACE);

    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", PROCESSOR_INTERFACE);

    auto type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());

    auto inventoryObjPath = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "InventoryObjPath", PROCESSOR_INTERFACE);

    auto nsmDevice = manager.getNsmDevice(uuid);

    if (type == "NSM_Processor")
    {
        auto associations = utils::getAssociations(objPath,
                                                   interface + ".Associations");
        auto associationSensor = std::make_shared<NsmProcessorAssociation>(
            bus, name, type, inventoryObjPath, associations);
        nsmDevice->deviceSensors.push_back(associationSensor);

        auto sensor = std::make_shared<NsmAcceleratorIntf>(bus, name, type,
                                                           inventoryObjPath);
        nsmDevice->deviceSensors.push_back(sensor);

        auto deviceUuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "DEVICE_UUID", PROCESSOR_INTERFACE);

        auto uuidSensor = std::make_shared<NsmUuidIntf>(
            bus, name, type, inventoryObjPath, deviceUuid);
        nsmDevice->deviceSensors.push_back(uuidSensor);
        auto gpuRevisionSensor = std::make_shared<NsmProcessorRevision>(
            bus, name, type, inventoryObjPath);
        nsmDevice->addStaticSensor(gpuRevisionSensor);

        auto persistentMemoryIntf = std::make_shared<PersistentMemoryInterface>(
            bus, inventoryObjPath.c_str());

        auto totalMemorySizeSensor = std::make_shared<NsmTotalMemorySize>(
            name, type, persistentMemoryIntf);
        nsmDevice->addStaticSensor(totalMemorySizeSensor);

        auto healthSensor = std::make_shared<NsmGpuHealth>(bus, name, type,
                                                           inventoryObjPath);
        nsmDevice->deviceSensors.push_back(healthSensor);
    }
    else if (type == "NSM_PortDisableFuture")
    {
        // Port disable future status on Processor
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());

        size_t pos = inventoryObjPath.find_last_of('/');
        std::string basePath = inventoryObjPath;
        std::string processorName = name;
        if (pos != std::string::npos)
        {
            basePath = inventoryObjPath.substr(0, pos);
            processorName = inventoryObjPath.substr(pos + 1);
        }

        auto nvProcessorPortDisableFuture =
            std::make_shared<NsmDevicePortDisableFuture>(processorName, type,
                                                         basePath);

        nvProcessorPortDisableFuture->pdi().portDisableFuture(
            std::vector<uint8_t>{});
        nsmDevice->addSensor(nvProcessorPortDisableFuture, priority);

        nsm::AsyncSetOperationHandler setPortDisableFutureHandler =
            std::bind(&NsmDevicePortDisableFuture::setPortDisableFuture,
                      nvProcessorPortDisableFuture, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);

        AsyncOperationManager::getInstance()
            ->getDispatcher(inventoryObjPath)
            ->addAsyncSetOperation(
                "com.nvidia.NVLink.NVLinkDisableFuture", "PortDisableFuture",
                AsyncSetOperationInfo{setPortDisableFutureHandler,
                                      nvProcessorPortDisableFuture, nsmDevice});
    }
    else if (type == "NSM_Location")
    {
        auto locationType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());
        auto sensor = std::make_shared<NsmLocationIntfProcessor>(
            bus, name, type, inventoryObjPath, locationType);
        nsmDevice->deviceSensors.push_back(sensor);
    }
    else if (type == "NSM_LocationCode")
    {
        auto locationCode = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationCode", interface.c_str());
        auto sensor = std::make_shared<NsmLocationCodeIntfProcessor>(
            bus, name, type, inventoryObjPath, locationCode);
        nsmDevice->deviceSensors.push_back(sensor);
    }
    else if (type == "NSM_Asset")
    {
        auto assetIntf =
            std::make_shared<AssetIntfProcessor>(bus, inventoryObjPath.c_str());
        auto manufacturer = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());

        auto assetObject = NsmAssetIntfProcessor<AssetIntfProcessor>(name, type,
                                                                     assetIntf);
        assetObject.pdi().manufacturer(manufacturer);
        // create sensor
        nsmDevice->addStaticSensor(
            std::make_shared<NsmInventoryProperty<AssetIntfProcessor>>(
                assetObject, DEVICE_PART_NUMBER));
        nsmDevice->addStaticSensor(
            std::make_shared<NsmInventoryProperty<AssetIntfProcessor>>(
                assetObject, SERIAL_NUMBER));
        nsmDevice->addStaticSensor(
            std::make_shared<NsmInventoryProperty<AssetIntfProcessor>>(
                assetObject, MARKETING_NAME));
    }
    else if (type == "NSM_MIG")
    {
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());

        auto sensor = std::make_shared<NsmMigMode>(bus, name, type,
                                                   inventoryObjPath, nsmDevice);
        nsmDevice->addSensor(sensor, priority);

        AsyncOperationManager::getInstance()
            ->getDispatcher(inventoryObjPath)
            ->addAsyncSetOperation(
                "com.nvidia.MigMode", "MIGModeEnabled",
                AsyncSetOperationInfo{setMigModeEnabled, sensor, nsmDevice});
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
            auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_" +
                               std::to_string(idx); // port metrics dbus path
            auto pCieECCIntf =
                std::make_shared<PCieEccIntf>(bus, inventoryObjPath.c_str());
            auto pCiePortIntf =
                std::make_shared<PCieEccIntf>(bus, pcieObjPath.c_str());
            auto pciPortSensor =
                std::make_shared<NsmPciePortIntf>(bus, name, type, pcieObjPath);
            auto sensorGroup2 = std::make_shared<NsmPciGroup2>(
                name, type, pCieECCIntf, pCiePortIntf, deviceId,
                inventoryObjPath);
            auto sensorGroup3 = std::make_shared<NsmPciGroup3>(
                name, type, pCieECCIntf, pCiePortIntf, deviceId,
                inventoryObjPath);
            auto sensorGroup4 = std::make_shared<NsmPciGroup4>(
                name, type, pCieECCIntf, pCiePortIntf, deviceId,
                inventoryObjPath);
            nsmDevice->deviceSensors.push_back(pciPortSensor);
            nsmDevice->addSensor(sensorGroup2, priority);
            nsmDevice->addSensor(sensorGroup3, priority);
            nsmDevice->addSensor(sensorGroup4, priority);
        }
    }
    else if (type == "NSM_ECC")
    {
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());

        auto eccIntf = std::make_shared<EccModeIntf>(bus,
                                                     inventoryObjPath.c_str());

        auto eccModeSensor = std::make_shared<NsmEccMode>(name, type, eccIntf,
                                                          inventoryObjPath);

        nsmDevice->addSensor(eccModeSensor, priority);

        auto eccErrorCntSensor = std::make_shared<NsmEccErrorCounts>(
            name, type, eccIntf, inventoryObjPath);

        nsmDevice->addSensor(eccErrorCntSensor, priority);

        AsyncOperationManager::getInstance()
            ->getDispatcher(inventoryObjPath)
            ->addAsyncSetOperation(eccIntf->interface, "ECCModeEnabled",
                                   AsyncSetOperationInfo{setECCModeEnabled,
                                                         eccModeSensor,
                                                         nsmDevice});
    }
    else if (type == "NSM_EDPp")
    {
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto sensor = std::make_shared<NsmEDPpScalingFactor>(bus, name, type,
                                                             inventoryObjPath);
        nsmDevice->addSensor(sensor, priority);
    }
    else if (type == "NSM_CpuOperatingConfig")
    {
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto cpuOperatingConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
            bus, inventoryObjPath.c_str());

        auto clockFreqSensor = std::make_shared<NsmCurrClockFreq>(
            name, type, cpuOperatingConfigIntf, inventoryObjPath);
        auto clockLimitSensor = std::make_shared<NsmClockLimitGraphics>(
            name, type, cpuOperatingConfigIntf, inventoryObjPath);
        auto minGraphicsClockFreq = std::make_shared<NsmMinGraphicsClockLimit>(
            name, type, cpuOperatingConfigIntf);
        auto maxGraphicsClockFreq = std::make_shared<NsmMaxGraphicsClockLimit>(
            name, type, cpuOperatingConfigIntf);
        auto currentUtilization = std::make_shared<NsmCurrentUtilization>(
            name + "_CurrentUtilization", type, cpuOperatingConfigIntf,
            inventoryObjPath);

        auto defaultBoostClockSpeed =
            std::make_shared<NsmDefaultBoostClockSpeed>(name, type,
                                                        cpuOperatingConfigIntf);
        auto defaultBaseClockSpeed = std::make_shared<NsmDefaultBaseClockSpeed>(
            name, type, cpuOperatingConfigIntf);
        nsmDevice->addStaticSensor(defaultBaseClockSpeed);
        nsmDevice->addStaticSensor(defaultBoostClockSpeed);

        nsmDevice->addSensor(clockFreqSensor, priority);
        nsmDevice->addSensor(clockLimitSensor, priority);
        nsmDevice->addSensor(currentUtilization, priority);

        nsmDevice->addStaticSensor(minGraphicsClockFreq);
        nsmDevice->addStaticSensor(maxGraphicsClockFreq);

        AsyncOperationManager::getInstance()
            ->getDispatcher(inventoryObjPath)
            ->addAsyncSetOperation(
                cpuOperatingConfigIntf->interface, "SpeedConfig",
                AsyncSetOperationInfo{
                    std::bind_front(setCPUSpeedConfig, GRAPHICS_CLOCK),
                    clockLimitSensor, nsmDevice});
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
            std::make_shared<NsmProcessorThrottleReason>(
                name, type, processorPerfIntf, inventoryObjPath);
        
        auto throttleDurationSensor =
                std::make_shared<NsmProcessorThrottleDuration>(
                    name, type, processorPerfIntf, inventoryObjPath);

        auto gpuUtilSensor = std::make_shared<NsmAccumGpuUtilTime>(
            name, type, processorPerfIntf, inventoryObjPath);
        auto pciRxTxSensor = std::make_shared<NsmPciGroup5>(
            name, type, processorPerfIntf, deviceId, inventoryObjPath);

        nsmDevice->addSensor(gpuUtilSensor, priority);
        nsmDevice->addSensor(pciRxTxSensor, priority);
        nsmDevice->addSensor(throttleReasonSensor, priority);
        nsmDevice->addSensor(throttleDurationSensor, priority);
    }
    else if (type == "NSM_MemCapacityUtil")
    {
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto totalMemorySensor = std::make_shared<NsmTotalMemory>(name, type);
        nsmDevice->addSensor(totalMemorySensor, priority);
        auto sensor = std::make_shared<NsmMemoryCapacityUtil>(
            bus, name, type, inventoryObjPath, totalMemorySensor);
        nsmDevice->addSensor(sensor, priority);
    }
    else if (type == "NSM_PowerCap")
    {
        std::vector<std::string> candidateForList =
            utils::DBusHandler().tryGetDbusProperty<std::vector<std::string>>(
                objPath.c_str(), "CompositeNumericSensors", interface.c_str());
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        // create power cap , clear power cap and power limit interface
        auto powerCapIntf = std::make_shared<NsmPowerCapIntf>(
            bus, inventoryObjPath.c_str(), name, candidateForList, nsmDevice);

        AsyncOperationManager::getInstance()
            ->getDispatcher(inventoryObjPath)
            ->addAsyncSetOperation(
                powerCapIntf->interface, "PowerCap",
                AsyncSetOperationInfo{
                    std::bind_front(&NsmPowerCapIntf::setPowerCap,
                                    powerCapIntf),
                    {},
                    nsmDevice});

        auto clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(
            bus, inventoryObjPath.c_str());

        auto clearPowerCapAsyncIntf =
            std::make_shared<NsmClearPowerCapAsyncIntf>(
                bus, inventoryObjPath.c_str(), nsmDevice, powerCapIntf,
                clearPowerCapIntf);

        auto powerLimitIntf =
            std::make_shared<PowerLimitIface>(bus, inventoryObjPath.c_str());

        // create sensors for power cap properties
        auto powerCap = std::make_shared<NsmPowerCap>(
            name, type, powerCapIntf, candidateForList, inventoryObjPath);
        nsmDevice->addSensor(powerCap, priority);
        nsmDevice->capabilityRefreshSensors.emplace_back(powerCap);
        manager.powerCapList.emplace_back(powerCap);

        auto defaultPowerCap = std::make_shared<NsmDefaultPowerCap>(
            name, type, clearPowerCapIntf, clearPowerCapAsyncIntf);
        manager.defaultPowerCapList.emplace_back(defaultPowerCap);

        auto maxPowerCap = std::make_shared<NsmMaxPowerCap>(
            name, type, powerCapIntf, powerLimitIntf);
        manager.maxPowerCapList.emplace_back(maxPowerCap);

        auto minPowerCap = std::make_shared<NsmMinPowerCap>(
            name, type, powerCapIntf, powerLimitIntf);
        manager.minPowerCapList.emplace_back(minPowerCap);

        nsmDevice->addStaticSensor(defaultPowerCap);
        nsmDevice->addStaticSensor(maxPowerCap);
        nsmDevice->addStaticSensor(minPowerCap);
    }
    else if (type == "NSM_InbandReconfigPermissions")
    {
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto featuresNames =
            utils::DBusHandler().getDbusProperty<std::vector<std::string>>(
                objPath.c_str(), "Features", interface.c_str());
        std::map<ReconfigSettingsIntf::FeatureType, std::string> features;
        for (auto& featureName : featuresNames)
        {
            auto feature = ReconfigSettingsIntf::convertFeatureTypeFromString(
                "com.nvidia.InbandReconfigSettings.FeatureType." + featureName);
            features[feature] = featureName;
        }
        for (auto [feature, featureName] : features)
        {
            NsmInterfaceProvider<ReconfigSettingsIntf> interfaceProvider(
                featureName, type,
                path(inventoryObjPath) / "InbandReconfigPermissions");
            auto reconfigurePermissionsSensor =
                std::make_shared<NsmReconfigPermissions>(interfaceProvider,
                                                         feature);
            nsmDevice->addSensor(reconfigurePermissionsSensor, priority);
        }
    }
    else if (type == "NSM_PowerSmoothing")
    {
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());

        std::shared_ptr<OemPowerSmoothingFeatIntf> pwrSmoothingIntf =
            std::make_shared<OemPowerSmoothingFeatIntf>(bus, inventoryObjPath,
                                                        nsmDevice);
        auto controlSensor = std::make_shared<NsmPowerSmoothing>(
            name, type, inventoryObjPath, pwrSmoothingIntf);
        nsmDevice->deviceSensors.emplace_back(controlSensor);

        auto lifetimeCicuitrySensor = std::make_shared<NsmHwCircuitryTelemetry>(
            name, type, inventoryObjPath, pwrSmoothingIntf);
        nsmDevice->deviceSensors.emplace_back(lifetimeCicuitrySensor);

        std::shared_ptr<OemAdminProfileIntf> adminProfileIntf =
            std::make_shared<OemAdminProfileIntf>(bus, inventoryObjPath,
                                                  nsmDevice);

        auto adminProfileSensor =
            std::make_shared<NsmPowerSmoothingAdminOverride>(
                name, type, adminProfileIntf, inventoryObjPath);
        nsmDevice->deviceSensors.emplace_back(adminProfileSensor);

        auto getAllPowerProfileSensor =
            std::make_shared<NsmPowerProfileCollection>(
                name, type, inventoryObjPath, nsmDevice);
        nsmDevice->deviceSensors.emplace_back(getAllPowerProfileSensor);

        std::shared_ptr<OemCurrentPowerProfileIntf> pwrSmoothingCurProfileIntf =
            std::make_shared<OemCurrentPowerProfileIntf>(
                bus, inventoryObjPath, adminProfileIntf->getInventoryObjPath(),
                nsmDevice);

        auto currentProfileSensor =
            std::make_shared<NsmCurrentPowerSmoothingProfile>(
                name, type, inventoryObjPath, pwrSmoothingCurProfileIntf,
                getAllPowerProfileSensor, adminProfileSensor);
        nsmDevice->deviceSensors.emplace_back(currentProfileSensor);

        nsmDevice->addSensor(getAllPowerProfileSensor, priority);
        nsmDevice->addSensor(adminProfileSensor, priority);
        nsmDevice->addSensor(controlSensor, priority);
        nsmDevice->addSensor(lifetimeCicuitrySensor, priority);
        nsmDevice->addSensor(currentProfileSensor, priority);
    }
    else if (type == "NSM_TotalNvLinksCount")
    {
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto  totalNvLinkInterface = std::make_shared<TotalNvLinkInterface>(
            bus, inventoryObjPath.c_str());
        auto totalNvLinkSensor = std::make_shared<NsmTotalNvLinks>(
            name, type, totalNvLinkInterface, inventoryObjPath);
        nsmDevice->addSensor(totalNvLinkSensor, priority); 
    } 
}

dbus::Interfaces nsmProcessorInterfaces = {
    "xyz.openbmc_project.Configuration.NSM_Processor",
    "xyz.openbmc_project.Configuration.NSM_Processor.MIGMode",
    "xyz.openbmc_project.Configuration.NSM_Processor.ECCMode",
    "xyz.openbmc_project.Configuration.NSM_Processor.PCIe",
    "xyz.openbmc_project.Configuration.NSM_Processor.EDPpScalingFactor",   
    "xyz.openbmc_project.Configuration.NSM_Processor.ProcessorPerformance",
    "xyz.openbmc_project.Configuration.NSM_Processor.CpuOperatingConfig",
    "xyz.openbmc_project.Configuration.NSM_Processor.MemCapacityUtil",
    "xyz.openbmc_project.Configuration.NSM_Processor.Location",
    "xyz.openbmc_project.Configuration.NSM_Processor.LocationCode",
    "xyz.openbmc_project.Configuration.NSM_Processor.Asset",
    "xyz.openbmc_project.Configuration.NSM_Processor.PortDisableFuture",
    "xyz.openbmc_project.Configuration.NSM_Processor.PowerCap",
    "xyz.openbmc_project.Configuration.NSM_Processor.InbandReconfigPermissions",
    "xyz.openbmc_project.Configuration.NSM_Processor.PowerSmoothing",
    "xyz.openbmc_project.Configuration.NSM_Processor.TotalNvLinksCount"};

REGISTER_NSM_CREATION_FUNCTION(createNsmProcessorSensor, nsmProcessorInterfaces)

} // namespace nsm
