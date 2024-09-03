/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
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

#include "globals.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Security/Signing/server.hpp>
#include <xyz/openbmc_project/Software/BuildType/server.hpp>
#include <xyz/openbmc_project/Software/ExtendedVersion/server.hpp>
#include <xyz/openbmc_project/Software/SecurityVersion/server.hpp>
#include <xyz/openbmc_project/Software/Settings/server.hpp>
#include <xyz/openbmc_project/Software/Signing/server.hpp>
#include <xyz/openbmc_project/Software/Slot/server.hpp>
#include <xyz/openbmc_project/Software/State/server.hpp>
#include <xyz/openbmc_project/Software/VersionComparison/server.hpp>

#include <vector>

namespace nsm
{

using namespace sdbusplus::common::xyz::openbmc_project::software;
using namespace sdbusplus::server::xyz::openbmc_project;
using namespace sdbusplus::server;
using AssociationDefinitionsIntf = object_t<association::Definitions>;
using SecSigningIntf = object_t<security::Signing>;
using BuildTypeIntf = object_t<software::BuildType>;
using ExtendedVersionIntf = object_t<software::ExtendedVersion>;
using SettingsIntf = object_t<software::Settings>;
using SigningTypeIntf = object_t<software::Signing>;
using SlotIntf = object_t<software::Slot>;
using StateIntf = object_t<software::State>;
using SecurityVersionIntf = object_t<software::SecurityVersion>;
using VersionComparisonIntf = object_t<software::VersionComparison>;

class NsmFirmwareSlot :
    public AssociationDefinitionsIntf,
    public SecSigningIntf,
    public BuildTypeIntf,
    public ExtendedVersionIntf,
    public SettingsIntf,
    public SigningTypeIntf,
    public SlotIntf,
    public StateIntf,
    public SecurityVersionIntf,
    public VersionComparisonIntf
{
  public:
    NsmFirmwareSlot(sdbusplus::bus::bus& bus, const std::string& chassisPath,
                    const std::vector<utils::Association>& associations,
                    int slotNum, SlotIntf::FirmwareType fwType);

    void update(
        const struct ::nsm_firmware_slot_info& info,
        const struct ::nsm_firmware_erot_state_info_hdr_resp& fq_resp_hdr);

    void update(const uint16_t activeKeyIndex, const uint16_t pendingKeyIndex,
                const std::vector<uint8_t>& activeTrustedKeyIndices,
                const std::vector<uint8_t>& activeRevokedKeyIndices,
                const std::vector<uint8_t>& pendingTrustedKeyIndices,
                const std::vector<uint8_t>& pendingRevokedKeyIndices);

  private:
    std::string getPath(const std::string& chassisPath, int slotNum)
    {
        using namespace std::string_literals;
        return chassisPath + "/Slots/"s + std::to_string(slotNum);
    }
    void updateActiveSlotAssociation();
    void updateSlotKeyData();

    uint16_t activeKeyIndex{0};
    uint16_t pendingKeyIndex{0};
    std::vector<uint8_t> activeTrustedKeyIndices;
    std::vector<uint8_t> activeRevokedKeyIndices;
    std::vector<uint8_t> pendingTrustedKeyIndices;
    std::vector<uint8_t> pendingRevokedKeyIndices;
};

} // namespace nsm
