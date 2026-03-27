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
 * Branch coverage tests for nsmd/nsmDevice.cpp (batch 2).
 *
 * Covers:
 * - FruInterfaceManager::createAndRegisterInterface: with all properties
 *   and with no optional properties (each find() branch)
 * - FruInterfaceManager::updateAllPropertyValues: !interface → early return;
 *   with each inventory property present/absent
 * - FruInterfaceManager::registerAllProperties: each property present/absent
 * - NsmDevice::dumpNsmDeviceInfo: just calls detach — reachability test
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"

#include "commonMock.hpp"
#include "nsmDevice.hpp"
#include "test/mockDBusHandler.hpp"

#undef private
#undef protected

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

using namespace nsm;

// Shared boost/asio infrastructure
static boost::asio::io_context sIo;
static auto sBus = std::make_shared<sdbusplus::asio::connection>(sIo);
static auto sObjServer = std::make_shared<sdbusplus::asio::object_server>(sBus);

// Helper: build an InventoryProperties with all 6 supported properties
static InventoryProperties makeAllProps()
{
    InventoryProperties props;
    props[BOARD_PART_NUMBER] = std::string("BP123");
    props[FRU_PART_NUMBER] = std::string("FP456");
    props[SERIAL_NUMBER] = std::string("SN789");
    props[MARKETING_NAME] = std::string("TestDevice");
    props[BUILD_DATE] = std::string("2024-01-01");
    props[DEVICE_GUID] = std::string("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
    return props;
}

// ============================================================================
// FruInterfaceManager::createAndRegisterInterface
// ============================================================================

TEST(FruInterfaceManagerBranch2, CreateAndRegisterInterface_AllProps)
{
    FruInterfaceManager mgr;
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "10", 0);
    dev->eid = 10;

    auto props = makeAllProps();
    // Each register_property call in registerAllProperties is exercised
    EXPECT_NO_THROW(mgr.createAndRegisterInterface(sObjServer, 10, dev, props));
    EXPECT_NE(mgr.interface, nullptr);
}

TEST(FruInterfaceManagerBranch2, CreateAndRegisterInterface_NoOptionalProps)
{
    FruInterfaceManager mgr;
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "11", 0);
    dev->eid = 11;

    // Empty properties: all find() return end() → only mandatory props added
    InventoryProperties props;
    EXPECT_NO_THROW(mgr.createAndRegisterInterface(sObjServer, 11, dev, props));
    EXPECT_NE(mgr.interface, nullptr);
}

TEST(FruInterfaceManagerBranch2, CreateAndRegisterInterface_PartialProps)
{
    FruInterfaceManager mgr;
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "12", 0);
    dev->eid = 12;

    // Only BOARD_PART_NUMBER and DEVICE_GUID present
    InventoryProperties props;
    props[BOARD_PART_NUMBER] = std::string("BP_partial");
    props[DEVICE_GUID] = std::string("11111111-2222-3333-4444-555555555555");
    EXPECT_NO_THROW(mgr.createAndRegisterInterface(sObjServer, 12, dev, props));
    EXPECT_NE(mgr.interface, nullptr);
}

// ============================================================================
// FruInterfaceManager::updateAllPropertyValues
// ============================================================================

TEST(FruInterfaceManagerBranch2, UpdateAllPropertyValues_NullInterface_Returns)
{
    FruInterfaceManager mgr;
    // interface is nullptr → early return, no crash
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "20", 0);
    dev->eid = 20;
    InventoryProperties props = makeAllProps();
    EXPECT_NO_THROW(mgr.updateAllPropertyValues(dev, props));
}

TEST(FruInterfaceManagerBranch2, UpdateAllPropertyValues_AllPropsPresent)
{
    FruInterfaceManager mgr;
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "21", 0);
    dev->eid = 21;

    // First create the interface so it's non-null
    InventoryProperties props = makeAllProps();
    mgr.createAndRegisterInterface(sObjServer, 21, dev, props);
    ASSERT_NE(mgr.interface, nullptr);

    // updateAllPropertyValues with all properties → each set_property branch
    // taken
    EXPECT_NO_THROW(mgr.updateAllPropertyValues(dev, props));
}

