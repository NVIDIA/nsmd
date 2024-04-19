#include "nsmSensorHelper.hpp"

#include "sensorManager.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

/**
 * @brief Gets the NsmDevice object from manager using dbus object path and
 * interface
 *
 * @param manager[in] Reference to sensor manager
 * @param objPath[in] Dbus object path
 * @param interface[in] Interface name for finding UUID
 * @return std::shared_ptr<NsmDevice>
 */
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

void addDeviceSensor(SensorManager& manager,
                     const std::shared_ptr<NsmObject>& deviceSensor,
                     const std::string& objPath, const std::string& interface)
{

    auto device = getNsmDevice(manager, objPath, interface);
    if (device)
    {
        device->deviceSensors.emplace_back(deviceSensor);
    }
}

NsmSensor& addSensor(SensorManager& manager, std::shared_ptr<NsmSensor> sensor,
                     const std::string& objPath, const std::string& interface,
                     const std::string& uuidInterface, bool isStatic)
{
    auto device = getNsmDevice(manager, objPath, uuidInterface);
    if (!device)
    {
        return *sensor;
    }

    if (isStatic)
    {
        device->deviceSensors.emplace_back(sensor);
        sensor->update(manager, manager.getEid(device)).detach();
    }
    else
    {
        auto isPriority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        if (isPriority)
        {
            device->prioritySensors.emplace_back(sensor);
        }
        else
        {
            device->roundRobinSensors.emplace_back(sensor);
        }
    }
    return *sensor;
}

} // namespace nsm