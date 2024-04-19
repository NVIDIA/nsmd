#pragma once

#include "common/types.hpp"
#include "nsmDevice.hpp"

namespace nsm
{

class SensorManager;

/**
 * @brief Gets NsmDevice from manager and adds device sensor to it.
 * Functions first read dbus property 'UUID' using given interface name and
 * and it sensor is not static (it is dynamic) it read dbus property
 * 'Priority' for the provided interface
 *
 * @param manager[in] Reference to sensor manager
 * @param deviceSensor[in] Pointer to device sensor
 * @param objPath[in] Dbus object path
 * @param interface[in] Interface name for finding UUID property
 */
void addDeviceSensor(SensorManager& manager,
                     const std::shared_ptr<NsmObject>& deviceSensor,
                     const std::string& objPath, const std::string& interface);
/**
 * @brief Gets NsmDevice from manager and adds sensor to it. Functions
 * first read dbus property 'UUID' using given interface name and and it
 * sensor is not static (it is dynamic) it read dbus property 'Priority' for
 * the provided interface
 *
 * @param manager[in] Reference to sensor manager
 * @param sensor[in] Pointer to sensor
 * @param objPath[in] Dbus object path
 * @param interface[in] Interface name for finding Priority property
 * @param uuidInterface[in] Interface name for finding UUID property
 * @param isStatic[in] true if Add static sensor to deviceSensors collection
 * @return NsmSensor& reference to added sensor
 */
[[maybe_unused]] NsmSensor&
    addSensor(SensorManager& manager, std::shared_ptr<NsmSensor> sensor,
              const std::string& objPath, const std::string& interface,
              const std::string& uuidInterface, bool isStatic = true);

} // namespace nsm