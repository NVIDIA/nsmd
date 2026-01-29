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

#include "nsmLeakDetection.hpp"

#include "base.h"
#include "libnsm/platform-environmental.h"

#include "asyncOperationManager.hpp"
#include "dBusAsyncUtils.hpp"
#include "interfaceWrapper.hpp"
#include "nsmObjectFactory.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <tuple>
#include <vector>

namespace nsm
{

NsmLeakDetection::NsmLeakDetection(
    std::string& name, const std::string& type, sdbusplus::bus::bus& bus,
    const std::vector<uint64_t>& sensorIdMap,
    const std::vector<std::string>& sensorNameMap,
    const std::string& chassisPath) : NsmSensor(name, type)
{
    for (auto [sensorId, sensorName] :
         std::views::zip(sensorIdMap, sensorNameMap))
    {
        lg2::debug(
            "Creating LeakDetection sensor: SensorId={SENSOR_ID}, SensorName={SENSOR_NAME}",
            "SENSOR_ID", sensorId, "SENSOR_NAME", sensorName);

        std::string leakDetectorInventoryObjPath =
            "/xyz/openbmc_project/inventory/leakdetectors/" + sensorName;
        std::string leakDetectorStateObjPath =
            "/xyz/openbmc_project/state/leakdetectors/" + sensorName;
        std::string leakDetectorSensorObjPath =
            "/xyz/openbmc_project/sensors/voltage/" + sensorName;

        auto leakDetectorInventoryIntf = std::make_shared<LeakDetectorIntf>(
            bus, leakDetectorInventoryObjPath.c_str());
        auto inventoryAssociationIntf =
            std::make_shared<AssociationDefinitionsInft>(
                bus, leakDetectorInventoryObjPath.c_str());

        auto stateAssociationIntf =
            std::make_shared<AssociationDefinitionsInft>(
                bus, leakDetectorStateObjPath.c_str());
        auto operationalStatusIntf = std::make_shared<OperationalStatusIntf>(
            bus, leakDetectorStateObjPath.c_str());
        auto detectorStateIntf = std::make_shared<StateLeakDetectorIntf>(
            bus, leakDetectorStateObjPath.c_str());

        auto leakDetectorSensorAssociationIntf =
            std::make_shared<AssociationDefinitionsInft>(
                bus, leakDetectorSensorObjPath.c_str());
        auto leakDetectorSensorValueIntf = std::make_shared<SensorValueIntf>(
            bus, leakDetectorSensorObjPath.c_str());
        auto leakDetectorSensorThresholdIntf =
            std::make_shared<SensorThresholdCriticalIntf>(
                bus, leakDetectorSensorObjPath.c_str());

        // Initialize interface values
        leakDetectorInventoryIntf->leakDetectorType(
            LeakDetectorIntf::convertLeakDetectorTypeEnumFromString(
                "xyz.openbmc_project.Inventory.Item.LeakDetector.LeakDetectorTypeEnum.Moisture"));
        operationalStatusIntf->functional(false);
        operationalStatusIntf->state(OperationalStatusIntf::convertStateTypeFromString(
            "xyz.openbmc_project.State.Decorator.OperationalStatus.StateType.None"));
        detectorStateIntf->detectorState(
            StateLeakDetectorIntf::convertDetectorStateEnumFromString(
                "xyz.openbmc_project.State.LeakDetector.DetectorStateEnum.Warning"));

        leakDetectorSensorValueIntf->value(0);
        leakDetectorSensorValueIntf->unit(
            SensorValueIntf::convertUnitFromString(
                "xyz.openbmc_project.Sensor.Value.Unit.Volts"));

        // Add association for sensor
        addAssociationOnObj(inventoryAssociationIntf, stateAssociationIntf,
                            leakDetectorSensorAssociationIntf,
                            leakDetectorInventoryObjPath, chassisPath);

        leakDetectorInventoryIntfMap[static_cast<uint8_t>(sensorId)] =
            std::make_tuple(leakDetectorInventoryIntf,
                            inventoryAssociationIntf);

        leakDetectorStateIntfMap[static_cast<uint8_t>(sensorId)] =
            std::make_tuple(stateAssociationIntf, operationalStatusIntf,
                            detectorStateIntf);

        sensorValueIntfMap[static_cast<uint8_t>(sensorId)] = std::make_tuple(
            leakDetectorSensorValueIntf, leakDetectorSensorAssociationIntf,
            leakDetectorSensorThresholdIntf);
    }
}

void NsmLeakDetection::addAssociationOnObj(
    std::shared_ptr<AssociationDefinitionsInft> inventoryAssociationIntf,
    std::shared_ptr<AssociationDefinitionsInft> stateAssociationIntf,
    std::shared_ptr<AssociationDefinitionsInft> sensorAssociationIntf,
    const std::string& leakDetectorInventoryObjPath,
    const std::string& chassisPath)
{
    inventoryAssociationIntf->associations(
        {{"chassis", "contained_by", chassisPath}});
    stateAssociationIntf->associations(
        {{"inventory", "leak_detecting", leakDetectorInventoryObjPath}});
    sensorAssociationIntf->associations(
        {{"chassis", "all_sensors", chassisPath}});
}

void NsmLeakDetection::updateLeakDetectorState(uint8_t sensorId,
                                               uint8_t leakState)
{
    if (!leakDetectorStateIntfMap.contains(sensorId))
    {
        return;
    }

    auto& [associationIntf, operationalStatusIntf,
           detectorStateIntf] = leakDetectorStateIntfMap[sensorId];
    operationalStatusIntf->functional(true);

    switch (leakState)
    {
        case NSM_LEAK_STATE_NOMINAL_READING:
            operationalStatusIntf->state(
                OperationalStatusIntf::convertStateTypeFromString(
                    "xyz.openbmc_project.State.Decorator.OperationalStatus.StateType.Enabled"));
            detectorStateIntf->detectorState(
                StateLeakDetectorIntf::convertDetectorStateEnumFromString(
                    "xyz.openbmc_project.State.LeakDetector.DetectorStateEnum.OK"));
            break;
        case NSM_LEAK_STATE_LEAK:
            operationalStatusIntf->state(
                OperationalStatusIntf::convertStateTypeFromString(
                    "xyz.openbmc_project.State.Decorator.OperationalStatus.StateType.Enabled"));
            detectorStateIntf->detectorState(
                StateLeakDetectorIntf::convertDetectorStateEnumFromString(
                    "xyz.openbmc_project.State.LeakDetector.DetectorStateEnum.Critical"));
            break;
        case NSM_LEAK_STATE_SENSOR_SHORT:
        case NSM_LEAK_STATE_SENSOR_OPEN:
            operationalStatusIntf->state(
                OperationalStatusIntf::convertStateTypeFromString(
                    "xyz.openbmc_project.State.Decorator.OperationalStatus.StateType.Degraded"));
            detectorStateIntf->detectorState(
                StateLeakDetectorIntf::convertDetectorStateEnumFromString(
                    "xyz.openbmc_project.State.LeakDetector.DetectorStateEnum.Critical"));
            break;
        default:
            lg2::debug(
                "Invalid leakState value: {LEAK_STATE} for sensorId: {SENSOR_ID}",
                "LEAK_STATE", leakState, "SENSOR_ID", sensorId);
            operationalStatusIntf->state(
                OperationalStatusIntf::convertStateTypeFromString(
                    "xyz.openbmc_project.State.Decorator.OperationalStatus.StateType.None"));
            detectorStateIntf->detectorState(
                StateLeakDetectorIntf::convertDetectorStateEnumFromString(
                    "xyz.openbmc_project.State.LeakDetector.DetectorStateEnum.Warning"));
            break;
    }
}

void NsmLeakDetection::updateSensorValue(
    uint8_t sensorId, const struct nsm_leak_detection_sensors_data* sensor,
    uint8_t numberOfThresholdLevels)
{
    if (!sensorValueIntfMap.contains(sensorId))
    {
        return;
    }

    auto& [sensorValueIntf, sensorAssociationIntf,
           sensorThresholdIntf] = sensorValueIntfMap[sensorId];

    // All the reading values are in mV, so convert to Volts for DBus
    sensorValueIntf->value(static_cast<double>(sensor->adc_reading) / 1000.0);

    if (numberOfThresholdLevels > NSM_LEAK_THRESHOLD_LEVEL_MIN_LEAK)
    {
        sensorValueIntf->minAllowableValue(static_cast<double>(
            sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MIN_LEAK] / 1000.0));
    }
    if (numberOfThresholdLevels > NSM_LEAK_THRESHOLD_LEVEL_MAX_LEAK)
    {
        sensorThresholdIntf->criticalLow(static_cast<double>(
            sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_LEAK] / 1000.0));
    }
    if (numberOfThresholdLevels > NSM_LEAK_THRESHOLD_LEVEL_MAX_NORMAL)
    {
        sensorValueIntf->maxAllowableValue(static_cast<double>(
            sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_NORMAL] / 1000.0));
    }
}

