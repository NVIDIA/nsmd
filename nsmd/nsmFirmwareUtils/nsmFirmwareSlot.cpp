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

#include "nsmFirmwareSlot.hpp"

namespace nsm
{

NsmFirmwareSlot::NsmFirmwareSlot(
    sdbusplus::bus::bus& bus, const std::string& chassisPath,
    const std::vector<utils::Association>& associations, int slotNum,
    SlotIntf::FirmwareType fwType) :
    AssociationDefinitionsIntf(bus, getPath(chassisPath, slotNum).c_str()),
    SecSigningIntf(bus, getPath(chassisPath, slotNum).c_str()),
    BuildTypeIntf(bus, getPath(chassisPath, slotNum).c_str()),
    ExtendedVersionIntf(bus, getPath(chassisPath, slotNum).c_str()),
    SettingsIntf(bus, getPath(chassisPath, slotNum).c_str()),
    SigningTypeIntf(bus, getPath(chassisPath, slotNum).c_str()),
    SlotIntf(bus, getPath(chassisPath, slotNum).c_str()),
    StateIntf(bus, getPath(chassisPath, slotNum).c_str()),
    SecurityVersionIntf(bus, getPath(chassisPath, slotNum).c_str()),
    VersionComparisonIntf(bus, getPath(chassisPath, slotNum).c_str())
{
    lg2::info("NsmFirmwareSlot - {CHASSIS} - {SLOT}", "CHASSIS", chassisPath,
              "SLOT", slotNum);
    AssociationDefinitionsIntf::associations(
        utils::getAssociations(associations));
    slotId(slotNum);
    type(fwType);
}

void NsmFirmwareSlot::updateActiveSlotAssociation()
{
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    for (const auto& association : associations())
    {
        if (std::get<0>(association) == "software")
        {
            const auto backwardAssoc = isActive() ? "ActiveSlot"
                                                  : "InactiveSlot";
            associationsList.emplace_back(std::get<0>(association),
                                          backwardAssoc,
                                          std::get<2>(association));
        }
        else
        {
            associationsList.emplace_back(association);
        }
    }
    associations(associationsList);
}

void NsmFirmwareSlot::updateSlotKeyData()
{
    if (isActive())
    {
        signingKeyIndex(activeKeyIndex);
        trustedKeys(activeTrustedKeyIndices);
        revokedKeys(activeRevokedKeyIndices);
    }
    else
    {
        signingKeyIndex(pendingKeyIndex);
        trustedKeys(pendingTrustedKeyIndices);
        revokedKeys(pendingRevokedKeyIndices);
    }
}

void NsmFirmwareSlot::update(
    const struct ::nsm_firmware_slot_info& info,
    const struct ::nsm_firmware_erot_state_info_hdr_resp& fq_resp_hdr)
{
    static constexpr FirmwareState stateTbl[] = {
        FirmwareState::Unknown,
        FirmwareState::Activated,
        FirmwareState::PendingActivation,
        FirmwareState::Staged,
        FirmwareState::WriteInProgress,
        FirmwareState::Inactive,
        FirmwareState::FailedAuthentication};
    FirmwareBuildType btype = info.build_type == 0
                                  ? FirmwareBuildType::Development
                                  : FirmwareBuildType::Release;
    FirmwareState fstate = info.firmware_state <
                                   (sizeof(stateTbl) / sizeof(stateTbl[0]))
                               ? stateTbl[info.firmware_state]
                               : FirmwareState::Unknown;
    buildType(btype);
    state(fstate);
    slotId(info.slot_id);
    isActive(fq_resp_hdr.active_slot == info.slot_id);
    extendedVersion(std::string(
        reinterpret_cast<const char*>(info.firmware_version_string)));
    firmwareComparisonNumber(info.version_comparison_stamp);
    writeProtected(info.write_protect_state);
    version(info.security_version_number);
    switch (info.signing_type)
    {
        case 0:
            signingType(SigningTypeIntf::SigningTypes::Debug);
            break;
        case 1:
            signingType(SigningTypeIntf::SigningTypes::Production);
            break;
        case 2:
            signingType(SigningTypeIntf::SigningTypes::External);
            break;
        case 3:
            signingType(SigningTypeIntf::SigningTypes::DOT);
            break;
        default:
            lg2::error("Invalid signing type - type={TYPE}", "TYPE",
                       info.signing_type);
            break;
    }

    updateActiveSlotAssociation();
    updateSlotKeyData();
}

void NsmFirmwareSlot::update(
    const uint16_t activeKeyIndex, const uint16_t pendingKeyIndex,
    const std::vector<uint8_t>& activeTrustedKeyIndices,
    const std::vector<uint8_t>& activeRevokedKeyIndices,
    const std::vector<uint8_t>& pendingTrustedKeyIndices,
    const std::vector<uint8_t>& pendingRevokedKeyIndices)
{
    this->activeKeyIndex = activeKeyIndex;
    this->pendingKeyIndex = pendingKeyIndex;
    this->activeTrustedKeyIndices = activeTrustedKeyIndices;
    this->activeRevokedKeyIndices = activeRevokedKeyIndices;
    this->pendingTrustedKeyIndices = pendingTrustedKeyIndices;
    this->pendingRevokedKeyIndices = pendingRevokedKeyIndices;

    updateSlotKeyData();
}

} // namespace nsm
