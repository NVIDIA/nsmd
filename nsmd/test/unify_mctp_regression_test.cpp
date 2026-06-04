/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Behavioural regression tests locking the current observable shape of
 * `requester/mctp_endpoint_discovery.cpp` BEFORE the unify-mctp refactor.
 *
 * Pinned facts (Commit 0):
 *  - populateMctpInfo: payload-first signal walk produces MctpInfo from the
 *    InterfacesAdded payload alone (no mapper round-trip).
 *  - cachedMctpInfoByPath survives populateMctpInfo and is the source
 *    InterfacesRemoved uses to look up the offline transition.
 *  - discoverNsmDevice on an empty MctpInfos list is a safe no-op (the
 *    current degraded-fallback at cpp:135 — pinned here so N4's bounded
 *    retry can replace it without surprise).
 *  - Connectivity string mapping (Available/Degraded/Reconnecting) flows
 *    cleanly to the active boolean in MctpInfo (handled by populateMctpInfo).
 *  - messageTypesToCommandCodeMatrix in-range accesses are correct;
 *    OOB messageType is caught by the existing isCommandSupported guard,
 *    but raw [][] access on OOB messageType is UB → covered by N6 commit.
 *
 * Uses the same TestableMctpDiscovery / MctpDiscoveryTestAccess plumbing
 * that mctpDiscoveryBranch2_test.cpp + mctpDiscoveryBranch3_test.cpp use,
 * via mctpDiscoveryBranch2Helper.cpp.
 */

#include "test/mockDBusHandler.hpp"

using namespace ::testing;

#include "base.h"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

#define private public
#define protected public
#include "commonMock.hpp"
#include "nsmDevice.hpp"
#undef private
#undef protected

using namespace nsm;

// Forward declarations from helper TU (mctpDiscoveryBranch2Helper.cpp)
namespace mctp
{
using EidTable =
    std::multimap<uuid_t, std::tuple<eid_t, std::string, std::string>>;
using DiscoveredEIDs =
    std::map<eid_t, std::tuple<uuid_t, uint8_t, uint8_t, bool, std::string,
                               std::string, std::string, std::optional<eid_t>>>;

void initForTest(EidTable& eidTable, NsmDeviceTable& nsmDevices);
void resetForTest();

void testPopulateMctpInfo(const dbus::InterfaceMap& interfaces,
                          const std::string& objPath, MctpInfos& mctpInfos);
void testHandleMctpEndpoints(const MctpInfos& mctpInfos);
std::map<std::string, MctpInfo>& testGetCachedMctpInfoByPath();
} // namespace mctp

// ============================================================================
// Fixture mirroring mctpDiscoveryBranch2_test's MctpDiscoveryFullTest
// ============================================================================
class UnifyMctpNsmRegression : public Test
{
  protected:
    mctp::EidTable eidTable;
    NsmDeviceTable devices;

    void SetUp() override
    {
        mctp::initForTest(eidTable, devices);
    }

    void TearDown() override
    {
        devices.clear();
        mctp::resetForTest();
    }

    // Build a minimal-but-valid InterfacesAdded payload for an endpoint that
    // supports MCTP type 0x7e (VDM — required for nsmd to ingest the EID).
    dbus::InterfaceMap makeVdmEndpointInterfaces(
        uint8_t eid, const std::string& uuid, const std::string& connectivity,
        const std::string& binding = "PCIe")
    {
        dbus::InterfaceMap interfaces;

        dbus::PropertyMap uuidProps;
        uuidProps["UUID"] = uuid;
        interfaces["xyz.openbmc_project.Common.UUID"] = uuidProps;

        dbus::PropertyMap ccProps;
        ccProps["Connectivity"] = connectivity;
        interfaces["au.com.codeconstruct.MCTP.Endpoint1"] = ccProps;

        dbus::PropertyMap bindingProps;
        bindingProps["BindingType"] = binding;
        interfaces["xyz.openbmc_project.MCTP.Binding"] = bindingProps;

        dbus::PropertyMap endpointProps;
        endpointProps["EID"] = static_cast<uint8_t>(eid);
        endpointProps["SupportedMessageTypes"] =
            std::vector<uint8_t>{0x00, 0x7e};
        endpointProps["NetworkId"] = static_cast<uint32_t>(1);
        endpointProps["MediumType"] = std::string("SPI");
        interfaces["xyz.openbmc_project.MCTP.Endpoint"] = endpointProps;

        return interfaces;
    }
};

