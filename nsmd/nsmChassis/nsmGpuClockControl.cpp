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

#include "sensorManager.hpp"
#include "nsmGpuClockControl.hpp"
#include <phosphor-logging/lg2.hpp>
#include <nsmCommon/nsmCommon.hpp>
#include <nsmCommon/sharedMemCommon.hpp>
#include "asyncOperationManager.hpp"

#include <cmath>
#include <optional>
#include <vector>

namespace nsm
{

enum class clockLimitFlag{
    PERSISTENCE = 0,
    CLEAR = 1
};

NsmChassisClockControl::NsmChassisClockControl(
    sdbusplus::bus::bus& bus, const std::string& name,
    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf,
    const std::vector<utils::Association>& associations, std::string& type,
    const std::string& inventoryObjPath, const std::string& physicalContext,
    const std::string& clockMode) :
    NsmSensor(name, type), cpuOperatingConfigIntf(cpuOperatingConfigIntf),
    inventoryObjPath(inventoryObjPath)
{
    decoratorAreaIntf = std::make_shared<DecoratorAreaIntf>(bus, inventoryObjPath.c_str());
    decoratorAreaIntf->physicalContext(
        sdbusplus::common::xyz::openbmc_project::inventory::decorator::Area::
            convertPhysicalContextTypeFromString(physicalContext));
    associationDefinitionsInft = std::make_unique<AssociationDefinitionsInft>(
        bus, inventoryObjPath.c_str());
    processorModeIntf =
        std::make_unique<ProcessorModeIntf>(bus, inventoryObjPath.c_str());
    processorModeIntf->clockMode(
        ProcessorModeIntf::convertModeFromString(clockMode));
    // handle associations
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDefinitionsInft->associations(associations_list);
}

void NsmChassisClockControl::updateMetricOnSharedMemory()
{

#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(cpuOperatingConfigIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "SettingMin";
    nv::sensor_aggregation::DbusVariantType settingMin{
        cpuOperatingConfigIntf->requestedSpeedLimitMin()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, settingMin);

    propName = "SettingMax";
    nv::sensor_aggregation::DbusVariantType settingMax{
        cpuOperatingConfigIntf->requestedSpeedLimitMax()};
    nsm_shmem_utils::updateSharedMemoryOnSuccess(
        inventoryObjPath, ifaceName, propName, smbusData, settingMax); 
#endif

}

std::optional<std::vector<uint8_t>>
    NsmChassisClockControl::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_clock_limit_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    uint8_t clock_id = GRAPHICS_CLOCK;
    auto rc = encode_get_clock_limit_req(instanceId, clock_id, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("NsmChassisClockControl: encode_get_clock_limit_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmChassisClockControl::handleResponseMsg(const struct nsm_msg* responseMsg,
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
        cpuOperatingConfigIntf->requestedSpeedLimitMin(
            clockLimit.requested_limit_min);
        cpuOperatingConfigIntf->requestedSpeedLimitMax(
            clockLimit.requested_limit_max);
        updateMetricOnSharedMemory();
    }
    else
    {
        lg2::error(
            "NsmChassisClockControl: handleResponseMsg decode_get_clock_limit_resp  "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return cc;
}

requester::Coroutine NsmChassisClockControl::setMinClockLimits(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const uint32_t* minReqSpeed = std::get_if<uint32_t>(&value);
    if (minReqSpeed == NULL)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("set RequestedSpeedLimitMin On Device for EID: {EID}", "EID",
              eid);

    uint32_t allowableMin = cpuOperatingConfigIntf->minSpeed();
    uint32_t allowableMax = cpuOperatingConfigIntf->maxSpeed();
    uint32_t maxReqSpeed = cpuOperatingConfigIntf->requestedSpeedLimitMax();

    if (allowableMin > *minReqSpeed || allowableMax < *minReqSpeed)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    
    maxReqSpeed = std::max(maxReqSpeed, *minReqSpeed);
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_clock_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_set_clock_limit_req(
        0, GRAPHICS_CLOCK, static_cast<uint8_t>(clockLimitFlag::PERSISTENCE),
        *minReqSpeed, maxReqSpeed, requestMsg);

    if (rc)
    {
        lg2::error(
            "NsmChassisClockControl::setMinClockLimits encode_set_clock_limit_req failed. eid={EID} rc={RC}",
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
            "NsmChassisClockControl::setMinClockLimits SendRecvNsmMsgSync failed for while setting requested speed limit "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc_);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint16_t data_size = 0;
    rc = decode_set_clock_limit_resp(responseMsg.get(), responseLen, &cc,
                                     &reason_code, &data_size);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info(
            "NsmChassisClockControl::setMinClockLimits for EID: {EID} completed",
            "EID", eid);
    }
    else
    {
        lg2::error(
            "NsmChassisClockControl::setMinClockLimits decode_set_clock_limit_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmChassisClockControl::setMaxClockLimits(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const uint32_t* maxReqSpeed = std::get_if<uint32_t>(&value);
    if (maxReqSpeed == NULL)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("set RequestedSpeedLimitMax On Device for EID: {EID}", "EID",
              eid);

    uint32_t allowableMin = cpuOperatingConfigIntf->minSpeed();
    uint32_t allowableMax = cpuOperatingConfigIntf->maxSpeed();
    uint32_t minReqSpeed = cpuOperatingConfigIntf->requestedSpeedLimitMin();

    if (allowableMin > *maxReqSpeed || allowableMax < *maxReqSpeed)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    
    minReqSpeed = std::min(minReqSpeed, *maxReqSpeed);
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_clock_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_set_clock_limit_req(
        0, GRAPHICS_CLOCK, static_cast<uint8_t>(clockLimitFlag::PERSISTENCE),
        minReqSpeed, *maxReqSpeed, requestMsg);

    if (rc)
    {
        lg2::error(
            "NsmChassisClockControl::setMaxClockLimits encode_set_clock_limit_req failed. eid={EID} rc={RC}",
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
            "NsmChassisClockControl::setMaxClockLimits SendRecvNsmMsgSync failed for while setting requested speed limit "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc_);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint16_t data_size = 0;
    rc = decode_set_clock_limit_resp(responseMsg.get(), responseLen, &cc,
                                     &reason_code, &data_size);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info(
            "NsmChassisClockControl::setMaxClockLimits for EID: {EID} completed",
            "EID", eid);
    }
    else
    {
        lg2::error(
            "NsmChassisClockControl::setMaxClockLimits decode_set_clock_limit_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}


static void CreateControlGpuClock(SensorManager& manager,
                                  const std::string& interface,
                                  const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());

    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    auto type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());

    auto priority = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());

    auto inventoryObjPath = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "InventoryObjPath", interface.c_str());

    auto associations = utils::getAssociations(objPath,
                                               interface + ".Associations");
    auto physicalContext = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "PhysicalContext", interface.c_str());
    auto clockMode = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "ClockMode", interface.c_str());

    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of CreateControlGpuClock PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        return;
    }

    inventoryObjPath += "/Controls/ClockLimit_0";
    auto cpuOperatingConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());

