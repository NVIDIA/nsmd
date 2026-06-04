/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Helper TU for mctpDiscoveryBranch2_test.
 *
 * Uses MctpDiscoveryTestAccess (friend of MctpDiscovery) to access private
 * members. TestableMctpDiscovery overrides init() as no-op so no real
 * D-Bus or match_t is needed.
 */

#include "common/event.hpp"
#include "eventManager.hpp"
#include "instance_id.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "socket_handler.hpp"
#include "socket_manager.hpp"
#include "test/mockDBusHandler.hpp"

#include <sdbusplus/test/sdbus_mock.hpp>

namespace mctp
{
extern std::unique_ptr<MctpDiscovery> mctpDiscoveryInstance;

class TestableMctpDiscovery : public MctpDiscovery
{
  public:
    TestableMctpDiscovery(sdbusplus::bus_t& bus, mctp_socket::Handler& handler,
                          std::shared_ptr<nsm::NsmMessageHandler> nsmMsgHandler,
                          EidTable& eidTable, nsm::NsmDeviceTable& nsmDevices,
                          sdbusplus::asio::object_server& objServer) :
        MctpDiscovery(bus, handler, nsmMsgHandler, eidTable, nsmDevices,
                      objServer)
    {}
    void init() override {}
};

namespace
{
common::Event stubEvent;
nsm::InstanceIdDb stubIdDb;
mctp_socket::Manager stubSm;
nsm::EventManager stubEm;
requester::Handler<requester::Request> stubRh{stubEvent, stubIdDb, stubSm,
                                              false};
mctp_socket::InKernelHandler stubIkh{stubEvent, stubRh, stubEm, stubSm, false};

// SdBusMock provides a gmock sd_bus interface that doesn't need a real D-Bus.
sdbusplus::SdBusMock sdbusMock;
} // namespace

void initForTest(EidTable& eidTable, nsm::NsmDeviceTable& nsmDevices)
{
    if (mctpDiscoveryInstance)
    {
        throw std::logic_error(
            "Initialize called on an already initialized MctpDiscovery");
    }
    static auto bus = sdbusplus::bus_t(nullptr, &sdbusMock);

    alignas(sdbusplus::asio::object_server) static char
        fakeBuf[sizeof(sdbusplus::asio::object_server)]{};
    auto& objServer =
        reinterpret_cast<sdbusplus::asio::object_server&>(fakeBuf);

    mctpDiscoveryInstance = std::unique_ptr<MctpDiscovery>(
        new TestableMctpDiscovery(bus, stubIkh, nullptr, eidTable, nsmDevices,
                                  objServer));
}

void resetForTest()
{
    mctpDiscoveryInstance.reset();
}

void testLogProberSummaries()
{
    MctpDiscovery::logProberSummaries();
}

std::shared_ptr<nsm::NsmDevice> testGetNsmDeviceFromEid(eid_t eid)
{
    return MctpDiscovery::getInstance().getNsmDeviceFromEid(eid);
}

std::shared_ptr<nsm::NsmDevice> testGetNsmDeviceFromStaticUUID(uuid_t uuid)
{
    return MctpDiscovery::getInstance().getNsmDeviceFromStaticUUID(uuid);
}

std::shared_ptr<nsm::NsmDevice>
    testGetNsmDeviceByIdentification(uint8_t dt, uint8_t inst, uint8_t role)
{
    return MctpDiscovery::getInstance().getNsmDeviceByIdentification(dt, inst,
                                                                     role);
}

nsm::DiscoveryEvents& testDiscoveryEvents(eid_t eid)
{
    return MctpDiscovery::getInstance().discoveryEvents(eid);
}

// ============================================================================
// MctpDiscoveryTestAccess — friend class for accessing private members
// ============================================================================

class MctpDiscoveryTestAccess
{
  public:
    static auto& inst()
    {
        return *mctpDiscoveryInstance;
    }

    static void populateMctpInfo(const dbus::InterfaceMap& interfaces,
                                 const std::string& objPath,
                                 MctpInfos& mctpInfos)
    {
        inst().populateMctpInfo(interfaces, objPath, mctpInfos);
    }

    static bool insertIntoEidTableifNotExist(
        uuid_t uuid, const std::tuple<eid_t, MctpMedium, MctpBinding>& value)
    {
        return inst().insertIntoEidTableifNotExist(uuid, value);
    }

    static void handleMctpStateTransition(const std::string& objPath)
    {
        inst().handleMctpStateTransition(objPath);
    }

    static void handleMctpEndpoints(const MctpInfos& mctpInfos)
    {
        inst().handleMctpEndpoints(mctpInfos);
    }

