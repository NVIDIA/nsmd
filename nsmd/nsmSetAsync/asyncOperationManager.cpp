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

#include "asyncOperationManager.hpp"

#include "sensorManager.hpp"

namespace nsm
{

AsyncOperationManager* AsyncOperationManager::getInstance()
{
    static const std::unique_ptr<AsyncOperationManager> instance{
        new AsyncOperationManager{AsyncOperationResultObjPath}};
    return instance.get();
}

AsyncOperationManager::AsyncOperationManager(
    const std::string& asyncOperationResultObjPath) :
    asyncOperationResultObjPath(asyncOperationResultObjPath)
{}

AsyncSetOperationDispatcher*
    AsyncOperationManager::getDispatcher(const std::string& objPath)
{
    return &dispatchers
                .try_emplace(objPath, utils::DBusHandler::getBus(),
                             objPath.c_str())
                .first->second;
}

std::pair<bool, size_t> AsyncOperationManager::getCurrentObjectCount()
{
    const std::string objPath = asyncOperationResultObjPath + "/" +
                                std::to_string(currentObjectCount);

    auto statusInterface = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(), objPath.c_str());

    auto valueInterface = std::make_shared<AsyncValueIntf>(
        utils::DBusHandler::getBus(), objPath.c_str());

    statusInterfaces[currentObjectCount] = statusInterface;
    valueInterfaces[currentObjectCount] = valueInterface;
    const size_t returnValue = currentObjectCount;

    auto timer =
        std::make_shared<sdbusplus::Timer>([this, index = returnValue]() {
        statusInterfaces.erase(index);
        valueInterfaces.erase(index);
        objectPathTimers.erase(index);
        return false; // Don't restart timer
    });
    timer->start(std::chrono::minutes(2));

    objectPathTimers[returnValue] = std::move(timer);

    ++currentObjectCount;

    return {true, returnValue};
}

std::pair<std::string, std::shared_ptr<AsyncStatusIntf>>
    AsyncOperationManager::getNewStatusInterface()
{
    const auto [success, currentCount] = getCurrentObjectCount();

    if (!success)
    {
        return {};
    }

    const std::string objPath = asyncOperationResultObjPath + "/" +
                                std::to_string(currentCount);

    std::shared_ptr<AsyncStatusIntf> statusIntf =
        statusInterfaces[currentCount];

    statusIntf->status(AsyncOperationStatusType::InProgress, false);

    return {objPath, statusIntf};
}

std::tuple<std::string, std::shared_ptr<AsyncStatusIntf>,
           std::shared_ptr<AsyncValueIntf>>
    AsyncOperationManager::getNewStatusValueInterface()
{
    const auto [success, currentCount] = getCurrentObjectCount();

    if (!success)
    {
        return {};
    }

    const std::string objPath = asyncOperationResultObjPath + "/" +
                                std::to_string(currentCount);

    std::shared_ptr<AsyncValueIntf> valueIntf = valueInterfaces[currentCount];

    std::shared_ptr<AsyncStatusIntf> statusIntf =
        statusInterfaces[currentCount];

    valueIntf->value({});
    statusIntf->status(AsyncOperationStatusType::InProgress, false);

    return {objPath, statusIntf, valueIntf};
}

sdbusplus::message::object_path
    AsyncSetOperationDispatcher::set(std::string interface,
                                     std::string property,
                                     AsyncSetOperationValueType value)
{
    const auto result =
        AsyncOperationManager::getInstance()->getNewStatusInterface();

    if (result.first.empty())
    {
        lg2::error(
            "AsyncSet : no available result Object to allocate for the request. "
            "Interface - {INTF}, Property - {PROP}",
            "PROP", property, "INTF", interface);

        throw sdbusplus::error::xyz::openbmc_project::common::InternalFailure{};
    }

    setImpl(interface, property, value, result.second).detach();

    return result.first;
}

int AsyncSetOperationDispatcher::addAsyncSetOperation(
    const std::string& interface, const std::string& property,
    const AsyncSetOperationInfo& info)
{
    asyncOperations[interface][property] = info;

    return NSM_SW_SUCCESS;
};

requester::Coroutine AsyncSetOperationDispatcher::setImpl(
    const std::string& interface, const std::string& property,
    const AsyncSetOperationValueType value,
    std::shared_ptr<AsyncStatusIntf> resultIntf)
{
    auto findInterface = asyncOperations.find(interface);
    if (findInterface == asyncOperations.end())
    {
        lg2::error("AsyncSet request Interface {INTF} not found", "INTF",
                   interface);

        throw sdbusplus::error::xyz::openbmc_project::common::
            UnsupportedRequest{};
    }

    auto findProperty = findInterface->second.find(property);
    if (findProperty == findInterface->second.end())
    {
        lg2::error(
            "AsyncSet request Property {PROP} not found for Interface {INTF}",
            "PROP", property, "INTF", interface);

        throw sdbusplus::error::xyz::openbmc_project::common::
            UnsupportedRequest{};
    }

    auto& operation = findProperty->second;

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    resultIntf->status(AsyncOperationStatusType::InProgress, false);

    try
    {
        co_await operation.handler(value, &status, operation.device);

        if (operation.sensor)
        {
            if (operation.device->isDeviceActive)
            {
                const eid_t eid =
                    SensorManager::getInstance().getEid(operation.device);
                co_await operation.sensor->update(SensorManager::getInstance(),
                                                  eid);
            }
        }
    }
    catch (
        const sdbusplus::error::xyz::openbmc_project::common::InvalidArgument&)
    {
        status = AsyncOperationStatusType::InvalidArgument;
    }
    catch (...)
    {
        status = AsyncOperationStatusType::InternalFailure;
    }

    resultIntf->status(status);

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
};
} // namespace nsm
