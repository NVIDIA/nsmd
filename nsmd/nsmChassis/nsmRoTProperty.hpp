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

#include "firmware-utils.h"

#include "asyncOperationManager.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <com/nvidia/ImageCopy/server.hpp>
#include <com/nvidia/ImageCopyPolicy/server.hpp>
#include <com/nvidia/InbandUpdatePolicy/server.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/timer.hpp>

#include <array>
#include <memory>

namespace nsm
{

using namespace sdbusplus::common::xyz::openbmc_project::common;
using namespace sdbusplus::common::xyz::openbmc_project::software;
using namespace sdbusplus::server;
using InbandUpdatePolicyIntf =
    object_t<sdbusplus::server::com::nvidia::InbandUpdatePolicy>;
using ImageCopyIntf = object_t<sdbusplus::server::com::nvidia::ImageCopy>;
using ImageCopyPolicyIntf =
    object_t<sdbusplus::server::com::nvidia::ImageCopyPolicy>;

class NsmInbandUpdatePolicy :
    public InbandUpdatePolicyIntf,
    public StateChangeLogger
{
  public:
    NsmInbandUpdatePolicy(sdbusplus::bus::bus& bus, const std::string& objPath,
                          uint16_t classificationIn, uint16_t identifierIn,
                          uint8_t indexIn, NsmSensor& nsmSensor) :
        InbandUpdatePolicyIntf(bus, objPath.c_str()),
        classification(classificationIn), identifier(identifierIn),
        index(indexIn), nsmSensor(nsmSensor)
    {}

    virtual ~NsmInbandUpdatePolicy() = default;

    void updateProperties(
        const struct ::nsm_firmware_erot_state_info_resp& erot_info);

  private:
    uint16_t classification;
    uint16_t identifier;
    uint8_t index;
    NsmSensor& nsmSensor;

    static constexpr uint8_t ARGUMENT_DATA_LENGTH = 2;
};

/**
 * @brief Object class for RoT Property that inherits from both NsmSensor and
 * NsmInterfaceContainer
 */
class NsmInbandUpdatePolicyObject : public NsmSensor
{
  private:
    std::string getPath(const std::string& chassisName)
    {
        return std::string(chassisInventoryBasePath) + "/" + chassisName;
    }

  public:
    NsmInbandUpdatePolicyObject(sdbusplus::bus::bus& bus,
                                const std::string& chassisName,
                                uint16_t classificationIn,
                                uint16_t identifierIn, uint8_t indexIn);

    NsmInbandUpdatePolicyObject() = delete;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::string objectPath;
    std::unique_ptr<NsmInbandUpdatePolicy> nsmInbandUpdatePolicy;
    uint16_t classification;
    uint16_t identifier;
    uint8_t index;
};

/**
 * @brief Handler class for async Inband Update Policy operation
 * This class is used to handle writes to the InbandUpdatePolicy property
 * on the InbandUpdatePolicy D-Bus interface. It inherits from StateChangeLogger
 * to enable state-aware logging.
 */
class InbandUpdatePolicyHandler : public StateChangeLogger
{
  public:
    InbandUpdatePolicyHandler() = default;
    virtual ~InbandUpdatePolicyHandler() = default;

    requester::Coroutine updateInbandUpdatePolicy(
        const AsyncSetOperationValueType& value,
        AsyncOperationStatusType* status, std::shared_ptr<NsmDevice> device,
        uint16_t classification, uint16_t identifier, uint8_t index);
};

/**
 * @brief Standalone wrapper for async Inband Update Policy operation
 * This function is registered with addAsyncSetOperation to handle
 * writes to the InbandUpdatePolicy property on the InbandUpdatePolicy D-Bus
 * interface.
 */
requester::Coroutine updateInbandUpdatePolicyHandler(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device, uint16_t classification,
    uint16_t identifier, uint8_t index);

// Structure to hold component information for image copy
struct ComponentInfo
{
    uint16_t classification;
    uint16_t identifier;
    uint8_t index;
};

/**
 * @brief Class for initiating image copy for a chassis
 */
class NsmImageCopy : public ImageCopyIntf, public StateChangeLogger
{
  public:
    NsmImageCopy(sdbusplus::bus::bus& bus, const std::string& objPath,
                 const uuid_t& uuidIn, NsmObject& nsmObject) :
        ImageCopyIntf(bus, objPath.c_str()), uuid(uuidIn), nsmObject(nsmObject)
    {
        imageCopyRequestStatus(ImageCopyRequestStatus::None);
        errorCode(ErrorCode::None);
    }

