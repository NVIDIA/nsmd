#pragma once

#include "common/types.hpp"
#include "instance_id.hpp"
#include "nsmDevice.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdeventplus/event.hpp>

#include <map>
#include <queue>
#include <utility>
#include <variant>
#include <vector>

namespace nsm
{

using RequesterHandler = requester::Handler<requester::Request>;

/** @class DeviceManager
 *
 * The Manager class provides handle function for MctpDiscovery class to
 * discovery NSM device from the enumerated MCTP endpoints and then exposes the
 * retrieved FRU data from discoveryed NSM device to D-Bus FruDevice PDI
 */
class DeviceManager : public mctp::MctpDiscoveryHandlerIntf
{
  public:
    DeviceManager(
        sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::pair<eid_t, MctpMedium>>& eidTable) :
        event(event),
        handler(handler), instanceIdDb(instanceIdDb), objServer(objServer),
        eidTable(eidTable)
    {}

    void handleMctpEndpoints(const MctpInfos& mctpInfos)
    {
        discoverNsmDevice(mctpInfos);
    }

    requester::Coroutine SendRecvNsmMsg(eid_t eid, Request& request,
                                        const nsm_msg** responseMsg,
                                        size_t* responseLen);

  private:
    void discoverNsmDevice(const MctpInfos& mctpInfos);

    requester::Coroutine discoverNsmDeviceTask();

    requester::Coroutine ping(eid_t eid);
    requester::Coroutine
        getSupportedNvidiaMessageType(eid_t eid,
                                      std::vector<uint8_t>& supportedTypes);
    requester::Coroutine getFRU(eid_t eid,
                                nsm::InventoryProperties& properties);
    requester::Coroutine
        getInventoryInformation(eid_t eid, uint8_t& propertyIdentifier,
                                InventoryProperties& properties);

    requester::Coroutine
        getQueryDeviceIdentification(eid_t eid, uint8_t& deviceIdentification,
                                     uint8_t& instanceId);

    sdeventplus::Event& event;
    requester::Handler<requester::Request>& handler;
    nsm::InstanceIdDb& instanceIdDb;
    sdbusplus::asio::object_server& objServer;
    std::multimap<uuid_t, std::pair<eid_t, MctpMedium>>& eidTable;

    std::queue<MctpInfos> queuedMctpInfos;
    std::coroutine_handle<> discoverNsmDeviceTaskHandle;

    std::map<eid_t, std::shared_ptr<NsmDevice>> nsmDevices;
};

} // namespace nsm