std::optional<std::vector<uint8_t>>
    NsmLeakDetection::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_leak_detection_info_req));
    auto requestPtr = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_leak_detection_info_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        LG2_ERROR_FLT(
            "LeakDetection: encode_get_leak_detection_info_req failed. rc={RC}",
            "RC", rc, "EID", eid, "INSTANCE_ID", instanceId);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmLeakDetection::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint8_t numberOfSensors = 0;
    uint8_t numberOfThresholdLevels = 0;
    uint16_t reasonCode = ERR_NULL;

    std::vector<uint8_t> sensorsData(responseLen, 0);
    size_t sensorsDataLen = sensorsData.size();

    auto rc = decode_get_leak_detection_info_resp(
        responseMsg, responseLen, &cc, &reasonCode, &numberOfSensors,
        &numberOfThresholdLevels, sensorsData.data(), &sensorsDataLen);

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        LG2_ERROR_FLT(
            "decode_get_leak_detection_info_resp failed: rc={RC}, cc={CC}, reasonCode={REASON}",
            "RC", rc, "CC", cc, "REASON", reasonCode);
        return cc ? cc : rc;
    }

    // Parse each sensor's data
    size_t expectedSensorInfoSize =
        sizeof(nsm_leak_detection_sensors_data) +
        ((numberOfThresholdLevels - 1) * sizeof(uint16_t));
    uint8_t* ptr = sensorsData.data();

    for (uint8_t i = 0; i < numberOfSensors; i++)
    {
        auto* sensor =
            reinterpret_cast<struct nsm_leak_detection_sensors_data*>(ptr);

        uint8_t sensorId = sensor->sensor_id;

        // Update leak detector inventory interfaces
        updateLeakDetectorState(sensorId, sensor->leak_state);

        // Update sensor value interfaces
        updateSensorValue(sensorId, sensor, numberOfThresholdLevels);
        ptr += expectedSensorInfoSize;
    }

    return cc ? cc : rc;
}

