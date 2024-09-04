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

#include "asyncOperationManager.hpp"
#include "nsmDevice.hpp"
#include "nsmInterface.hpp"
#include "nsmSensor.hpp"

#include <com/nvidia/PCIe/AERErrorStatus/server.hpp>

#include <optional>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AERErrorStatusIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::pc_ie::AERErrorStatus>;

class NsmAERErrorStatusIntf : public AERErrorStatusIntf
{
  public:
    NsmAERErrorStatusIntf(sdbusplus::bus::bus& bus, const char* path,
                          uint8_t deviceIndex,
                          std::shared_ptr<NsmDevice> device) :
        AERErrorStatusIntf(bus, path), deviceIndex(deviceIndex), device(device)
    {}
    sdbusplus::message::object_path clearAERStatus() override;
    requester::Coroutine clearAERError(AsyncOperationStatusType* status);
    requester::Coroutine doclearAERErrorOnDevice(
        std::shared_ptr<AsyncStatusIntf> statusInterface);

    uint8_t deviceIndex;
    std::shared_ptr<NsmDevice> device;
};

class NsmPCIeAERErrorStatus : public NsmSensor
{
  public:
    NsmPCIeAERErrorStatus(
        const std::string& name, const std::string& type,
        std::shared_ptr<NsmAERErrorStatusIntf> aerErrorStatusIntf,
        uint8_t deviceIndex);

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::shared_ptr<NsmAERErrorStatusIntf> aerErrorStatusIntf;
    uint8_t deviceIndex;
};
} // namespace nsm
