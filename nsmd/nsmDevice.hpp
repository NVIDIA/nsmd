/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "base.h"
#include "device-capability-discovery.h"
#include "platform-environmental.h"

#include "circularQueue.hpp"
#include "coroutineSemaphore.hpp"
#include "instance_id.hpp"
#include "nsmEvent.hpp"
#include "nsmGroupSensor.hpp"
#include "nsmInterface.hpp"
#include "nsmMsghandler.hpp"
#include "nsmObject.hpp"
#include "nsmSensor.hpp"
#include "requester/handler.hpp"
#include "sleep.hpp"
#include "stateChangeLogger.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/timer.hpp>

#include <coroutine>
#include <deque>
#include <map>
#include <optional>
#include <ranges> // For ranges::find_if
#include <set>
#include <string>
#include <string_view>
#include <vector>

#define MAX_SENSOR_UPDATE_BATCH_SIZE 10

namespace nsm
{

class NsmGPUSWInventoryDriverVersionAndStatus;
class SensorManager;
class NsmNumericAggregator;
class NsmDevice;
class NsmLongRunningEvent;
class ProgressCounters;
class DiscoveryEvents;
using NsmDeviceTable = std::vector<std::shared_ptr<NsmDevice>>;
using SensorQueue = CircularQueue<std::shared_ptr<NsmObject>>;

struct ActiveLongRunningHandlerInfo
{
    uint8_t messageType;
    uint8_t commandCode;
    std::shared_ptr<NsmLongRunningEvent> sensorInstance;
};

enum class PollingType
{
    Priority,                 // Priority polling [150ms] for priority sensors
    GpuPerformanceMonitoring, // Gpu Performance Monitoring [1000ms]
    LongRunning,              // Long running polling for long running sensors
    Static,                   // One time polling for static sensors
    RoundRobin,               // Round Robing polling for non-priority sensors
};

enum class PollingState
{
    Idle,
    PollingPriority,
    PollingNonPriority,
};

struct FruInterfaceManager
{
    std::unique_ptr<sdbusplus::asio::dbus_interface> interface;
    std::set<std::string> supportedProperties;
    bool initialized = false;

    // Public method declarations only
    bool needsRecreation(const std::set<std::string>& newProperties) const;
    void markInitialized(const std::set<std::string>& properties);
    bool isPropertySupported(const std::string& propertyName) const;
    void reset();
    void createAndRegisterInterface(
        std::shared_ptr<sdbusplus::asio::object_server> objServer, uint8_t eid,
        std::shared_ptr<NsmDevice> nsmDevice,
        const InventoryProperties& properties);
    void updateAllPropertyValues(std::shared_ptr<NsmDevice> nsmDevice,
                                 const InventoryProperties& properties);

