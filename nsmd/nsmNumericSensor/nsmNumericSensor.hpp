#pragma once

#include "nsmSensor.hpp"

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>
#include <xyz/openbmc_project/Time/EpochTime/server.hpp>

namespace utils
{
struct Association;
}

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
using TimestampIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Time::server::EpochTime>;

class NsmNumericSensorValue;

class NsmNumericSensor : public NsmSensor
{
  public:
    NsmNumericSensor(const std::string& name, const std::string& type,
                     uint8_t sensorId,
                     std::shared_ptr<NsmNumericSensorValue> sensorValue) :
        NsmSensor(name, type),
        sensorId(sensorId), sensorValue(sensorValue){};

    std::shared_ptr<NsmNumericSensorValue> getSensorValueObject()
    {
        return sensorValue;
    }

  protected:
    uint8_t sensorId;
    std::shared_ptr<NsmNumericSensorValue> sensorValue;
};

/** @class NsmNumericSensorValue
 *
 * Abstract class for Observers of Numeric sensor reading and timestamp.
 * If you want to observe a sensor reading (for example to compute total power
 * output), derive publically from this class and add it to the sensorValue
 * member data of Numeric sensor classes (NsmPower, NsmVoltage, etc).
 */
class NsmNumericSensorValue
{
  public:
    virtual ~NsmNumericSensorValue() = default;
    virtual void updateReading(double value, uint64_t timestamp = 0) = 0;
};

class NsmNumericSensorDbusValue : public NsmNumericSensorValue
{
  public:
    NsmNumericSensorDbusValue(
        sdbusplus::bus::bus& bus, const std::string& name,
        const std::string& sensor_type, const SensorUnit unit,
        const std::vector<utils::Association>& association);
    void updateReading(double value, uint64_t timestamp = 0) override;

  private:
    ValueIntf valueIntf;
    AssociationDefinitionsInft associationDefinitionsIntf;
};

class NsmNumericSensorDbusValueTimestamp : public NsmNumericSensorDbusValue
{
  public:
    NsmNumericSensorDbusValueTimestamp(
        sdbusplus::bus::bus& bus, const std::string& name,
        const std::string& sensor_type, const SensorUnit unit,
        const std::vector<utils::Association>& association);
    void updateReading(double value, uint64_t timestamp = 0) final;

  private:
    TimestampIntf timestampIntf;
};

class NsmNumericSensorValueAggregate : public NsmNumericSensorValue
{
  public:
    template <typename... Args>
    NsmNumericSensorValueAggregate(Args&&... args)
    {
        (objects.emplace_back(std::forward<Args>(args)), ...);
    }

    void updateReading(double value, uint64_t timestamp = 0) final;

  private:
    std::vector<std::unique_ptr<NsmNumericSensorValue>> objects;
};

class NsmNumericSensorStatus
{
  public:
    virtual ~NsmNumericSensorStatus() = default;
    virtual void updateStatus(bool available, bool functional) = 0;
};

class NsmNumericSensorDbusStatus : public NsmNumericSensorStatus
{
  public:
    NsmNumericSensorDbusStatus(sdbusplus::bus::bus& bus,
                               const std::string& name,
                               const std::string& sensor_type);
    void updateStatus(bool available, bool functional) final;

  private:
    AvailabilityIntf availabilityIntf;
    OperationalStatusIntf operationalStatusIntf;
};

class NsmNumericSensorShmem : public NsmNumericSensorValue
{
  public:
    NsmNumericSensorShmem(const std::string& name,
                          const std::string& sensor_type,
                          const std::string& association);
    void updateReading(double value, uint64_t timestamp = 0) final;

  private:
    static const std::string valueInterface;
    static const std::string valueProperty;

    const std::string objPath;
    const std::string association;
};

} // namespace nsm
