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
 * The Manager class provides a handle function for MctpDiscovery class to
 * discover NSM devices from the enumerated MCTP endpoints and then exposes the
 * retrieved FRU data from discovered NSM devices to D-Bus FruDevice PDI.
 */
class DeviceManager : public mctp::MctpDiscoveryHandlerIntf
{
  public:
    // Singleton access method that simply returns the instance
    static DeviceManager& getInstance()
    {
        if (!instance)
        {
            throw std::runtime_error(
                "DeviceManager instance is not initialized yet");
        }
        return *instance;
    }

    // Initialization method to create and setup the singleton instance
    static void initialize(
        sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        NsmDeviceTable& nsmDevices)
    {
        static DeviceManager inst(event, handler, instanceIdDb, objServer,
                                  eidTable, nsmDevices);
        instance = &inst;
    }

    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;
    DeviceManager(DeviceManager&&) = delete;
    DeviceManager& operator=(DeviceManager&&) = delete;

    void handleMctpEndpoints(const MctpInfos& mctpInfos)
    {
        discoverNsmDevice(mctpInfos);
    }

    requester::Coroutine SendRecvNsmMsg(eid_t eid, Request& request,
                                        const nsm_msg** responseMsg,
                                        size_t* responseLen);

    const std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
        getEidTable() const
    {
        return eidTable;
    }

    const NsmDeviceTable& getNsmDevices() const
    {
        return nsmDevices;
    }

  private:
    DeviceManager(
        sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        NsmDeviceTable& nsmDevices) :
        event(event),
        handler(handler), instanceIdDb(instanceIdDb), objServer(objServer),
        eidTable(eidTable), nsmDevices(nsmDevices)
    {}

    void discoverNsmDevice(const MctpInfos& mctpInfos);

    requester::Coroutine discoverNsmDeviceTask();
    static DeviceManager* instance;

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
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>& eidTable;

    std::queue<MctpInfos> queuedMctpInfos;
    std::coroutine_handle<> discoverNsmDeviceTaskHandle;
    NsmDeviceTable& nsmDevices;
};
} // namespace nsm
