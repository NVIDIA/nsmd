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

#include "nsmGpuPciePort.hpp"

#include "libnsm/pci-links.h"
#include "nsmPriorityMapping.h"

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
#include "asyncOperationManager.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmCommon/nsmPcieGroup.hpp"
#include "nsmCommon/nsmPciePortIntf.hpp"
#include "nsmInterface.hpp"
#include "nsmPCIeLinkSpeed.hpp"

#include <cstdint>
#include <unordered_map>

#define GPU_PCIe_INTERFACE "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0"
namespace nsm
{

NsmGpuPciePort::NsmGpuPciePort(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::string& health, const std::string& chasisState,
    const std::vector<utils::Association>& associations,
    const std::string& inventoryObjPath) : NsmObject(name, type)
{
    lg2::info("NsmGpuPciePort: create sensor:{NAME}", "NAME", name.c_str());
    associationDefIntf =
        std::make_unique<AssociationDefIntf>(bus, inventoryObjPath.c_str());

    chasisStateIntf =
        std::make_unique<ChasisStateIntf>(bus, inventoryObjPath.c_str());
    chasisStateIntf->currentPowerState(
        ChasisStateIntf::convertPowerStateFromString(chasisState));

    healthIntf = std::make_unique<HealthIntf>(bus, inventoryObjPath.c_str());
    healthIntf->health(HealthIntf::convertHealthTypeFromString(health));

    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDefIntf->associations(associations_list);
}

NsmGpuPciePortInfo::NsmGpuPciePortInfo(
    const std::string& name, const std::string& type,
    const std::string& portType, const std::string& portProtocol,
    std::shared_ptr<PortInfoIntf> portInfoIntf) :
    NsmObject(name, type), portInfoIntf(portInfoIntf)
{
    lg2::info("NsmGpuPciePortInfo: create sensor:{NAME}", "NAME", name.c_str());
    portInfoIntf->type(PortInfoIntf::convertPortTypeFromString(portType));
    portInfoIntf->protocol(
        PortInfoIntf::convertPortProtocolFromString(portProtocol));
}

NsmClearPCIeCounters::NsmClearPCIeCounters(
    const std::string& name, const std::string& type, const uint8_t groupId,
    uint8_t deviceIndex, std::shared_ptr<ClearPCIeIntf> clearPCIeIntf) :
    NsmObject(name, type), groupId(groupId), deviceIndex(deviceIndex),
    clearPCIeIntf(clearPCIeIntf)
{
    lg2::info("NsmClearPCIeCounters:: create sensor {NAME} of group {GROUP_ID}",
              "NAME", name, "GROUP_ID", groupId);
}

requester::Coroutine
    NsmClearPCIeCounters::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        0, deviceIndex, groupId, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_query_available_clearable_scalar_data_sources_v1_req failed for group {A}. eid={EID} rc={RC}",
            "A", groupId, "EID", nsmDevice->getEid(), "RC", rc);
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
            "NsmClearPCIeCounters SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint8_t mask_length;
    bitfield8_t availableSource[MAX_SCALAR_SOURCE_MASK_SIZE];
    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE];

    rc = decode_query_available_clearable_scalar_data_sources_v1_resp(
        responseMsg.get(), responseLen, &cc, &dataSize, &reason_code,
        &mask_length, (uint8_t*)availableSource, (uint8_t*)clearableSource);

    LG2_ERROR_FLT(
        "decode_query_available_clearable_scalar_data_sources_v1_respp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reason_code, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(clearableSource);
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

void NsmClearPCIeCounters::findAndUpdateCounter(
    CounterType counter, std::vector<CounterType>& clearableCounters)
{
    if (std::find(clearableCounters.begin(), clearableCounters.end(),
                  counter) == clearableCounters.end())
    {
        clearableCounters.push_back(counter);
    }
}

void NsmClearPCIeCounters::updateReading(const bitfield8_t clearableSource[])
{
    std::vector<CounterType> clearableCounters =
        clearPCIeIntf->clearableCounters();

    switch (groupId)
    {
        case 2:
            if (clearableSource[0].bits.bit0)
            {
                findAndUpdateCounter(CounterType::NonFatalErrorCount,
                                     clearableCounters);
            }
            if (clearableSource[0].bits.bit1)
            {
                findAndUpdateCounter(CounterType::FatalErrorCount,
                                     clearableCounters);
            }
            if (clearableSource[0].bits.bit2)
            {
                findAndUpdateCounter(CounterType::UnsupportedRequestCount,
                                     clearableCounters);
            }
            if (clearableSource[0].bits.bit3)
            {
                findAndUpdateCounter(CounterType::CorrectableErrorCount,
                                     clearableCounters);
            }
            break;
        case 3:
            if (clearableSource[0].bits.bit0)
            {
                findAndUpdateCounter(CounterType::L0ToRecoveryCount,
                                     clearableCounters);
            }
            break;
        case 4:
            if (clearableSource[0].bits.bit1)
            {
                findAndUpdateCounter(CounterType::NAKReceivedCount,
                                     clearableCounters);
            }
            if (clearableSource[0].bits.bit2)
            {
                findAndUpdateCounter(CounterType::NAKSentCount,
                                     clearableCounters);
            }
            if (clearableSource[0].bits.bit4)
            {
                findAndUpdateCounter(CounterType::ReplayRolloverCount,
                                     clearableCounters);
            }
            if (clearableSource[0].bits.bit6)
            {
                findAndUpdateCounter(CounterType::ReplayCount,
                                     clearableCounters);
            }
            break;
        default:
            break;
    }
    clearPCIeIntf->clearableCounters(clearableCounters);
}

requester::Coroutine NsmClearPCIeIntf::clearPCIeErrorCounter(
    AsyncOperationStatusType* status, const uint8_t deviceIndex,
    const uint8_t groupId, const uint8_t dsId)
{
    auto eid = device->getEid();
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_clear_data_source_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // first argument instanceid=0 is irrelevant
    auto rc = encode_clear_data_source_v1_req(0, deviceIndex, groupId, dsId,
                                              requestMsg);

    if (rc)
    {
        lg2::error(
            "clearPCIeErrorCounter encode_clear_data_source_v1_req failed. eid={EID}, rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await device->postPatchIO(eid, request, responseMsg,
                                            responseLen);
    if (rc_)
    {
        lg2::error(
            "clearPCIeErrorCounter postPatchIO failed for for eid = {EID} rc = {RC}",
            "EID", eid, "RC", rc_);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint16_t data_size = 0;
    rc = decode_clear_data_source_v1_resp(responseMsg.get(), responseLen, &cc,
                                          &data_size, &reason_code);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        std::shared_ptr<NsmPcieGroup> sensor =
            getClearCounterSensorFromGroup(groupId);
        if (sensor)
        {
            lg2::info("clearPCIeErrorCounter refreshing group id : {CTR}",
                      "CTR", groupId);
            co_await sensor->update(device);
        }
        else
        {
            lg2::error(
                "clearPCIeErrorCounter unable to find groupID {A} sensor ", "A",
                groupId);
        }
        lg2::info("clearPCIeErrorCounter for EID: {EID} completed", "EID", eid);
    }
    else
    {
        lg2::error(
            "clearPCIeErrorCounter decode_clear_data_source_v1_resp failed.eid ={EID},CC = {CC} reasoncode = {RC},RC = {A} ",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmClearPCIeIntf::doClearPCIeCountersOnDevice(
    std::shared_ptr<AsyncStatusIntf> statusInterface,
    const std::string& Counter)
{
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    const uint8_t groupId = std::get<0>(counterToGroupIdMap[Counter]);
    const uint8_t dsId = std::get<1>(counterToGroupIdMap[Counter]);

    auto rc_ = co_await clearPCIeErrorCounter(&status, deviceIndex, groupId,
                                              dsId);

    statusInterface->status(status);
    // coverity[missing_return]
    co_return rc_;
};

sdbusplus::message::object_path
    NsmClearPCIeIntf::clearCounter(std::string Counter)
{
    lg2::info("NsmClearPCIeIntf::clearCounter, counter: {CTR}", "CTR", Counter);
    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        lg2::error(
            "NsmClearPCIeCounters::clearCounter failed. No available result Object to allocate for the Post request.");
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    if (counterToGroupIdMap.find(Counter) == counterToGroupIdMap.end())
    {
        lg2::error("Invalid Counter Name. Counter: {CTR}", "CTR", Counter);
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    doClearPCIeCountersOnDevice(statusInterface, Counter).detach();

    return objectPath;
}

void NsmClearPCIeIntf::addClearCoutnerSensor(
    uint8_t groupId, std::shared_ptr<NsmPcieGroup> sensor)
{
    auto [it, inserted] = clearCoutnerSensorMap.try_emplace(groupId, sensor);
    if (!inserted)
    {
        lg2::error(
            "NsmClearPCIeIntf::addClearCoutnerSensor, GroupdId already linked: {A}",
            "A", groupId);
    }
}

std::shared_ptr<NsmPcieGroup>
    NsmClearPCIeIntf::getClearCounterSensorFromGroup(uint8_t groupId)
{
    auto it = clearCoutnerSensorMap.find(groupId);
    return (it != clearCoutnerSensorMap.end()) ? it->second : nullptr;
}

static requester::Coroutine createNsmGpuPcieSensor(SensorManager& manager,
                                                   const std::string& interface,
                                                   const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();

        dbus::PropertyMap allBaseIfaceProperties;
        auto rc = co_await utils::coGetCachedBaseProperties(
            objPath, GPU_PCIe_INTERFACE, allBaseIfaceProperties);
        if (rc != NSM_SUCCESS)
        {
            co_return rc;
        }
        auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
            utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

        std::string name{};
        if (allBaseIfaceProperties.count("Name"))
        {
            name = std::get<std::string>(allBaseIfaceProperties.at("Name"));
        }
        uuid_t uuid{};
        if (allBaseIfaceProperties.count("UUID"))
        {
            uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
        }
        std::string type{};
        if (allCurrentIfaceProperties.count("Type"))
        {
            type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
        }
        std::string inventoryObjPath{};
        if (allBaseIfaceProperties.count("InventoryObjPath"))
        {
            inventoryObjPath = std::get<std::string>(
                allBaseIfaceProperties.at("InventoryObjPath"));
        }

        auto processorPath = inventoryObjPath;
        inventoryObjPath = inventoryObjPath + "/Ports/PCIe_0";

        auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
        if (!nsmDevice)
        {
            // cannot found a nsmDevice for the sensor
            lg2::error(
                "The UUID of NSM_GPU_PCIe_0 PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);
            // coverity[missing_return]
            co_return NSM_ERROR;
        }
        if (type == "NSM_GPU_PCIe_0")
        {
            std::vector<utils::Association> associations{};
            co_await utils::coGetAssociations(
                objPath, interface + ".Associations", associations);
            std::string health{};
            if (allCurrentIfaceProperties.count("Health"))
            {
                health = std::get<std::string>(
                    allCurrentIfaceProperties.at("Health"));
            }
            std::string chasisState{};
            if (allCurrentIfaceProperties.count("ChasisPowerState"))
            {
                chasisState = std::get<std::string>(
                    allCurrentIfaceProperties.at("ChasisPowerState"));
            }

            auto sensor = std::make_shared<NsmGpuPciePort>(
                bus, name, type, health, chasisState, associations,
                inventoryObjPath);
            nsmDevice->getDeviceSensors().emplace_back(sensor);
            uint64_t deviceIndex{};
            if (allBaseIfaceProperties.count("DeviceIndex"))
            {
                deviceIndex = std::get<uint64_t>(
                    allBaseIfaceProperties.at("DeviceIndex"));
            }
            std::vector<uint64_t> clearableScalarGroup{};
            if (allBaseIfaceProperties.count("ClearableScalarGroup"))
            {
                clearableScalarGroup = std::get<std::vector<uint64_t>>(
                    allBaseIfaceProperties.at("ClearableScalarGroup"));
            }

            auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
                bus, inventoryObjPath.c_str(), deviceIndex, nsmDevice);

            for (auto groupId : clearableScalarGroup)
            {
                auto clearPCIeSensorGroup =
                    std::make_shared<NsmClearPCIeCounters>(
                        name, type, groupId, deviceIndex, clearPCIeIntf);
                nsmDevice->addStaticSensor(clearPCIeSensorGroup);
            }

            auto laneErrorIntf =
                std::make_shared<LaneErrorIntf>(bus, inventoryObjPath.c_str());
            auto perLanErrorSensor = std::make_shared<NsmPCIeECCGroup8>(
                name, type, laneErrorIntf, deviceIndex, inventoryObjPath);
            nsmDevice->addSensor(perLanErrorSensor,
                                 PER_LANE_ERROR_COUNT_PRIORITY);

            // add PCIe and CC interface and corresponding sensors
            // TODO: read priority from em config

            bool priority = false;
            auto pcieECCIntf =
                std::make_shared<PCIeEccIntf>(bus, processorPath.c_str());
            auto pcieDeviceProvider =
                NsmInterfaceProvider(name, type, processorPath, pcieECCIntf);

            nsmDevice->addSensor(
                std::make_shared<NsmPCIeLinkSpeed<PCIeEccIntf>>(
                    pcieDeviceProvider, deviceIndex),
                priority);

            auto pciePortIntf =
                std::make_shared<PCIeEccIntf>(bus, inventoryObjPath.c_str());

            auto pciPortSensor = std::make_shared<NsmPciePortIntf>(
                bus, name, type, inventoryObjPath);

            auto sensorGroup2 = std::make_shared<NsmPciGroup2>(
                name, type, pcieECCIntf, pciePortIntf, deviceIndex,
                processorPath);

            auto sensorGroup3 = std::make_shared<NsmPciGroup3>(
                name, type, pcieECCIntf, pciePortIntf, deviceIndex,
                processorPath);

            auto sensorGroup4 = std::make_shared<NsmPciGroup4>(
                name, type, pcieECCIntf, pciePortIntf, deviceIndex,
                processorPath);
            nsmDevice->getDeviceSensors().push_back(pciPortSensor);
            nsmDevice->addSensor(sensorGroup2, priority);
            nsmDevice->addSensor(sensorGroup3, priority);
            nsmDevice->addSensor(sensorGroup4, priority);
            clearPCIeIntf->addClearCoutnerSensor(2, sensorGroup2);
            clearPCIeIntf->addClearCoutnerSensor(3, sensorGroup3);
            clearPCIeIntf->addClearCoutnerSensor(4, sensorGroup4);
            lg2::info("Type NSM_GPU_PCIe_0 all sensors created");
        }
        else if (type == "NSM_PortInfo")
        {
            std::string portType{};
            if (allCurrentIfaceProperties.count("PortType"))
            {
                portType = std::get<std::string>(
                    allCurrentIfaceProperties.at("PortType"));
            }
            std::string portProtocol{};
            if (allCurrentIfaceProperties.count("PortProtocol"))
            {
                portProtocol = std::get<std::string>(
                    allCurrentIfaceProperties.at("PortProtocol"));
            }
            bool priority{};
            if (allCurrentIfaceProperties.count("Priority"))
            {
                priority =
                    std::get<bool>(allCurrentIfaceProperties.at("Priority"));
            }
            uint64_t deviceIndex{};
            if (allBaseIfaceProperties.count("DeviceIndex"))
            {
                deviceIndex = std::get<uint64_t>(
                    allBaseIfaceProperties.at("DeviceIndex"));
            }

            auto portInfoIntf =
                std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
            auto portWidthIntf =
                std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
            auto portInfoSensor = std::make_shared<NsmGpuPciePortInfo>(
                name, type, portType, portProtocol, portInfoIntf);
            nsmDevice->getDeviceSensors().emplace_back(portInfoSensor);
            auto pcieECCIntfSensorGroup1 = std::make_shared<NsmPCIeECCGroup1>(
                name, type, inventoryObjPath, portInfoIntf, portWidthIntf,
                deviceIndex);

            nsmDevice->addSensor(pcieECCIntfSensorGroup1, priority);
        }
    }

    catch (const std::exception& e)
    {
        lg2::error(
            "Error while addSensor for path {PATH} and interface {INTF}, {ERROR}",
            "PATH", objPath, "INTF", interface, "ERROR", e);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmGpuPcieSensor, "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmGpuPcieSensor,
    "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0.PortInfo")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmGpuPcieSensor,
    "xyz.openbmc_project.Configuration.NSM_GPU_PCIe_0.PortState")
} // namespace nsm
