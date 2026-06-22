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

#pragma once

#include "nsmDevice.hpp"
#include "nsmSensor.hpp"
#include "requester/handler.hpp"

#include <com/nvidia/Async/Set/server.hpp>
#include <com/nvidia/Async/Status/server.hpp>
#include <com/nvidia/Async/Value/server.hpp>
#include <xyz/openbmc_project/Common/Progress/server.hpp>

namespace nsm
{

// Maximum supported parallel PATCH/POST request count.
// nsmd will not accept more parallel PATCH/POST request than this value
// and DBus Method will return with an error.
static constexpr size_t MaxAsyncOperationResultObjectCount{32};
static constexpr auto AsyncOperationResultObjPath{
    "/com/nvidia/nsmd/AsyncOperation"};

using AsyncStatusIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::Async::server::Status>;
using AsyncSetIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::Async::server::Set>;
using AsyncValueIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::Async::server::Value>;

using AsyncOperationStatusType =
    sdbusplus::common::com::nvidia::async::Status::AsyncOperationStatus;
using AsyncSetOperationValueType = std::variant<
    bool, uint8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t,
    double, std::string, std::vector<uint8_t>, std::tuple<bool, uint32_t>,
    std::vector<std::tuple<std::string, uint32_t>>,
    std::vector<std::tuple<std::string, std::variant<bool, uint32_t, double,
                                                     std::vector<uint8_t>>>>>;
using AsyncCallOperationValueType = AsyncSetOperationValueType;

using AsyncSetOperationHandler = std::function<requester::Coroutine(
    const AsyncSetOperationValueType&, AsyncOperationStatusType*,
    std::shared_ptr<NsmDevice>)>;

struct AsyncSetOperationInfo
{
    AsyncSetOperationHandler handler;
    std::shared_ptr<NsmObject> sensor;
    std::shared_ptr<NsmDevice> device;
};

class AsyncSetOperationDispatcher;

class AsyncOperationManager
{
  public:
    static AsyncOperationManager* getInstance();

    AsyncSetOperationDispatcher* getDispatcher(const std::string& objPath);

    /** @brief Remove a dispatcher for the given D-Bus object path.
     *
     * @param objPath[in] D-Bus object path of the dispatcher
     */
    void removeDispatcher(const std::string& objPath);

    std::pair<std::string, std::shared_ptr<AsyncStatusIntf>>
        getNewStatusInterface();

    std::tuple<std::string, std::shared_ptr<AsyncStatusIntf>,
               std::shared_ptr<AsyncValueIntf>>
        getNewStatusValueInterface();

  private:
    AsyncOperationManager(const std::string& asyncOperationResultObjPath);

    std::pair<bool, size_t> getCurrentObjectCount();
    size_t currentObjectCount{0};

    /**
     * @brief Clear the value stored in the value interface, special attention
     *        should be given to any type that needs specific cleanup.
     *
     * @param valueIntf - pointer to the value interface instance
     */
    void clearValueInterface(std::shared_ptr<AsyncValueIntf> valueIntf);

    const std::string asyncOperationResultObjPath;

    std::unordered_map<uint64_t, std::shared_ptr<AsyncStatusIntf>>
        statusInterfaces;
    std::unordered_map<uint64_t, std::shared_ptr<AsyncValueIntf>>
        valueInterfaces;
    std::unordered_map<uint64_t, std::shared_ptr<sdbusplus::Timer>>
        objectPathTimers;

    std::unordered_map<std::string, AsyncSetOperationDispatcher> dispatchers;
};

class AsyncSetOperationDispatcher : public AsyncSetIntf
{
  public:
    using AsyncSetIntf::AsyncSetIntf;

    int addAsyncSetOperation(const std::string& interface,
                             const std::string& property,
                             const AsyncSetOperationInfo& info);

  private:
    requester::Coroutine setImpl(const std::string& interface,
                                 const std::string& property,
                                 const AsyncSetOperationValueType value,
                                 std::shared_ptr<AsyncStatusIntf> resultIntf);

    sdbusplus::object_path set(std::string interface, std::string property,
                               AsyncSetOperationValueType value) override;

    std::unordered_map<std::string,
                       std::unordered_map<std::string, AsyncSetOperationInfo>>
        asyncOperations;
};

} // namespace nsm