std::optional<std::tuple<std::shared_ptr<SensorValueIntf>,
                         std::shared_ptr<SensorThresholdCriticalIntf>>>
    NsmLeakDetection::getSensorInterfaces(uint8_t sensorId)
{
    if (!sensorValueIntfMap.contains(sensorId))
    {
        return std::nullopt;
    }
    auto& [sensorValueIntf, associationIntf,
           thresholdIntf] = sensorValueIntfMap[sensorId];
    return std::make_tuple(sensorValueIntf, thresholdIntf);
}

NsmLeakDetectionThresholdsPatch::NsmLeakDetectionThresholdsPatch(
    uint8_t sensorId, std::shared_ptr<SensorValueIntf> sensorValueIntf,
    std::shared_ptr<SensorThresholdCriticalIntf> thresholdIntf) :
    NsmObject("LeakDetectionThresholdsPatch_" + std::to_string(sensorId),
              "NSM_LeakDetection_ThresholdsPatch"),
    sensorId(sensorId), sensorValueIntf(sensorValueIntf),
    thresholdIntf(thresholdIntf)
{}

bool NsmLeakDetectionThresholdsPatch::getLeakDetectionThresholdsData(
    struct nsm_leak_detection_thresholds_data* data, size_t dataLen)
{
    size_t requiredLen = sizeof(uint8_t) + sizeof(uint8_t) +
                         ((expectedNumberOfThresholdLevels) * sizeof(uint16_t));