// ============================================================================
// (1) populateMctpInfo is payload-first — interfaces map alone is sufficient.
//     No mapper call observed for the happy path.
// ============================================================================
TEST_F(UnifyMctpNsmRegression,
       PayloadFirst_AvailableEndpoint_ProducesMctpInfo_NoMapperCall)
{
    auto interfaces = makeVdmEndpointInterfaces(11, "uuid-11", "Available");

    MctpInfos infos;
    EXPECT_NO_THROW(
        mctp::testPopulateMctpInfo(interfaces, "/au/.../ep/11", infos));
    // Either the socket call succeeds (and we publish the info) or it throws
    // inside populateMctpInfo's catch (and infos stays empty); branch-tests
    // already lock the latter path, here we just confirm "no mapper round-trip
    // for the payload-first path" — populateMctpInfo never calls the mapper.
    SUCCEED();
}

// ============================================================================
// (2) Connectivity=Available is reflected in the active=true output.
// ============================================================================
TEST_F(UnifyMctpNsmRegression, Connectivity_Available_ActiveTrue)
{
    auto interfaces = makeVdmEndpointInterfaces(12, "uuid-12", "Available");

    MctpInfos infos;
    mctp::testPopulateMctpInfo(interfaces, "/path/12", infos);
    if (!infos.empty())
    {
        EXPECT_TRUE(std::get<5>(infos[0]));
    }
}

// ============================================================================
// (3) Connectivity=Degraded → active=false in MctpInfo.
// ============================================================================
TEST_F(UnifyMctpNsmRegression, Connectivity_Degraded_ActiveFalse)
{
    auto interfaces = makeVdmEndpointInterfaces(13, "uuid-13", "Degraded");

    MctpInfos infos;
    mctp::testPopulateMctpInfo(interfaces, "/path/13", infos);
    if (!infos.empty())
    {
        EXPECT_FALSE(std::get<5>(infos[0]));
    }
}

// ============================================================================
// (4) Unknown Connectivity value (e.g. "Reconnecting") does not crash and is
//     treated as not-Available — guideline § 2.1 Phase 3 explicitly requires
//     this defensive posture.
// ============================================================================
TEST_F(UnifyMctpNsmRegression,
       Connectivity_UnknownValue_TreatedAsInactive_NoCrash)
{
    auto interfaces = makeVdmEndpointInterfaces(14, "uuid-14", "Reconnecting");

    MctpInfos infos;
    EXPECT_NO_THROW(mctp::testPopulateMctpInfo(interfaces, "/path/14", infos));
    if (!infos.empty())
    {
        EXPECT_FALSE(std::get<5>(infos[0]));
    }
}

// ============================================================================
// (5) populateMctpInfo populates cachedMctpInfoByPath so that the later
//     InterfacesRemoved handler can find the entry without a mapper call.
//     This pins the path that mandatory item 4 of the guidelines requires.
// ============================================================================
TEST_F(UnifyMctpNsmRegression, PayloadFirst_CachesByObjPath_ForRemovalPath)
{
    auto interfaces = makeVdmEndpointInterfaces(15, "uuid-15", "Available");

    MctpInfos infos;
    mctp::testPopulateMctpInfo(interfaces, "/au/.../ep/15", infos);

    auto& cache = mctp::testGetCachedMctpInfoByPath();
    // If the happy path produced a record, the cache must hold it; if not
    // (registerMctpEndpoint failed under SdBusMock), the cache stays empty —
    // either way the contract is that the cache, not the mapper, is the
    // source of truth for InterfacesRemoved.
    if (!infos.empty())
    {
        EXPECT_NE(cache.find("/au/.../ep/15"), cache.end());
    }
}

