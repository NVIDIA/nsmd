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
#ifdef ENABLE_SYSTEM_GUID
#include "network-ports.h"

#include <sys/random.h> // uuid_generate for sysguid
#endif
#include "platform-environmental.h"

#include "asyncOperationManager.hpp"
#include "dBusAsyncUtils.hpp"
#include "deviceManager.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmDevice.hpp"
#include "nsmErrorInjectionCommon.hpp"
#include "nsmInterface.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPCIeLinkSpeed.hpp"
#include "nsmPort/nsmPortDisableFuture.hpp"
#include "nsmPowerSmoothing.hpp"
#include "nsmReconfigPermissions.hpp"
#include "nsmSetCpuOperatingConfig.hpp"
#include "nsmSetECCMode.hpp"
#include "nsmSetReconfigSettings.hpp"
#include "nsmWorkloadPowerProfile.hpp"
#include "sharedMemCommon.hpp"

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

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

#ifdef ENABLE_SYSTEM_GUID
NsmSysGuidIntf::NsmSysGuidIntf(sdbusplus::bus::bus& bus, std::string& name,
                               std::string& type,
                               std::string& inventoryObjPath) :
    NsmObject(name, type),
    inventoryObjPath(inventoryObjPath)
{
    sysguidIntf = std::make_unique<SysGuidIntf>(bus, inventoryObjPath.c_str());
    sysguidIntf->sysGUID();
}

uint8_t NsmSysGuidIntf::sysGUID[8] = {0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00};
bool NsmSysGuidIntf::sysGuidGenerated = false;

