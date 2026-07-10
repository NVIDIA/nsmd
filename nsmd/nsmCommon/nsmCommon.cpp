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
#include "nsmCommon.hpp"

#include "platform-environmental.h"

#include "log.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensorManager.hpp"
#include "stateChangeLogger.hpp"

#include <nsmd/sensorManager.hpp>
#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{

// Maximum size for NSM response data buffer
constexpr size_t MAX_NSM_RESPONSE_SIZE = 65535;

NsmMemoryCapacity::NsmMemoryCapacity(const std::string& name,
                                     const std::string& type) :
    NsmSensor(name, type)

{}

std::optional<std::vector<uint8_t>>
    NsmMemoryCapacity::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_inventory_information_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(
        instanceId, MAXIMUM_MEMORY_CAPACITY, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_inventory_information_req failed for property Maximum Memory Capacity "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmMemoryCapacity::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    std::vector<uint8_t> data(MAX_NSM_RESPONSE_SIZE, 0);
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;

    auto rc = decode_get_inventory_information_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode, data.data());

    LG2_ERROR_FLT(
        "decode_get_inventory_information_resp for Maximum Memory Capacity failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        uint32_t maximumMemoryCapacityMiB =
            *reinterpret_cast<uint32_t*>(data.data());
        updateReading(maximumMemoryCapacityMiB);
    }
    else
    {
        updateReading(std::nullopt);
    }

    return cc ? cc : rc;
}

NsmTotalMemory::NsmTotalMemory(const std::string& name,
                               const std::string& type) :
    NsmMemoryCapacity(name, type)
{
    lg2::info("NsmTotalMemory : create sensor:{NAME}", "NAME", name.c_str());
}

void NsmTotalMemory::updateReading(
    std::optional<uint32_t> maximumMemoryCapacity)
{
    totalMemoryCapacity = maximumMemoryCapacity;
}

const std::optional<uint32_t> NsmTotalMemory::getReading()
{
    return totalMemoryCapacity;
}

NsmMemoryCapacityUtil::NsmMemoryCapacityUtil(
    const NsmInterfaceProvider<DimmMemoryMetricsIntf>& provider,
    std::shared_ptr<NsmTotalMemory> totalMemory, bool isLongRunning,
    std::shared_ptr<NsmDevice> device) :
    NsmLongRunningSensor(provider.getName(), provider.getType(), isLongRunning,
                         device, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                         NSM_GET_MEMORY_CAPACITY_UTILIZATION),
    NsmInterfaceContainer(provider), totalMemory(totalMemory)
{
    lg2::info("NsmMemoryCapacityUtil: create sensor:{NAME}", "NAME",
              NsmSensor::getName().c_str());
    updateMetricOnSharedMemory();
}

void NsmMemoryCapacityUtil::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    invoke([](const auto& path, auto& pdi) {
        std::vector<uint8_t> smbusData = {};
        nsm_shmem_utils::SharedMemoryManager::cacheTALData(
            path, pdi.interface, "CapacityUtilizationPercent", smbusData,
            static_cast<uint16_t>(pdi.capacityUtilizationPercent()));
    });
#endif
}

void NsmMemoryCapacityUtil::updateReading(
    const struct nsm_memory_capacity_utilization& data)
{
    const auto totalMemoryCapacity = totalMemory->getReading();
    if (!totalMemoryCapacity)
    {
        return;
    }
    else if ((*totalMemoryCapacity) == 0)
    {
        return;
    }

    uint8_t usedMemoryPercent = (data.used_memory + data.reserved_memory) *
                                100 / (*totalMemoryCapacity);
    invoke(pdiMethod(capacityUtilizationPercent), usedMemoryPercent);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmMemoryCapacityUtil::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_memory_capacity_util_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_memory_capacity_util_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmMemoryCapacityUtil::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    struct nsm_memory_capacity_utilization data;
    uint16_t dataSize = 0;
    uint16_t reasonCode = ERR_NULL;

    auto handleFunctionName = isLongRunning
                                  ? "decode_get_memory_capacity_util_event_resp"
                                  : "decode_get_memory_capacity_util_resp";
    auto rc = isLongRunning
                  ? decode_get_memory_capacity_util_event_resp(
                        responseMsg, responseLen, &cc, &reasonCode, &data)
                  : decode_get_memory_capacity_util_resp(
                        responseMsg, responseLen, &cc, &dataSize, &reasonCode,
                        &data);

    LG2_ERROR_FLT(
        "{FUNCNAME} failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "FUNCNAME", handleFunctionName, "REASONCODE", reasonCode, "CC", cc,
        "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(data);
    }

    return cc ? cc : rc;
}
requester::Coroutine
    NsmMemoryCapacityUtil::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    auto rc = co_await totalMemory->update(nsmDevice);
    rc = co_await NsmLongRunningSensor::update(nsmDevice);
    // coverity[missing_return]
    co_return rc;
}

