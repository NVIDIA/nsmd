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

#include "nsmPreBootDiag.hpp"

#include "asyncOperationManager.hpp"
#include "nsmDevice.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <unordered_map>

namespace nsm
{

namespace
{
std::unordered_map<eid_t, std::shared_ptr<NsmPreBootDiag>> instances;
} // namespace

std::shared_ptr<NsmPreBootDiag>
    NsmPreBootDiag::getOrCreate(std::shared_ptr<NsmDevice> device)
{
    auto eid = device->getEid();
    auto it = instances.find(eid);
    if (it != instances.end())
    {
        return it->second;
    }

    std::string objPath = "/com/nvidia/prebootdiag/" + std::to_string(eid);
    auto& bus = utils::DBusHandler::getBus();
    auto instance = std::make_shared<NsmPreBootDiag>(
        bus, "PreBootDiag", "NSM_PreBootDiag", objPath, device);
    lg2::info("PreBootDiag: Created for EID={EID} instance={INST} path={PATH}",
              "EID", eid, "INST", device->getInstanceNumber(), "PATH", objPath);
    instances.emplace(eid, instance);
    return instance;
}

NsmPreBootDiag::NsmPreBootDiag(sdbusplus::bus::bus& bus,
                               const std::string& name, const std::string& type,
                               const std::string& objPath,
                               std::shared_ptr<NsmDevice> device) :
    NsmObject(name, type)
{
    systemConfigIntf =
        std::make_shared<NsmPreBootDiagConfigSystemIntf>(bus, objPath.c_str());
    tidConfigIntf =
        std::make_shared<NsmPreBootDiagConfigTIDIntf>(bus, objPath.c_str());

    auto* dispatcher =
        AsyncOperationManager::getInstance()->getDispatcher(objPath);

    // Capture the interface shared_ptrs by value so the dispatcher (which
    // is a daemon-lifetime singleton) keeps each interface alive for as
    // long as it holds the registered handler. Binding via `.get()` would
    // hand the dispatcher a raw pointer that becomes dangling if the
    // owning NsmPreBootDiag (and hence NsmDevice) is ever destroyed.
    dispatcher->addAsyncSetOperation(
        "com.nvidia.PreBootDiag.Config.System", "Value",
        AsyncSetOperationInfo{
            [intf = systemConfigIntf](
                const AsyncSetOperationValueType& v,
                AsyncOperationStatusType* s,
                std::shared_ptr<NsmDevice> d) -> requester::Coroutine {
        co_return co_await intf->setValueAsync(v, s, std::move(d));
    },
            /*sensor=*/nullptr, device});

    dispatcher->addAsyncSetOperation(
        "com.nvidia.PreBootDiag.Config.TID", "Value",
        AsyncSetOperationInfo{
            [intf = tidConfigIntf](
                const AsyncSetOperationValueType& v,
                AsyncOperationStatusType* s,
                std::shared_ptr<NsmDevice> d) -> requester::Coroutine {
        co_return co_await intf->setValueAsync(v, s, std::move(d));
    },
            /*sensor=*/nullptr, device});
}

} // namespace nsm