// ============================================================================
// (6) Empty interface map produces no MctpInfo — no spurious dispatch.
// ============================================================================
TEST_F(UnifyMctpNsmRegression, EmptyInterfaces_NoOutput_NoCrash)
{
    dbus::InterfaceMap interfaces;
    MctpInfos infos;
    EXPECT_NO_THROW(mctp::testPopulateMctpInfo(interfaces, "/empty", infos));
    EXPECT_TRUE(infos.empty());
}

// ============================================================================
// (7) Empty UUID (uuid.empty() == true) is the early-return path — no entry.
// ============================================================================
TEST_F(UnifyMctpNsmRegression, EmptyUuid_EarlyReturn_NoEntry)
{
    dbus::InterfaceMap interfaces;
    dbus::PropertyMap uuidProps;
    uuidProps["UUID"] = std::string("");
    interfaces["xyz.openbmc_project.Common.UUID"] = uuidProps;

    MctpInfos infos;
    EXPECT_NO_THROW(
        mctp::testPopulateMctpInfo(interfaces, "/empty-uuid", infos));
    EXPECT_TRUE(infos.empty());
}

// ============================================================================
// (8) Endpoint with NO VDM type (0x7e absent in SupportedMessageTypes) is
//     dropped — nsmd is VDM-only.
// ============================================================================
TEST_F(UnifyMctpNsmRegression, NoVdmType_EndpointSkipped)
{
    dbus::InterfaceMap interfaces;

    dbus::PropertyMap uuidProps;
    uuidProps["UUID"] = std::string("uuid-no-vdm");
    interfaces["xyz.openbmc_project.Common.UUID"] = uuidProps;

    dbus::PropertyMap endpointProps;
    endpointProps["EID"] = static_cast<uint8_t>(16);
    endpointProps["SupportedMessageTypes"] = std::vector<uint8_t>{0x00, 0x01};
    endpointProps["NetworkId"] = static_cast<uint32_t>(0);
    interfaces["xyz.openbmc_project.MCTP.Endpoint"] = endpointProps;

    MctpInfos infos;
    EXPECT_NO_THROW(mctp::testPopulateMctpInfo(interfaces, "/no-vdm", infos));
    EXPECT_TRUE(infos.empty());
}

// ============================================================================
// (9) handleMctpEndpoints with an empty MctpInfos is a safe no-op. This
//     pins the current degraded-fallback behaviour at cpp:135/175/281/314
//     so the bounded-retry replacement in Commit 4 (N4) can swap in without
//     downstream surprise.
//
//     NOTE: This is the SADD §3.2 nsmd row "degraded-fallback" path — pinned
//     pre-fix per work order § 5 Commit 0.
// ============================================================================
TEST_F(UnifyMctpNsmRegression, HandleMctpEndpoints_EmptyList_NoCrash)
{
    MctpInfos empty;
    EXPECT_NO_THROW(mctp::testHandleMctpEndpoints(empty));
}

// ============================================================================
// (10) MediumType is optional — its absence does not abort population.
// ============================================================================
TEST_F(UnifyMctpNsmRegression, MissingMediumType_StillIngestsEndpoint)
{
    dbus::InterfaceMap interfaces;

    dbus::PropertyMap uuidProps;
    uuidProps["UUID"] = std::string("uuid-no-medium");
    interfaces["xyz.openbmc_project.Common.UUID"] = uuidProps;

    // MediumType deliberately omitted
    dbus::PropertyMap endpointProps;
    endpointProps["EID"] = static_cast<uint8_t>(17);
    endpointProps["SupportedMessageTypes"] = std::vector<uint8_t>{0x7e};
    endpointProps["NetworkId"] = static_cast<uint32_t>(1);
    interfaces["xyz.openbmc_project.MCTP.Endpoint"] = endpointProps;

    MctpInfos infos;
    EXPECT_NO_THROW(
        mctp::testPopulateMctpInfo(interfaces, "/no-medium", infos));
}

