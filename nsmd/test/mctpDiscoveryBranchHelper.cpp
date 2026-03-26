/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Helper TU for mctpDiscoveryBranch_test.
 *
 * Including mctp_endpoint_discovery.hpp in the same TU as
 * sdbusplus::asio::connection(io) causes a runtime crash. This file
 * isolates all MctpDiscovery interactions in a separate TU and exposes
 * thin wrappers called from the main test file.
 */

// Include mctp_endpoint_discovery.hpp FIRST without #define private public
// to keep sdbusplus match_t internals intact.
#include "requester/mctp_endpoint_discovery.hpp"

// Now include the rest with private->public for NsmDevice access
#include "common/event.hpp"
#include "eventManager.hpp"
#include "instance_id.hpp"
#include "requester/handler.hpp"
#include "socket_handler.hpp"
#include "socket_manager.hpp"
#include "test/mockDBusHandler.hpp"

namespace mctp
{
extern std::unique_ptr<MctpDiscovery> mctpDiscoveryInstance;

void initMctpDiscoveryForTest(nsm::NsmDeviceTable& devices,
                              sdbusplus::asio::object_server& objServer)
{
    common::Event ev;
    nsm::InstanceIdDb idDb;
    mctp_socket::Manager sm;
    nsm::EventManager em;
    requester::Handler<requester::Request> rh{ev, idDb, sm, false};
    mctp_socket::InKernelHandler ikh{ev, rh, em, sm, false};
    EidTable et;
    auto& bus = utils::DBusHandler::getBus();
    MctpDiscovery::initialize(bus, ikh, nullptr, et, devices, objServer);
}

void resetMctpDiscoveryForTest()
{
    mctpDiscoveryInstance.reset();
}

MctpDiscovery& getMctpDiscoveryInstance()
{
    return MctpDiscovery::getInstance();
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

// handleMctpStateTransition, populateMctpInfo, discoveredEIDs, deviceMap
// are private — would need friend class or production API change to test.

void testLogProberSummaries()
{
    MctpDiscovery::logProberSummaries();
}

} // namespace mctp