    if (dataLen < requiredLen)
    {
        lg2::error(
            "getLeakDetectionThresholdsData: Insufficient buffer size. Required={REQ}, Provided={PROV}",
            "REQ", requiredLen, "PROV", dataLen);
        return false;
    }

    data->sensor_id = sensorId;
    data->reserved = 0;
    // Read current thresholds from D-Bus interfaces (dbus values are in Volts,
    // convert to mV)
    data->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MIN_LEAK] =
        static_cast<uint16_t>(sensorValueIntf->minAllowableValue() * 1000.0);
    data->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_LEAK] =
        static_cast<uint16_t>(thresholdIntf->criticalLow() * 1000.0);
    data->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_NORMAL] =
        static_cast<uint16_t>(sensorValueIntf->maxAllowableValue() * 1000.0);
    return true;
}

requester::Coroutine
    NsmLeakDetectionThresholdsPatch::setLeakDetectionThresholdsOnDevice(
        struct nsm_leak_detection_thresholds_data* data, size_t dataLen,
        [[maybe_unused]] AsyncOperationStatusType* status,
        std::shared_ptr<NsmDevice> device)
{
    auto eid = device->getEid();
    lg2::info(
        "setLeakDetectionThresholdsOnDevice for EID: {EID}, sensorId: {ID}",
        "EID", eid, "ID", data->sensor_id);

    constexpr uint8_t numberOfSensors = 1;
    size_t requestSize = sizeof(nsm_msg_hdr) +
                         sizeof(nsm_set_leak_detection_thresholds_req) - 1 +
                         dataLen;

    Request request(requestSize);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_set_leak_detection_thresholds_req(
        0, numberOfSensors, expectedNumberOfThresholdLevels,
        reinterpret_cast<uint8_t*>(data), dataLen, requestMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "setLeakDetectionThresholdsOnDevice encode_set_leak_detection_thresholds_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await device->postPatchIO(eid, request, responseMsg,
                                            responseLen);
    if (rc_)
    {
        lg2::error(
            "setLeakDetectionThresholdsOnDevice postPatchIO failed for eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc_));
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    rc = decode_set_leak_detection_thresholds_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        lg2::info(
            "setLeakDetectionThresholdsOnDevice for EID: {EID} sensorId: {ID} completed",
            "EID", eid, "ID", data->sensor_id);
    }
    else
    {
        lg2::error(
            "setLeakDetectionThresholdsOnDevice decode_set_leak_detection_thresholds_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reasonCode, "A", rc);
        lg2::error("throwing write failure exception");
        *status = AsyncOperationStatusType::WriteFailure;

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmLeakDetectionThresholdsPatch::setLeakDetectionThresholdsPatch(
        const AsyncSetOperationValueType& value,
        AsyncOperationStatusType* status, std::shared_ptr<NsmDevice> device)
{
    if (asyncPatchInProgress)
    {
        lg2::error(
            "setLeakDetectionThresholdsPatch: Async patch operation already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        co_return NSM_SW_ERROR;
    }

    // Allocate memory for threshold data with proper size for flexible array
    size_t thresholdsDataLen =
        sizeof(struct nsm_leak_detection_thresholds_data) +
        ((expectedNumberOfThresholdLevels - 1) * sizeof(uint16_t));
    std::vector<uint8_t> thresholdsDataBuffer(thresholdsDataLen, 0);
    auto* thresholdsPatchData =
        reinterpret_cast<struct nsm_leak_detection_thresholds_data*>(
            thresholdsDataBuffer.data());

    // Get current threshold data from D-Bus interfaces
    if (!getLeakDetectionThresholdsData(thresholdsPatchData, thresholdsDataLen))
    {
        lg2::error(
            "setLeakDetectionThresholdsPatch: Failed to get threshold data");
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR;
    }

    const auto* patchRequestedValues = std::get_if<std::vector<
        std::tuple<std::string, std::variant<bool, uint32_t, double,
                                             std::vector<uint8_t>>>>>(&value);
    if (!patchRequestedValues)
    {
        lg2::error(
            "setLeakDetectionThresholdsPatch: Failed to get patch values - invalid type");
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (patchRequestedValues->empty())
    {
        lg2::error("setLeakDetectionThresholdsPatch: Empty patch values list");
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    for (const auto& [key, val] : *patchRequestedValues)
    {
        const double* thresholdValue = std::get_if<double>(&val);
        if (!thresholdValue)
        {
            lg2::error(
                "setLeakDetectionThresholdsPatch: Invalid type for {PROPERTY}",
                "PROPERTY", key);
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }

        if (key == "MinAllowableValue")
        {
            lg2::info(
                "Patch SetLeakDetectionThresholds: MinAllowableValue with value : {VALUE} V",
                "VALUE", *thresholdValue);
            thresholdsPatchData->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MIN_LEAK] =
                static_cast<uint16_t>(*thresholdValue *
                                      1000); // Convert from volts to millivolts
        }
        else if (key == "CriticalLow")
        {
            lg2::info(
                "Patch SetLeakDetectionThresholds: CriticalLow with value : {VALUE} V",
                "VALUE", *thresholdValue);
            thresholdsPatchData->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_LEAK] =
                static_cast<uint16_t>(*thresholdValue *
                                      1000); // Convert from volts to millivolts
        }
        else if (key == "MaxAllowableValue")
        {
            lg2::info(
                "Patch SetLeakDetectionThresholds: MaxAllowableValue with value : {VALUE} V",
                "VALUE", *thresholdValue);
            thresholdsPatchData
                ->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_NORMAL] =
                static_cast<uint16_t>(*thresholdValue *
                                      1000); // Convert from volts to millivolts
        }
        else
        {
            lg2::error(
                "setLeakDetectionThresholdsPatch: Unrecognized threshold type {TYPE}",
                "TYPE", key);
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }
    }

    asyncPatchInProgress = true;
    try
    {
        const auto rc = co_await setLeakDetectionThresholdsOnDevice(
            thresholdsPatchData, thresholdsDataLen, status, device);
        asyncPatchInProgress = false;
        co_return rc;
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "setLeakDetectionThresholdsPatch: Exception during setLeakDetectionThresholdsOnDevice: {ERROR}",
            "ERROR", e.what());
        asyncPatchInProgress = false;
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR;
    }
}

NsmSetLeakDetectionThresholds::NsmSetLeakDetectionThresholds(
    const std::string& name, const std::string& type,
    const std::vector<uint64_t>& sensorIdMap,
    const std::vector<uint64_t>& minThresholds,
    const std::vector<uint64_t>& criticalThresholds,
    const std::vector<uint64_t>& maxThresholds) :
    NsmObject(name, type), sensorIdMap(sensorIdMap),
    minThresholds(minThresholds), criticalThresholds(criticalThresholds),
    maxThresholds(maxThresholds)
{}

requester::Coroutine
    NsmSetLeakDetectionThresholds::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    uint8_t numberOfSensors = static_cast<uint8_t>(sensorIdMap.size());

    // Calculate expected sensor info size
    size_t expectedSensorInfoSize =
        sizeof(struct nsm_leak_detection_thresholds_data) +
        ((expectedNumberOfThresholdLevels - 1) * sizeof(uint16_t));
    size_t thresholdsDataLen = numberOfSensors * expectedSensorInfoSize;

    // Build thresholds data buffer
    std::vector<uint8_t> thresholdsData(thresholdsDataLen, 0);
    uint8_t* ptr = thresholdsData.data();

    for (size_t i = 0; i < numberOfSensors; i++)
    {
        auto* sensor =
            reinterpret_cast<struct nsm_leak_detection_thresholds_data*>(ptr);

        sensor->sensor_id = static_cast<uint8_t>(sensorIdMap[i]);
        sensor->reserved = 0;
        sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MIN_LEAK] =
            static_cast<uint16_t>(minThresholds[i]);
        sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_LEAK] =
            static_cast<uint16_t>(criticalThresholds[i]);
        sensor->thresholds[NSM_LEAK_THRESHOLD_LEVEL_MAX_NORMAL] =
            static_cast<uint16_t>(maxThresholds[i]);

        ptr += expectedSensorInfoSize;
    }

    size_t requestSize = sizeof(nsm_msg_hdr) +
                         sizeof(nsm_set_leak_detection_thresholds_req) - 1 +
                         thresholdsDataLen;

    Request request(requestSize);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_set_leak_detection_thresholds_req(
        0, numberOfSensors, expectedNumberOfThresholdLevels,
        thresholdsData.data(), thresholdsDataLen, requestMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        LG2_ERROR_FLT(
            "encode_set_leak_detection_thresholds_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc != NSM_SW_SUCCESS)
    {
        LG2_ERROR_FLT(
            "NsmSetLeakDetectionThresholds sensorIO failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    rc = decode_set_leak_detection_thresholds_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        LG2_ERROR_FLT(
            "decode_set_leak_detection_thresholds_resp failed. eid={EID} rc={RC} cc={CC} reasonCode={REASON}",
            "EID", nsmDevice->getEid(), "RC", rc, "CC", cc, "REASON",
            reasonCode);
    }

    co_return cc ? cc : rc;
}

requester::Coroutine
    nsmLeakDetectionCreateSensors(SensorManager& manager,
                                  [[maybe_unused]] const std::string& interface,
                                  const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_LeakDetection";
    auto& bus = utils::DBusHandler::getBus();
    std::string name{};
    std::string type{};
    uuid_t uuid{};
    std::vector<uint64_t> sensorIdMap{};
    std::vector<std::string> sensorNameMap{};
    std::string chassisPath{};
    std::vector<uint64_t> minThresholds{};
    std::vector<uint64_t> criticalThresholds{};
    std::vector<uint64_t> maxThresholds{};

    dbus::PropertyMap allCurrentIfaceProperties =
        co_await utils::coGetAllDbusProperty(utils::entityManagerServiceStr,
                                             objPath, baseInterface);
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    if (allCurrentIfaceProperties.count("Type"))
    {
        type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
    }
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }
    if (allCurrentIfaceProperties.count("SensorIdMap"))
    {
        sensorIdMap = std::get<std::vector<uint64_t>>(
            allCurrentIfaceProperties.at("SensorIdMap"));
    }
    if (allCurrentIfaceProperties.count("SensorNameMap"))
    {
        sensorNameMap = std::get<std::vector<std::string>>(
            allCurrentIfaceProperties.at("SensorNameMap"));
    }
    if (allCurrentIfaceProperties.count("ChassisPath"))
    {
        chassisPath =
            std::get<std::string>(allCurrentIfaceProperties.at("ChassisPath"));
    }
    if (allCurrentIfaceProperties.count("MinThresholdsmV"))
    {
        minThresholds = std::get<std::vector<uint64_t>>(
            allCurrentIfaceProperties.at("MinThresholdsmV"));
    }
    if (allCurrentIfaceProperties.count("CriticalThresholdsmV"))
    {
        criticalThresholds = std::get<std::vector<uint64_t>>(
            allCurrentIfaceProperties.at("CriticalThresholdsmV"));
    }
    if (allCurrentIfaceProperties.count("MaxThresholdsmV"))
    {
        maxThresholds = std::get<std::vector<uint64_t>>(
            allCurrentIfaceProperties.at("MaxThresholdsmV"));
    }

    // Validate array sizes match sensorIdMap size
    if (sensorNameMap.size() != sensorIdMap.size() ||
        minThresholds.size() != sensorIdMap.size() ||
        criticalThresholds.size() != sensorIdMap.size() ||
        maxThresholds.size() != sensorIdMap.size())
    {
        lg2::error(
            "Array size mismatch: SensorIdMap={SENSOR_SIZE}, SensorNameMap={NAME_SIZE}, MinThresholds={MIN_SIZE}, CriticalThresholds={CRIT_SIZE}, MaxThresholds={MAX_SIZE}",
            "SENSOR_SIZE", sensorIdMap.size(), "NAME_SIZE",
            sensorNameMap.size(), "MIN_SIZE", minThresholds.size(), "CRIT_SIZE",
            criticalThresholds.size(), "MAX_SIZE", maxThresholds.size());
        co_return NSM_ERROR;
    }

    auto device = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error(
            "Failed to get NSM device for LeakDetection: UUID={UUID}, Name={NAME}",
            "UUID", uuid, "NAME", name);
        co_return NSM_ERROR;
    }

    // Create NsmSetLeakDetectionThresholds for initial threshold setup
    auto setThresholdsSensor = std::make_shared<NsmSetLeakDetectionThresholds>(
        name + "_SetThresholds", type + "_SetThresholds", sensorIdMap,
        minThresholds, criticalThresholds, maxThresholds);
    device->addStaticSensor(setThresholdsSensor);

    // Create NsmLeakDetection sensor with all interfaces
    auto leakDetectorInfoObject = std::make_shared<NsmLeakDetection>(
        name, type, bus, sensorIdMap, sensorNameMap, chassisPath);
    device->addSensor(leakDetectorInfoObject, true);

    // Create NsmLeakDetectionThresholdsPatch for each sensor for runtime
    // threshold patching. Register async set operations on the sensor's voltage
    // object path.
    for (auto [sensorId, sensorName] :
         std::views::zip(sensorIdMap, sensorNameMap))
    {
        // Get sensor interfaces from NsmLeakDetection
        auto interfaces = leakDetectorInfoObject->getSensorInterfaces(
            static_cast<uint8_t>(sensorId));
        if (!interfaces)
        {
            lg2::error("Failed to get sensor interfaces for sensorId: {ID}",
                       "ID", sensorId);
            continue;
        }

        auto& [sensorValueIntf, thresholdIntf] = *interfaces;

        // Create NsmLeakDetectionThresholdsPatch for this sensor
        auto thresholdsPatchObject =
            std::make_shared<NsmLeakDetectionThresholdsPatch>(
                static_cast<uint8_t>(sensorId), sensorValueIntf, thresholdIntf);
        device->addStaticSensor(thresholdsPatchObject);

        // Register async set operations on the sensor's voltage object path
        std::string sensorObjPath = "/xyz/openbmc_project/sensors/voltage/" +
                                    sensorName;

        nsm::AsyncSetOperationHandler setLeakDetectionThresholdsPatchHandler =
            std::bind(&NsmLeakDetectionThresholdsPatch::
                          setLeakDetectionThresholdsPatch,
                      thresholdsPatchObject, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);

        // Register for MinThreshold
        AsyncOperationManager::getInstance()
            ->getDispatcher(sensorObjPath)
            ->addAsyncSetOperation(
                "xyz.openbmc_project.Sensor.Value", "MinAllowableValue",
                AsyncSetOperationInfo{setLeakDetectionThresholdsPatchHandler,
                                      thresholdsPatchObject, device});

        // Register for CriticalThreshold
        AsyncOperationManager::getInstance()
            ->getDispatcher(sensorObjPath)
            ->addAsyncSetOperation(
                "xyz.openbmc_project.Sensor.Threshold.Critical", "CriticalLow",
                AsyncSetOperationInfo{setLeakDetectionThresholdsPatchHandler,
                                      thresholdsPatchObject, device});

        // Register for MaxThreshold
        AsyncOperationManager::getInstance()
            ->getDispatcher(sensorObjPath)
            ->addAsyncSetOperation(
                "xyz.openbmc_project.Sensor.Value", "MaxAllowableValue",
                AsyncSetOperationInfo{setLeakDetectionThresholdsPatchHandler,
                                      thresholdsPatchObject, device});
    }

    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    nsmLeakDetectionCreateSensors,
    "xyz.openbmc_project.Configuration.NSM_LeakDetection")
} // namespace nsm
