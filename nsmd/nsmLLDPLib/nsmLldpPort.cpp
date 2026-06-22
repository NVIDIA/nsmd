/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */
#include "nsmLldpPort.hpp"

#include "device-configuration.h"
#include "network-ports.h"

#include "asyncOperationManager.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmLldpMode.hpp"
#include "nsmLldpPacket.hpp"
#include "utils.hpp"

#include <algorithm>

namespace nsm
{

namespace
{
constexpr auto kNetworkAdapterEmIntf =
    "xyz.openbmc_project.Configuration.NSM_NetworkAdapter";
} // namespace

bool isLldpModeIndexSupported(const std::vector<int64_t>& deviceModesSupported)
{
    return DEVICE_MODE_LLDP < deviceModesSupported.size() &&
           deviceModesSupported[DEVICE_MODE_LLDP] != DEVICE_MODE_NOT_SUPPORTED;
}

requester::Coroutine coIsLldpPacketSupported(uuid_t uuid, bool& supported)
{
    supported = false;
    auto subtree = utils::DBusHandler().getSubtree("/", 0,
                                                   {kNetworkAdapterEmIntf});
    for (const auto& [emPath, serviceMap] : subtree)
    {
        for (const auto& [service, interfaces] : serviceMap)
        {
            if (std::find(interfaces.begin(), interfaces.end(),
                          kNetworkAdapterEmIntf) == interfaces.end())
            {
                continue;
            }
            auto props = co_await utils::coGetAllDbusProperty(
                service, emPath, kNetworkAdapterEmIntf);
            if (props.empty())
            {
                co_return NSM_SW_ERROR;
            }
            if (!props.count("UUID") ||
                std::get<uuid_t>(props.at("UUID")) != uuid)
            {
                continue;
            }
            if (props.count("DeviceModesSupported"))
            {
                supported =
                    isLldpModeIndexSupported(std::get<std::vector<int64_t>>(
                        props.at("DeviceModesSupported")));
            }
            co_return NSM_SUCCESS;
        }
    }
    co_return NSM_SUCCESS;
}

void createLldpPacketSensorsForPort(sdbusplus::bus_t& bus,
                                    const std::shared_ptr<NsmDevice>& nsmDevice,
                                    const std::string& sensorNamePrefix,
                                    const std::string& type,
                                    const std::string& portInventoryPath,
                                    uint16_t portNumber, bool priority)
{
    const std::string lldpBase = portInventoryPath + "/LLDP/";
    auto rx = std::make_shared<NsmLldpPacket>(
        bus, sensorNamePrefix + "_LLDP_RX", type, lldpBase + "RX", portNumber,
        NSM_LLDP_DIRECTION_RX);
    auto tx = std::make_shared<NsmLldpPacket>(
        bus, sensorNamePrefix + "_LLDP_TX", type, lldpBase + "TX", portNumber,
        NSM_LLDP_DIRECTION_TX);
    nsmDevice->addSensor(rx, priority);
    nsmDevice->addSensor(tx, priority);
}

void createLldpModeSensor(sdbusplus::bus_t& bus,
                          const std::shared_ptr<NsmDevice>& nsmDevice,
                          const std::string& name, const std::string& type,
                          const std::string& networkAdapterPath)
{
    const std::string lldpObjPath = networkAdapterPath +
                                    "/Settings/Oem/Nvidia/LLDPModes";

    auto lldpMode = std::make_shared<NsmLldpMode>(
        bus, name + "_LLDPMode", type, lldpObjPath, networkAdapterPath,
        nsmDevice);
    nsmDevice->addStaticSensor(lldpMode);

    auto txHandler = std::bind_front(&NsmLldpMode::setTxMode, lldpMode);
    AsyncOperationManager::getInstance()
        ->getDispatcher(lldpObjPath)
        ->addAsyncSetOperation(
            "com.nvidia.Network.LLDP.Modes", "TXMode",
            AsyncSetOperationInfo{std::move(txHandler), lldpMode, nsmDevice});

    auto rxHandler = std::bind_front(&NsmLldpMode::setRxMode, lldpMode);
    AsyncOperationManager::getInstance()
        ->getDispatcher(lldpObjPath)
        ->addAsyncSetOperation(
            "com.nvidia.Network.LLDP.Modes", "RXMode",
            AsyncSetOperationInfo{std::move(rxHandler), lldpMode, nsmDevice});

    auto dcbxHandler = std::bind_front(&NsmLldpMode::setDcbxMode, lldpMode);
    AsyncOperationManager::getInstance()
        ->getDispatcher(lldpObjPath)
        ->addAsyncSetOperation(
            "com.nvidia.Network.LLDP.Modes", "DCBXMode",
            AsyncSetOperationInfo{std::move(dcbxHandler), lldpMode, nsmDevice});
}

} // namespace nsm
