#pragma once

#include "common/types.hpp"
#include "nsmDevice.hpp"

namespace nsm
{

class SensorManager;

/**
 * @brief Reads dbus property 'UUID' using given interface and object path
 *
 * @param manager[in] Reference to sensor manager
 * @param objPath[in] Dbus object path
 * @param interface[in] Interface name for finding UUID property
 * @return std::shared_ptr<NsmDevice>
 */
std::shared_ptr<NsmDevice> getNsmDevice(SensorManager& manager,
                                        const std::string& objPath,
                                        const std::string& interface);

/**
 * @brief Adds device/static sensor to to NsmDevice.
 *
 * @param device[in,out] Pointer to shared NsmDevice
 * @param sensor[in] Pointer to device/static sensor
 */
void addSensor(std::shared_ptr<NsmDevice>& device,
               const std::shared_ptr<NsmObject>& sensor);

/**
 * @brief Adds dynamic sensor to NsmDevice. It read dbus property 'Priority' for
 * the provided interface
 *
 * @param device[in,out] Pointer to shared NsmDevice
 * @param sensor[in] Pointer to dynamic sensor
 * @param priority[in] Flag to add sensor as priority sensor
 */
void addSensor(std::shared_ptr<NsmDevice>& device,
               std::shared_ptr<NsmSensor> sensor, bool priority);

/**
 * @brief Adds dynamic sensor to NsmDevice. It read dbus property 'Priority' for
 * the provided interface
 *
 * @param device[in,out] Pointer to shared NsmDevice
 * @param sensor[in] Pointer to dynamic sensor
 * @param objPath[in] Dbus object path
 * @param interface[in] Interface name for finding Priority property
 */
void addSensor(std::shared_ptr<NsmDevice>& device,
               std::shared_ptr<NsmSensor> sensor, const std::string& objPath,
               const std::string& interface);

/**
 * @brief Adds static sensor to NsmDevice. Function calls update
 * method of the sensor and detach its co_await call
 *
 * @param manager[in,out] Reference to sensor manager
 * @param device[in,out] Pointer to shared NsmDevice
 * @param sensor[in] Pointer to static sensor
 */
void addSensor(SensorManager& manager, std::shared_ptr<NsmDevice>& device,
               std::shared_ptr<NsmSensor> sensor);

} // namespace nsm