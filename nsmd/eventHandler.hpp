#pragma once

#include "libnsm/base.h"

#include "common/types.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>

#include <cassert>
#include <functional>
#include <map>
#include <optional>
#include <vector>

using NsmEventId = uint8_t;

namespace nsm
{

using EventHandlerFunc =
    std::function<void(eid_t eid, const nsm_msg* event, size_t eventLen)>;
using Level = sdbusplus::xyz::openbmc_project::Logging::server::Entry::Level;

class EventHandler
{
  public:
    virtual ~EventHandler() = default;
    /** @brief Invoke a NSM event handler
     *
     *  @param[in] eid - source eid
     *  @param[in] eventId - event id
     *  @param[in] event - nsm event
     *  @param[in] eventLen - nsm event size
     */
    void handle(eid_t eid, NsmEventId eventId, const nsm_msg* event,
                size_t eventLen)
    {
        if (handlers.find(eventId) == handlers.end())
        {
            // unsupported event id
            lg2::info(
                "No event id{EVENTID} handler found for received NSM event from EID={EID}.",
                "EVENTID", eventId, "EID", eid);
            unsupportedEvent(eid, event, eventLen);
            return;
        }

        handlers.at(eventId)(eid, event, eventLen);
    }

    virtual void unsupportedEvent(uint8_t eid, const nsm_msg* event,
                                  size_t eventLen)
    {
        std::string messageId = "Received an unhandled NSM Event";

        auto createLog = [&messageId](
                             std::map<std::string, std::string>& addData,
                             Level& level) {
            static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
            static constexpr auto logInterface =
                "xyz.openbmc_project.Logging.Create";
            auto& bus = utils::DBusHandler::getBus();

            try
            {
                auto service =
                    utils::DBusHandler().getService(logObjPath, logInterface);
                auto severity = sdbusplus::xyz::openbmc_project::Logging::
                    server::convertForMessage(level);
                auto method = bus.new_method_call(service.c_str(), logObjPath,
                                                  logInterface, "Create");
                method.append(messageId, severity, addData);
                bus.call_noreply(method);
            }
            catch (const std::exception& e)
            {
                lg2::error(
                    "Failed to create D-Bus log entry for message registry, {ERROR}.",
                    "ERROR", e);
            }
        };

        uint8_t type = event->hdr.nvidia_msg_type;
        uint8_t eventId = event->payload[1];

        std::map<std::string, std::string> addData;
        addData["EID"] = std::to_string(eid);
        addData["TYPE"] = std::to_string(type);
        addData["ID"] = std::to_string(eventId);
        addData["LENGTH"] = std::to_string(eventLen);
        Level level = Level::Informational;
        createLog(addData, level);
    }

    virtual uint8_t nsmType() = 0;

  protected:
    /** @brief map of NSM event id to handler - to be populated by derived
     *         classes.
     */
    std::map<NsmEventId, EventHandlerFunc> handlers;
};

} // namespace nsm