bool NsmMemoryCapacityUtil::equals(const NsmSensor& other) const
{
    auto otherAsMemoryCapacityUtil =
        dynamic_cast<const NsmMemoryCapacityUtil*>(&other);

    return otherAsMemoryCapacityUtil &&
           totalMemory->equals(*otherAsMemoryCapacityUtil->totalMemory) &&
           NsmSensor::equals(other);
}

NsmMinGraphicsClockLimit::NsmMinGraphicsClockLimit(
    std::string& name, std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    std::string& inventoryObjPath) :
    NsmObject(name, type), cpuOperatingConfigIntf(cpuConfigIntf),
    inventoryObjPath(inventoryObjPath)
{
    lg2::info("NsmMinGraphicsClockLimit: create sensor:{NAME}", "NAME",
              name.c_str());

    updateMetricOnSharedMemory();
}

requester::Coroutine
    NsmMinGraphicsClockLimit::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MINIMUM_GRAPHICS_CLOCK_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmMinGraphicsClockLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmMinGraphicsClockLimit sensorIO failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog(
            "NsmMinGraphicsClockLimit decode_get_inventory_information_resp",
            reasonCode, cc, rc, dataSize != sizeof(value)))
    {
        LG2_ERROR(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        cpuOperatingConfigIntf->minSpeed(value);
        updateMetricOnSharedMemory();
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

void NsmMinGraphicsClockLimit::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(cpuOperatingConfigIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "MinSpeed";
    nv::sensor_aggregation::DbusVariantType minSpeedVal{
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::minSpeed()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, minSpeedVal);
#endif
}

NsmMaxGraphicsClockLimit::NsmMaxGraphicsClockLimit(
    std::string& name, std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    std::string& inventoryObjPath) :
    NsmObject(name, type), cpuOperatingConfigIntf(cpuConfigIntf),
    inventoryObjPath(inventoryObjPath)
{
    lg2::info("NsmMaxGraphicsClockLimit: create sensor:{NAME}", "NAME",
              name.c_str());

    updateMetricOnSharedMemory();
}

requester::Coroutine
    NsmMaxGraphicsClockLimit::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MAXIMUM_GRAPHICS_CLOCK_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmMaxGraphicsClockLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmMaxGraphicsClockLimit sensorIO failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog(
            "NsmMaxGraphicsClockLimit decode_get_inventory_information_resp",
            reasonCode, cc, rc, dataSize != sizeof(value)))
    {
        LG2_ERROR(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        cpuOperatingConfigIntf->maxSpeed(value);
        updateMetricOnSharedMemory();
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

void NsmMaxGraphicsClockLimit::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(cpuOperatingConfigIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "MaxSpeed";
    nv::sensor_aggregation::DbusVariantType maxSpeedVal{
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::maxSpeed()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, maxSpeedVal);
#endif
}

requester::Coroutine
    getDeviceUUID(std::shared_ptr<NsmDevice> nsmDevice,
                  [[maybe_unused]] mctp::MctpDiscovery& mctpDiscovery,
                  uuid_t& uuid)
{
    static std::map<eid_t, StateChangeLogger> loggers;
    if (nsmDevice)
    {
        // Scenario 1:
        // Handle special case for baseboard/FPGA, where we do not event
        // want to send command to device ref bug Invalid NSM data detected
        // by FPGA in MCTP bridge(4790483)
        if (nsmDevice->getDeviceType() == NSM_DEV_ID_BASEBOARD)
        {
            lg2::info(
                "getDeviceUUID::baseboard/FPGA, using mctp uuid eid={EID}",
                "EID", nsmDevice->getEid());
            uuid = nsmDevice->getUuid();
            nsmDevice->deviceUuid = uuid;
            co_return NSM_SW_SUCCESS;
        }

        // Create request message for getting inventory information
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_inventory_information_req));
        auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());

        // Encode request for device GUID
        auto rc = encode_get_inventory_information_req(DEFAULT_INSTANCE_ID,
                                                       DEVICE_GUID, requestPtr);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::debug(
                "getDeviceUUID::encode_get_inventory_information_req failed. eid={EID} rc={RC}",
                "EID", nsmDevice->getEid(), "RC", rc);
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        // Send request and get response
        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request,
                                          responseMsg, responseLen, false);
        if (rc)
        {
            // Scenario 2: MessageType 3 or Get Inventory is itself not
            // supported e.g. ERot
            if (rc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
            {
                // inside this block means Get Inventory is not supported
                // so fallback to mctp uuid
                uuid = nsmDevice->getUuid();
                nsmDevice->deviceUuid = uuid; // falling back to mctp uuid
                lg2::info(
                    "getDeviceUUID::sensorIO failed with rc not supported, assinging mctp uuid eid={EID} rc={RC}",
                    "EID", nsmDevice->getEid(), "RC", rc);
                co_return NSM_SW_SUCCESS;
            }
            lg2::error("getDeviceUUID::sensorIO failed. eid={EID} rc={RC}",
                       "EID", nsmDevice->getEid(), "RC", rc);
            co_return rc;
        }

        // Decode response
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason_code = ERR_NULL;
        uint16_t dataSize = 0;
        std::vector<uint8_t> data(MAX_NSM_RESPONSE_SIZE, 0);

        rc = decode_get_inventory_information_resp(
            responseMsg.get(), responseLen, &cc, &reason_code, &dataSize,
            data.data());

        if (loggers[nsmDevice->getEid()].shouldLog(
                "getDeviceUUID::decode_get_inventory_information_resp",
                reason_code, cc, rc))
        {
            lg2::error(
                "getDeviceUUID::decode_get_inventory_information_resp failed. eid={EID} cc={CC} reasonCode={REASONCODE} rc={RC}",
                "EID", nsmDevice->getEid(), "CC", cc, "REASONCODE", reason_code,
                "RC", rc);
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }

        if (cc == NSM_SUCCESS)
        {
            std::vector<uint8_t> nvu8ArrVal(UUID_INT_SIZE, 0);
            // DEVICE_GUID is a fixed-width field: require the wire dataSize to
            // be exactly the UUID width and reject anything else, rather than
            // accepting an oversized value and truncating into the buffer.
            if (dataSize != UUID_INT_SIZE || data.size() < UUID_INT_SIZE)
            {
                lg2::error(
                    "getDeviceUUID::decode_get_inventory_information_resp invalid property data. eid={EID} property={PROP}",
                    "EID", nsmDevice->getEid(), "PROP", DEVICE_GUID);
                co_return NSM_SW_ERROR_LENGTH;
            }
            memcpy(nvu8ArrVal.data(), data.data(), UUID_INT_SIZE);
            uuid = utils::convertUUIDToString(nvu8ArrVal);
            if (uuid.empty())
            {
                lg2::error(
                    "getDeviceUUID::getInventoryInformation received incorrect GUID for property={PROP} for eid={EID}",
                    "PROP", DEVICE_GUID, "EID", nsmDevice->getEid());
                co_return NSM_SW_ERROR_COMMAND_FAIL;
            }

            nsmDevice->deviceUuid = uuid;
            lg2::info(
                "getDeviceUUID::SendRecvNsmMsg pass assinging device uuid from getInventory command eid={EID} rc={RC}",
                "EID", nsmDevice->getEid(), "RC", rc);
            co_return NSM_SW_SUCCESS;
        }
        // Case 3: Type 3 Get Inventory information is supported but
        // specific arg (DEVICE_GUID) command is not supported for devices
        // like cx7 which do not support DEVICE_GUID identifier will return
        // NSM_ERR_INVALID_DATA.
        else if (cc == NSM_ERR_INVALID_DATA)
        {
            // inside this block means DEVICE_GUID is not supported
            // so fallback to mctp uuid
            lg2::info(
                "getDeviceUUID::Got CC value as invalid data, assinging mctp uuid eid={EID} rc={RC}",
                "EID", nsmDevice->getEid(), "RC", rc);
            uuid = nsmDevice->getUuid();
            nsmDevice->deviceUuid = uuid; // falling back to mctp uuid
            co_return NSM_SW_SUCCESS;
        }
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_ERROR_COMMAND_FAIL;
}
} // namespace nsm
