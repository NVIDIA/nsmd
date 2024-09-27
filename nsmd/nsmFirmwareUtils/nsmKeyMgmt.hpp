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

#include "nsmFirmwareSlot.hpp"
#include "sensorManager.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Common/Progress/server.hpp>
#include <xyz/openbmc_project/Security/Signing/server.hpp>
#include <xyz/openbmc_project/Security/SigningConfig/server.hpp>
#include <xyz/openbmc_project/Software/SecurityCommon/common.hpp>

#include <memory>
#include <vector>

namespace nsm
{

using namespace sdbusplus::common::xyz::openbmc_project::common;
using namespace sdbusplus::common::xyz::openbmc_project::software;
using namespace sdbusplus::server::xyz::openbmc_project;
using ProgressIntf = object_t<Common::server::Progress>;
using SecSigningIntf = object_t<security::Signing>;
using SecSigningConfigIntf = object_t<security::SigningConfig>;

class NsmKeyMgmt :
    public NsmSensor,
    public SecSigningIntf,
    public SecSigningConfigIntf
{
  public:
    NsmKeyMgmt(sdbusplus::bus::bus& bus, const std::string& chassisName,
               const std::string& type, const uuid_t& uuid,
               std::shared_ptr<ProgressIntf> progressIntf,
               uint16_t componentClassification, uint16_t componentIdentifier,
               uint8_t componentClassificationIndex);

    void addSlotObject(std::shared_ptr<NsmFirmwareSlot>& slot)
    {
        fwSlotObjects.emplace_back(slot);
    }

    void revokeKeys(SecurityCommon::RequestTypes requestType, uint64_t nonce,
                    std::vector<uint8_t> indices);
    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t eid,
                                                      uint8_t instanceId);
    uint8_t handleResponseMsg(const nsm_msg* responseMsg, size_t responseLen);

  private:
    std::string getPath(const std::string& chassisName)
    {
        return std::string(chassisInventoryBasePath) + "/" + chassisName;
    }

    int startOperation();
    void finishOperation(Progress::OperationStatus status);
    std::tuple<uint16_t, std::string> getErrorCode(uint16_t cc);
    requester::Coroutine
        revokeKeysAsyncHandler(std::shared_ptr<Request> request);

    uuid_t uuid;
    std::shared_ptr<ProgressIntf> progressIntf = nullptr;

    std::vector<std::shared_ptr<NsmFirmwareSlot>> fwSlotObjects;
    std::unique_ptr<SecSigningIntf> settingsObject;

    uint16_t componentClassification;
    uint16_t componentIdentifier;
    uint8_t componentClassificationIndex;

    uint8_t bitmapLength;

    bool opInProgress{false};
};

} // namespace nsm
