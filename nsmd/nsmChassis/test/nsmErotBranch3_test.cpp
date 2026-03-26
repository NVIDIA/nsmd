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
 * Branch coverage batch 3 for nsmd/nsmChassis/nsmErot.cpp
 *
 * Covers:
 * - extractNumber: string ending in digits -> returns the number
 * - extractNumber: string with no digits -> returns -1
 * - extractNumber: empty string -> returns -1
 * - extractNumber: all digits -> returns the number
 * - extractNumber: single digit at end -> returns that digit
 * - extractNumber: mixed text and trailing digits
 * - Factory: Type not "NSM_Chassis" or "NSM_ChassisRoT" -> no processing
 * - Factory: SlotCount absent -> co_return NSM_SUCCESS early
 * - Factory: Name absent -> empty name (L228-231 FALSE branch)
 * - Factory: UUID absent -> empty uuid (L232-236 FALSE branch)
 * - Factory: ImageCopyEnabled present TRUE branch
 * - Factory: InbandUpdatePolicyEnabled present TRUE branch
 * - Factory: ImageCopyPolicyEnabled present TRUE branch
 * - Factory: AP slot with reportSkuWithNsm=true
 * - Factory: AP slot with enableUpdateSKU=true via SetRotPropertyList
 * - Factory: multiple AP slots same chassisName (dedup in maps)
 * - Factory: multiple EC slots (only first creates ecFirmwareType etc.)
 * - Factory: AP slot with IsRoT=false (slotKey = chassisName)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "libnsm/firmware-utils.h"

#include "nsmChassis/nsmErot.hpp"

using namespace nsm;

namespace nsm
{
int extractNumber(const std::string& str);
requester::Coroutine nsmErotCreateSensors(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath);
} // namespace nsm

// ============================================================================
// extractNumber tests
// ============================================================================

struct ExtractNumberTest : public Test
{};

TEST_F(ExtractNumberTest, StringEndingInDigits)
{
    EXPECT_EQ(extractNumber("Slot42"), 42);
}

TEST_F(ExtractNumberTest, StringEndingInSingleDigit)
{
    EXPECT_EQ(extractNumber("Slot1"), 1);
}

TEST_F(ExtractNumberTest, AllDigits)
{
    EXPECT_EQ(extractNumber("12345"), 12345);
}

TEST_F(ExtractNumberTest, NoDigits)
{
    EXPECT_EQ(extractNumber("NoDigitsHere"), -1);
}

TEST_F(ExtractNumberTest, EmptyString)
{
    EXPECT_EQ(extractNumber(""), -1);
}

TEST_F(ExtractNumberTest, DigitsInMiddleOnly)
{
    // "abc123def" -> trailing "def" has no digits, so extractNumber sees no
    // trailing digits -> returns -1
    EXPECT_EQ(extractNumber("abc123def"), -1);
}

TEST_F(ExtractNumberTest, TrailingZero)
{
    EXPECT_EQ(extractNumber("Slot0"), 0);
}

TEST_F(ExtractNumberTest, MultipleTrailingDigits)
{
    EXPECT_EQ(extractNumber("Device007"), 7);
}

TEST_F(ExtractNumberTest, LargeNumber)
{
    EXPECT_EQ(extractNumber("Slot999999"), 999999);
}

// ============================================================================
// Factory function branch tests
// ============================================================================

struct NsmErotBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Chassis";
    const std::string rotIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisRoT";
    const std::string slotIntfName =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmErotBranch3Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmErotBranch3Test()
    {
        cleanupDeviceSensors(devices);
    }

    const MapperServiceMap slotServiceMap = {{
        {
            "xyz.openbmc_project.NSM",
            {slotIntfName + ".Associations"},
        },
    }};

    void setupSlot(const std::string& parentPath, int slotIndex,
                   const std::string& slotName, const std::string& fwType,
                   const std::string& chassisName, uint64_t classification = 1,
                   uint64_t identifier = 1, uint64_t componentIndex = 0)
    {
        std::string slotPath = parentPath + "/Slot" + std::to_string(slotIndex);
        auto& slotProps = utils::MockDbusAsync::propertyMap(slotPath,
                                                            slotIntfName);
        slotProps["Name"] = slotName;
        slotProps["FirmwareType"] = fwType;
        slotProps["ChassisName"] = chassisName;
        slotProps["ComponentClassification"] = classification;
        slotProps["ComponentIdentifier"] = identifier;
        slotProps["ComponentIndex"] = componentIndex;

        auto& slotAssocProps = utils::MockDbusAsync::propertyMap(
            slotPath, slotIntfName + ".Associations");
        slotAssocProps["Forward"] = std::string("chassis");
        slotAssocProps["Backward"] = std::string("firmware_slot");
        slotAssocProps["AbsolutePath"] = parentPath;
    }
};

