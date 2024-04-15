#pragma once

#include "device-capability-discovery.h"

#include "deviceManager.hpp"
#include "eventHandler.hpp"
#include "nsmDevice.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <memory>

namespace nsm
{

class EventType0Handler : public EventHandler
{
  public:
    // Member to hold reference to DeviceManager
    DeviceManager& deviceManager;
    SensorManager& sensorManager;
    EventType0Handler() :
        deviceManager(DeviceManager::getInstance()),
        sensorManager(SensorManager::getInstance())
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
        // update sensors for capabilities refresh
        // Get UUID from EID
        auto uuidOptional =
            utils::getUUIDFromEID(deviceManager.getEidTable(), eid);
        if (uuidOptional)
        {
            std::string uuid = *uuidOptional;
            // findNSMDevice instance for that eid
            lg2::info("rediscovery event : UUID found: {UUID}", "UUID", uuid);

            std::shared_ptr<NsmDevice> nsmDevice{};
            auto nsmDevices = deviceManager.getNsmDevices();
            for (auto device : nsmDevices)
            {
                if ((device->uuid).substr(0, UUID_LEN) ==
                    uuid.substr(0, UUID_LEN))
                {
                    nsmDevice = device;
                    break;
                }
            }

            if (nsmDevice)
            {
                lg2::info(
                    "Rediscovery event : The NSM device has been discovered for , uuid={UUID}",
                    "UUID", uuid);
                auto& sensors = nsmDevice->staticSensors;
                for (auto& sensor : sensors)
                {
                    sensor->update(sensorManager, eid).detach();
                }
            }
            else
            {
                lg2::error(
                    "Rediscovery event : The NSM device has not been discovered for , uuid={UUID}",
                    "UUID", uuid);
            }
        }
        else
        {
            lg2::error("Rediscovery event : No UUID found for EID {EID}", "EID",
                       eid);
        }

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