    virtual ~NsmImageCopy() = default;

    void requestImageCopy(
        std::vector<sdbusplus::message::object_path> objectPaths) override;

  private:
    requester::Coroutine
        initiateImageCopyAsync(std::vector<std::string> objectPaths,
                               std::shared_ptr<AsyncStatusIntf> statusIntf,
                               std::shared_ptr<AsyncValueIntf> valueIntf);

    requester::Coroutine
        imageCopyAsyncHandler(const std::vector<ComponentInfo>& componentInfos,
                              uint8_t& cc, uint16_t& reasonCode);

    requester::Coroutine
        getActiveSlotComponentInfo(const std::string& objectPath,
                                   ComponentInfo& componentInfo);

    void setImageCopyResult(std::shared_ptr<AsyncValueIntf> valueIntf,
                            uint8_t cc, uint16_t reasonCode,
                            ImageCopyRequestStatus status,
                            uint16_t errorCodeReason = ERR_NULL);

    const uuid_t uuid;
    NsmObject& nsmObject;
    std::shared_ptr<sdbusplus::Timer> imageCopyTimeoutTimer;
};

/**
 * @brief Object class for Image Copy that inherits from NsmObject
 */
class NsmImageCopyObject : public NsmObject
{
  private:
    std::string getPath(const std::string& chassisName)
    {
        return std::string(chassisInventoryBasePath) + "/" + chassisName;
    }

  public:
    NsmImageCopyObject(sdbusplus::bus::bus& bus, const std::string& chassisName,
                       const uuid_t& uuid);
    NsmImageCopyObject() = delete;

  private:
    std::string objectPath;
    std::unique_ptr<NsmImageCopy> nsmImageCopy;
};

/**
 * @brief D-Bus interface class for RoT Image Copy Policy
 *
 * This class exposes the ImageCopyPolicy D-Bus interface for managing the
 * background copy policy (Manual or Automatic) on Root of Trust (RoT) devices.
 * It reads and updates the redundancy policy state from eROT firmware
 * state information responses.
 */
class NsmImageCopyPolicy : public ImageCopyPolicyIntf, public StateChangeLogger
{
  public:
    NsmImageCopyPolicy(sdbusplus::bus::bus& bus, const std::string& objPath,
                       NsmSensor& nsmSensor);

    virtual ~NsmImageCopyPolicy() = default;

    void updateProperties(
        const struct ::nsm_firmware_erot_state_info_resp& erot_info);

  private:
    NsmSensor& nsmSensor;
};

/**
 * @brief NSM Sensor object class for RoT Image Copy Policy
 *
 * This class inherits from NsmSensor and manages the polling and communication
 * with eROT devices to retrieve and update the image copy policy state.
 * It generates NSM request messages to query eROT state parameters and
 * handles response messages to update the associated NsmImageCopyPolicy
 * D-Bus interface.
 */
class NsmImageCopyPolicyObject : public NsmSensor
{
  private:
    std::string getPath(const std::string& chassisName)
    {
        return std::string(chassisInventoryBasePath) + "/" + chassisName;
    }

  public:
    NsmImageCopyPolicyObject(sdbusplus::bus::bus& bus, const std::string& name,
                             const std::string& type, uint16_t classificationIn,
                             uint16_t identifierIn, uint8_t indexIn);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::string objectPath;
    std::unique_ptr<NsmImageCopyPolicy> imageCopyPolicyObject;
    uint16_t classification;
    uint16_t identifier;
    uint8_t index;
};

/**
 * @brief Handler class for async Image Copy Policy operation
 * This class is used to handle writes to the ImageCopyPolicy property
 * on the ImageCopyPolicy D-Bus interface. It inherits from StateChangeLogger
 * to enable state-aware logging.
 */
class ImageCopyPolicyHandler : public StateChangeLogger
{
  public:
    ImageCopyPolicyHandler() = default;
    virtual ~ImageCopyPolicyHandler() = default;

    requester::Coroutine updateImageCopyPolicy(
        const AsyncSetOperationValueType& value,
        AsyncOperationStatusType* status, std::shared_ptr<NsmDevice> device,
        uint16_t classification, uint16_t identifier, uint8_t index);
};

/**
 * @brief Standalone wrapper for async Image Copy Policy operation
 * This function is registered with addAsyncSetOperation to handle
 * writes to the ImageCopyPolicy property on the ImageCopyPolicy D-Bus
 * interface.
 */
requester::Coroutine updateImageCopyPolicyHandler(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device, uint16_t classification,
    uint16_t identifier, uint8_t index);

} // namespace nsm