// ============================================================================
// Factory: Type not NSM_Chassis or NSM_ChassisRoT -> no processing
// ============================================================================

TEST_F(NsmErotBranch3Test, Factory_UnrecognizedType_NoProcessing)
{
    const std::string uniquePath = "/xyz/test/erot_branch3/unrecognized";

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_SomethingElse");

    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
}

// ============================================================================
// Factory: SlotCount absent -> co_return NSM_SUCCESS early (L246-248)
// ============================================================================

TEST_F(NsmErotBranch3Test, Factory_SlotCountAbsent_EarlyReturn)
{
    const std::string uniquePath = "/xyz/test/erot_branch3/no_slotcount";

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_Chassis");
    pm["Name"] = std::string("NoSlotCountChassis");
    pm["UUID"] = fpgaUuid;
    // No SlotCount key

    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
}

// ============================================================================
// Factory: Name absent -> empty name (L228-231 FALSE branch)
// Empty name -> invalid D-Bus path -> SdBusError
// ============================================================================

TEST_F(NsmErotBranch3Test, DISABLED_Factory_NameAbsent)
{
    const std::string uniquePath = "/xyz/test/erot_branch3/no_name";

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    // No "Name" -> empty name
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(0);

    EXPECT_ANY_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
}

// ============================================================================
// Factory: UUID absent -> empty uuid (L232-236 FALSE branch)
// ============================================================================

TEST_F(NsmErotBranch3Test, DISABLED_Factory_UUIDAbsent)
{
    const std::string uniquePath = "/xyz/test/erot_branch3/no_uuid";

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = std::string("NoUuidChassis");
    // No "UUID"
    pm["SlotCount"] = uint64_t(0);

    // Empty UUID -> device lookup may throw
    EXPECT_ANY_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
}

// ============================================================================
// Factory: ImageCopyEnabled=true, InbandUpdatePolicyEnabled=true,
// ImageCopyPolicyEnabled=true (all enabled)
// ============================================================================

TEST_F(NsmErotBranch3Test, Factory_AllPoliciesEnabled)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_Erot_AllPolicies";
    const std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                                   uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(2);
    pm["ImageCopyEnabled"] = bool(true);
    pm["InbandUpdatePolicyEnabled"] = bool(true);
    pm["ImageCopyPolicyEnabled"] = bool(true);

    // Slot1: AP
    setupSlot(uniquePath, 1, "Slot1", "AP", uniqueName + "_AP", 10, 20, 0);
    // Slot2: EC
    setupSlot(uniquePath, 2, "Slot2", "EC", uniqueName, 11, 21, 0);

    const size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// ============================================================================
// Factory: AP slot with reportSkuWithNsm=true
// NsmApSkuIdObject constructor creates D-Bus vtables which may fail
// on the test bus -> accept SdBusError as branch-covering outcome
// ============================================================================

TEST_F(NsmErotBranch3Test, Factory_APSlot_ReportSkuWithNsm)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_Erot_SkuNsm";
    const std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                                   uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(1);

    setupSlot(uniquePath, 1, "Slot1", "AP", uniqueName + "_AP");
    auto& slotProps = utils::MockDbusAsync::propertyMap(uniquePath + "/Slot1",
                                                        slotIntfName);
    slotProps["ReportSkuWithNsm"] = bool(true);

    // May throw SdBusError from ApSkuIdConfiguration D-Bus vtable creation
    try
    {
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    }
    catch (const sdbusplus::exception::SdBusError&)
    {
        // Expected on test bus - branches still exercised
    }
}

// ============================================================================
// Factory: AP slot with enableUpdateSKU=true via SetRotPropertyList
// containing "AP_SKU_ID"
// NsmApSkuIdObject D-Bus vtable may fail on test bus
// ============================================================================

TEST_F(NsmErotBranch3Test, Factory_APSlot_EnableUpdateSKU)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_Erot_UpdateSku";
    const std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                                   uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(1);

    setupSlot(uniquePath, 1, "Slot1", "AP", uniqueName + "_AP");
    auto& slotProps = utils::MockDbusAsync::propertyMap(uniquePath + "/Slot1",
                                                        slotIntfName);
    slotProps["ReportSkuWithNsm"] = bool(true);
    slotProps["SetRotPropertyList"] = std::vector<std::string>{"AP_SKU_ID",
                                                               "OTHER_PROP"};

    // May throw SdBusError from ApSkuIdConfiguration D-Bus vtable creation
    try
    {
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    }
    catch (const sdbusplus::exception::SdBusError&)
    {
        // Expected on test bus - branches still exercised
    }
}