  private:
    void registerAllProperties(std::shared_ptr<NsmDevice> nsmDevice,
                               const InventoryProperties& properties);
};

enum class DeviceRemapProperty
{
    MCTP_EID,
    MCTP_UUID,
    NSM_DEVICE_INSTANCE_NUMBER,
    MCTP_ASSOCIATION
};

class NsmDevice :
    public StateChangeLogger,
    public std::enable_shared_from_this<NsmDevice>
{
  protected:
    NsmDevice() = default;

  public:
    virtual ~NsmDevice() = default;
    /**
     * @brief Constructor for NsmDevice
     *
     * @param objServer - Pointer to sdbusplus::asio::object_server
     * @param nsmMsgHandler - Pointer to NsmMessageHandler
     * @param deviceType - Device type
     * @param instanceNumber - Device instance number
     * @param remapPropName - Remap property name
     * @param remapPropValue - Remap property value
     * @param deviceRole - Device role
     */

    NsmDevice(std::shared_ptr<sdbusplus::asio::object_server> objServer,
              std::shared_ptr<NsmMessageHandler> nsmMsgHandler,
              uint8_t deviceType, uint8_t instanceNumber,
              std::string remapPropName,
              std::vector<std::string> remapPropValues,
              uint8_t deviceRole = NSM_DEV_ROLE_RESERVED) :
        messageTypesToCommandCodeMatrix(
            NUM_NSM_TYPES, std::vector<bool>(NUM_COMMAND_CODES, false)),
        eventMode(GLOBAL_EVENT_GENERATION_DISABLE), isDeviceActive(false),
        deviceType(deviceType), instanceNumber(instanceNumber),
        deviceRole(deviceRole), nsmMsgHandler(nsmMsgHandler),
        objServer(objServer)
    {
        registerLongRunningEventHandler();
        if (remapPropName == "NSM_DEVICE_INSTANCE_NUMBER")
        {
            deviceRemapProp = DeviceRemapProperty::NSM_DEVICE_INSTANCE_NUMBER;
            std::vector<uint8_t> values;
            for (const auto& remapPropValue : remapPropValues)
            {
                try
                {
                    values.emplace_back(
                        static_cast<uint8_t>(std::stoi(remapPropValue)));
                }
                catch (const std::invalid_argument& e)
                {
                    LG2_ERROR(
                        "Got Error E : {E}, while converting Remapping Property Value: {REMAP_PROP_VALUE} to NsmDeviceInstanceNumber",
                        "E", e.what(), "REMAP_PROP_VALUE", remapPropValue);
                };
            }
            deviceRemapValues = values;
        }
        else if (remapPropName == "MCTP_EID")
        {
            deviceRemapProp = DeviceRemapProperty::MCTP_EID;
            std::vector<eid_t> values;
            for (const auto& remapPropValue : remapPropValues)
            {
                try
                {
                    values.emplace_back(
                        static_cast<eid_t>(std::stoi(remapPropValue)));
                }
                catch (const std::invalid_argument& e)
                {
                    LG2_ERROR(
                        "Got Error E : {E}, while converting Reampping Property Value: {REMAP_PROP_VALUE} to EID",
                        "E", e.what(), "REMAP_PROP_VALUE", remapPropValue);
                };
            }
            deviceRemapValues = values;
        }
        else if (remapPropName == "MCTP_UUID")
        {
            deviceRemapProp = DeviceRemapProperty::MCTP_UUID;
            std::vector<uuid_t> values;
            for (const auto& remapPropValue : remapPropValues)
            {
                values.emplace_back(remapPropValue);
            }
            deviceRemapValues = values;
        }

        else if (remapPropName == "MCTP_ASSOCIATION")
        {
            deviceRemapProp = DeviceRemapProperty::MCTP_ASSOCIATION;
            std::vector<std::string> values;
            for (const auto& remapPropValue : remapPropValues)
            {
                values.emplace_back(remapPropValue);
            }
            deviceRemapValues = values;
        }
        else
        {
            LG2_ERROR("Invalid Reampping Property: {REMAP_PROP_NAME} provided",
                      "REMAP_PROP_NAME", remapPropName);
        }
        discoveryPending = false;
        isDeviceActive = false;
        initMsgTypesSensor();
    }

    uuid_t deviceUuid;
    requester::Coroutine task, longRunningTask;

    std::shared_ptr<NsmNumericAggregator>
        findAggregatorByType(const std::string& type);

    void setEventMode(uint8_t mode);
    uint8_t getEventMode();
    bool allCommandCodesAreRetrieved();
    bool isCommandSupported(uint8_t messageType, uint8_t commandCode);
    void updateMessageTypesToCommandCodeMatrix(
        uint8_t messageType, const bitfield8_t supportedCommands[],
        size_t supportedCommandsSize);
    void addDeviceEvent(std::shared_ptr<NsmEvent> event, NsmType type,
                        NsmEventId eventId)
    {
        deviceEvents.push_back(event);
        eventDispatcher.addEvent(type, eventId, event);
    }

    /**
     * @brief Inserts device/static sensor to to NsmDevice.
     *
     * @tparam SensorType Final derived sensor type
     * @param sensor[in,out] Pointer to device/static sensor
     */
    template <typename SensorType>
    void addStaticSensor(SensorType sensor)
    //    [[deprecated("Use addSensor(sensor, PollingType::Static) instead")]]
    {
        addSensor(sensor, PollingType::Static);
    }

    /**
     * @brief Inserts dynamic sensor to NsmDevice. If sensor of same type is
     * already added, it will add the interfaces to the existing sensor instead
     * of adding duplicated sensor
     *
     * @tparam SensorType Final derived sensor type
     * @param sensor[in,out] Pointer to dynamic sensor
     * @param priority[in] Flag to add sensor as priority sensor
     * @param isLongRunning[in] Flag to add sensor as Long Running sensor
     */
    template <typename SensorType>
    void addSensor(SensorType sensor, bool priority, bool isLongRunning = false)
    //    [[deprecated("Use addSensor(sensor, pollingType) instead")]]
    {
        addSensor(sensor, isLongRunning ? PollingType::LongRunning
                                        : (priority ? PollingType::Priority
                                                    : PollingType::RoundRobin));
    }

    /**
     * @brief Inserts dynamic sensor to NsmDevice. If sensor of same type is
     * already added, it will add the interfaces to the existing sensor instead
     * of adding duplicated sensor
     *
     * @tparam SensorType Final derived sensor type
     * @param sensor[in] Pointer to dynamic sensor
     * @param pollingType[in] Pooling type for the sensor
     */
    template <typename SensorType>
    void addSensor(const std::shared_ptr<SensorType>& sensor,
                   PollingType pollingType)
    {
        auto sensorCopy = sensor;
        addSensor(sensorCopy, pollingType);
    }

    /**
     * @brief Inserts dynamic sensor to NsmDevice. If sensor of same type is
     * already added, it will add the interfaces to the existing sensor instead
     * of adding duplicated sensor
     *
     * @tparam SensorType Final derived sensor type
     * @param sensor[in,out] Pointer to dynamic sensor
     * @param pollingType[in] Pooling type for the sensor
     */
    template <typename SensorType>
    void addSensor(std::shared_ptr<SensorType>& sensor, PollingType pollingType)
        requires std::is_base_of_v<NsmSensor, SensorType> &&
                 std::is_base_of_v<
                     NsmInterfaces<typename SensorType::IntfType_t>, SensorType>
    {
        auto it = std::ranges::find_if(
            deviceSensors, [&sensor](const std::shared_ptr<NsmObject>& object) {
            auto objectAsSensor = std::dynamic_pointer_cast<SensorType>(object);
            // find object that can be casted to same type and NsmSensor
            // requests are equal
            return objectAsSensor && *sensor == *objectAsSensor;
        });
        if (it != deviceSensors.end())
        {
            // SensorType must be derived from
            // NsmInterfaces<InterfaceType>
            auto existingSensor = std::static_pointer_cast<SensorType>(*it);
            if constexpr (std::is_base_of_v<NsmGroupSensor, SensorType>)
            {
                existingSensor->sensors.emplace_back(sensor);
                // clang-format off
                lg2::info(
                    "Sensor {NAME} ({TYPE}, {SENSOR_TYPE}, {INTF_TYPE}) already exists. "
                    "Grouped it into the existing sensor {EXISTING_NAME} ({EXISTING_TYPE}). "
                    "It now contains {COUNT} subsensors.",
                    "NAME", sensor->NsmObject::getName(), 
                    "TYPE", sensor->NsmObject::getType(), 
                    "SENSOR_TYPE", utils::typeName<SensorType>(),
                    "INTF_TYPE", SensorType::interface(), 
                    "EXISTING_NAME", existingSensor->NsmObject::getName(), 
                    "EXISTING_TYPE", existingSensor->NsmObject::getType(), 
                    "COUNT", (existingSensor->sensors.size() + 1));
                // clang-format on
            }
            else if (existingSensor->moveInterfaces(*sensor))
            {
                // clang-format off
                lg2::info(
                    "Sensor {NAME} ({TYPE}, {SENSOR_TYPE}, {INTF_TYPE}) already exists. "
                    "Merged its PDIs into the existing sensor {EXISTING_NAME} ({EXISTING_TYPE}). "
                    "It now contains {COUNT} PDIs.",
                    "NAME", sensor->NsmObject::getName(), 
                    "TYPE", sensor->NsmObject::getType(), 
                    "SENSOR_TYPE", utils::typeName<SensorType>(),
                    "INTF_TYPE", SensorType::interface(), 
                    "EXISTING_NAME", existingSensor->NsmObject::getName(), 
                    "EXISTING_TYPE", existingSensor->NsmObject::getType(), 
                    "COUNT", existingSensor->size());
                // clang-format on
            }
            // updating sensor with existing sensor
            sensor = existingSensor;
        }
        else
        {
            // sensors was not added to deviceSensors
            addSensorBase(sensor, pollingType);
        }
    }

    /**
     * @brief Adds dynamic sensor to NsmDevice. If sensor of same type is
     * already added, it will add the interfaces to the existing sensor instead
     * of adding duplicated sensor. This is fallback method for
     * addSensor(sensor, PollingType) with `requires` clause.
     *
     * @tparam SensorType Final derived sensor type
     * @param sensor[in,out] Pointer to dynamic sensor
     * @param pollingType[in] Pooling type for the sensor
     */
    template <typename SensorType>
    void addSensor(std::shared_ptr<SensorType>& sensor, PollingType pollingType)
    {
        addSensorBase(sensor, pollingType);
    }

    virtual requester::Coroutine updateNsmDevice();
    bool updateDiscoveryIdentifiers(eid_t eid, uuid_t uuid,
                                    uint8_t deviceInstanceNumber,
                                    std::string& associatedPath,
                                    std::string& mctpMedium,
                                    std::string& mctpBinding,
                                    std::optional<eid_t> mctpLocalEid);
    virtual requester::Coroutine refreshCapabilitySensor();
    requester::Coroutine refreshCommandMatrix();

    eid_t getEid()
    {
        return eid;
    }

    uuid_t getUuid()
    {
        return uuid;
    }

    /** @brief MCTP LocalEID (BMC endpoint on the route to this device);
     *  nullopt if not yet set from MCTP discovery */
    std::optional<eid_t> getMctpLocalEid() const
    {
        return mctpLocalEid;
    }

    /** @brief Record event subscription status for logDump. Does not clear
     * cached Set request/response bytes; use the 3-arg overload with
     * std::nullopt for both to clear. */
    void recordEventSubscriptionStatus(std::string_view status);

    /** @brief Record status and last NSM Set Event Subscription request /
     * response bytes (decode_nsm_set_event_subscription_resp payload) for
     * logDump. */
    void recordEventSubscriptionStatus(
        std::string_view status, std::optional<std::vector<uint8_t>> request,
        std::optional<std::vector<uint8_t>> response);

    /** @brief Get last event subscription status for logDump. Returns cached
     * string or nullopt if never attempted. */
    std::optional<std::string> getLastEventSubscriptionStatus() const;

    /** @brief Last Set Event Subscription request bytes, if recorded. */
    std::optional<std::vector<uint8_t>> getLastEventSubscriptionRequest() const;

    /** @brief Last Set Event Subscription response bytes, if recorded. */
    std::optional<std::vector<uint8_t>>
        getLastEventSubscriptionResponse() const;

    /** @brief Mark that this device has NSM_EventSetting config (used for
     * logDump to distinguish pending vs N/A). */
    void setHasNsmEventSettingConfig(bool hasConfig);

    /** @brief Whether this device has NSM_EventSetting config. */
    bool hasNsmEventSettingConfig() const;

    /** @brief getter of deviceType */
    uint8_t getDeviceType()
    {
        return deviceType;
    }

    /** @brief getter of deviceRole */
    uint8_t getDeviceRole()
    {
        return deviceRole;
    }

    /** @brief getter of static instanceNumber */
    uint8_t getInstanceNumber()
    {
        return instanceNumber;
    }

    /** @brief getter of NsmDevice Instance Number */
    uint8_t getNsmDeviceInstanceNumber()
    {
        return nsmDeviceInstanceNumber;
    }

    /** @brief getter of Device Remap Property */
    DeviceRemapProperty getDeviceRemapProp()
    {
        return deviceRemapProp;
    }

    std::variant<std::vector<uint8_t>, std::vector<uuid_t>>
        getDeviceRemapValues()
    {
        return deviceRemapValues;
    }

    /** @brief getter of Device Active State*/
    bool isOnline()
    {
        return isDeviceActive;
    }

    /** @brief getter of Device Ready State*/
    bool isReady()
    {
        return isDeviceReady;
    }

    /** @brief mark the device as ready */
    void markDeviceAsReady()
    {
        isDeviceReady = true;
    }

    /** @brief mark the device as not ready */
    void markDeviceAsNotReady()
    {
        isDeviceReady = false;
    }

    void initDeviceDiscovery()
    {
        discoveryPending = true;
        isDeviceActive = false;
    }
    void finishDeviceDiscovery()
    {
        discoveryPending = false;
    }
    bool isDiscoveryPending()
    {
        return discoveryPending;
    }
    void addCapabilityRefreshSensor(std::shared_ptr<NsmObject> sensor)
    {
        capabilityRefreshSensors.emplace_back(sensor);
    }
    void addSetSensor(std::shared_ptr<NsmObject> sensor)
    {
        setSensors.emplace_back(sensor);
    }
    void addStandByToDcRefreshSensor(std::shared_ptr<NsmObject> sensor)
    {
        standByToDcRefreshSensors.emplace_back(sensor);
    }
    void addGpuDriverSensor(
        std::shared_ptr<NsmGPUSWInventoryDriverVersionAndStatus> sensor)
    {
        gpuDriverSensor = sensor;
    }
    void addDeviceSensors(std::shared_ptr<NsmObject> sensor)
    {
        deviceSensors.push_back(sensor);
    }

    /** @brief Getter for the longRunningSemaphore */
    common::CoroutineSemaphore& getSemaphore()
    {
        return longRunningSemaphore;
    }

    /**
     * @brief Registers a long-running handler for a specific message type and
     * command code.
     *
     * @param messageType The message type associated with the long-running
     * event.
     * @param commandCode The command code associated with the long-running
     * event.
     * @param handler The event handler to register.
     */
    void registerLongRunningHandler(
        uint8_t messageType, uint8_t commandCode,
        std::shared_ptr<NsmLongRunningEvent> sensorInstance);

    /**
     * @brief Clears the registered long-running handler.
     */
    void clearLongRunningHandler();

    /**
     * @brief Retrieves the active long-running handler, if any.
     *
     * @return std::optional<ActiveLongRunningHandlerInfo> containing the active
     * handler, or an empty optional if no handler is active.
     */
    std::optional<nsm::ActiveLongRunningHandlerInfo>
        getActiveLongRunningHandler() const;
    int invokeLongRunningHandler(eid_t eid, NsmType type, NsmEventId eventId,
                                 const nsm_msg* event, size_t eventLen);

    std::coroutine_handle<> updateNsmDeviceTaskHandle;
    /* sensorIO should be used for polling sensors only*/
    virtual requester::Coroutine
        sensorIO(eid_t eid, Request& request,
                 std::shared_ptr<const nsm_msg>& responseMsg,
                 size_t& responseLen, bool bypassCommandCheck = false);
    /* postPatchIO should be used for post/patch operations on NsmDevice*/
    virtual requester::Coroutine
        postPatchIO(eid_t eid, Request& request,
                    std::shared_ptr<const nsm_msg>& responseMsg,
                    size_t& responseLen);

    void dumpNsmDeviceInfo();

  private:
    std::vector<std::vector<bitfield8_t>> commands;
    std::vector<std::vector<bool>> messageTypesToCommandCodeMatrix;
    uint8_t eventMode;
    eid_t eid = 0;
    std::optional<eid_t> mctpLocalEid;
    std::optional<std::string> lastEventSubscriptionStatus;
    /** Set Event Subscription (nsm_set_event_subscription_req / resp) only. */
    std::optional<std::vector<uint8_t>> lastEventSubscriptionRequest;
    std::optional<std::vector<uint8_t>> lastEventSubscriptionResponse;
    bool hasNsmEventSettingConfigFlag = false;
    uuid_t uuid;
    bool isDeviceActive = false;
    bool isDeviceReady = false;
    bool discoveryPending = false;
    uint8_t deviceType = 0;
    uint8_t instanceNumber = 0;
    uint8_t deviceRole = 0;
    DeviceRemapProperty deviceRemapProp;
    std::variant<std::vector<uint8_t>, std::vector<std::string>>
        deviceRemapValues;
    std::string mctpMedium;
    std::string mctpBinding;
    uint8_t nsmDeviceInstanceNumber;
    std::string associatedPath;
    std::shared_ptr<NsmMessageHandler> nsmMsgHandler;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::vector<std::shared_ptr<NsmEvent>> deviceEvents;
    std::shared_ptr<ProgressCounters> sensorProgressCounters = nullptr;
    const common::Event event;

    void registerLongRunningEventHandler();
    common::CoroutineSemaphore
        longRunningSemaphore; // Semaphore for synchronizing long running
    // commands
    common::CoroutineSemaphore
        discoverNsmDeviceSemaphore; // Semaphore for synchronizing update nsm
                                    // device during init if mctp rediscovery
                                    // signal is also received
    FruInterfaceManager fruDeviceManager;
    std::unique_ptr<void, std::function<void(void*)>> nsmRawCmdIntf;
    // Track if NSM message types were successfully retrieved
    bool areMessageTypesRetrieved{false};

    // Store the retrieved NSM message types
    std::vector<uint8_t> retrievedMessageTypes;

    // Track success status for each message type's command codes
    std::unordered_map<uint8_t, bool> commandCodesRetrieved;
    std::vector<std::shared_ptr<NsmObject>> setSensors;
    std::vector<std::shared_ptr<NsmObject>> capabilityRefreshSensors;

    void initMsgTypesSensor();
    requester::Coroutine markSensorsUnrefreshed();
    requester::Coroutine updateSensorsForOffline();
    requester::Coroutine dumpNsmDeviceInfoTask();

    /**
     * @brief Adds dynamic sensor to NsmDevice.
     *
     * @param sensor[in] Pointer to dynamic sensor
     * @param pollingType[in] Pooling type for the sensor
     */
    void addSensorBase(const std::shared_ptr<NsmObject>& sensor,
                       PollingType pollingType);
    requester::Coroutine waitForNsmDeviceUpdate();

    /* To be used for updating parameters at the time of device discovery*/
    requester::Coroutine
        SendRecvNsmMsg(eid_t eid, Request& request,
                       std::shared_ptr<const nsm_msg>& responseMsg,
                       size_t* responseLen);
    requester::Coroutine updateFruDeviceIntf();
    requester::Coroutine getFRU(nsm::InventoryProperties& properties,
                                const uint8_t& deviceType);
    requester::Coroutine
        getInventoryInformation(uint8_t& propertyIdentifier,
                                nsm::InventoryProperties& properties);
    requester::Coroutine getSupportedNvidiaMessageType(
        std::vector<uint8_t>& supportedNvidiaMessageTypes);
    requester::Coroutine
        getSupportedCommandCodes(uint8_t nvidia_message_type,
                                 std::vector<uint8_t>& supportedCommands);
    friend class mctp::MctpDiscovery;
    /** @brief set the nsmDevice to online state */
    requester::Coroutine setOnline();

    /** @brief set the nsmDevice to offline state */
    requester::Coroutine setOffline();

  public:
    /** Public members not for common use. It shall only be used by
     * SensorManager and NsmDevice itself. **/

    std::vector<std::shared_ptr<NsmObject>> deviceSensors;
    std::vector<std::shared_ptr<NsmObject>> standByToDcRefreshSensors;
    std::vector<std::shared_ptr<NsmNumericAggregator>> sensorAggregators;
    std::shared_ptr<NsmGPUSWInventoryDriverVersionAndStatus> gpuDriverSensor =
        nullptr; // for GPU driver
    std::shared_ptr<NsmObject> msgTypesSensor =
        nullptr; // Sensor for retrieving message types
    EventDispatcher eventDispatcher;
    std::optional<ActiveLongRunningHandlerInfo> longRunningHandler;

    /* Polling sensors management */
    PollingType nonPriorityPollingType = PollingType::GpuPerformanceMonitoring;
    std::unordered_map<PollingType, SensorQueue> sensors = {
        {PollingType::Priority, SensorQueue()},
        {PollingType::GpuPerformanceMonitoring, SensorQueue()},
        {PollingType::LongRunning, SensorQueue()},
        {PollingType::Static, SensorQueue()},
        {PollingType::RoundRobin, SensorQueue()},
    };
    SensorQueue& prioritySensors = sensors[PollingType::Priority];
    SensorQueue& gpmSensors = sensors[PollingType::GpuPerformanceMonitoring];
    SensorQueue& longRunningSensors = sensors[PollingType::LongRunning];
    SensorQueue& staticSensors = sensors[PollingType::Static];
    SensorQueue& roundRobinSensors = sensors[PollingType::RoundRobin];
    ProgressCounters& progressCounters();
    virtual DiscoveryEvents& discoveryEvents();
};

} // namespace nsm
