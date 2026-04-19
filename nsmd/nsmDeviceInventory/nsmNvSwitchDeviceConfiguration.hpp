/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "device-configuration.h"

#include "asyncOperationManager.hpp"
#include "nsmDevice.hpp"
#include "nsmEvent.hpp"
#include "nsmObject.hpp"

#include <com/nvidia/DeviceConfiguration/ConfigRequester/server.hpp>
#include <com/nvidia/DeviceConfiguration/ConfigUpdater/common.hpp>
#include <com/nvidia/DeviceConfiguration/ConfigUpdater/server.hpp>
#include <sdbusplus/message/types.hpp>

#include <cstddef>

namespace nsm
{

/** PDI enumeration for Set/Get device configuration type selector
 *  (com.nvidia.DeviceConfiguration.ConfigUpdater). */
using ConfigUpdaterConfigurationType = sdbusplus::common::com::nvidia::
    device_configuration::ConfigUpdater::ConfigurationType;

using NsmNvSwitchDeviceConfigIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::DeviceConfiguration::server::ConfigUpdater,
    sdbusplus::com::nvidia::DeviceConfiguration::server::ConfigRequester>;

class NsmNvSwitchDeviceConfigurationAsync;

/** Forwards Type 5 NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1
 *  (enum nsm_device_configuration_event_id, device-configuration.h) with class
 *  NSM_GENERAL_EVENT_CLASS to D-Bus DeviceConfigurationRequested.
 */
class NsmNvSwitchDeviceConfigurationRequestEvent : public NsmEvent
{
  public:
    explicit NsmNvSwitchDeviceConfigurationRequestEvent(
        std::weak_ptr<NsmNvSwitchDeviceConfigurationAsync> target);

    int handle(eid_t eid, NsmType type, NsmEventId eventId,
               const nsm_msg* event, size_t eventLen) override;

  private:
    std::weak_ptr<NsmNvSwitchDeviceConfigurationAsync> target_;
};

/** NVSwitch NSM Type 5 device configuration 0x10 / 0x11 (V2). */
class NsmNvSwitchDeviceConfigurationAsync :
    public NsmObject,
    public NsmNvSwitchDeviceConfigIntf
{
  public:
    NsmNvSwitchDeviceConfigurationAsync(sdbusplus::bus::bus& bus,
                                        const std::string& name,
                                        const std::string& type,
                                        const std::string& objPath,
                                        std::shared_ptr<NsmDevice> device);

    void emitDeviceConfigurationRequestedSignal();

    sdbusplus::message::object_path
        setDeviceConfiguration(ConfigUpdaterConfigurationType configurationType,
                               sdbusplus::message::unix_fd data) override;

    sdbusplus::message::object_path
        getDeviceConfiguration(ConfigUpdaterConfigurationType configurationType,
                               sdbusplus::message::unix_fd query) override;

  private:
    requester::Coroutine doSet(std::shared_ptr<AsyncStatusIntf> statusInterface,
                               ConfigUpdaterConfigurationType configurationType,
                               std::vector<uint8_t> data);

    requester::Coroutine doGet(std::shared_ptr<AsyncStatusIntf> statusInterface,
                               std::shared_ptr<AsyncValueIntf> valueInterface,
                               ConfigUpdaterConfigurationType configurationType,
                               std::vector<uint8_t> query);

    std::shared_ptr<NsmDevice> device;
    std::string dbusObjPath_;
};

/** When EM sets SupportNvSwitchDeviceConfiguration (NVSwitch device config) on
 * NSM_NVSwitch, register Type 5 device configuration (0x10/0x11) on @p
 * dbusObjPath (inventoryObjPath+name). */
void addNvSwitchDeviceConfigurationSensorIfEnabled(
    bool supportNvSwitchDeviceConfiguration, sdbusplus::bus::bus& bus,
    const std::string& name, const std::string& dbusObjPath,
    std::shared_ptr<NsmDevice> device);

} // namespace nsm
