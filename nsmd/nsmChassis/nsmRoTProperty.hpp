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

#include "firmware-utils.h"

#include "asyncOperationManager.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <com/nvidia/InbandUpdatePolicy/server.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <array>
#include <memory>

namespace nsm
{

using namespace sdbusplus::common::xyz::openbmc_project::common;
using namespace sdbusplus::common::xyz::openbmc_project::software;
using namespace sdbusplus::server;
using InbandUpdatePolicyIntf =
    object_t<sdbusplus::server::com::nvidia::InbandUpdatePolicy>;

class NsmInbandUpdatePolicy :
    public InbandUpdatePolicyIntf,
    public StateChangeLogger
{
  public:
    NsmInbandUpdatePolicy(sdbusplus::bus::bus& bus, const std::string& objPath,
                          const uuid_t& uuidIn, uint16_t classificationIn,
                          uint16_t identifierIn, uint8_t indexIn,
                          NsmSensor& nsmSensor) :
        InbandUpdatePolicyIntf(bus, objPath.c_str()), uuid(uuidIn),
        classification(classificationIn), identifier(identifierIn),
        index(indexIn), nsmSensor(nsmSensor)
    {}

    virtual ~NsmInbandUpdatePolicy() = default;

    void updateProperties(
        const struct ::nsm_firmware_erot_state_info_resp& erot_info);
    void updatePolicy(bool state) override;

  private:
    requester::Coroutine
        policyAsyncHandler(std::shared_ptr<Request> request,
                           std::shared_ptr<AsyncStatusIntf> statusIntf,
                           std::shared_ptr<AsyncValueIntf> valueIntf);
    uuid_t uuid;
    uint16_t classification;
    uint16_t identifier;
    uint8_t index;
    NsmSensor& nsmSensor;

    static constexpr uint8_t ARGUMENT_DATA_LENGTH = 2;
};

/**
 * @brief Object class for RoT Property that inherits from both NsmSensor and
 * NsmInterfaceContainer
 */
class NsmInbandUpdatePolicyObject : public NsmSensor
{
  private:
    std::string getPath(const std::string& chassisName)
    {
        using namespace std::string_literals;
        return std::string(chassisInventoryBasePath) + "/" + chassisName;
    }

  public:
    NsmInbandUpdatePolicyObject(sdbusplus::bus::bus& bus,
                                const std::string& chassisName,
                                const uuid_t& uuid, uint16_t classificationIn,
                                uint16_t identifierIn, uint8_t indexIn);

    NsmInbandUpdatePolicyObject() = delete;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::string objectPath;
    std::unique_ptr<NsmInbandUpdatePolicy> nsmInbandUpdatePolicy;
    uint16_t classification;
    uint16_t identifier;
    uint8_t index;
};

} // namespace nsm