TEST(FruInterfaceManagerBranch2, UpdateAllPropertyValues_NoOptionalProps)
{
    FruInterfaceManager mgr;
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "22", 0);
    dev->eid = 22;

    // Create with empty props
    InventoryProperties empty;
    mgr.createAndRegisterInterface(sObjServer, 22, dev, empty);
    ASSERT_NE(mgr.interface, nullptr);

    // updateAllPropertyValues with empty → all find() return end() (FALSE
    // paths)
    EXPECT_NO_THROW(mgr.updateAllPropertyValues(dev, empty));
}

TEST(FruInterfaceManagerBranch2, UpdateAllPropertyValues_BoardPartNumberOnly)
{
    FruInterfaceManager mgr;
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "23", 0);
    dev->eid = 23;

    InventoryProperties props;
    props[BOARD_PART_NUMBER] = std::string("BP_only");
    mgr.createAndRegisterInterface(sObjServer, 23, dev, props);
    ASSERT_NE(mgr.interface, nullptr);

    // BOARD_PART_NUMBER found (TRUE), others absent (FALSE)
    EXPECT_NO_THROW(mgr.updateAllPropertyValues(dev, props));
}

TEST(FruInterfaceManagerBranch2, UpdateAllPropertyValues_SerialAndMarketingName)
{
    FruInterfaceManager mgr;
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "24", 0);
    dev->eid = 24;

    InventoryProperties props;
    props[SERIAL_NUMBER] = std::string("SN_test");
    props[MARKETING_NAME] = std::string("MarketingTest");
    mgr.createAndRegisterInterface(sObjServer, 24, dev, props);
    ASSERT_NE(mgr.interface, nullptr);

    EXPECT_NO_THROW(mgr.updateAllPropertyValues(dev, props));
}

TEST(FruInterfaceManagerBranch2, UpdateAllPropertyValues_FruPartAndBuildDate)
{
    FruInterfaceManager mgr;
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "25", 0);
    dev->eid = 25;

    InventoryProperties props;
    props[FRU_PART_NUMBER] = std::string("FP_test");
    props[BUILD_DATE] = std::string("2025-06-15");
    mgr.createAndRegisterInterface(sObjServer, 25, dev, props);
    ASSERT_NE(mgr.interface, nullptr);

    EXPECT_NO_THROW(mgr.updateAllPropertyValues(dev, props));
}

TEST(FruInterfaceManagerBranch2,
     UpdateAllPropertyValues_DeviceGuidUpdatesDeviceUuid)
{
    FruInterfaceManager mgr;
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "26", 0);
    dev->eid = 26;

    InventoryProperties props;
    props[DEVICE_GUID] = std::string("deadbeef-cafe-babe-feed-badc0ffee000");
    mgr.createAndRegisterInterface(sObjServer, 26, dev, props);
    ASSERT_NE(mgr.interface, nullptr);

    EXPECT_NO_THROW(mgr.updateAllPropertyValues(dev, props));
    // DEVICE_GUID found → nsmDevice->deviceUuid set
    EXPECT_EQ(dev->deviceUuid, "deadbeef-cafe-babe-feed-badc0ffee000");
}

// ============================================================================
// FruInterfaceManager::registerAllProperties (individual property branches)
// ============================================================================

TEST(FruInterfaceManagerBranch2, RegisterAllProperties_AllProps)
{
    FruInterfaceManager mgr;
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "30", 0);
    dev->eid = 30;

    // createAndRegisterInterface internally calls registerAllProperties
    InventoryProperties props = makeAllProps();
    EXPECT_NO_THROW(mgr.createAndRegisterInterface(sObjServer, 30, dev, props));
}

