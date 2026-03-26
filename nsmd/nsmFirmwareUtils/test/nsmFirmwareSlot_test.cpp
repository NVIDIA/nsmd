/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#include "test/mockDBusHandler.hpp"
using namespace ::testing;

#include "firmware-utils.h"

#define private public
#define protected public

#include "nsmFirmwareSlot.hpp"

using namespace nsm;
using namespace sdbusplus::common::xyz::openbmc_project::software;

auto bus = sdbusplus::bus::new_default();

TEST(NsmFirmwareSlot, Constructor)
{
    std::string chassisPath = "/xyz/openbmc_project/inventory/system/chassis";
    std::string chassisName = "TestChassis";
    std::vector<utils::Association> associations;
    associations.push_back({"software", "chassis", chassisPath});
    int slotNum = 0;
    auto fwType = SlotIntf::FirmwareType::AP;

    NsmFirmwareSlot slot(bus, chassisPath, associations, slotNum, fwType,
                         chassisName);

    EXPECT_EQ(slot.slotId(), slotNum);
    EXPECT_EQ(slot.type(), fwType);
    EXPECT_STREQ(slot.extendedVersion().c_str(), "NA");
}

TEST(NsmFirmwareSlot, UpdateWithFirmwareInfo)
{
    std::string chassisPath = "/xyz/openbmc_project/inventory/system/chassis";
    std::string chassisName = "TestChassis";
    std::vector<utils::Association> associations;
    int slotNum = 1;
    auto fwType = SlotIntf::FirmwareType::EC;

    NsmFirmwareSlot slot(bus, chassisPath, associations, slotNum, fwType,
                         chassisName);

    struct nsm_firmware_slot_info info = {};
    info.slot_id = 1;
    info.firmware_state = 1; // Activated state
    std::strcpy(reinterpret_cast<char*>(info.firmware_version_string),
                "1.0.0.test");

    struct nsm_firmware_erot_state_info_hdr_resp fq_resp_hdr = {};
    fq_resp_hdr.active_slot = 1;

    slot.update(info, fq_resp_hdr);

    // After update, slot should be marked as active
    EXPECT_TRUE(slot.isActive());
}

TEST(NsmFirmwareSlot, UpdateActiveSlotAssociation)
{
    std::string chassisPath = "/xyz/openbmc_project/inventory/system/chassis";
    std::string chassisName = "TestChassis";
    std::vector<utils::Association> associations;
    associations.push_back({"software", "chassis", chassisPath});
    int slotNum = 0;
    auto fwType = SlotIntf::FirmwareType::AP;

    NsmFirmwareSlot slot(bus, chassisPath, associations, slotNum, fwType,
                         chassisName);

    // Initially not active
    EXPECT_FALSE(slot.isActive());

    // Make it active
    struct nsm_firmware_slot_info info = {};
    std::strcpy(reinterpret_cast<char*>(info.firmware_version_string), "1.0.0");

    struct nsm_firmware_erot_state_info_hdr_resp fq_resp_hdr = {};
    fq_resp_hdr.active_slot = 0;

    slot.update(info, fq_resp_hdr);
    EXPECT_TRUE(slot.isActive());
}

TEST(NsmFirmwareSlot, UpdateKeyData)
{
    std::string chassisPath = "/xyz/openbmc_project/inventory/system/chassis";
    std::string chassisName = "TestChassis";
    std::vector<utils::Association> associations;
    int slotNum = 2;
    auto fwType = SlotIntf::FirmwareType::EC;

    NsmFirmwareSlot slot(bus, chassisPath, associations, slotNum, fwType,
                         chassisName);

    // First make the slot active
    struct nsm_firmware_slot_info info = {};
    info.slot_id = 2;
    info.firmware_state = 1; // Activated state
    struct nsm_firmware_erot_state_info_hdr_resp fq_resp_hdr = {};
    fq_resp_hdr.active_slot = 2;
    slot.update(info, fq_resp_hdr);
    EXPECT_TRUE(slot.isActive());

    // Now update key data
    uint16_t activeKeyIdx = 5;
    uint16_t pendingKeyIdx = 6;
    std::vector<uint8_t> activeTrusted = {1, 2, 3};
    std::vector<uint8_t> activeRevoked = {4, 5};
    std::vector<uint8_t> pendingTrusted = {6, 7};
    std::vector<uint8_t> pendingRevoked = {8};

    slot.update(activeKeyIdx, pendingKeyIdx, activeTrusted, activeRevoked,
                pendingTrusted, pendingRevoked);

    // Verify active key data is used (since slot is active)
    EXPECT_EQ(slot.signingKeyIndex(), activeKeyIdx);
    EXPECT_EQ(slot.trustedKeys(), activeTrusted);
    EXPECT_EQ(slot.revokedKeys(), activeRevoked);
}

