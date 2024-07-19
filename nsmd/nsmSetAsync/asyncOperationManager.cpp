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
        new AsyncOperationManager{MaxAsyncOperationResultObjectCount,
                                  AsyncOperationResultObjPath}};
    return instance.get();
}

AsyncOperationManager::AsyncOperationManager(
    const size_t maxResultObjectCount,
    const std::string& asyncOperationResultObjPath) :
    maxObjectCount(maxResultObjectCount),
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
    const size_t numResultObjects = statusInterfaces.size();

    // check if maximum result object count is reached
    if (numResultObjects >= maxObjectCount)
    {
        size_t count{currentObjectCount};

        // check for the next available not in progress result object
        while (statusInterfaces[count]->status() ==
               AsyncOperationStatusType::InProgress)
        {
            ++count;

            if (count >= maxObjectCount)
            {
                count = 0;
            }

            // all the result Objects are checked and no Object is found with
            // status not in progress
            if (count == currentObjectCount)
            {
                lg2::error(
                    "AsyncOperationManager : no available result Object to allocate for the request.");

                return {false, {}};
            }
        }

        currentObjectCount = count;
    }
    else
    {
        const std::string objPath = asyncOperationResultObjPath + "/" +
                                    std::to_string(currentObjectCount);

        auto statusInterface = std::make_shared<AsyncStatusIntf>(
            utils::DBusHandler::getBus(), objPath.c_str());

        auto valueInterface = std::make_shared<AsyncValueIntf>(
            utils::DBusHandler::getBus(), objPath.c_str());

        statusInterfaces.push_back(statusInterface);
        valueInterfaces.push_back(valueInterface);
    }

    const size_t returnValue{currentObjectCount};

    ++currentObjectCount;
    if (currentObjectCount >= maxObjectCount)
    {
        currentObjectCount = 0;
    }

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
    const AsyncSetOperationValueType& value,
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

    resultIntf->status(status);

    co_return NSM_SW_SUCCESS;
};
} // namespace nsm
