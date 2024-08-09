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
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/BuildType/server.hpp>
#include <xyz/openbmc_project/Software/Slot/server.hpp>
#include <xyz/openbmc_project/Software/State/server.hpp>

#include <array>
#include <memory>

namespace nsm
{

using namespace sdbusplus::server::xyz::openbmc_project::software;
using namespace sdbusplus::server;

using AssociationDefinitionsIntf = object_t<Association::server::Definitions>;
using BuildTypeIntf = object_t<BuildType>;
using SlotIntf = object_t<Slot>;
using StateIntf =
    object_t<sdbusplus::server::xyz::openbmc_project::software::State>;

class FirmwareSlot :
    public BuildType,
    AssociationDefinitionsIntf,
    SlotIntf,
    StateIntf
{
  private:
    static auto slotName(const std::string& name, int slotNum)
    {
        using namespace std::string_literals;
        return name + "/Slots/"s + std::to_string(slotNum);
    }
    void updateActiveSlotAssociation();

  public:
    using getBuildTypeFn = std::function<void(uint16_t, uint8_t, uint16_t)>;

    FirmwareSlot(sdbusplus::bus::bus& bus, const std::string& name,
                 const std::vector<utils::Association>& _associations, int slot,
                 SlotIntf::FirmwareType fwType);

    virtual ~FirmwareSlot() = default;

    void update(
        const struct ::nsm_firmware_slot_info& info,
        const struct ::nsm_firmware_erot_state_info_hdr_resp& fq_resp_hdr);
};

class NsmBuildTypeObject : public NsmSensor
{
  public:
    NsmBuildTypeObject(sdbusplus::bus::bus& bus, const std::string& name,
                       const std::vector<utils::Association>& associations,
                       const std::string& type, const uuid_t& uuid,
                       int slot, int classification, int identifier,
                       SlotIntf::FirmwareType fwType
                       );

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    static constexpr auto numSlots = 2;

    static std::string getName(const std::string& name)
    {
        return buildTypeBasePath / name;
    }

    std::array<std::unique_ptr<FirmwareSlot>, numSlots> fwSlots;
    std::string objectPath;
    uuid_t uuid;
    nsm_firmware_erot_state_info_req nsmRequest;
    const int slotNum;
};

} // namespace nsm
