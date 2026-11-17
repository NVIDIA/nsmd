/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

#include "deviceCoordinator.hpp"

#include "libnsm/base.h"

#include "common/globals.hpp"
#include "devices/baseboard.hpp"
#include "devices/bluefield.hpp"
#include "devices/bluefield_sma.hpp"
#include "devices/bmc_erot.hpp"
#include "devices/bmc_hpm_sma.hpp"
#include "devices/bmc_irot.hpp"
#include "devices/cpu.hpp"
#include "devices/cx_io.hpp"
#include "devices/cx_nic.hpp"
#include "devices/cx_sma.hpp"
#include "devices/gpu.hpp"
#include "devices/gpu_sma.hpp"
#include "devices/gpu_sxm.hpp"
#include "devices/hmc_erot.hpp"
#include "devices/hmc_hpm_sma.hpp"
#include "devices/nvswitch.hpp"
#include "featureDriver.hpp"
#include "overrides/overrideApply.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <format>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace nsm::features
{

namespace
{

struct KindInfo
{
    FeatureList (*features)();
    uint8_t deviceType;
    uint8_t deviceRole;
};

// EM "Kind" -> device list. A kind whose generations differ in NSM device
// role gets one entry per role, all pointing at the same list.
const std::unordered_map<std::string_view, KindInfo> deviceKinds = {
    {"gpu", {gpuFeatures, NSM_DEV_ID_GPU, NSM_DEV_ROLE_RESERVED}},
    {"cpu", {cpuFeatures, NSM_DEV_ID_CPU, NSM_DEV_ROLE_RESERVED}},
    {"nvswitch", {nvswitchFeatures, NSM_DEV_ID_SWITCH, NSM_DEV_ROLE_RESERVED}},
    {"baseboard",
     {baseboardFeatures, NSM_DEV_ID_BASEBOARD, NSM_DEV_ROLE_RESERVED}},
    {"erot", {erotFeatures, NSM_DEV_ID_EROT, NSM_DEV_ROLE_RESERVED}},
    {"bmc_irot", {irotFeatures, NSM_DEV_ID_EROT, NSM_DEV_ROLE_RESERVED}},
    {"cx7", {cxFeatures, NSM_DEV_ID_PCIE_BRIDGE, NSM_PCIE_BRIDGE_DEV_ROLE_CX7}},
    {"cx8", {cxFeatures, NSM_DEV_ID_PCIE_BRIDGE, NSM_PCIE_BRIDGE_DEV_ROLE_CX8}},
    {"cx9", {cxFeatures, NSM_DEV_ID_PCIE_BRIDGE, NSM_PCIE_BRIDGE_DEV_ROLE_CX9}},
    {"bluefield_nic",
     {bluefieldFeatures, NSM_DEV_ID_PCIE_BRIDGE,
      NSM_PCIE_BRIDGE_DEV_ROLE_CX_BLUEFIELD_NIC}},
    {"cx9_nic",
     {cxNicFeatures, NSM_DEV_ID_PCIE_BRIDGE, NSM_PCIE_BRIDGE_DEV_ROLE_CX_NIC}},
    {"cx9_io",
     {cxIoFeatures, NSM_DEV_ID_PCIE_BRIDGE, NSM_PCIE_BRIDGE_DEV_ROLE_CX9_IO}},
    {"bluefield_sma",
     {bluefieldSmaFeatures, NSM_DEV_ID_MCTP_BRIDGE,
      NSM_MCTP_BRIDGE_DEV_ROLE_BAY_C_SMA}},
    {"cx_sma",
     {cxSmaFeatures, NSM_DEV_ID_MCTP_BRIDGE, NSM_MCTP_BRIDGE_DEV_ROLE_CX_SMA}},
    {"gpu_sxm",
     {gpuSxmFeatures, NSM_DEV_ID_MCTP_BRIDGE,
      NSM_MCTP_BRIDGE_DEV_ROLE_SXM_SMA}},
    {"gpu_sma",
     {gpuSmaFeatures, NSM_DEV_ID_MCTP_BRIDGE,
      NSM_MCTP_BRIDGE_DEV_ROLE_SXM_SMA}},
    {"hmc_hpm_sma",
     {hmcHpmSmaFeatures, NSM_DEV_ID_MCTP_BRIDGE,
      NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA}},
    {"bmc_hpm_sma",
     {bmcHpmSmaFeatures, NSM_DEV_ID_MCTP_BRIDGE,
      NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA}},
    {"hmc_erot", {hmcErotFeatures, NSM_DEV_ID_EROT, NSM_DEV_ROLE_RESERVED}},
    {"bmc_erot", {bmcErotFeatures, NSM_DEV_ID_EROT, NSM_DEV_ROLE_RESERVED}},
};

/** @brief Whether a param of the list reads token, as a var() binding or as
 *  text in a string or string-array value
 */
bool usesTemplate(const FeatureList& features, const std::string& token)
{
    auto inText = [&token](const dbus::Value& value) {
        if (const auto* text = std::get_if<std::string>(&value))
        {
            return text->find(token) != std::string::npos;
        }
        if (const auto* texts = std::get_if<std::vector<std::string>>(&value))
        {
            return std::ranges::any_of(*texts, [&token](const auto& text) {
                return text.find(token) != std::string::npos;
            });
        }
        return false;
    };
    for (const auto& feature : features)
    {
        for (const auto& [name, param] : feature.params)
        {
            if (param.var == token || inText(param.value))
            {
                return true;
            }
        }
    }
    return false;
}

uuid_t makeStaticUuid(const DeviceContext& dev)
{
    const unsigned typeAndRole = (dev.deviceRole << 8) | dev.deviceType;
    if (dev.eid)
    {
        return std::format("STATIC:{}:{}:MCTP_EID:{}", typeAndRole,
                           dev.instance, *dev.eid);
    }
    return std::format("STATIC:{}:{}:NSM_DEVICE_INSTANCE_NUMBER:{}",
                       typeAndRole, dev.instance, dev.instance);
}

} // namespace

DeviceCoordinator::DeviceCoordinator(SensorManager& manager,
                                     sdbusplus::bus_t& bus,
                                     const common::Event& event,
                                     std::function<void()> onDeviceCreated) :
    manager(manager), event(event), onDeviceCreated(std::move(onDeviceCreated)),
    policy(bus, event, [this]() { start(); }),
    discovery(bus, event,
              [this](DiscoveredDevice device) { onDevice(std::move(device)); })
{}

void DeviceCoordinator::onDevice(DiscoveredDevice device)
{
    pending.push(std::move(device));
    policy.arm();
    if (policy.ready())
    {
        start();
    }
}

void DeviceCoordinator::start()
{
    // A running loop picks up devices queued while it is suspended.
    requester::Coroutine::assign(taskHandle,
                                 [this]() { return processQueue(); });
}

requester::Coroutine DeviceCoordinator::processQueue()
{
    YieldBudget budget(event);
    while (!pending.empty())
    {
        auto device = std::move(pending.front());
        pending.pop();
        std::string telemetryPath = device.telemetryPath;
        uint8_t rc = co_await resolveAndCreate(std::move(device), budget);
        if (rc != NSM_SW_SUCCESS)
        {
            // Failed before any feature was created; a later
            // InterfacesAdded for the record may retry it.
            discovery.forget(telemetryPath);
            continue;
        }
        if (onDeviceCreated)
        {
            onDeviceCreated();
        }
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    DeviceCoordinator::resolveAndCreate(DiscoveredDevice device,
                                        YieldBudget& budget)
{
    DeviceContext dev;
    dev.configPath = device.telemetryPath;
    dev.name = device.name;
    dev.kind = device.kind;
    dev.instance = device.instance;
    dev.eid = device.eid;
    dev.parentChassis = device.parentChassis;
    dev.platformPrefix = device.platformPrefix;
    dev.variables = device.variables;
    auto templateValues = policy.templateValuesFor(dev.kind, dev.name);
    for (const auto& [key, value] : templateValues)
    {
        dev.variables.insert_or_assign(key, value);
    }

    auto kindIt = deviceKinds.find(dev.kind);
    if (kindIt == deviceKinds.end())
    {
        lg2::error("Device {NAME}: unknown Kind {KIND}, device skipped", "NAME",
                   dev.name, "KIND", dev.kind);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_DATA;
    }
    const auto& kindInfo = kindIt->second;
    dev.deviceType = kindInfo.deviceType;
    dev.deviceRole = kindInfo.deviceRole;
    dev.staticUuid = makeStaticUuid(dev);
    dev.chassisPath = (chassisInventoryBasePath / dev.name).string();

    try
    {
        dev.device = manager.getNsmDeviceFromStaticUUID(dev.staticUuid);
    }
    catch (const std::exception& e)
    {
        lg2::error("Device {NAME}: binding {UUID} failed: {ERROR}", "NAME",
                   dev.name, "UUID", dev.staticUuid, "ERROR", e);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    if (!dev.device)
    {
        lg2::error("Device {NAME}: no NsmDevice for {UUID}", "NAME", dev.name,
                   "UUID", dev.staticUuid);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    FeatureList features = kindInfo.features();
    auto telemetryStats = applyOverrides(features, device.telemetry, dev.name);
    auto policyStats = applyOverrides(
        features, policy.overridesFor(dev.kind, dev.name), dev.name);
    for (const auto& [key, value] : templateValues)
    {
        if (!usesTemplate(features, "{" + key + "}"))
        {
            lg2::warning(
                "Device {NAME}: policy TemplateValues {KEY} is used by no feature",
                "NAME", dev.name, "KEY", key);
        }
    }

    lg2::info(
        "Device {NAME}: Kind={KIND} UUID={UUID} features={COUNT} telemetry applied={T_APPLIED} rejected={T_REJECTED} policy applied={P_APPLIED} rejected={P_REJECTED}",
        "NAME", dev.name, "KIND", dev.kind, "UUID", dev.staticUuid, "COUNT",
        features.size(), "T_APPLIED", telemetryStats.applied, "T_REJECTED",
        telemetryStats.rejected, "P_APPLIED", policyStats.applied, "P_REJECTED",
        policyStats.rejected);

    co_await createDeviceFeatures(manager, std::move(dev), std::move(features),
                                  budget);
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

} // namespace nsm::features