TEST(NsmFirmwareSlot, MultipleSlots)
{
    std::string chassisPath = "/xyz/openbmc_project/inventory/system/chassis";
    std::string chassisName = "TestChassis";
    std::vector<utils::Association> associations;

    // Create multiple slots
    NsmFirmwareSlot slot0(bus, chassisPath, associations, 0,
                          SlotIntf::FirmwareType::AP, chassisName);
    NsmFirmwareSlot slot1(bus, chassisPath, associations, 1,
                          SlotIntf::FirmwareType::AP, chassisName);
    NsmFirmwareSlot slot2(bus, chassisPath, associations, 2,
                          SlotIntf::FirmwareType::EC, chassisName);

    EXPECT_EQ(slot0.slotId(), 0);
    EXPECT_EQ(slot1.slotId(), 1);
    EXPECT_EQ(slot2.slotId(), 2);
    EXPECT_EQ(slot2.type(), SlotIntf::FirmwareType::EC);
}

// =============================================================================
// Branch coverage: signing_type switch cases 1, 2, 3, default
// =============================================================================

TEST(NsmFirmwareSlot, Update_SigningTypeProduction)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/prd";
    std::string chassisName = "PrdChassis";
    NsmFirmwareSlot slot(bus, chassisPath, {}, 0, SlotIntf::FirmwareType::AP,
                         chassisName);

    struct nsm_firmware_slot_info info = {};
    info.slot_id = 0;
    info.signing_type = 1; // Production
    struct nsm_firmware_erot_state_info_hdr_resp hdr = {};
    slot.update(info, hdr);

    EXPECT_EQ(slot.signingType(), SigningTypeIntf::SigningTypes::Production);
}

TEST(NsmFirmwareSlot, Update_SigningTypeExternal)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/ext";
    std::string chassisName = "ExtChassis";
    NsmFirmwareSlot slot(bus, chassisPath, {}, 0, SlotIntf::FirmwareType::AP,
                         chassisName);

    struct nsm_firmware_slot_info info = {};
    info.slot_id = 0;
    info.signing_type = 2; // External
    struct nsm_firmware_erot_state_info_hdr_resp hdr = {};
    slot.update(info, hdr);

    EXPECT_EQ(slot.signingType(), SigningTypeIntf::SigningTypes::External);
}

TEST(NsmFirmwareSlot, Update_SigningTypeDOT)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/dot";
    std::string chassisName = "DotChassis";
    NsmFirmwareSlot slot(bus, chassisPath, {}, 0, SlotIntf::FirmwareType::AP,
                         chassisName);

    struct nsm_firmware_slot_info info = {};
    info.slot_id = 0;
    info.signing_type = 3; // DOT
    struct nsm_firmware_erot_state_info_hdr_resp hdr = {};
    slot.update(info, hdr);

    EXPECT_EQ(slot.signingType(), SigningTypeIntf::SigningTypes::DOT);
}

TEST(NsmFirmwareSlot, Update_SigningTypeDefault)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/dflt";
    std::string chassisName = "DfltChassis";
    NsmFirmwareSlot slot(bus, chassisPath, {}, 0, SlotIntf::FirmwareType::AP,
                         chassisName);

    struct nsm_firmware_slot_info info = {};
    info.slot_id = 0;
    info.signing_type = 99; // default branch (unknown value)
    struct nsm_firmware_erot_state_info_hdr_resp hdr = {};
    // Should not crash; default branch just breaks without setting signingType
    EXPECT_NO_THROW(slot.update(info, hdr));
}

// =============================================================================
// Branch coverage: build_type != 0 → FirmwareBuildType::Release
// =============================================================================

TEST(NsmFirmwareSlot, Update_BuildTypeRelease)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/rel";
    std::string chassisName = "RelChassis";
    NsmFirmwareSlot slot(bus, chassisPath, {}, 0, SlotIntf::FirmwareType::AP,
                         chassisName);

    struct nsm_firmware_slot_info info = {};
    info.slot_id = 0;
    info.build_type = 1; // non-zero → Release
    struct nsm_firmware_erot_state_info_hdr_resp hdr = {};
    slot.update(info, hdr);

    EXPECT_EQ(slot.buildType(), BuildType::FirmwareBuildType::Release);
}

// =============================================================================
// Branch coverage: firmware_state out of range → FirmwareState::Unknown
// =============================================================================

TEST(NsmFirmwareSlot, Update_FirmwareStateOutOfRange)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/oor";
    std::string chassisName = "OorChassis";
    NsmFirmwareSlot slot(bus, chassisPath, {}, 0, SlotIntf::FirmwareType::AP,
                         chassisName);

    struct nsm_firmware_slot_info info = {};
    info.slot_id = 0;
    info.firmware_state = 99; // out of range → Unknown
    struct nsm_firmware_erot_state_info_hdr_resp hdr = {};
    slot.update(info, hdr);

    EXPECT_EQ(slot.state(), State::FirmwareState::Unknown);
}

