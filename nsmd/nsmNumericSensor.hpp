#pragma once

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

namespace nsm
{

using SensorUnit = sdbusplus::xyz::openbmc_project::Sensor::server::Value::Unit;
using ValueIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Sensor::server::Value>;
using AvailabilityIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Availability>;
using OperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;
using AssociationDefinitionsInft = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

class NsmNumericSensor
{
  public:
    NsmNumericSensor(sdbusplus::bus::bus& bus, const std::string& name,
                     const std::string& sensor_type, const SensorUnit unit,
                     const std::string& association);
    virtual ~NsmNumericSensor() = default;

    // also updates available and functional properties to true
    void updateReading(double value, uint64_t timestamp = 0);

    // doesn't affect sensor reading
    void updateStatus(bool available, bool functional);

  private:
    static const std::string valueInterface;
    static const std::string valueProperty;

  private:
    std::unique_ptr<ValueIntf> valueIntf = nullptr;
    std::unique_ptr<AvailabilityIntf> availabilityIntf = nullptr;
    std::unique_ptr<OperationalStatusIntf> operationalStatusIntf = nullptr;
    std::unique_ptr<AssociationDefinitionsInft> associationDefinitionsIntf =
        nullptr;

    const std::string objPath;
    const std::string association;
};

} // namespace nsm