    static std::shared_ptr<nsm::NsmDevice> mapNsmDeviceUsingEid(
        eid_t eid, uuid_t mctpUuid, uint8_t deviceType, uint8_t instanceNumber,
        std::string associatedPath, bool active, MctpMedium mctpMedium,
        MctpBinding mctpBinding, std::optional<eid_t> localEid = std::nullopt)
    {
        return inst().mapNsmDeviceUsingEid(
            eid, mctpUuid, deviceType, instanceNumber, associatedPath, active,
            mctpMedium, mctpBinding, localEid);
    }

    static int mapMctpEIDForNsmDevice(std::shared_ptr<nsm::NsmDevice> nsmDevice)
    {
        return inst().mapMctpEIDForNsmDevice(nsmDevice);
    }

    static DiscoveredEIDs& getDiscoveredEIDs()
    {
        return inst().discoveredEIDs;
    }

    static std::map<uint8_t,
                    std::map<uint16_t, std::shared_ptr<nsm::NsmDevice>>>&
        getDeviceMap()
    {
        return inst().deviceMap;
    }

    static std::map<std::string, MctpInfo>& getCachedMctpInfoByPath()
    {
        return inst().cachedMctpInfoByPath;
    }

    static EidTable& getEidTable()
    {
        return inst().eidTable;
    }

    static nsm::NsmDeviceTable& getNsmDevices()
    {
        return inst().nsmDevices;
    }

    // unify-mctp Commit 1 (N1) — resolvedMctpServices accessor
    static std::set<std::string>& getResolvedMctpServices()
    {
        return inst().resolvedMctpServices;
    }

    // unify-mctp Commit 2 (N2) — initEnumerateTaskHandle accessor
    static std::coroutine_handle<>& getInitEnumerateTaskHandle()
    {
        return inst().initEnumerateTaskHandle;
    }
};

// ============================================================================
// Thin wrapper functions called from test TU
// ============================================================================

void testPopulateMctpInfo(const dbus::InterfaceMap& interfaces,
                          const std::string& objPath, MctpInfos& mctpInfos)
{
    MctpDiscoveryTestAccess::populateMctpInfo(interfaces, objPath, mctpInfos);
}

bool testInsertIntoEidTableifNotExist(
    uuid_t uuid, const std::tuple<eid_t, MctpMedium, MctpBinding>& value)
{
    return MctpDiscoveryTestAccess::insertIntoEidTableifNotExist(uuid, value);
}

void testHandleMctpStateTransition(const std::string& objPath)
{
    MctpDiscoveryTestAccess::handleMctpStateTransition(objPath);
}

void testHandleMctpEndpoints(const MctpInfos& mctpInfos)
{
    MctpDiscoveryTestAccess::handleMctpEndpoints(mctpInfos);
}

std::shared_ptr<nsm::NsmDevice> testMapNsmDeviceUsingEid(
    eid_t eid, uuid_t mctpUuid, uint8_t deviceType, uint8_t instanceNumber,
    std::string associatedPath, bool active, MctpMedium mctpMedium,
    MctpBinding mctpBinding, std::optional<eid_t> localEid)
{
    return MctpDiscoveryTestAccess::mapNsmDeviceUsingEid(
        eid, mctpUuid, deviceType, instanceNumber, associatedPath, active,
        mctpMedium, mctpBinding, localEid);
}

int testMapMctpEIDForNsmDevice(std::shared_ptr<nsm::NsmDevice> nsmDevice)
{
    return MctpDiscoveryTestAccess::mapMctpEIDForNsmDevice(nsmDevice);
}

DiscoveredEIDs& testGetDiscoveredEIDs()
{
    return MctpDiscoveryTestAccess::getDiscoveredEIDs();
}

std::map<uint8_t, std::map<uint16_t, std::shared_ptr<nsm::NsmDevice>>>&
    testGetDeviceMap()
{
    return MctpDiscoveryTestAccess::getDeviceMap();
}

std::map<std::string, MctpInfo>& testGetCachedMctpInfoByPath()
{
    return MctpDiscoveryTestAccess::getCachedMctpInfoByPath();
}

EidTable& testGetEidTable()
{
    return MctpDiscoveryTestAccess::getEidTable();
}

nsm::NsmDeviceTable& testGetNsmDevices()
{
    return MctpDiscoveryTestAccess::getNsmDevices();
}

std::set<std::string>& testGetResolvedMctpServices()
{
    return MctpDiscoveryTestAccess::getResolvedMctpServices();
}

std::coroutine_handle<>& testGetInitEnumerateTaskHandle()
{
    return MctpDiscoveryTestAccess::getInitEnumerateTaskHandle();
}

} // namespace mctp