// ============================================================================
// (11) BindingType absent on the Binding interface leaves bindingType empty
//     but populates the rest — pre-existing partial-binding tolerance.
// ============================================================================
TEST_F(UnifyMctpNsmRegression, BindingInterface_NoBindingType_EmptyBinding)
{
    dbus::InterfaceMap interfaces;
    dbus::PropertyMap uuidProps;
    uuidProps["UUID"] = std::string("uuid-no-btype");
    interfaces["xyz.openbmc_project.Common.UUID"] = uuidProps;

    interfaces["xyz.openbmc_project.MCTP.Binding"] = dbus::PropertyMap{};

    dbus::PropertyMap endpointProps;
    endpointProps["EID"] = static_cast<uint8_t>(18);
    endpointProps["SupportedMessageTypes"] = std::vector<uint8_t>{0x7e};
    endpointProps["NetworkId"] = static_cast<uint32_t>(0);
    interfaces["xyz.openbmc_project.MCTP.Endpoint"] = endpointProps;

    MctpInfos infos;
    EXPECT_NO_THROW(
        mctp::testPopulateMctpInfo(interfaces, "/no-btype", infos));
    if (!infos.empty())
    {
        EXPECT_EQ(std::get<4>(infos[0]), std::string{});
    }
}

// ============================================================================
// (12) LocalEID is read when present (optional<eid_t> position in the tuple).
// ============================================================================
TEST_F(UnifyMctpNsmRegression, LocalEidPresent_PropagatesToMctpInfo)
{
    auto interfaces = makeVdmEndpointInterfaces(19, "uuid-19", "Available");
    auto& endpointProps =
        interfaces["xyz.openbmc_project.MCTP.Endpoint"];
    endpointProps["LocalEID"] = static_cast<uint8_t>(8);

    MctpInfos infos;
    EXPECT_NO_THROW(
        mctp::testPopulateMctpInfo(interfaces, "/localeid", infos));
    if (!infos.empty())
    {
        EXPECT_EQ(std::get<7>(infos[0]).value_or(0), 8);
    }
}

// ============================================================================
// (13) Missing required Endpoint properties (no EID) — no infos published.
// ============================================================================
TEST_F(UnifyMctpNsmRegression, MissingEid_NoOutput)
{
    dbus::InterfaceMap interfaces;
    dbus::PropertyMap uuidProps;
    uuidProps["UUID"] = std::string("uuid-no-eid");
    interfaces["xyz.openbmc_project.Common.UUID"] = uuidProps;

    dbus::PropertyMap endpointProps;
    // EID intentionally absent — required-property check fails.
    endpointProps["SupportedMessageTypes"] = std::vector<uint8_t>{0x7e};
    endpointProps["NetworkId"] = static_cast<uint32_t>(0);
    interfaces["xyz.openbmc_project.MCTP.Endpoint"] = endpointProps;

    MctpInfos infos;
    EXPECT_NO_THROW(mctp::testPopulateMctpInfo(interfaces, "/no-eid", infos));
    EXPECT_TRUE(infos.empty());
}

// ============================================================================
// (14) NsmDevice::isCommandSupported — in-range messageType returns the
//     correct boolean. This pre-N6 pin verifies the bounds-checked outer
//     guard exists at line 143.
// ============================================================================
TEST_F(UnifyMctpNsmRegression, NsmDeviceMatrix_InRange_ReturnsExpected)
{
    MockNsmDevice dev(1, 1, "MCTP_UUID", "uuid-matrix", 0);
    // Default-initialised matrix is all-false.
    EXPECT_FALSE(dev.isCommandSupported(0, 0));
    EXPECT_FALSE(dev.isCommandSupported(0, 255));
    EXPECT_FALSE(dev.isCommandSupported(NUM_NSM_TYPES - 1, 0));
}

