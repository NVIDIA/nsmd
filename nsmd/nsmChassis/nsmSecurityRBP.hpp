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

#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Common/Progress/server.hpp>
#include <xyz/openbmc_project/Software/MinSecVersionConfig/server.hpp>
#include <xyz/openbmc_project/Software/SecurityCommon/common.hpp>
#include <xyz/openbmc_project/Software/SecurityConfig/server.hpp>
#include <xyz/openbmc_project/Software/SecurityVersion/server.hpp>

#include <array>
#include <memory>

namespace nsm
{

using namespace sdbusplus::common::xyz::openbmc_project::common;
using namespace sdbusplus::common::xyz::openbmc_project::software;
using namespace sdbusplus::server;
using SecurityVersionIntf = object_t<
    sdbusplus::server::xyz::openbmc_project::software::SecurityVersion>;
using SecurityConfigIntf =
    object_t<sdbusplus::server::xyz::openbmc_project::software::SecurityConfig>;
using MinSecVersionIntf = object_t<
    sdbusplus::server::xyz::openbmc_project::software::MinSecVersionConfig>;
using ProgressIntf = object_t<Common::server::Progress>;

class SecurityConfiguration : public SecurityConfigIntf
{
  public:
    SecurityConfiguration(sdbusplus::bus::bus& bus, const std::string& objPath,
                          const uuid_t& uuidIn,
                          std::shared_ptr<ProgressIntf> progressIntfIn,
                          NsmSensor* nsmSensorIn);

    virtual ~SecurityConfiguration() = default;
    void updateState(
        const struct ::nsm_firmware_irreversible_config_request_0_resp&
            cfg_state);
    void updateNonce(
        const struct ::nsm_firmware_irreversible_config_request_2_resp&
            cfg_resp);
    void updateIrreversibleConfig(bool state) override;

  private:
    requester::Coroutine
        securityCfgAsyncHandler(std::shared_ptr<Request> request,
                                uint8_t requestType);
    int startOperation();
    void finishOperation(Progress::OperationStatus status);
    uuid_t uuid;
    std::mutex mutex;
    std::shared_ptr<ProgressIntf> progressIntf = nullptr;
    NsmSensor* nsmSensor = nullptr;
};

class NsmSecurityCfgObject : public NsmSensor
{
  private:
    std::string getPath(const std::string& name)
    {
        using namespace std::string_literals;
        return std::string(chassisInventoryBasePath) + "/" + name;
    }

  public:
    NsmSecurityCfgObject(sdbusplus::bus::bus& bus, const std::string& objPath,
                         const std::string& type, const uuid_t& uuid,
                         std::shared_ptr<ProgressIntf> progressIntfIn);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::string objectPath;
    std::unique_ptr<SecurityConfiguration> securityCfgObject;
};

class MinSecurityVersion : public MinSecVersionIntf
{
  public:
    MinSecurityVersion(sdbusplus::bus::bus& bus, const std::string& objPath,
                       const uuid_t& uuidIn, uint16_t classificationIn,
                       uint16_t identifierIn, uint8_t indexIn,
                       std::shared_ptr<ProgressIntf> progressIntfIn,
                       NsmSensor* nsmSensorIn);

    virtual ~MinSecurityVersion() = default;
    void updateProperties(
        const struct ::nsm_firmware_security_version_number_resp& sec_info);
    void updateMinSecVersion(SecurityCommon::RequestTypes requestType,
                             uint64_t nonce,
                             uint16_t reqMinSecVersion) override;

  private:
    requester::Coroutine
        minSecVersionAsyncHandler(std::shared_ptr<Request> request);
    int startOperation();
    void finishOperation(Progress::OperationStatus status);
    uuid_t uuid;
    uint16_t classification;
    uint16_t identifier;
    uint8_t index;
    std::mutex mutex;
    std::unique_ptr<SecurityVersionIntf> securityVersionObject = nullptr;
    std::unique_ptr<SecurityVersionIntf> securityVersionSettingsObject =
        nullptr;
    std::shared_ptr<ProgressIntf> progressIntf = nullptr;
    NsmSensor* nsmSensor = nullptr;
};

class NsmMinSecVersionObject : public NsmSensor
{
  private:
    std::string getPath(const std::string& chassisName)
    {
        using namespace std::string_literals;
        return std::string(chassisInventoryBasePath) + "/" + chassisName;
    }

  public:
    NsmMinSecVersionObject(sdbusplus::bus::bus& bus,
                           const std::string& chassisName,
                           const std::string& type, const uuid_t& uuid,
                           uint16_t classificationIn, uint16_t identifierIn,
                           uint8_t indexIn,
                           std::shared_ptr<ProgressIntf> progressIntfIn);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::string objectPath;
    std::unique_ptr<MinSecurityVersion> minSecVersion;
    uint16_t classification;
    uint16_t identifier;
    uint8_t index;
};

} // namespace nsm
