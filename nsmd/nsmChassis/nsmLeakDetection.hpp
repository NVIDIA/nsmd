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

#include "asyncOperationManager.hpp"
#include "nsmDevice.hpp"
#include "nsmInterface.hpp"
#include "nsmSensor.hpp"

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/LeakDetector/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>
#include <xyz/openbmc_project/State/LeakDetector/server.hpp>

#include <optional>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using OperationalStatusIntf =
    object_t<State::Decorator::server::OperationalStatus>;
using LeakDetectorIntf = object_t<Inventory::Item::server::LeakDetector>;
using StateLeakDetectorIntf = object_t<State::server::LeakDetector>;
using SensorValueIntf = object_t<Sensor::server::Value>;
using SensorThresholdCriticalIntf =
    object_t<Sensor::Threshold::server::Critical>;

constexpr uint8_t expectedNumberOfThresholdLevels = 3;

class NsmLeakDetection : public NsmSensor
{
  public:
    NsmLeakDetection(std::string& name, const std::string& type,
                     sdbusplus::bus::bus& bus,
                     const std::vector<uint64_t>& sensorIdMap,
                     const std::vector<std::string>& sensorNameMap,
                     const std::string& chassisPath);
    NsmLeakDetection() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    /** @brief Get sensor value and threshold interfaces for a sensor
     *  @param sensorId - The sensor ID to get interfaces for
     *  @return Tuple of (SensorValueIntf, SensorThresholdCriticalIntf) or
     * nullopt if not found
     */
    std::optional<std::tuple<std::shared_ptr<SensorValueIntf>,
                             std::shared_ptr<SensorThresholdCriticalIntf>>>
        getSensorInterfaces(uint8_t sensorId);

  private:
    void addAssociationOnObj(
        std::shared_ptr<AssociationDefinitionsInft> inventoryAssociationIntf,
        std::shared_ptr<AssociationDefinitionsInft> stateAssociationIntf,
        std::shared_ptr<AssociationDefinitionsInft> sensorAssociationIntf,
        const std::string& leakDetectorInventoryObjPath,
        const std::string& chassisPath);

    void updateLeakDetectorState(uint8_t sensorId, uint8_t leakState);

    void updateSensorValue(uint8_t sensorId,
                           const struct nsm_leak_detection_sensors_data* sensor,
                           uint8_t numberOfThresholdLevels);

    std::map<uint8_t, std::tuple<std::shared_ptr<LeakDetectorIntf>,
                                 std::shared_ptr<AssociationDefinitionsInft>>>
        leakDetectorInventoryIntfMap;

    std::map<uint8_t, std::tuple<std::shared_ptr<AssociationDefinitionsInft>,
                                 std::shared_ptr<OperationalStatusIntf>,
                                 std::shared_ptr<StateLeakDetectorIntf>>>
        leakDetectorStateIntfMap;

    std::map<uint8_t, std::tuple<std::shared_ptr<SensorValueIntf>,
                                 std::shared_ptr<AssociationDefinitionsInft>,
                                 std::shared_ptr<SensorThresholdCriticalIntf>>>
        sensorValueIntfMap;
};

/** @class NsmLeakDetectionThresholdsPatch
 *
 *  Class to handle patch operations for leak detection thresholds.
 *  This allows patching one or more threshold properties at runtime.
 */
class NsmLeakDetectionThresholdsPatch : public NsmObject
{
  public:
    NsmLeakDetectionThresholdsPatch(
        uint8_t sensorId, std::shared_ptr<SensorValueIntf> sensorValueIntf,
        std::shared_ptr<SensorThresholdCriticalIntf> thresholdIntf);
    NsmLeakDetectionThresholdsPatch() = delete;

    requester::Coroutine
        update([[maybe_unused]] std::shared_ptr<NsmDevice> nsmDevice) override
    {
        co_return NSM_SW_SUCCESS;
    }

    /** @brief Get current threshold data from D-Bus interfaces
     *  @param data - Pointer to pre-allocated thresholds data structure
     *  @param dataLen - Size of allocated memory for data
     *  @return true if successful, false if allocated size is insufficient
     */
    bool getLeakDetectionThresholdsData(
        struct nsm_leak_detection_thresholds_data* data, size_t dataLen);

    /** @brief Send set leak detection thresholds request to device
     *  @param data - Pointer to thresholds data to set
     *  @param dataLen - Size of thresholds data
     *  @param status - Pointer to async operation status
     *  @param device - Target NSM device
     *  @return Coroutine returning completion code
     */
    requester::Coroutine setLeakDetectionThresholdsOnDevice(
        struct nsm_leak_detection_thresholds_data* data, size_t dataLen,
        AsyncOperationStatusType* status, std::shared_ptr<NsmDevice> device);

    /** @brief Handle patch operation for leak detection thresholds
     *
     *  This method allows patching one or more threshold properties in a single
     *  operation. Properties not included in the patch request retain their
     *  current values. Supported property name formats:
     *  - "MinThreshold" - Min threshold (minAllowableValue)
     *  - "CriticalThreshold" - Critical threshold (criticalLow)
     *  - "MaxThreshold" - Max threshold (maxAllowableValue)
     *
     *  @param value - Patch values as vector of (property_name, value) tuples
     *  @param status - Pointer to async operation status
     *  @param device - Target NSM device
     *  @return Coroutine returning completion code
     */
    requester::Coroutine
        setLeakDetectionThresholdsPatch(const AsyncSetOperationValueType& value,
                                        AsyncOperationStatusType* status,
                                        std::shared_ptr<NsmDevice> device);

  private:
    uint8_t sensorId;
    std::shared_ptr<SensorValueIntf> sensorValueIntf;
    std::shared_ptr<SensorThresholdCriticalIntf> thresholdIntf;
    bool asyncPatchInProgress{false};
};

class NsmSetLeakDetectionThresholds : public NsmObject
{
  public:
    NsmSetLeakDetectionThresholds(
        const std::string& name, const std::string& type,
        const std::vector<uint64_t>& sensorIdMap,
        const std::vector<uint64_t>& minThresholds,
        const std::vector<uint64_t>& criticalThresholds,
        const std::vector<uint64_t>& maxThresholds);
    NsmSetLeakDetectionThresholds() = delete;

    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;

  private:
    std::vector<uint64_t> sensorIdMap;
    std::vector<uint64_t> minThresholds;
    std::vector<uint64_t> criticalThresholds;
    std::vector<uint64_t> maxThresholds;
};

} // namespace nsm
