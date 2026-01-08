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
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <array>
#include <memory>

namespace nsm
{

using namespace sdbusplus::server::xyz::openbmc_project::software;
using namespace sdbusplus::server;

struct SlotInfo
{
    std::string slotName;
    uint64_t classification = 0;
    uint64_t identifier = 0;
    uint64_t index = 0;
    std::string fwType;
    std::vector<utils::Association> associations;
    std::string chassisName;
    bool isRoT = false;
    bool reportSkuWithNsm = false;
    bool enableUpdateSKU = false;
};

class NsmBuildTypeObject : public NsmSensor
{
  public:
    NsmBuildTypeObject(const std::string& chassisName, const std::string& type,
                       const uuid_t& uuid, int classification, int identifier);

    void addSlotObject(std::shared_ptr<NsmFirmwareSlot>& slot)
    {
        fwSlotObjects.emplace_back(slot);
    }

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::vector<std::shared_ptr<NsmFirmwareSlot>> fwSlotObjects;

    uuid_t uuid;
    nsm_firmware_erot_state_info_req nsmRequest;
    /**
     * @brief Parse the slots from the D-Bus interface
     * @param path The path of the D-Bus interface
     * @param slotCount The number of slots
     * @param rotSlotInterface The interface of the RoT slots
     * @param result The result of the parsing
     * @return The result of the parsing
     */
    requester::Coroutine parseSlots(const std::string& path, uint64_t slotCount,
                                    const std::string& rotSlotInterface,
                                    std::vector<SlotInfo>& slots);
};

} // namespace nsm