requester::Coroutine NsmSysGuidIntf::update(SensorManager& manager, eid_t eid)
{
    Request readSysGuid(sizeof(nsm_msg_hdr) +
                        sizeof(struct nsm_get_system_guid_req));
    auto readSysGuidMsg = reinterpret_cast<struct nsm_msg*>(readSysGuid.data());

    auto rc = encode_get_system_guid_req(0, readSysGuidMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmGetSysGuid encode_get_system_guid_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> readSysGuidResponseMsg;
    size_t readSysGuidResponseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(
        eid, readSysGuid, readSysGuidResponseMsg, readSysGuidResponseLen);
    if (rc)
    {
        lg2::debug(
            "NsmGetSysGuid SendRecvNsmMsg failed with RC={RC}, eid={EID}", "RC",
            rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint8_t data[8] = {0};
    uint16_t dataLen = 8;

    rc = decode_get_system_guid_resp(readSysGuidResponseMsg.get(),
                                     readSysGuidResponseLen, &cc, &reason_code,
                                     data, dataLen);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        bool sysGuidAllZeros = true;
        for (auto i = 0; i < 8; i++)
        {
            if (data[i] != 0x00)
            {
                sysGuidAllZeros = false;
            }
        }

        if (!sysGuidGenerated)
        {
            lg2::info("First instance, generating SysGUID");
            if (sysGuidAllZeros)
            {
                lg2::info("GPU returned 0x00's, generating random SysGUID");

                for (auto i = 0; i < 8; i++)
                {
                    auto sysrc = getrandom(sysGUID, 8, 0);

                    if (sysrc != 8)
                    {
                        lg2::error("getrandom failed. Return={RC}", "RC",
                                   sysrc);
                    }
                }
            }
            else
            {
                lg2::info("GPU returned a SysGUID, using it");
                for (auto i = 0; i < 8; i++)
                {
                    sysGUID[i] = data[i];
                }
            }
            sysGuidGenerated = true;
        }

        bool setSysGuidNeeded = false;
        for (auto i = 0; i < 8; i++)
        {
            if (data[i] != sysGUID[i])
            {
                setSysGuidNeeded = true;
                break;
            }
        }

        if (setSysGuidNeeded)
        {
            Request setSysGuid(sizeof(nsm_msg_hdr) +
                               sizeof(struct nsm_set_system_guid_req));
            auto setSysGuidMsg =
                reinterpret_cast<struct nsm_msg*>(setSysGuid.data());

            rc = encode_set_system_guid_req(0, setSysGuidMsg, sysGUID, 8);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::debug(
                    "NsmGetSysGuid encode_set_system_guid_req failed. eid={EID} rc={RC}",
                    "EID", eid, "RC", rc);
                co_return rc;
            }

            std::shared_ptr<const nsm_msg> setSysGuidResponseMsg;
            size_t setSysGuidResponseLen = 0;
            rc = co_await manager.SendRecvNsmMsg(
                eid, setSysGuid, setSysGuidResponseMsg, setSysGuidResponseLen);
            if (rc)
            {
                lg2::debug(
                    "NsmGetSysGuid SendRecvNsmMsg failed with RC={RC}, eid={EID}",
                    "RC", rc, "EID", eid);
                co_return rc;
            }

            Request reReadSysGuid(sizeof(nsm_msg_hdr) +
                                  sizeof(struct nsm_get_system_guid_req));
            auto reReadSysGuidMsg =
                reinterpret_cast<struct nsm_msg*>(reReadSysGuid.data());

            rc = encode_get_system_guid_req(0, reReadSysGuidMsg);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::debug(
                    "NsmGetSysGuid encode_get_system_guid_req failed. eid={EID} rc={RC}",
                    "EID", eid, "RC", rc);
                co_return rc;
            }

            std::shared_ptr<const nsm_msg> reReadSysGuidResponseMsg;
            size_t reReadSysGuidResponseLen = 0;
            rc = co_await manager.SendRecvNsmMsg(eid, reReadSysGuid,
                                                 reReadSysGuidResponseMsg,
                                                 reReadSysGuidResponseLen);
            if (rc)
            {
                lg2::debug(
                    "NsmGetSysGuid SendRecvNsmMsg failed with RC={RC}, eid={EID}",
                    "RC", rc, "EID", eid);
                co_return rc;
            }

            uint8_t cc = NSM_ERROR;
            reason_code = ERR_NULL;
            dataLen = 8;

            rc = decode_get_system_guid_resp(reReadSysGuidResponseMsg.get(),
                                             readSysGuidResponseLen, &cc,
                                             &reason_code, data, dataLen);
        }

        // convert it to a string
        std::ostringstream oss;
        for (auto& guidtoken : data)
        {
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(guidtoken);
        }
        sysguidIntf->sysGUID(oss.str());
        clearErrorBitMap("NsmGetSysGuid decode_get_system_guid_resp");
    }
    else
    {
        logHandleResponseMsg("NsmGetSysGuid decode_get_system_guid_resp",
                             reason_code, cc, rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return rc;
}
#endif

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
                       [[maybe_unused]] std::shared_ptr<NsmDevice> device,
                       bool isLongRunning) :
    NsmSensor(name, type, isLongRunning),
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
        lg2::debug("encode_get_MIG_mode_req failed. "
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
        clearErrorBitMap("decode_get_temperature_reading_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_temperature_reading_resp", reason_code,
                             cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmEccMode::NsmEccMode(std::string& name, std::string& type,
                       std::shared_ptr<EccModeIntf> eccIntf,
                       std::string& inventoryObjPath, bool isLongRunning) :
    NsmSensor(name, type, isLongRunning),
    inventoryObjPath(inventoryObjPath)

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
        lg2::debug("encode_get_ECC_mode_req failed. "
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
        clearErrorBitMap("decode_get_ECC_mode_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_ECC_mode_resp", reason_code, cc, rc);

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
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath)

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
        lg2::debug("encode_get_ECC_error_counts_req failed. "
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
        clearErrorBitMap("decode_get_ECC_error_counts_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_ECC_error_counts_resp", reason_code,
                             cc, rc);

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
        lg2::debug(
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

    propName = "UnsupportedRequestCount";
    nv::sensor_aggregation::DbusVariantType unsupportedRequestCount{
        pCieEccIntf->ceCount()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 unsupportedRequestCount);

    // pcie port metrics
    ifaceName = std::string(pCiePortIntf->interface);
    propName = "nonfeCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, nonfeCountVal);

    ifaceName = std::string(pCiePortIntf->interface);
    propName = "feCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, feCountVal);

    ifaceName = std::string(pCiePortIntf->interface);
    propName = "ceCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, ceCountVal);

    ifaceName = std::string(pCiePortIntf->interface);
    propName = "UnsupportedRequestCount";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 unsupportedRequestCount);

#endif
}

void NsmPciGroup2::updateReading(
    const nsm_query_scalar_group_telemetry_group_2& data)
{
    pCieEccIntf->nonfeCount(data.non_fatal_errors);
    pCieEccIntf->feCount(data.fatal_errors);
    pCieEccIntf->ceCount(data.correctable_errors);
    pCieEccIntf->unsupportedRequestCount(data.unsupported_request_count);
    // pcie port metrics
    pCiePortIntf->nonfeCount(data.non_fatal_errors);
    pCiePortIntf->feCount(data.fatal_errors);
    pCiePortIntf->ceCount(data.correctable_errors);
    pCiePortIntf->unsupportedRequestCount(data.unsupported_request_count);
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
        clearErrorBitMap(
            "NsmPciGroup2 decode_query_scalar_group_telemetry_v1_group2_resp");
    }
    else
    {
        logHandleResponseMsg(
            "NsmPciGroup2 decode_query_scalar_group_telemetry_v1_group2_resp",
            reason_code, cc, rc);
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
        clearErrorBitMap(
            "NsmPCIeECCGroup3 decode_query_scalar_group_telemetry_v1_group3_resp");
    }
    else
    {
        logHandleResponseMsg(
            "NsmPCIeECCGroup3 decode_query_scalar_group_telemetry_v1_group3_resp",
            reason_code, cc, rc);
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
        clearErrorBitMap(
            "NsmPCIeECCGroup4 decode_query_scalar_group_telemetry_v1_group4_resp");
    }
    else
    {
        logHandleResponseMsg(
            "NsmPCIeECCGroup4 decode_query_scalar_group_telemetry_v1_group4_resp",
            reason_code, cc, rc);
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
        clearErrorBitMap("decode_query_scalar_group_telemetry_v1_group5_resp");
    }
    else
    {
        logHandleResponseMsg(
            "decode_query_scalar_group_telemetry_v1_group5_resp", reason_code,
            cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

NsmEDPpScalingFactor::NsmEDPpScalingFactor(
    std::string& name, std::string& type, std::string& inventoryObjPath,
    std::shared_ptr<EDPpLocal> eDPpIntf,
    std::shared_ptr<NsmResetEdppAsyncIntf> resetEdppAsyncIntf) :
    NsmSensor(name, type),
    eDPpIntf(eDPpIntf), resetEdppAsyncIntf(resetEdppAsyncIntf),
    inventoryObjPath(inventoryObjPath)
{
    lg2::info("NsmEDPpScalingFactor: create sensor:{NAME}", "NAME",
              name.c_str());
    persistence = false;
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
    eDPpIntf->setPoint(
        std::tuple(scaling_factors.enforced_scaling_factor, persistence));
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
        lg2::debug("encode_get_programmable_EDPp_scaling_factor_req failed. "
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
        clearErrorBitMap("decode_get_programmable_EDPp_scaling_factor_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_programmable_EDPp_scaling_factor_resp",
                             reason_code, cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

requester::Coroutine NsmEDPpScalingFactor::patchSetPoint(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const std::tuple<bool, uint32_t>* reqSetPoint =
        std::get_if<std::tuple<bool, uint32_t>>(&value);
    if (reqSetPoint == NULL)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_DATA;
    }

    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("patch EDPp setpoint On Device for EID: {EID}", "EID", eid);

    uint32_t allowableMin = eDPpIntf->allowableMin();
    uint32_t allowableMax = eDPpIntf->allowableMax();

    uint32_t reqLimit = std::get<1>(*reqSetPoint);
    bool reqPersistence = std::get<0>(*reqSetPoint);

    if (allowableMin > reqLimit || allowableMax < reqLimit)
    {
        lg2::error("req SetPoint Limit not in allowed range");
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_DATA;
    }

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_programmable_EDPp_scaling_factor_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_set_programmable_EDPp_scaling_factor_req(
        0, NEW_SCALING_FACTOR, reqPersistence, static_cast<uint8_t>(reqLimit),
        requestMsg);

    if (rc)
    {
        lg2::debug(
            "NsmEDPpScalingFactor::patchSetPoint  failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen);
    if (rc_)
    {
        lg2::error(
            "NsmEDPpScalingFactor::patchSetPoint SendRecvNsmMsgSync failed for while setting edpp setpoint "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc_);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint16_t data_size = 0;
    rc = decode_set_programmable_EDPp_scaling_factor_resp(
        responseMsg.get(), responseLen, &cc, &reason_code, &data_size);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info(
            "NsmEDPpScalingFactor::patchSetPoint for EID: {EID} completed",
            "EID", eid);
    }
    else
    {
        lg2::error(
            "NsmEDPpScalingFactor::patchSetPoint decode_set_programmable_EDPp_scaling_factor_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    persistence = reqPersistence;

    co_return NSM_SW_SUCCESS;
}

NsmMaxEDPpLimit::NsmMaxEDPpLimit(std::string& name, std::string& type,
                                 std::shared_ptr<EDPpLocal> eDPpIntf) :
    NsmObject(name, type),
    eDPpIntf(eDPpIntf)
{
    lg2::info("NsmMaxEDPpLimit: create sensor:{NAME}", "NAME", name.c_str());
}

requester::Coroutine NsmMaxEDPpLimit::update(SensorManager& manager, eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MAXIMUM_EDPP_SCALING_FACTOR;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmMaxEDPpLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::debug(
            "NsmMaxEDPpLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint8_t value;
    std::vector<uint8_t> data(1, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        eDPpIntf->allowableMax(value);
        clearErrorBitMap(
            "NsmMaxEDPpLimit decode_get_inventory_information_resp");
    }
    else
    {
        logHandleResponseMsg(
            "NsmMaxEDPpLimit decode_get_inventory_information_resp",
            reason_code, cc, rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
}

NsmMinEDPpLimit::NsmMinEDPpLimit(std::string& name, std::string& type,
                                 std::shared_ptr<EDPpLocal> eDPpIntf) :
    NsmObject(name, type),
    eDPpIntf(eDPpIntf)
{
    lg2::info("NsmMinEDPpLimit: create sensor:{NAME}", "NAME", name.c_str());
}

requester::Coroutine NsmMinEDPpLimit::update(SensorManager& manager, eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MINIMUM_EDPP_SCALING_FACTOR;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmMinEDPpLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::debug(
            "NsmMinEDPpLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint8_t value;
    std::vector<uint8_t> data(1, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        eDPpIntf->allowableMin(value);
        clearErrorBitMap(
            "NsmMinEDPpLimit decode_get_inventory_information_resp");
    }
    else
    {
        logHandleResponseMsg(
            "NsmMinEDPpLimit decode_get_inventory_information_resp",
            reason_code, cc, rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return cc;
}

NsmClockLimitGraphics::NsmClockLimitGraphics(
    const std::string& name, const std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type),
    inventoryObjPath(inventoryObjPath)

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

    propName = "SpeedLimit";
    nv::sensor_aggregation::DbusVariantType speedLimitVal{
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::speedLimit()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, speedLimitVal);
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
        lg2::debug("encode_get_clock_limit_req failed. "
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
        clearErrorBitMap("decode_get_clock_limit_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_clock_limit_resp", reason_code, cc,
                             rc);
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
        lg2::debug("encode_get_curr_clock_freq_req failed. "
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
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_curr_clock_freq_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &clockFreq);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(clockFreq);
        clearErrorBitMap("decode_get_curr_clock_freq_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_curr_clock_freq_resp", reason_code, cc,
                             rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmDefaultBaseClockSpeed::NsmDefaultBaseClockSpeed(
    std::string& name, std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf) :
    NsmObject(name, type),
    cpuOperatingConfigIntf(cpuConfigIntf)
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
        lg2::debug(
            "NsmDefaultBaseClockSpeed: encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::debug(
            "NsmDefaultBaseClockSpeed SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        // coverity[missing_return]
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
        clearErrorBitMap(
            "NsmDefaultBaseClockSpeed decode_get_inventory_information_resp");
    }
    else
    {
        logHandleResponseMsg(
            "NsmDefaultBaseClockSpeed decode_get_inventory_information_resp",
            reason_code, cc, rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return cc;
}

NsmDefaultBoostClockSpeed::NsmDefaultBoostClockSpeed(
    std::string& name, std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf) :
    NsmObject(name, type),
    cpuOperatingConfigIntf(cpuConfigIntf)
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
        lg2::debug(
            "NsmDefaultBoostClockSpeed: encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::debug(
            "NsmDefaultBoostClockSpeed: SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        // coverity[missing_return]
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
        clearErrorBitMap("decode_get_inventory_information_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_inventory_information_resp",
                             reason_code, cc, rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return cc;
}

NsmCurrentUtilization::NsmCurrentUtilization(
    const std::string& name, const std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    std::shared_ptr<SMUtilizationIntf> smUtilizationIntf,
    std::string& inventoryObjPath, bool isLongRunning) :
    NsmSensor(name, type, isLongRunning),
    cpuOperatingConfigIntf(cpuConfigIntf), smUtilizationIntf(smUtilizationIntf),
    inventoryObjPath(inventoryObjPath),
    smUtilizationIntfName(smUtilizationIntf->interface),
    smUtilizationPropertyName("SMUtilization")
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
        lg2::debug("encode_get_current_utilization_req failed. "
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
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, smUtilizationIntfName, smUtilizationPropertyName,
        smbusData, utilizationVal);
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
        smUtilizationIntf->smUtilization(gpu_utilization);
        updateMetricOnSharedMemory();
        clearErrorBitMap("decode_get_current_utilization_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_current_utilization_resp", reason_code,
                             cc, rc);

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
        lg2::debug("encode_get_current_clock_event_reason_code_req failed. "
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
        clearErrorBitMap("decode_get_current_clock_event_reason_code_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_current_clock_event_reason_code_resp",
                             reason_code, cc, rc);
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
        lg2::debug("encode_get_accum_GPU_util_time_req failed. "
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
        clearErrorBitMap("decode_get_accum_GPU_util_time_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_accum_GPU_util_time_resp", reason_code,
                             cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmTotalMemorySize::NsmTotalMemorySize(
    std::string& name, std::string& type,
    std::shared_ptr<PersistentMemoryInterface> persistentMemoryInterface) :
    NsmObject(name, type),
    persistentMemoryInterface(persistentMemoryInterface)
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
        lg2::debug(
            "NsmTotalMemorySize: encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::debug(
            "NsmTotalMemorySize: SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        // coverity[missing_return]
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
        persistentMemoryInterface->volatileSizeInKiB(value * 1024);
        clearErrorBitMap("decode_get_inventory_information_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_inventory_information_resp",
                             reason_code, cc, rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return cc;
}

NsmTotalNvLinks::NsmTotalNvLinks(
    const std::string& name, const std::string& type,
    std::shared_ptr<TotalNvLinkInterface> totalNvLinkInterface,
    std::string& inventoryObjPath) :
    NsmSensor(name, type),
    totalNvLinkInterface(totalNvLinkInterface),
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
        lg2::debug("encode_query_ports_available_req failed. "
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
        clearErrorBitMap("decode_query_ports_available_resp");
    }
    else
    {
        logHandleResponseMsg("decode_query_ports_available_resp", reason_code,
                             cc, rc);
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
        lg2::debug("encode_get_inventory_information_req failed. "
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
        clearErrorBitMap("decode_get_inventory_information_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_inventory_information_resp",
                             reason_code, cc, rc);
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

NsmPowerCap::NsmPowerCap(
    std::string& name, std::string& type,
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
    const std::vector<std::string>& parents,
    const std::shared_ptr<PowerPersistencyIntf> persistencyIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type),
    powerCapIntf(powerCapIntf), parents(parents),
    persistencyIntf(persistencyIntf), inventoryObjPath(inventoryObjPath)
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
        lg2::debug(
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
        // check if device returned invalid power limit, report invalid
        // value as is on dbus
        uint32_t reading = (enforced_limit_in_miliwatts == INVALID_POWER_LIMIT)
                               ? INVALID_POWER_LIMIT
                               : enforced_limit_in_miliwatts / 1000;
        updateReading(reading);
        clearErrorBitMap("decode_get_power_limit_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_power_limit_resp", reason_code, cc,
                             rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmMaxPowerCap::NsmMaxPowerCap(std::string& name, std::string& type,
                               std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
                               std::shared_ptr<PowerLimitIface> powerLimitIntf,
                               std::string& inventoryObjPath) :
    NsmObject(name, type),
    powerCapIntf(powerCapIntf), powerLimitIntf(powerLimitIntf),
    inventoryObjPath(inventoryObjPath)
{
    updateMetricOnSharedMemory();
}

void NsmMaxPowerCap::updateValue(uint32_t value)
{
    powerCapIntf->maxPowerCapValue(value);
    powerLimitIntf->maxPowerWatts(value);
    lg2::info("NsmMaxPowerCap::updateValue {VALUE}", "VALUE", value);
}

void NsmMaxPowerCap::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(powerLimitIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "MaxPowerWatts";
    nv::sensor_aggregation::DbusVariantType maxPowerVal{
        powerLimitIntf->maxPowerWatts()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, maxPowerVal);
#endif
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
        lg2::debug(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::debug(
            "NsmMaxPowerCap SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        // coverity[missing_return]
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
        // check if device returned invalid power limit, report invalid
        // value as is on dbus
        uint32_t reading = (value == INVALID_POWER_LIMIT) ? INVALID_POWER_LIMIT
                                                          : value / 1000;
        updateValue(reading);
        clearErrorBitMap("decode_get_inventory_information_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_inventory_information_resp",
                             reason_code, cc, rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return cc;
}

NsmMinPowerCap::NsmMinPowerCap(std::string& name, std::string& type,
                               std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
                               std::shared_ptr<PowerLimitIface> powerLimitIntf,
                               std::string& inventoryObjPath) :
    NsmObject(name, type),
    powerCapIntf(powerCapIntf), powerLimitIntf(powerLimitIntf),
    inventoryObjPath(inventoryObjPath)
{
    updateMetricOnSharedMemory();
}

void NsmMinPowerCap::updateValue(uint32_t value)
{
    powerCapIntf->minPowerCapValue(value);
    powerLimitIntf->minPowerWatts(value);
    lg2::info("NsmMinPowerCap::updateValue {VALUE}", "VALUE", value);
}

void NsmMinPowerCap::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(powerLimitIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "MinPowerWatts";
    nv::sensor_aggregation::DbusVariantType minPowerVal{
        powerLimitIntf->minPowerWatts()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, minPowerVal);
#endif
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
        lg2::debug(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::debug(
            "NsmMinPowerCap SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        // coverity[missing_return]
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
        // check if device returned invalid power limit, report invalid
        // value as is on dbus
        uint32_t reading = (value == INVALID_POWER_LIMIT) ? INVALID_POWER_LIMIT
                                                          : value / 1000;
        updateValue(reading);
        clearErrorBitMap("decode_get_inventory_information_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_inventory_information_resp",
                             reason_code, cc, rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return cc;
}

NsmDefaultPowerCap::NsmDefaultPowerCap(
    std::string& name, std::string& type,
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf,
    std::shared_ptr<NsmClearPowerCapAsyncIntf> clearPowerCapAsyncIntf) :
    NsmObject(name, type),
    defaultPowerCapIntf(clearPowerCapIntf),
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
        lg2::debug(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::debug(
            "NsmDefaultPowerCap SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        // coverity[missing_return]
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
        // check if device returned invalid power limit, report invalid
        // value as is on dbus
        uint32_t reading = (value == INVALID_POWER_LIMIT) ? INVALID_POWER_LIMIT
                                                          : value / 1000;
        updateValue(reading);
        clearErrorBitMap("decode_get_inventory_information_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_inventory_information_resp",
                             reason_code, cc, rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return cc;
}

NsmProcessorThrottleDuration::NsmProcessorThrottleDuration(
    std::string& name, std::string& type,
    std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
    std::string& inventoryObjPath, bool isLongRunning) :
    NsmSensor(name, type, isLongRunning),
    processorPerformanceIntf(processorPerfIntf),
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
    nv::sensor_aggregation::DbusVariantType
        globalSoftwareViolationThrottleDuration{
            processorPerformanceIntf
                ->globalSoftwareViolationThrottleDuration()};
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
        lg2::debug("encode_get_violation_duration_req failed. "
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
        clearErrorBitMap("decode_get_violation_duration_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_violation_duration_resp", reason_code,
                             cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

NsmConfidentialCompute::NsmConfidentialCompute(
    const std::string& name, const std::string& type,
    std::shared_ptr<ConfidentialComputeIntf> confidentialComputeIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type),
    confidentialComputeIntf(confidentialComputeIntf),
    inventoryObjPath(inventoryObjPath)
{
    updateMetricOnSharedMemory();
}

void NsmConfidentialCompute::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(confidentialComputeIntf->interface);
    std::vector<uint8_t> smbusData = {};

    nv::sensor_aggregation::DbusVariantType ccModeEnabled{
        confidentialComputeIntf->ccModeEnabled()};
    std::string propName = "CCModeEnabled";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, ccModeEnabled);

    propName = "PendingCCModeState";
    nv::sensor_aggregation::DbusVariantType pendingCCModeState{
        confidentialComputeIntf->pendingCCModeState()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, pendingCCModeState);

    nv::sensor_aggregation::DbusVariantType ccDevModeEnabled{
        confidentialComputeIntf->ccDevModeEnabled()};
    propName = "CCDevModeEnabled";
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, ccDevModeEnabled);

    propName = "PendingCCDevModeState";
    nv::sensor_aggregation::DbusVariantType pendingCCDevModeState{
        confidentialComputeIntf->pendingCCDevModeState()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(inventoryObjPath, ifaceName,
                                                 propName, smbusData,
                                                 pendingCCDevModeState);

#endif
}

std::optional<std::vector<uint8_t>>
    NsmConfidentialCompute::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_confidential_compute_mode_v1_req(instanceId,
                                                          requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_confidential_compute_mode_v1_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmConfidentialCompute::handleResponseMsg(const struct nsm_msg* responseMsg,
                                              size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint8_t current_mode;
    uint8_t pending_mode;
    uint16_t data_size;
    uint16_t reason_code = ERR_NULL;

    auto rc = decode_get_confidential_compute_mode_v1_resp(
        responseMsg, responseLen, &cc, &data_size, &reason_code, &current_mode,
        &pending_mode);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        updateReading(current_mode, pending_mode);
        clearErrorBitMap("decode_get_confidential_compute_mode_v1_resp");
    }
    else
    {
        logHandleResponseMsg("decode_get_confidential_compute_mode_v1_resp",
                             reason_code, cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return cc;
}

void NsmConfidentialCompute::updateReading(uint8_t current_mode,
                                           uint8_t pending_mode)
{
    switch (current_mode)
    {
        case NO_MODE:
            confidentialComputeIntf->ccModeEnabled(false);
            confidentialComputeIntf->ccDevModeEnabled(false);
            break;
        case PRODUCTION_MODE:
            confidentialComputeIntf->ccModeEnabled(true);
            confidentialComputeIntf->ccDevModeEnabled(false);
            break;
        case DEVTOOLS_MODE:
            confidentialComputeIntf->ccModeEnabled(false);
            confidentialComputeIntf->ccDevModeEnabled(true);
            break;
    }

    switch (pending_mode)
    {
        case NO_MODE:
            confidentialComputeIntf->pendingCCModeState(false);
            confidentialComputeIntf->pendingCCDevModeState(false);
            break;
        case PRODUCTION_MODE:
            confidentialComputeIntf->pendingCCModeState(true);
            confidentialComputeIntf->pendingCCDevModeState(false);
            break;
        case DEVTOOLS_MODE:
            confidentialComputeIntf->pendingCCModeState(false);
            confidentialComputeIntf->pendingCCDevModeState(true);
            break;
    }

    updateMetricOnSharedMemory();
}

requester::Coroutine NsmConfidentialCompute::patchCCMode(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const bool* reqSetting = std::get_if<bool>(&value);
    if (reqSetting == NULL)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("set Confidential Compute Mode On Device for EID: {EID}", "EID",
              eid);

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_confidential_compute_mode_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    uint8_t mode;
    if (*reqSetting == true)
    {
        mode = PRODUCTION_MODE;
    }
    else
    {
        if (confidentialComputeIntf->pendingCCDevModeState())
        {
            mode = DEVTOOLS_MODE;
        }
        else
        {
            mode = NO_MODE;
        }
    }

    auto rc = encode_set_confidential_compute_mode_v1_req(0, mode, requestMsg);

    if (rc)
    {
        lg2::error(
            "NsmConfidentialCompute :: patchCCMode encode_set_confidential_compute_mode_v1_req failed. eid = {EID} rc = { RC } ",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen);
    if (rc_)
    {
        lg2::error(
            "NsmConfidentialCompute :: patchCCMode SendRecvNsmMsgSync failed"
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc_);

        *status = AsyncOperationStatusType::WriteFailure;

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint16_t data_size = 0;
    rc = decode_set_confidential_compute_mode_v1_resp(
        responseMsg.get(), responseLen, &cc, &data_size, &reason_code);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info(
            "NsmConfidentialCompute :: patchCCMode for EID: {EID} completed",
            "EID", eid);
    }
    else
    {
        lg2::error(
            "NsmConfidentialCompute :: patchCCMode decode_set_confidential_compute_mode_v1_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmConfidentialCompute::patchCCDevMode(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const bool* reqSetting = std::get_if<bool>(&value);
    if (reqSetting == NULL)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("set Confidential Compute Devtools Mode On Device for EID: {EID}",
              "EID", eid);

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_confidential_compute_mode_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    uint8_t mode;
    if (*reqSetting == true)
    {
        mode = DEVTOOLS_MODE;
    }
    else
    {
        if (confidentialComputeIntf->pendingCCModeState())
        {
            mode = PRODUCTION_MODE;
        }
        else
        {
            mode = NO_MODE;
        }
    }

    auto rc = encode_set_confidential_compute_mode_v1_req(0, mode, requestMsg);

    if (rc)
    {
        lg2::error(
            "NsmConfidentialCompute :: patchCCDevMode encode_set_confidential_compute_mode_v1_req failed. eid = {EID} rc = { RC } ",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen);
    if (rc_)
    {
        lg2::error(
            "NsmConfidentialCompute :: patchCCDevMode SendRecvNsmMsgSync failed"
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc_);

        *status = AsyncOperationStatusType::WriteFailure;

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint16_t data_size = 0;
    rc = decode_set_confidential_compute_mode_v1_resp(
        responseMsg.get(), responseLen, &cc, &data_size, &reason_code);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info(
            "NsmConfidentialCompute :: patchCCDevMode for EID: {EID} completed",
            "EID", eid);
    }
    else
    {
        lg2::error(
            "NsmConfidentialCompute :: patchCCDevMode decode_set_confidential_compute_mode_v1_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine createNsmProcessorSensor(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", PROCESSOR_INTERFACE);

    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", PROCESSOR_INTERFACE);

    auto type = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());

    auto inventoryObjPath = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "InventoryObjPath", PROCESSOR_INTERFACE);

    auto nsmDevice = manager.getNsmDevice(uuid);

    if (type == "NSM_Processor")
    {
        std::vector<utils::Association> associations{};
        co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                          associations);
        auto associationSensor = std::make_shared<NsmProcessorAssociation>(
            bus, name, type, inventoryObjPath, associations);
        nsmDevice->deviceSensors.push_back(associationSensor);

#ifdef ACCELERATOR_DBUS
        auto sensor = std::make_shared<NsmAcceleratorIntf>(bus, name, type,
                                                           inventoryObjPath);
        nsmDevice->deviceSensors.push_back(sensor);
#endif

        auto deviceUuid = co_await utils::coGetDbusProperty<uuid_t>(
            objPath.c_str(), "DEVICE_UUID", PROCESSOR_INTERFACE);

        auto uuidSensor = std::make_shared<NsmUuidIntf>(
            bus, name, type, inventoryObjPath, deviceUuid);
        nsmDevice->addStaticSensor(uuidSensor);

#ifdef ENABLE_SYSTEM_GUID
        auto sysGuidSensor = std::make_shared<NsmSysGuidIntf>(bus, name, type,
                                                              inventoryObjPath);

        // Note: Setting this to addSensor as opposed to addStaticSensor:
        //       Ideally this would be a static sensor, but the GPU changes
        //       state with the host. A static sensor does not update so
        //       the System GUID gets wiped. Polling here resolves the issue
        //       and ensures the GUID is always correctly loaded.
        // TBD:  A more optimal solution would be to update static sensors
        //       when nsmObjects go online/offline but that may have other
        //       impacts so needs some further investigation.
        nsmDevice->addSensor(sysGuidSensor, false, true);
#endif

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

        createNsmErrorInjectionSensors(manager, nsmDevice, inventoryObjPath);
        auto confidentialComputeIntf =
            std::make_shared<ConfidentialComputeIntf>(bus,
                                                      inventoryObjPath.c_str());
        auto confidentialComputeSensor =
            std::make_shared<NsmConfidentialCompute>(
                name, type, confidentialComputeIntf, inventoryObjPath);
        nsmDevice->addSensor(confidentialComputeSensor,
                             CONFIDENTIAL_COMPUTE_MODE_PRIORITY);
        nsm::AsyncSetOperationHandler setconfidentialComputeHandler =
            std::bind(&NsmConfidentialCompute::patchCCMode,
                      confidentialComputeSensor, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);

        AsyncOperationManager::getInstance()
            ->getDispatcher(inventoryObjPath)
            ->addAsyncSetOperation(
                "com.nvidia.CCMode", "CCModeEnabled",
                AsyncSetOperationInfo{setconfidentialComputeHandler,
                                      confidentialComputeSensor, nsmDevice});

        nsm::AsyncSetOperationHandler setconfidentialComputeDevtoolHandler =
            std::bind(&NsmConfidentialCompute::patchCCDevMode,
                      confidentialComputeSensor, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);

        AsyncOperationManager::getInstance()
            ->getDispatcher(inventoryObjPath)
            ->addAsyncSetOperation(
                "com.nvidia.CCMode", "CCDevModeEnabled",
                AsyncSetOperationInfo{setconfidentialComputeDevtoolHandler,
                                      confidentialComputeSensor, nsmDevice});
    }
    else if (type == "NSM_PortDisableFuture")
    {
        // Port disable future status on Processor
        auto priority = co_await utils::coGetDbusProperty<bool>(
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
        auto locationType = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());
        auto sensor = std::make_shared<NsmLocationIntfProcessor>(
            bus, name, type, inventoryObjPath, locationType);
        nsmDevice->deviceSensors.push_back(sensor);
    }
    else if (type == "NSM_LocationCode")
    {
        auto locationCode = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "LocationCode", interface.c_str());
        auto sensor = std::make_shared<NsmLocationCodeIntfProcessor>(
            bus, name, type, inventoryObjPath, locationCode);
        nsmDevice->deviceSensors.push_back(sensor);
    }
    else if (type == "NSM_Asset")
    {
        auto assetIntf =
            std::make_shared<NsmAssetIntf>(bus, inventoryObjPath.c_str());
        auto manufacturer = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());

        auto assetObject = NsmInterfaceProvider<NsmAssetIntf>(
            name, type, inventoryObjPath, assetIntf);
        assetObject.pdi().manufacturer(manufacturer);
        // create sensor
        nsmDevice->addStaticSensor(
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
                assetObject, DEVICE_PART_NUMBER));
        nsmDevice->addStaticSensor(
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
                assetObject, SERIAL_NUMBER));
        nsmDevice->addStaticSensor(
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
                assetObject, MARKETING_NAME));
    }
    else if (type == "NSM_MIG")
    {
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());

        bool isLongRunning{false};

        try
        {
            isLongRunning = co_await utils::coGetDbusProperty<bool>(
                objPath.c_str(), "LongRunning", interface.c_str());
        }
        catch (...)
        {}

        auto sensor = std::make_shared<NsmMigMode>(
            bus, name, type, inventoryObjPath, nsmDevice, isLongRunning);

        nsmDevice->addSensor(sensor, priority, isLongRunning);

        AsyncOperationManager::getInstance()
            ->getDispatcher(inventoryObjPath)
            ->addAsyncSetOperation(
                "com.nvidia.MigMode", "MIGModeEnabled",
                AsyncSetOperationInfo{
                    std::bind_front(setMigModeEnabled, isLongRunning), sensor,
                    nsmDevice});
    }
    if (type == "NSM_PCIe")
    {
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto deviceId = co_await utils::coGetDbusProperty<uint64_t>(
            objPath.c_str(), "DeviceId", interface.c_str());
        int count = co_await utils::coGetDbusProperty<uint64_t>(
            objPath.c_str(), "Count", interface.c_str());
        auto pcieDeviceProvider = NsmInterfaceProvider<PCieEccIntf>(
            name, type, dbus::Interfaces{inventoryObjPath});
        auto pCieECCIntf =
            pcieDeviceProvider.interfaces[path(inventoryObjPath)];
        nsmDevice->addSensor(std::make_shared<NsmPCIeLinkSpeed<PCIeEccIntf>>(
                                 pcieDeviceProvider, deviceId),
                             priority);
        for (auto idx = 0; idx < count; idx++)
        {
            auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_" +
                               std::to_string(idx); // port metrics dbus path
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
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());

        bool isLongRunning{false};

        try
        {
            isLongRunning = co_await utils::coGetDbusProperty<bool>(
                objPath.c_str(), "LongRunning", interface.c_str());
        }
        catch (...)
        {}

        auto eccIntf = std::make_shared<EccModeIntf>(bus,
                                                     inventoryObjPath.c_str());

        auto eccModeSensor = std::make_shared<NsmEccMode>(
            name, type, eccIntf, inventoryObjPath, isLongRunning);

        nsmDevice->addSensor(eccModeSensor, priority, isLongRunning);

        auto eccErrorCntSensor = std::make_shared<NsmEccErrorCounts>(
            name, type, eccIntf, inventoryObjPath);

        nsmDevice->addSensor(eccErrorCntSensor, priority);

        AsyncOperationManager::getInstance()
            ->getDispatcher(inventoryObjPath)
            ->addAsyncSetOperation(
                eccIntf->interface, "ECCModeEnabled",
                AsyncSetOperationInfo{
                    std::bind_front(setECCModeEnabled, isLongRunning),
                    eccModeSensor, nsmDevice});
    }
    else if (type == "NSM_EDPp")
    {
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto eDPpIntf = std::make_shared<EDPpLocal>(bus,
                                                    inventoryObjPath.c_str());
        auto resetEdppAsyncIntf = std::make_shared<NsmResetEdppAsyncIntf>(
            bus, inventoryObjPath.c_str(), nsmDevice);

        auto sensor = std::make_shared<NsmEDPpScalingFactor>(
            name, type, inventoryObjPath, eDPpIntf, resetEdppAsyncIntf);

        nsm::AsyncSetOperationHandler patchSetPoint = std::bind(
            &NsmEDPpScalingFactor::patchSetPoint, sensor, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3);

        nsmDevice->addSensor(sensor, priority);
        AsyncOperationManager::getInstance()
            ->getDispatcher(inventoryObjPath)
            ->addAsyncSetOperation(
                "com.nvidia.Edpp", "SetPoint",
                AsyncSetOperationInfo{patchSetPoint, sensor, nsmDevice});

        auto maxEdppSensor = std::make_shared<NsmMaxEDPpLimit>(name, type,
                                                               eDPpIntf);
        auto minEdppSensor = std::make_shared<NsmMinEDPpLimit>(name, type,
                                                               eDPpIntf);
        nsmDevice->addStaticSensor(maxEdppSensor);
        nsmDevice->addStaticSensor(minEdppSensor);
    }
    else if (type == "NSM_CpuOperatingConfig")
    {
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto cpuOperatingConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
            bus, inventoryObjPath.c_str());
        auto smUtilizationIntf =
            std::make_shared<SMUtilizationIntf>(bus, inventoryObjPath.c_str());

        auto clockFreqSensor = std::make_shared<NsmCurrClockFreq>(
            name, type, cpuOperatingConfigIntf, inventoryObjPath);
        auto clockLimitSensor = std::make_shared<NsmClockLimitGraphics>(
            name, type, cpuOperatingConfigIntf, inventoryObjPath);
        auto minGraphicsClockFreq = std::make_shared<NsmMinGraphicsClockLimit>(
            name, type, cpuOperatingConfigIntf, inventoryObjPath);
        auto maxGraphicsClockFreq = std::make_shared<NsmMaxGraphicsClockLimit>(
            name, type, cpuOperatingConfigIntf, inventoryObjPath);

        bool isCurrentUtilizationLongRunning{false};

        try
        {
            isCurrentUtilizationLongRunning =
                co_await utils::coGetDbusProperty<bool>(
                    objPath.c_str(), "CurrentUtilizationLongRunning",
                    interface.c_str());
        }
        catch (...)
        {}

        auto currentUtilization = std::make_shared<NsmCurrentUtilization>(
            name + "_CurrentUtilization", type, cpuOperatingConfigIntf,
            smUtilizationIntf, inventoryObjPath,
            isCurrentUtilizationLongRunning);

        auto defaultBoostClockSpeed =
            std::make_shared<NsmDefaultBoostClockSpeed>(name, type,
                                                        cpuOperatingConfigIntf);
        auto defaultBaseClockSpeed = std::make_shared<NsmDefaultBaseClockSpeed>(
            name, type, cpuOperatingConfigIntf);
        nsmDevice->addStaticSensor(defaultBaseClockSpeed);
        nsmDevice->addStaticSensor(defaultBoostClockSpeed);

        nsmDevice->addSensor(clockFreqSensor, priority);
        nsmDevice->addSensor(clockLimitSensor, priority);
        nsmDevice->addSensor(currentUtilization, priority,
                             isCurrentUtilizationLongRunning);

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
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto deviceId = co_await utils::coGetDbusProperty<uint64_t>(
            objPath.c_str(), "DeviceId", interface.c_str());

        bool isGetViolationDurationLongRunning{false};

        try
        {
            isGetViolationDurationLongRunning =
                co_await utils::coGetDbusProperty<bool>(
                    objPath.c_str(), "GetViolationDurationLongRunning",
                    interface.c_str());
        }
        catch (...)
        {}

        auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
            bus, inventoryObjPath.c_str());

        auto throttleReasonSensor =
            std::make_shared<NsmProcessorThrottleReason>(
                name, type, processorPerfIntf, inventoryObjPath);

        auto throttleDurationSensor =
            std::make_shared<NsmProcessorThrottleDuration>(
                name, type, processorPerfIntf, inventoryObjPath,
                isGetViolationDurationLongRunning);

        auto gpuUtilSensor = std::make_shared<NsmAccumGpuUtilTime>(
            name, type, processorPerfIntf, inventoryObjPath);
        auto pciRxTxSensor = std::make_shared<NsmPciGroup5>(
            name, type, processorPerfIntf, deviceId, inventoryObjPath);

        nsmDevice->addSensor(gpuUtilSensor, priority);
        nsmDevice->addSensor(pciRxTxSensor, priority);
        nsmDevice->addSensor(throttleReasonSensor, priority);
        nsmDevice->addSensor(throttleDurationSensor, priority,
                             isGetViolationDurationLongRunning);
    }
    else if (type == "NSM_MemCapacityUtil")
    {
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());

        bool isLongRunning{false};
        try
        {
            isLongRunning = co_await utils::coGetDbusProperty<bool>(
                objPath.c_str(), "LongRunning", interface.c_str());
        }
        catch (...)
        {}

        auto totalMemorySensor = std::make_shared<NsmTotalMemory>(name, type);
        nsmDevice->addSensor(totalMemorySensor, priority);
        auto sensor = std::make_shared<NsmMemoryCapacityUtil>(
            bus, name, type, inventoryObjPath, totalMemorySensor,
            isLongRunning);
        nsmDevice->addSensor(sensor, priority, isLongRunning);
    }
    else if (type == "NSM_PowerCap")
    {
        std::vector<std::string> candidateForList =
            co_await utils::coGetDbusProperty<std::vector<std::string>>(
                objPath.c_str(), "CompositeNumericSensors", interface.c_str());
        auto priority = co_await utils::coGetDbusProperty<bool>(
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

        auto persistencyIntf = std::make_shared<PowerPersistencyIntf>(
            bus, inventoryObjPath.c_str());

        // create sensors for power cap properties
        auto powerCap = std::make_shared<NsmPowerCap>(
            name, type, powerCapIntf, candidateForList, persistencyIntf,
            inventoryObjPath);
        nsmDevice->addSensor(powerCap, priority);
        nsmDevice->capabilityRefreshSensors.emplace_back(powerCap);
        manager.powerCapList.emplace_back(powerCap);

        auto defaultPowerCap = std::make_shared<NsmDefaultPowerCap>(
            name, type, clearPowerCapIntf, clearPowerCapAsyncIntf);
        manager.defaultPowerCapList.emplace_back(defaultPowerCap);

        auto maxPowerCap = std::make_shared<NsmMaxPowerCap>(
            name, type, powerCapIntf, powerLimitIntf, inventoryObjPath);
        manager.maxPowerCapList.emplace_back(maxPowerCap);

        auto minPowerCap = std::make_shared<NsmMinPowerCap>(
            name, type, powerCapIntf, powerLimitIntf, inventoryObjPath);
        manager.minPowerCapList.emplace_back(minPowerCap);

        nsmDevice->addStaticSensor(defaultPowerCap);
        nsmDevice->addStaticSensor(maxPowerCap);
        nsmDevice->addStaticSensor(minPowerCap);
    }
    else if (type == "NSM_InbandReconfigPermissions")
    {
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto featuresNames =
            co_await utils::coGetDbusProperty<std::vector<std::string>>(
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
            auto pdiObjPath = inventoryObjPath + "/InbandReconfigPermissions/" +
                              featureName;

            auto nsmReconfigSettings = std::make_shared<NsmSetReconfigSettings>(
                featureName, manager, pdiObjPath,
                NsmReconfigPermissions::getIndex(feature));
            auto sensor = std::make_shared<NsmReconfigPermissions>(
                *nsmReconfigSettings, feature);
            nsmDevice->deviceSensors.emplace_back(nsmReconfigSettings);
            nsmDevice->addSensor(sensor, priority);

            auto& asyncDispatcher =
                *AsyncOperationManager::getInstance()->getDispatcher(
                    pdiObjPath);
            auto allowOneShotConfig =
                std::bind_front(&NsmSetReconfigSettings::allowOneShotConfig,
                                nsmReconfigSettings.get());
            auto allowPersistentConfig =
                std::bind_front(&NsmSetReconfigSettings::allowPersistentConfig,
                                nsmReconfigSettings.get());
            auto allowFLRPersistentConfig = std::bind_front(
                &NsmSetReconfigSettings::allowFLRPersistentConfig,
                nsmReconfigSettings.get());
            asyncDispatcher.addAsyncSetOperation(
                "com.nvidia.InbandReconfigSettings", "AllowOneShotConfig",
                AsyncSetOperationInfo{allowOneShotConfig, sensor, nsmDevice});
            asyncDispatcher.addAsyncSetOperation(
                "com.nvidia.InbandReconfigSettings", "AllowPersistentConfig",
                AsyncSetOperationInfo{allowPersistentConfig, sensor,
                                      nsmDevice});
            asyncDispatcher.addAsyncSetOperation(
                "com.nvidia.InbandReconfigSettings", "AllowFLRPersistentConfig",
                AsyncSetOperationInfo{allowFLRPersistentConfig, sensor,
                                      nsmDevice});
        }
    }
    else if (type == "NSM_PowerSmoothing")
    {
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());

        std::shared_ptr<OemPowerSmoothingFeatIntf> pwrSmoothingIntf =
            std::make_shared<OemPowerSmoothingFeatIntf>(bus, inventoryObjPath,
                                                        nsmDevice);

        AsyncOperationManager::getInstance()
            ->getDispatcher(pwrSmoothingIntf->getInventoryObjPath())
            ->addAsyncSetOperation(
                pwrSmoothingIntf->PowerSmoothingIntf::interface,
                "PowerSmoothingEnabled",
                AsyncSetOperationInfo{
                    std::bind_front(
                        &OemPowerSmoothingFeatIntf::setPowerSmoothingEnabled,
                        pwrSmoothingIntf),
                    {},
                    nsmDevice});

        AsyncOperationManager::getInstance()
            ->getDispatcher(pwrSmoothingIntf->getInventoryObjPath())
            ->addAsyncSetOperation(
                pwrSmoothingIntf->PowerSmoothingIntf::interface,
                "ImmediateRampDownEnabled",
                AsyncSetOperationInfo{
                    std::bind_front(
                        &OemPowerSmoothingFeatIntf::setImmediateRampDownEnabled,
                        pwrSmoothingIntf),
                    {},
                    nsmDevice});
        auto controlSensor = std::make_shared<NsmPowerSmoothing>(
            name, type, inventoryObjPath, pwrSmoothingIntf);
        nsmDevice->deviceSensors.emplace_back(controlSensor);

        auto lifetimeCicuitrySensor = std::make_shared<NsmHwCircuitryTelemetry>(
            name, type, inventoryObjPath, pwrSmoothingIntf);
        nsmDevice->deviceSensors.emplace_back(lifetimeCicuitrySensor);

        std::shared_ptr<OemAdminProfileIntf> adminProfileIntf =
            std::make_shared<OemAdminProfileIntf>(bus, inventoryObjPath,
                                                  nsmDevice);

        AsyncOperationManager::getInstance()
            ->getDispatcher(adminProfileIntf->getInventoryObjPath())
            ->addAsyncSetOperation(
                adminProfileIntf->AdminPowerProfileIntf::interface,
                "TMPFloorPercent",
                AsyncSetOperationInfo{
                    std::bind_front(&OemAdminProfileIntf::setTmpFloorPercent,
                                    adminProfileIntf),
                    {},
                    nsmDevice});
        AsyncOperationManager::getInstance()
            ->getDispatcher(adminProfileIntf->getInventoryObjPath())
            ->addAsyncSetOperation(
                adminProfileIntf->AdminPowerProfileIntf::interface,
                "RampUpRate",
                AsyncSetOperationInfo{
                    std::bind_front(&OemAdminProfileIntf::setRampUpRate,
                                    adminProfileIntf),
                    {},
                    nsmDevice});

        AsyncOperationManager::getInstance()
            ->getDispatcher(adminProfileIntf->getInventoryObjPath())
            ->addAsyncSetOperation(
                adminProfileIntf->AdminPowerProfileIntf::interface,
                "RampDownRate",
                AsyncSetOperationInfo{
                    std::bind_front(&OemAdminProfileIntf::setRampDownRate,
                                    adminProfileIntf),
                    {},
                    nsmDevice});
        AsyncOperationManager::getInstance()
            ->getDispatcher(adminProfileIntf->getInventoryObjPath())
            ->addAsyncSetOperation(
                adminProfileIntf->AdminPowerProfileIntf::interface,
                "RampDownHysteresis",
                AsyncSetOperationInfo{
                    std::bind_front(&OemAdminProfileIntf::setRampDownHysteresis,
                                    adminProfileIntf),
                    {},
                    nsmDevice});

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

        std::shared_ptr<NsmPowerSmoothingAction> pwrSmoothingAction =
            std::make_shared<NsmPowerSmoothingAction>(
                bus, name, type, inventoryObjPath, nsmDevice);

        auto currentProfileSensor =
            std::make_shared<NsmCurrentPowerSmoothingProfile>(
                name, type, inventoryObjPath, pwrSmoothingCurProfileIntf,
                getAllPowerProfileSensor, adminProfileSensor);
        /*,pwrSmoothingAction*/
        // nsmDevice->deviceSensors.emplace_back(currentProfileSensor);
        nsmDevice->deviceSensors.emplace_back(pwrSmoothingAction);

        nsmDevice->addSensor(getAllPowerProfileSensor, priority);
        nsmDevice->addSensor(adminProfileSensor, priority);
        nsmDevice->addSensor(controlSensor, priority);
        nsmDevice->addSensor(lifetimeCicuitrySensor, priority);
        nsmDevice->addSensor(currentProfileSensor, priority);
    }
    else if (type == "NSM_TotalNvLinksCount")
    {
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto totalNvLinkInterface = std::make_shared<TotalNvLinkInterface>(
            bus, inventoryObjPath.c_str());
        auto totalNvLinkSensor = std::make_shared<NsmTotalNvLinks>(
            name, type, totalNvLinkInterface, inventoryObjPath);
        nsmDevice->addSensor(totalNvLinkSensor, priority);
    }
    else if (type == "NSM_WorkloadPowerProfile")
    {
        lg2::info("NSM_WorkloadPowerProfile added");
        auto priority = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto profileIdMap =
            co_await utils::coGetDbusProperty<std::vector<std::string>>(
                objPath.c_str(), "ProfileIdMap", interface.c_str());

        auto profileMapper =
            std::make_shared<NsmWorkLoadProfileEnum>(name, type, profileIdMap);
        nsmDevice->addStaticSensor(profileMapper);

        std::shared_ptr<OemProfileInfoIntf> profileStatusInfoIntf =
            std::make_shared<OemProfileInfoIntf>(bus, inventoryObjPath,
                                                 nsmDevice);

        std::shared_ptr<NsmWorkloadProfileInfoAsyncIntf> profileInfoAsyncIntf =
            std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
                bus, inventoryObjPath.c_str(), nsmDevice);
        auto workloadProfileStatusSensor =
            std::make_shared<NsmWorkLoadProfileStatus>(
                name, type, inventoryObjPath, profileStatusInfoIntf,
                profileInfoAsyncIntf);
        nsmDevice->addSensor(workloadProfileStatusSensor, priority);

        auto getAllPowerProfileSensor =
            std::make_shared<NsmWorkloadPowerProfileCollection>(
                name, type, inventoryObjPath, nsmDevice);
        nsmDevice->addStaticSensor(getAllPowerProfileSensor);

        auto workloadPowerProfilePageCollection =
            std::make_shared<NsmWorkloadPowerProfilePageCollection>(
                name, type, inventoryObjPath, nsmDevice);
        nsmDevice->addStaticSensor(workloadPowerProfilePageCollection);

        uint16_t firstPageIndex = 0;
        auto firstPage = std::make_shared<NsmWorkloadPowerProfilePage>(
            name, type, inventoryObjPath, nsmDevice, getAllPowerProfileSensor,
            workloadPowerProfilePageCollection, profileMapper, firstPageIndex);
        nsmDevice->addSensor(firstPage, priority);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
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
    "xyz.openbmc_project.Configuration.NSM_Processor.TotalNvLinksCount",
    "xyz.openbmc_project.Configuration.NSM_Processor.PCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_Processor.WorkloadPowerProfile"};

REGISTER_NSM_CREATION_FUNCTION(createNsmProcessorSensor, nsmProcessorInterfaces)

} // namespace nsm
