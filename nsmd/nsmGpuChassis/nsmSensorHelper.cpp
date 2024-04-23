#include "nsmSensorHelper.hpp"

#include "sensorManager.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

std::shared_ptr<NsmDevice> getNsmDevice(SensorManager& manager,
                                        const std::string& objPath,
                                        const std::string& interface)
{

    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    auto device = manager.getNsmDevice(uuid);
    if (!device)
    {
        lg2::error("Device not found for UUID: {UUID}", "UUID", uuid);
    }
    return device;
}

void addSensor(std::shared_ptr<NsmDevice>& device,
               const std::shared_ptr<NsmObject>& sensor)
{
    if (!device)
        return;
    device->deviceSensors.emplace_back(sensor);
}

void addSensor(std::shared_ptr<NsmDevice>& device,
               std::shared_ptr<NsmSensor> sensor, bool priority)
{
    if (priority)
    {
        device->prioritySensors.emplace_back(sensor);
    }
    else
    {
        device->roundRobinSensors.emplace_back(sensor);
    }
}
void addSensor(std::shared_ptr<NsmDevice>& device,
               std::shared_ptr<NsmSensor> sensor, const std::string& objPath,
               const std::string& interface)
{
    if (!device)
        return;
    auto priority = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());
    addSensor(device, sensor, priority);
}

void addSensor(SensorManager& manager, std::shared_ptr<NsmDevice>& device,
               std::shared_ptr<NsmSensor> sensor)
{
    if (!device)
        return;
    addSensor(device, sensor);
    sensor->update(manager, manager.getEid(device)).detach();
}

} // namespace nsm