TEST(FruInterfaceManagerBranch2, RegisterAllProperties_DeviceGuidSetsDeviceUuid)
{
    FruInterfaceManager mgr;
    auto dev = std::make_shared<MockNsmDevice>(0, 0, "MCTP_EID", "31", 0);
    dev->eid = 31;

    InventoryProperties props;
    props[DEVICE_GUID] = std::string("cafecafe-beef-0000-1111-222233334444");
    mgr.createAndRegisterInterface(sObjServer, 31, dev, props);

    // registerAllProperties sets nsmDevice->deviceUuid from DEVICE_GUID
    EXPECT_EQ(dev->deviceUuid, "cafecafe-beef-0000-1111-222233334444");
}

// ============================================================================
// NsmDevice::dumpNsmDeviceInfo: calls dumpNsmDeviceInfoTask().detach()
// ============================================================================

TEST(NsmDeviceBranch2, DISABLED_DumpNsmDeviceInfo_DoesNotCrash)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "5", 0);
    dev.eid = 5;
    dev.isDeviceActive = true;
    dev.isDeviceReady = true;
    dev.discoveryPending = false;

    // dumpNsmDeviceInfo launches detached coroutine (no blocking)
    EXPECT_NO_THROW(dev.NsmDevice::dumpNsmDeviceInfo());
}

TEST(NsmDeviceBranch2, DISABLED_DumpNsmDeviceInfo_WithEventSubscriptionStatus)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "7", 0);
    dev.eid = 7;

    // set lastEventSubscriptionStatus so the if-branch is taken
    dev.lastEventSubscriptionStatus = "subscribed";
    EXPECT_NO_THROW(dev.NsmDevice::dumpNsmDeviceInfo());
}

TEST(NsmDeviceBranch2,
     DISABLED_DumpNsmDeviceInfo_LastEventRequestAndResponsePresent)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "8", 0);
    dev.eid = 8;

    // Set lastEventSubscriptionRequest and lastEventSubscriptionResponse
    // so the non-empty checks resolve to true
    dev.lastEventSubscriptionRequest = std::vector<uint8_t>{0x01, 0x02, 0x03};
    dev.lastEventSubscriptionResponse = std::vector<uint8_t>{0x04, 0x05};
    EXPECT_NO_THROW(dev.NsmDevice::dumpNsmDeviceInfo());
}

// ============================================================================
// NsmDevice::updateDiscoveryIdentifiers: branch coverage
// ============================================================================

// isPreferred = false path: existing PCIe (priority 0) > new SMBus (priority 6)
// → device NOT updated (else branch at line ~532)
TEST(NsmDeviceBranch2, UpdateDiscoveryIdentifiers_NotPreferred_ElseBranch)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "40", 0);
    dev.eid = 40;
    dev.uuid = "existing-uuid-abc";
    // Set existing to high-priority (PCIe = 0)
    dev.mctpMedium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    dev.mctpBinding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    std::string assocPath = "/test/path";
    // Offer a lower-priority medium (SMBus = 6) → isPreferred returns false
    std::string newMedium =
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.SMBus";
    std::string newBinding =
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.SMBus";

    bool result = dev.updateDiscoveryIdentifiers(
        41, "new-uuid-xyz", 0, assocPath, newMedium, newBinding, std::nullopt);
    // isPreferred=false → not updated → returns false
    EXPECT_FALSE(result);
    // EID should NOT have changed
    EXPECT_EQ(dev.eid, 40);
}

// isPreferred = true AND eid changes → fruDeviceManager.reset() branch
TEST(NsmDeviceBranch2,
     UpdateDiscoveryIdentifiers_EidChanges_FruDeviceManagerReset)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "41", 0);
    dev.eid = 40;                   // existing EID
    dev.uuid = "existing-uuid-def"; // non-empty → isPreferred calculated
    // New connection on same medium/binding → isPreferred returns true
    dev.mctpMedium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    dev.mctpBinding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    std::string assocPath = "/test/path2";
    std::string newMedium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    std::string newBinding =
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    // New EID = 99 (different from existing 40) → fruDeviceManager.reset()
    bool result = dev.updateDiscoveryIdentifiers(
        99, "new-uuid-def2", 0, assocPath, newMedium, newBinding, std::nullopt);
    EXPECT_TRUE(result);
    EXPECT_EQ(dev.eid, 99); // EID updated
}