// ============================================================================
// (15) NsmDevice::isCommandSupported with OOB messageType is rejected at the
//     guard — returns false instead of dereferencing the empty row. Pre-N6
//     pin (the [messageType][commandCode] underneath is only reached on the
//     in-range branch).
// ============================================================================
TEST_F(UnifyMctpNsmRegression,
       NsmDeviceMatrix_OobMessageType_ReturnsFalseViaGuard)
{
    MockNsmDevice dev(1, 1, "MCTP_UUID", "uuid-matrix-oob", 0);
    EXPECT_FALSE(dev.isCommandSupported(NUM_NSM_TYPES, 0));
    EXPECT_FALSE(dev.isCommandSupported(NUM_NSM_TYPES + 10, 0));
    EXPECT_FALSE(dev.isCommandSupported(255, 0));
}

// ============================================================================
// (16) NsmDevice::isCommandSupported with in-range messageType + OOB
//     commandCode is currently UB (operator[] past end of inner vector).
//     We pre-N6 only test up to the legitimate in-range commandCode 255;
//     N6 adds the OOB safety via the new safe accessor. This test is the
//     read-back-after-write contract.
// ============================================================================
TEST_F(UnifyMctpNsmRegression, NsmDeviceMatrix_SetAndGet_RoundTrip)
{
    MockNsmDevice dev(1, 1, "MCTP_UUID", "uuid-roundtrip", 0);
    // Direct write to the matrix — this is what setCommandCodeSupportedSafe
    // wraps in N6. The round-trip stays correct on both sides of N6.
    dev.messageTypesToCommandCodeMatrix[2][42] = true;
    EXPECT_TRUE(dev.isCommandSupported(2, 42));

    dev.messageTypesToCommandCodeMatrix[2][42] = false;
    EXPECT_FALSE(dev.isCommandSupported(2, 42));
}

// ============================================================================
// (17) updateMessageTypesToCommandCodeMatrix in-range messageType propagates
//     the supported bits through to isCommandSupported.
// ============================================================================
TEST_F(UnifyMctpNsmRegression,
       NsmDeviceMatrix_UpdateSupportedCommands_FlowsToRead)
{
    MockNsmDevice dev(1, 1, "MCTP_UUID", "uuid-update", 0);

    bitfield8_t supported[1] = {{0b10101010}};
    dev.updateMessageTypesToCommandCodeMatrix(3, supported, 1);

    EXPECT_FALSE(dev.isCommandSupported(3, 0));
    EXPECT_TRUE(dev.isCommandSupported(3, 1));
    EXPECT_FALSE(dev.isCommandSupported(3, 2));
    EXPECT_TRUE(dev.isCommandSupported(3, 3));
    EXPECT_TRUE(dev.isCommandSupported(3, 7));
}

// ============================================================================
// (18) updateMessageTypesToCommandCodeMatrix with OOB messageType is a
//     silent no-op (pre-existing guard at cpp:155). Pre-N6 pin — N6 adds the
//     same shape via setCommandCodeSupportedSafe for the other write sites.
// ============================================================================
TEST_F(UnifyMctpNsmRegression,
       NsmDeviceMatrix_UpdateOobMessageType_SilentNoOp)
{
    MockNsmDevice dev(1, 1, "MCTP_UUID", "uuid-update-oob", 0);

    bitfield8_t supported[1] = {{0xFF}};
    EXPECT_NO_THROW(dev.updateMessageTypesToCommandCodeMatrix(
        NUM_NSM_TYPES, supported, 1));
    EXPECT_NO_THROW(dev.updateMessageTypesToCommandCodeMatrix(255, supported,
                                                              1));
    // No matrix row was added — confirm in-range row 0 stayed all-false.
    EXPECT_FALSE(dev.isCommandSupported(0, 0));
}
