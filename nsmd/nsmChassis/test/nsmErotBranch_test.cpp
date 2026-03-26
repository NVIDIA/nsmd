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

/*
 * Branch coverage for nsmd/nsmChassis/nsmErot.cpp
 *
 * Covers:
 * - extractNumber: no-digit string → returns -1 (L113-115)
 * - parseSlots: FALSE branches for all optional slot properties
 * - parseSlots: SetRotPropertyList without AP_SKU_ID (L185 FALSE)
 * - parseSlots: SetRotPropertyList wrong type → exception catch (L191-195)
 * - nsmErotCreateSensors: missing optional main props (ImageCopyEnabled etc.)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmChassis/nsmErot.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine nsmErotCreateSensors(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath);
} // namespace nsm

struct NsmErotBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_RoT";
    const std::string slotIntfName =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";
    const std::string chassisName = "HGX_RoT_BRANCH";
    const std::string objPath = std::string(chassisInventoryBasePath) + "/" +
                                chassisName;
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmErotBranchTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    void TearDown() override
    {
        cleanupDeviceSensors(devices);
    }

    const MapperServiceMap slotServiceMap = {{
        {
            "xyz.openbmc_project.NSM",
            {"xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations"},
        },
    }};
};

// ============================================================================
// Slot with FirmwareType="EC" but absent ComponentClassification,
// ComponentIdentifier, ComponentIndex, IsRoT, ReportSkuWithNsm,
// SetRotPropertyList → covers FALSE branches at L137-174 in parseSlots.
// Name and ChassisName are provided to avoid invalid D-Bus paths.
// (FirmwareType FALSE and ChassisName FALSE are already covered by dedicated
// tests in nsmErot_test.cpp.)
// ============================================================================

TEST_F(NsmErotBranchTest, SlotMissingNumericAndBoolOptionalProps)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    std::string uniqueName = chassisName + "_MinimalSlot";
    std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                             uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = std::string("NSM_ChassisRoT");
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = fpgaUuid;
    propertyMap["SlotCount"] = uint64_t(1);

    // Slot 1: FirmwareType="EC", Name and ChassisName present to produce a
    // valid D-Bus path (getPath = chassisPath/Slots/1/chassisName).
    // ComponentClassification, ComponentIdentifier, ComponentIndex, IsRoT,
    // ReportSkuWithNsm, SetRotPropertyList all ABSENT → FALSE branches at
    // L137, L142, L147, L166, L170, L177.
    std::string slot1Path = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(slot1Path,
                                                         slotIntfName);
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["FirmwareType"] = std::string("EC");
    slot1Props["ChassisName"] = uniqueName; // same as main → valid path

    auto& slot1AssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path, slotIntfName + ".Associations");
    slot1AssocProps["Forward"] = std::string("chassis");
    slot1AssocProps["Backward"] = std::string("firmware_slot");
    slot1AssocProps["AbsolutePath"] = uniquePath;

    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
}

// ============================================================================
// SetRotPropertyList without AP_SKU_ID: L185 FALSE → enableUpdateSKU stays
// false
// ============================================================================

TEST_F(NsmErotBranchTest, SlotSetRotPropertyListWithoutAPSkuId)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    std::string uniqueName = chassisName + "_NoSkuUpdate";
    std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                             uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = std::string("NSM_ChassisRoT");
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = fpgaUuid;
    propertyMap["SlotCount"] = uint64_t(1);

    std::string slot1Path = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(slot1Path,
                                                         slotIntfName);
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = uniqueName + "_AP";
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    // SetRotPropertyList present but does NOT contain AP_SKU_ID
    // → covers L185 FALSE branch (find() == end() → enableUpdateSKU stays
    // false)
    slot1Props["SetRotPropertyList"] =
        std::vector<std::string>{"SOME_OTHER_PROPERTY"};

    auto& slot1AssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path, slotIntfName + ".Associations");
    slot1AssocProps["Forward"] = std::string("chassis");
    slot1AssocProps["Backward"] = std::string("firmware_slot");
    slot1AssocProps["AbsolutePath"] = uniquePath;

    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
}

// ============================================================================
// SetRotPropertyList with wrong variant type → triggers catch at L191-195
// ============================================================================

TEST_F(NsmErotBranchTest, SlotSetRotPropertyListWrongType_CatchesBadVariant)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    std::string uniqueName = chassisName + "_BadPropertyList";
    std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                             uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = std::string("NSM_ChassisRoT");
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = fpgaUuid;
    propertyMap["SlotCount"] = uint64_t(1);

    std::string slot1Path = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(slot1Path,
                                                         slotIntfName);
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = uniqueName + "_AP";
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    // SetRotPropertyList with wrong type (uint64_t instead of vector<string>)
    // → std::get<vector<string>> throws std::bad_variant_access → catch at L191
    //
    slot1Props["SetRotPropertyList"] = uint64_t(42);

    auto& slot1AssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path, slotIntfName + ".Associations");
    slot1AssocProps["Forward"] = std::string("chassis");
    slot1AssocProps["Backward"] = std::string("firmware_slot");
    slot1AssocProps["AbsolutePath"] = uniquePath;

    // Should not throw externally; the exception is caught internally at L191
    //
    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
}

// ============================================================================
// Main props: ImageCopyEnabled, InbandUpdatePolicyEnabled,
// ImageCopyPolicyEnabled missing → FALSE branches at L252, L259, L266 Also:
// ImageCopyEnabled=true covers imageCopyEnabled=true path
// ============================================================================

TEST_F(NsmErotBranchTest, MainPropsMissingOptional_FalseBranches)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    std::string uniqueName = chassisName + "_MissingMainOpt";
    std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                             uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = std::string("NSM_ChassisRoT");
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = fpgaUuid;
    propertyMap["SlotCount"] = uint64_t(1);
    // ImageCopyEnabled, InbandUpdatePolicyEnabled, ImageCopyPolicyEnabled all
    // absent → covers L252, L259, L266 FALSE branches

    std::string slot1Path = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(slot1Path,
                                                         slotIntfName);
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = uniqueName + "_AP";
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);

    auto& slot1AssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path, slotIntfName + ".Associations");
    slot1AssocProps["Forward"] = std::string("chassis");
    slot1AssocProps["Backward"] = std::string("firmware_slot");
    slot1AssocProps["AbsolutePath"] = uniquePath;

    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
}

// ============================================================================
// SlotCount missing → slot loop not entered (count("SlotCount") FALSE)
// ============================================================================

TEST_F(NsmErotBranchTest, SlotCountMissing_NoSlotsProcessed)
{
    std::string uniqueName = chassisName + "_NoSlotCount";
    std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                             uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = std::string("NSM_ChassisRoT");
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = fpgaUuid;
    // SlotCount absent → slotCount stays 0 → parseSlots loop not entered

    size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
    EXPECT_EQ(fpga->deviceSensors.size(), before);
}

// ============================================================================
// IsRoT=true in slot: covers L166 TRUE branch (isRoT assigned true)
// ============================================================================

TEST_F(NsmErotBranchTest, SlotIsRoT_TrueBranch)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    std::string uniqueName = chassisName + "_IsRoT";
    std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                             uniqueName;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(uniquePath,
                                                          basicIntfName);
    propertyMap["Type"] = std::string("NSM_ChassisRoT");
    propertyMap["Name"] = uniqueName;
    propertyMap["UUID"] = fpgaUuid;
    propertyMap["SlotCount"] = uint64_t(1);

    std::string slot1Path = uniquePath + "/Slot1";
    auto& slot1Props = utils::MockDbusAsync::propertyMap(slot1Path,
                                                         slotIntfName);
    slot1Props["Name"] = std::string("Slot1");
    slot1Props["FirmwareType"] = std::string("AP");
    slot1Props["ChassisName"] = uniqueName + "_AP";
    slot1Props["ComponentClassification"] = uint64_t(1);
    slot1Props["ComponentIdentifier"] = uint64_t(1);
    slot1Props["ComponentIndex"] = uint64_t(0);
    slot1Props["IsRoT"] = bool(true);             // covers L166 TRUE branch
    slot1Props["ReportSkuWithNsm"] = bool(false); // covers L171 TRUE branch

    auto& slot1AssocProps = utils::MockDbusAsync::propertyMap(
        slot1Path, slotIntfName + ".Associations");
    slot1AssocProps["Forward"] = std::string("chassis");
    slot1AssocProps["Backward"] = std::string("firmware_slot");
    slot1AssocProps["AbsolutePath"] = uniquePath;

    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
}