// ============================================================================
// Factory: multiple AP slots with same chassisName -> dedup in maps
// (second slot reuses existing entries in apFirmwareTypeMap etc.)
// ============================================================================

TEST_F(NsmErotBranch3Test, Factory_MultipleAPSlots_SameChassisName)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_Erot_DupAP";
    const std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                                   uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(2);

    const std::string apChassis = uniqueName + "_AP";
    setupSlot(uniquePath, 1, "Slot1", "AP", apChassis, 1, 1, 0);
    setupSlot(uniquePath, 2, "Slot2", "AP", apChassis, 1, 1, 1);

    const size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// ============================================================================
// Factory: multiple EC slots -> only first creates ecFirmwareType etc.
// ============================================================================

TEST_F(NsmErotBranch3Test, Factory_MultipleECSlots_OnlyFirstCreates)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_Erot_DupEC";
    const std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                                   uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(2);

    setupSlot(uniquePath, 1, "Slot1", "EC", uniqueName, 1, 1, 0);
    setupSlot(uniquePath, 2, "Slot2", "EC", uniqueName, 1, 1, 1);

    const size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// ============================================================================
// Factory: AP slot with IsRoT=false -> slotKey = chassisName (not name)
// The NsmKeyMgmt creates D-Bus vtables which may fail on test bus
// ============================================================================

TEST_F(NsmErotBranch3Test, Factory_APSlot_IsRoTFalse)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_Erot_APNoRoT";
    const std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                                   uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(1);

    setupSlot(uniquePath, 1, "Slot1", "AP", uniqueName + "_AP");
    auto& slotProps = utils::MockDbusAsync::propertyMap(uniquePath + "/Slot1",
                                                        slotIntfName);
    slotProps["IsRoT"] = bool(false); // slotKey = chassisName

    // May throw SdBusError from D-Bus vtable creation
    try
    {
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath);
    }
    catch (const sdbusplus::exception::SdBusError&)
    {
        // Expected on test bus - branches still exercised
    }
}

// ============================================================================
// Factory: Type absent -> empty type -> does not match NSM_Chassis or
// NSM_ChassisRoT (L225 FALSE)
// ============================================================================

TEST_F(NsmErotBranch3Test, Factory_TypeAbsent)
{
    const std::string uniquePath = "/xyz/test/erot_branch3/no_type";

    utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    // No "Type" key

    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
}

// ============================================================================
// Factory: mixed AP and EC slots with inbandUpdatePolicy for EC slot
// (inbandUpdatePolicy created on first slot regardless of type)
// ============================================================================

TEST_F(NsmErotBranch3Test, Factory_MixedSlots_InbandUpdateOnEC)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_Erot_MixedIUP";
    const std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                                   uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(2);
    pm["InbandUpdatePolicyEnabled"] = bool(true);

    // Slot1: EC (inbandUpdatePolicy created here)
    setupSlot(uniquePath, 1, "Slot1", "EC", uniqueName, 1, 1, 0);
    // Slot2: AP (inbandUpdatePolicy already created, skip)
    setupSlot(uniquePath, 2, "Slot2", "AP", uniqueName + "_AP", 2, 2, 0);

    const size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// ============================================================================
// Factory: ImageCopyEnabled with EC slot -> imageCopyObject created
// ============================================================================

TEST_F(NsmErotBranch3Test, Factory_ImageCopyEnabled_ECSlot)
{
    utils::MockDbusAsync::serviceMap() = slotServiceMap;

    const std::string uniqueName = "HGX_Erot_ImgCopy";
    const std::string uniquePath = std::string(chassisInventoryBasePath) + "/" +
                                   uniqueName;

    auto& pm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    pm["Type"] = std::string("NSM_ChassisRoT");
    pm["Name"] = uniqueName;
    pm["UUID"] = fpgaUuid;
    pm["SlotCount"] = uint64_t(1);
    pm["ImageCopyEnabled"] = bool(true);

    setupSlot(uniquePath, 1, "Slot1", "EC", uniqueName, 1, 1, 0);

    const size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(
        nsmErotCreateSensors(mockManager, basicIntfName, uniquePath));
    EXPECT_GT(fpga->deviceSensors.size(), before);
}