    auto nsmChassisControlSensor = std::make_shared<NsmChassisClockControl>(
        bus, name, cpuOperatingConfigIntf, associations, type, inventoryObjPath,
        physicalContext, clockMode);
    nsmDevice->addSensor(nsmChassisControlSensor, priority);
    auto minGraphicsClockFreq = std::make_shared<NsmMinGraphicsClockLimit>(
        name, type, cpuOperatingConfigIntf);
    auto maxGraphicsClockFreq = std::make_shared<NsmMaxGraphicsClockLimit>(
        name, type, cpuOperatingConfigIntf);

    nsmDevice->addStaticSensor(minGraphicsClockFreq);
    nsmDevice->addStaticSensor(maxGraphicsClockFreq);

    // handler for set Min Clock Limit
    nsm::AsyncSetOperationHandler setMinClockLimHandler = std::bind(
        &NsmChassisClockControl::setMinClockLimits, nsmChassisControlSensor,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    AsyncOperationManager::getInstance()
        ->getDispatcher(inventoryObjPath)
        ->addAsyncSetOperation(
            "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig",
            "RequestedSpeedLimitMin",
            AsyncSetOperationInfo{setMinClockLimHandler,
                                  nsmChassisControlSensor, nsmDevice});

    // handler for set Max Clock Limit
    nsm::AsyncSetOperationHandler setMaxClockLimHandler = std::bind(
        &NsmChassisClockControl::setMaxClockLimits, nsmChassisControlSensor,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    AsyncOperationManager::getInstance()
        ->getDispatcher(inventoryObjPath)
        ->addAsyncSetOperation(
            "xyz.openbmc_project.Inventory.Item.Cpu.OperatingConfig",
            "RequestedSpeedLimitMax",
            AsyncSetOperationInfo{setMaxClockLimHandler,
                                  nsmChassisControlSensor, nsmDevice});
}

REGISTER_NSM_CREATION_FUNCTION(
    CreateControlGpuClock,
    "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0")
} // namespace nsm