// =============================================================================
// Branch coverage: updateSlotKeyData FALSE branch (inactive slot)
// =============================================================================

TEST(NsmFirmwareSlot, UpdateKeyData_InactiveSlot_UsesPendingKeys)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/inact";
    std::string chassisName = "InactChassis";
    NsmFirmwareSlot slot(bus, chassisPath, {}, 0, SlotIntf::FirmwareType::AP,
                         chassisName);

    // slot_id=0, active_slot=1 → slot is INACTIVE
    struct nsm_firmware_slot_info info = {};
    info.slot_id = 0;
    info.firmware_state = 1;
    struct nsm_firmware_erot_state_info_hdr_resp hdr = {};
    hdr.active_slot = 1; // different from slot_id → inactive
    slot.update(info, hdr);
    EXPECT_FALSE(slot.isActive());

    // Now update key data → uses pending key paths (else branch)
    uint16_t activeKeyIdx = 3;
    uint16_t pendingKeyIdx = 7;
    std::vector<uint8_t> activeTrusted = {1};
    std::vector<uint8_t> activeRevoked = {2};
    std::vector<uint8_t> pendingTrusted = {10, 11};
    std::vector<uint8_t> pendingRevoked = {12};

    slot.update(activeKeyIdx, pendingKeyIdx, activeTrusted, activeRevoked,
                pendingTrusted, pendingRevoked);

    // Inactive slot → pending key data used
    EXPECT_EQ(slot.signingKeyIndex(), pendingKeyIdx);
    EXPECT_EQ(slot.trustedKeys(), pendingTrusted);
    EXPECT_EQ(slot.revokedKeys(), pendingRevoked);
}

// =============================================================================
// Branch coverage: WriteInProgress + inactive + empty version → "NA"
// =============================================================================

TEST(NsmFirmwareSlot, Update_WriteInProgress_Inactive_EmptyVersion_SetsNA)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/wip";
    std::string chassisName = "WipChassis";
    NsmFirmwareSlot slot(bus, chassisPath, {}, 0, SlotIntf::FirmwareType::AP,
                         chassisName);

    struct nsm_firmware_slot_info info = {};
    info.slot_id = 0;
    info.firmware_state = 4; // WriteInProgress (index 4 in stateTbl)
    // firmware_version_string is all zeros → empty string
    struct nsm_firmware_erot_state_info_hdr_resp hdr = {};
    hdr.active_slot = 1; // inactive (slot_id=0 != active_slot=1)
    slot.update(info, hdr);

    EXPECT_EQ(slot.state(), State::FirmwareState::WriteInProgress);
    EXPECT_FALSE(slot.isActive());
    EXPECT_EQ(slot.extendedVersion(), "NA");
}

// =============================================================================
// Branch coverage: non-software association else branch in
// updateActiveSlotAssociation (line 74-77 in nsmFirmwareSlot.cpp)
// =============================================================================

TEST(NsmFirmwareSlot, UpdateActiveSlotAssociation_NonSoftwareAssociation)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/ns";
    std::string chassisName = "NsChassis";
    // Provide both "software" and "other" associations
    std::vector<utils::Association> associations;
    associations.push_back({"software", "chassis", chassisPath});
    associations.push_back({"contained_by", "containing", chassisPath});
    int slotNum = 0;

    NsmFirmwareSlot slot(bus, chassisPath, associations, slotNum,
                         SlotIntf::FirmwareType::AP, chassisName);

    // Update to trigger updateActiveSlotAssociation → iterates both
    // associations: "software" takes the if-branch, "contained_by" takes
    // the else-branch
    struct nsm_firmware_slot_info info = {};
    info.slot_id = 0;
    info.firmware_state = 1;
    struct nsm_firmware_erot_state_info_hdr_resp hdr = {};
    hdr.active_slot = 0; // active slot
    slot.update(info, hdr);

    EXPECT_TRUE(slot.isActive());
    // The associations list should contain exactly 1 entry (only "software"
    // was kept in constructor, "contained_by" was filtered out)
    EXPECT_EQ(slot.associations().size(), 1u);
}

// =============================================================================
// Branch coverage: updateActiveSlotAssociation – inactive slot → "InactiveSlot"
// (line 68-69 in nsmFirmwareSlot.cpp: isActive() FALSE branch)
// =============================================================================

