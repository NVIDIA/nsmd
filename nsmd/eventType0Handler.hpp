#pragma once

#include "device-capability-discovery.h"

#include "eventHandler.hpp"

namespace nsm
{

class EventType0Handler : public EventHandler
{
  public:
    EventType0Handler()
    {
        handlers.emplace(
            NSM_REDISCOVERY_EVENT,
            [this](eid_t eid, const nsm_msg* event, size_t eventLen) {
                return this->rediscovery(eid, event, eventLen);
            });
    };

    virtual uint8_t nsmType() override
    {
        // event type0, device capability discovery
        return NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    }

    /** @brief Handler for rediscovery event
     *
     *  @param[in] event - Request message
     *  @param[in] eventLen - Request message length
     */
    void rediscovery(uint8_t eid, const nsm_msg* event, size_t eventLen)
    {
        std::string messageId = "Rediscovery";

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

        uint8_t eventClass = 0;
        uint16_t eventState = 0;
        auto rc = decode_nsm_rediscovery_event(event, eventLen, &eventClass,
                                               &eventState);
        if (rc != NSM_SUCCESS)
        {
            lg2::error("Failed to decode rediscovery event {RC}.", "RC", rc);
            return;
        }

        std::map<std::string, std::string> addData;
        addData["EID"] = std::to_string(eid);
        addData["CLASS"] = std::to_string(eventClass);
        addData["STATE"] = std::to_string(eventState);
        Level level = Level::Informational;
        createLog(addData, level);
    }
};

} // namespace nsm