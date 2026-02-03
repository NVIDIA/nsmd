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