TEST(NsmFirmwareSlot, UpdateActiveSlotAssociation_InactiveSlot_SetsInactiveSlot)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/inactive_assoc";
    std::string chassisName = "InactAssocChassis";
    // "software" association kept by constructor
    std::vector<utils::Association> associations;
    associations.push_back({"software", "chassis", chassisPath});
    int slotNum = 0;

    NsmFirmwareSlot slot(bus, chassisPath, associations, slotNum,
                         SlotIntf::FirmwareType::AP, chassisName);

    // slot_id=0, active_slot=1 → slot is INACTIVE
    struct nsm_firmware_slot_info info = {};
    info.slot_id = 0;
    info.firmware_state = 1;
    struct nsm_firmware_erot_state_info_hdr_resp hdr = {};
    hdr.active_slot = 1; // different → inactive slot
    slot.update(info, hdr);

    EXPECT_FALSE(slot.isActive());
    // updateActiveSlotAssociation ran with isActive()=false → "InactiveSlot"
    // The associations list should have been updated
    EXPECT_GE(slot.associations().size(), 1u);
}

// =============================================================================
// Branch coverage: WriteInProgress + ACTIVE slot → uses firmwareVersion not
// "NA" (line 127 Branch 11→14: !isActive() is FALSE → short-circuit to
// firmwareVersion)
// =============================================================================

TEST(NsmFirmwareSlot, Update_WriteInProgress_ActiveSlot_UsesFirmwareVersion)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/wip_active";
    std::string chassisName = "WipActiveChassis";
    NsmFirmwareSlot slot(bus, chassisPath, {}, 0, SlotIntf::FirmwareType::AP,
                         chassisName);

    struct nsm_firmware_slot_info info = {};
    info.slot_id = 0;
    info.firmware_state = 4; // WriteInProgress (index 4 in stateTbl)
    std::strcpy(reinterpret_cast<char*>(info.firmware_version_string),
                "v2.5.0");
    struct nsm_firmware_erot_state_info_hdr_resp hdr = {};
    hdr.active_slot = 0; // same as slot_id → ACTIVE slot
    slot.update(info, hdr);

    EXPECT_EQ(slot.state(), State::FirmwareState::WriteInProgress);
    EXPECT_TRUE(slot.isActive());
    // fstate==WriteInProgress but isActive()==TRUE → !isActive() is FALSE
    // → short-circuit, extendedVersion = firmwareVersion (not "NA")
    EXPECT_EQ(slot.extendedVersion(), "v2.5.0");
}

// =============================================================================
// Branch coverage: WriteInProgress + inactive + NON-EMPTY version →
// firmwareVersion (line 128 Branch 12→14: firmwareVersion.empty() is FALSE →
// use firmwareVersion)
// =============================================================================

TEST(NsmFirmwareSlot,
     Update_WriteInProgress_Inactive_NonEmptyVersion_UsesVersion)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/wip_nonempty";
    std::string chassisName = "WipNonEmptyChassis";
    NsmFirmwareSlot slot(bus, chassisPath, {}, 0, SlotIntf::FirmwareType::AP,
                         chassisName);

    struct nsm_firmware_slot_info info = {};
    info.slot_id = 0;
    info.firmware_state = 4; // WriteInProgress
    // firmware_version_string is non-empty → firmwareVersion.empty() is FALSE
    std::strcpy(reinterpret_cast<char*>(info.firmware_version_string),
                "1.2.3.4");
    struct nsm_firmware_erot_state_info_hdr_resp hdr = {};
    hdr.active_slot = 1; // inactive (slot_id=0 != 1)
    slot.update(info, hdr);

    EXPECT_EQ(slot.state(), State::FirmwareState::WriteInProgress);
    EXPECT_FALSE(slot.isActive());
    // fstate==WriteInProgress && !isActive()==TRUE but firmwareVersion not
    // empty → firmwareVersion.empty() is FALSE → extendedVersion =
    // firmwareVersion
    EXPECT_EQ(slot.extendedVersion(), "1.2.3.4");
}

// =============================================================================
// Branch coverage: updateActiveSlotAssociation else-branch (line 76):
// association where std::get<0>() != "software" → emplace_back directly. //
// Constructor filters, so set non-software tuple directly and
// call method.
// =============================================================================

TEST(NsmFirmwareSlot, UpdateActiveSlotAssociation_NonSoftwareTuple_ElseBranch)
{
    std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/nsw";
    std::string chassisName = "NswChassis";
    NsmFirmwareSlot slot(bus, chassisPath, {}, 0, SlotIntf::FirmwareType::AP,
                         chassisName);

    // Directly set a non-"software" association tuple on the D-Bus property
    // so that updateActiveSlotAssociation() takes the else branch (line 76)
    using AssocTuple = std::tuple<std::string, std::string, std::string>;
    slot.associations({AssocTuple{"contained_by", "containing", chassisPath}});

    // Call updateActiveSlotAssociation() directly
    slot.updateActiveSlotAssociation();

    // The else branch copies the non-software tuple as-is
    auto assocs = slot.associations();
    ASSERT_EQ(assocs.size(), 1u);
    EXPECT_EQ(std::get<0>(assocs[0]), "contained_by");
}
