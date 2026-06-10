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

#include "config.h"

#include "nsmProcessor.hpp"

#include "device-configuration.h"
#include "pci-links.h"

#include "nsmCommon/nsmPcieGroup.hpp"
#include "nsmCommon/nsmPciePortIntf.hpp"
#include "nsmEvent/nsmCPEREvent.hpp"
#ifdef ENABLE_SYSTEM_GUID
#include "network-ports.h"

#include <sys/random.h> // uuid_generate for sysguid
#endif
#include "platform-environmental.h"

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
#include "asyncOperationManager.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmDevice.hpp"
#include "nsmErrorInjection/nsmErrorInjectionCommon.hpp"
#include "nsmGpuOperationalStatus.hpp"
#include "nsmInterface.hpp"
#include "nsmMNNVLinkTopologyIntf.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmOemResetStatistics.hpp"
#include "nsmPCIeLinkSpeed.hpp"
#include "nsmPort/nsmPortDisableFuture.hpp"
#include "nsmPowerSmoothing.hpp"
#include "nsmReconfigPermissions.hpp"
#include "nsmSetCpuOperatingConfig.hpp"
#include "nsmSetECCMode.hpp"
#include "nsmSetEgmMode.hpp"
#include "nsmSoCPowerSmoothing.hpp"
#include "nsmWorkloadPowerProfile.hpp"

#include <stdint.h>

#include <phosphor-logging/lg2.hpp>
#ifdef NVIDIA_SHMEM
#include "sharedMemCommon.hpp"

#include <telemetry_mrd_producer.hpp>
#endif

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#define PROCESSOR_INTERFACE "xyz.openbmc_project.Configuration.NSM_Processor"

using std::filesystem::path;

namespace nsm
{

void createMIGMode(std::shared_ptr<NsmDevice> nsmDevice,
                   sdbusplus::bus::bus& bus, std::string& name,
                   std::string& type, std::string& inventoryObjPath)
{
    bool priority = false;
    auto isLongRunning = true;

    auto sensor = std::make_shared<NsmMigMode>(
        bus, name, type, inventoryObjPath, nsmDevice, isLongRunning);
    auto setMigModeEnabled = std::make_shared<NsmSetMigMode>(isLongRunning,
                                                             nsmDevice);

    nsmDevice->addSensor(sensor, priority, isLongRunning);
    nsmDevice->addSetSensor(setMigModeEnabled);

    AsyncOperationManager::getInstance()
        ->getDispatcher(inventoryObjPath)
        ->addAsyncSetOperation(
            "com.nvidia.MigMode", "MIGModeEnabled",
            {std::bind_front(&NsmSetMigMode::set, setMigModeEnabled.get()),
             sensor, nsmDevice});
}

void createPortDisableFuture(std::shared_ptr<NsmDevice> nsmDevice,
                             std::string& name, std::string& type,
                             std::string& inventoryObjPath)
{
    bool priority = false;

    size_t pos = inventoryObjPath.find_last_of('/');
    std::string basePath = inventoryObjPath;
    std::string processorName = name;
    if (pos != std::string::npos)
    {
        basePath = inventoryObjPath.substr(0, pos);
        processorName = inventoryObjPath.substr(pos + 1);
    }

    auto nvProcessorPortDisableFuture =
        std::make_shared<NsmDevicePortDisableFuture>(processorName, type,
                                                     basePath);

    nvProcessorPortDisableFuture->invoke(pdiMethod(portDisableFuture),
                                         std::vector<uint8_t>{});
    nsmDevice->addSensor(nvProcessorPortDisableFuture, priority);

    nsm::AsyncSetOperationHandler setPortDisableFutureHandler =
        std::bind(&NsmDevicePortDisableFuture::setPortDisableFuture,
                  nvProcessorPortDisableFuture, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);

    AsyncOperationManager::getInstance()
        ->getDispatcher(inventoryObjPath)
        ->addAsyncSetOperation(
            "com.nvidia.NVLink.NVLinkDisableFuture", "PortDisableFuture",
            AsyncSetOperationInfo{setPortDisableFutureHandler,
                                  nvProcessorPortDisableFuture, nsmDevice});
}

void createECCMode(std::shared_ptr<NsmDevice> nsmDevice,
                   sdbusplus::bus::bus& bus, std::string& name,
                   std::string& type, std::string& inventoryObjPath)
{
    bool priority = false;
    auto isLongRunning = true;

    auto eccIntf = std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());

    auto eccModeSensor = std::make_shared<NsmEccMode>(
        name, type, eccIntf, inventoryObjPath, isLongRunning, nsmDevice);
    auto setEccModeEnabled = std::make_shared<NsmSetEccMode>(isLongRunning,
                                                             nsmDevice);

    nsmDevice->addSensor(eccModeSensor, priority, isLongRunning);

    auto eccErrorCntSensor = std::make_shared<NsmEccErrorCounts>(
        name, type, eccIntf, inventoryObjPath);

    nsmDevice->addSensor(eccErrorCntSensor, priority);
    nsmDevice->addSetSensor(setEccModeEnabled);

    AsyncOperationManager::getInstance()
        ->getDispatcher(inventoryObjPath)
        ->addAsyncSetOperation(
            eccIntf->interface, "ECCModeEnabled",
            {std::bind_front(&NsmSetEccMode::set, setEccModeEnabled.get()),
             eccModeSensor, nsmDevice});
}

void createEDPpScalingFactor(std::shared_ptr<NsmDevice> nsmDevice,
                             sdbusplus::bus::bus& bus, std::string& name,
                             std::string& type, std::string& inventoryObjPath)
{
    bool priority = false;

    auto eDPpIntf = std::make_shared<EDPpLocal>(bus, inventoryObjPath.c_str());
    auto resetEdppAsyncIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus, inventoryObjPath.c_str(), nsmDevice);

    auto sensor = std::make_shared<NsmEDPpScalingFactor>(
        name, type, inventoryObjPath, eDPpIntf, resetEdppAsyncIntf);

    nsm::AsyncSetOperationHandler patchSetPoint = std::bind(
        &NsmEDPpScalingFactor::patchSetPoint, sensor, std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3);

    nsmDevice->addSensor(sensor, priority);
    AsyncOperationManager::getInstance()
        ->getDispatcher(inventoryObjPath)
        ->addAsyncSetOperation(
            "com.nvidia.Edpp", "SetPoint",
            AsyncSetOperationInfo{patchSetPoint, sensor, nsmDevice});

    auto maxEdppSensor = std::make_shared<NsmMaxEDPpLimit>(name, type,
                                                           eDPpIntf);
    auto minEdppSensor = std::make_shared<NsmMinEDPpLimit>(name, type,
                                                           eDPpIntf);
    nsmDevice->addStaticSensor(maxEdppSensor);
    nsmDevice->addStaticSensor(minEdppSensor);
}

void createCpuOperatingConfig(std::shared_ptr<NsmDevice> nsmDevice,
                              sdbusplus::bus::bus& bus, std::string& name,
                              std::string& type, std::string& inventoryObjPath)
{
    bool priority = false;

    auto cpuOperatingConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, inventoryObjPath.c_str());
    auto smUtilizationIntf =
        std::make_shared<SMUtilizationIntf>(bus, inventoryObjPath.c_str());

    auto clockFreqSensor = std::make_shared<NsmCurrClockFreq>(
        name, type, cpuOperatingConfigIntf, inventoryObjPath);
    auto clockLimitSensor = std::make_shared<NsmClockLimitGraphics>(
        name, type, cpuOperatingConfigIntf, inventoryObjPath);
    auto minGraphicsClockFreq = std::make_shared<NsmMinGraphicsClockLimit>(
        name, type, cpuOperatingConfigIntf, inventoryObjPath);
    auto maxGraphicsClockFreq = std::make_shared<NsmMaxGraphicsClockLimit>(
        name, type, cpuOperatingConfigIntf, inventoryObjPath);

    bool isLongRunning = true;

    auto currentUtilization = std::make_shared<NsmCurrentUtilization>(
        name + "_CurrentUtilization", type, cpuOperatingConfigIntf,
        smUtilizationIntf, inventoryObjPath, isLongRunning, nsmDevice);

    auto defaultBoostClockSpeed = std::make_shared<NsmDefaultBoostClockSpeed>(
        name, type, cpuOperatingConfigIntf);
    auto defaultBaseClockSpeed = std::make_shared<NsmDefaultBaseClockSpeed>(
        name, type, cpuOperatingConfigIntf);
    nsmDevice->addStaticSensor(defaultBaseClockSpeed);
    nsmDevice->addStaticSensor(defaultBoostClockSpeed);

    nsmDevice->addSensor(clockFreqSensor, priority);
    nsmDevice->addSensor(clockLimitSensor, priority);
    nsmDevice->addSensor(currentUtilization, priority, isLongRunning);

    nsmDevice->addStaticSensor(minGraphicsClockFreq);
    nsmDevice->addStaticSensor(maxGraphicsClockFreq);

    AsyncOperationManager::getInstance()
        ->getDispatcher(inventoryObjPath)
        ->addAsyncSetOperation(
            cpuOperatingConfigIntf->interface, "SpeedConfig",
            AsyncSetOperationInfo{
                std::bind_front(setCPUSpeedConfig, GRAPHICS_CLOCK),
                clockLimitSensor, nsmDevice});
}

void createMemCapacityUtil(std::shared_ptr<NsmDevice> nsmDevice,
                           std::string& name, std::string& type,
                           std::string& inventoryObjPath)
{
    bool priority = false;
    auto isLongRunning = true;

    auto totalMemorySensor = std::make_shared<NsmTotalMemory>(name, type);
    auto provider = NsmInterfaceProvider<DimmMemoryMetricsIntf>(
        name, type, dbus::Interfaces{inventoryObjPath});
    auto sensor = std::make_shared<NsmMemoryCapacityUtil>(
        provider, totalMemorySensor, isLongRunning, nsmDevice);
    nsmDevice->addSensor(sensor, priority, isLongRunning);
}

void createTotalNvLinksCount(std::shared_ptr<NsmDevice> nsmDevice,
                             sdbusplus::bus::bus& bus, std::string& name,
                             std::string& type, std::string& inventoryObjPath)
{
    bool priority = false;

    auto totalNvLinkInterface =
        std::make_shared<TotalNvLinkInterface>(bus, inventoryObjPath.c_str());
    auto totalNvLinkSensor = std::make_shared<NsmTotalNvLinks>(
        name, type, totalNvLinkInterface, inventoryObjPath);
    nsmDevice->addSensor(totalNvLinkSensor, priority);
}

void createGpuOperationalStatus(std::shared_ptr<NsmDevice> nsmDevice,
                                sdbusplus::bus::bus& bus, std::string& name,
                                std::string& type,
                                std::string& inventoryObjPath)
{
    bool priority = false;

    auto gpuOpStatusSensor = std::make_shared<NsmGpuOperationalStatus>(
        bus, name, type, inventoryObjPath);
    nsmDevice->addSensor(gpuOpStatusSensor, priority);
}

void createEGMMode(std::shared_ptr<NsmDevice> nsmDevice,
                   sdbusplus::bus::bus& bus, std::string& name,
                   std::string& type, std::string& inventoryObjPath)
{
    bool priority = false;

    auto sensor = std::make_shared<NsmEgmMode>(bus, name, type,
                                               inventoryObjPath);

    nsmDevice->addSensor(sensor, priority);

    AsyncOperationManager::getInstance()
        ->getDispatcher(inventoryObjPath)
        ->addAsyncSetOperation(
            "com.nvidia.EgmMode", "EGMModeEnabled",
            AsyncSetOperationInfo{std::bind_front(setEgmModeEnabled), sensor,
                                  nsmDevice});
}

void createPowerSmoothing(std::shared_ptr<NsmDevice> nsmDevice,
                          sdbusplus::bus::bus& bus, std::string& name,
                          std::string& type, std::string& inventoryObjPath)
{
    bool priority = false;

    std::shared_ptr<OemPowerSmoothingFeatIntfV2> pwrSmoothingIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, inventoryObjPath,
                                                      nsmDevice);

    AsyncOperationManager::getInstance()
        ->getDispatcher(pwrSmoothingIntf->getInventoryObjPath())
        ->addAsyncSetOperation(
            pwrSmoothingIntf->PowerSmoothingIntf::interface,
            "PowerSmoothingEnabled",
            AsyncSetOperationInfo{
                std::bind_front(
                    &OemPowerSmoothingFeatIntfV2::setPowerSmoothingEnabled,
                    pwrSmoothingIntf),
                {},
                nsmDevice});

    AsyncOperationManager::getInstance()
        ->getDispatcher(pwrSmoothingIntf->getInventoryObjPath())
        ->addAsyncSetOperation(
            pwrSmoothingIntf->PowerSmoothingIntf::interface,
            "ImmediateRampDownEnabled",
            AsyncSetOperationInfo{
                std::bind_front(
                    &OemPowerSmoothingFeatIntfV2::setImmediateRampDownEnabled,
                    pwrSmoothingIntf),
                {},
                nsmDevice});
    auto controlSensor = std::make_shared<NsmPowerSmoothingV2>(
        name, type, inventoryObjPath, pwrSmoothingIntf, nsmDevice);
    nsmDevice->addDeviceSensors(controlSensor);

    auto lifetimeCicuitrySensor = std::make_shared<NsmHwCircuitryTelemetry>(
        name, type, inventoryObjPath, pwrSmoothingIntf);
    nsmDevice->addDeviceSensors(lifetimeCicuitrySensor);

    std::shared_ptr<OemAdminProfileIntfV2> adminProfileIntf =
        std::make_shared<OemAdminProfileIntfV2>(bus, inventoryObjPath,
                                                nsmDevice);

    AsyncOperationManager::getInstance()
        ->getDispatcher(adminProfileIntf->getInventoryObjPath())
        ->addAsyncSetOperation(
            adminProfileIntf->AdminPowerProfileIntf::interface,
            "TMPFloorPercent",
            AsyncSetOperationInfo{
                std::bind_front(&OemAdminProfileIntfV2::setTmpFloorPercent,
                                adminProfileIntf),
                {},
                nsmDevice});
    AsyncOperationManager::getInstance()
        ->getDispatcher(adminProfileIntf->getInventoryObjPath())
        ->addAsyncSetOperation(
            adminProfileIntf->AdminPowerProfileIntf::interface, "RampUpRate",
            AsyncSetOperationInfo{
                std::bind_front(&OemAdminProfileIntfV2::setRampUpRate,
                                adminProfileIntf),
                {},
                nsmDevice});

    AsyncOperationManager::getInstance()
        ->getDispatcher(adminProfileIntf->getInventoryObjPath())
        ->addAsyncSetOperation(
            adminProfileIntf->AdminPowerProfileIntf::interface, "RampDownRate",
            AsyncSetOperationInfo{
                std::bind_front(&OemAdminProfileIntfV2::setRampDownRate,
                                adminProfileIntf),
                {},
                nsmDevice});
    AsyncOperationManager::getInstance()
        ->getDispatcher(adminProfileIntf->getInventoryObjPath())
        ->addAsyncSetOperation(
            adminProfileIntf->AdminPowerProfileIntf::interface,
            "RampDownHysteresis",
            AsyncSetOperationInfo{
                std::bind_front(&OemAdminProfileIntfV2::setRampDownHysteresis,
                                adminProfileIntf),
                {},
                nsmDevice});
    AsyncOperationManager::getInstance()
        ->getDispatcher(adminProfileIntf->getInventoryObjPath())
        ->addAsyncSetOperation(
            adminProfileIntf->AdminPowerProfileIntf::interface,
            "SecondaryPowerFloorSetting",
            AsyncSetOperationInfo{
                std::bind_front(&OemAdminProfileIntfV2::setSecondaryPowerFloor,
                                adminProfileIntf),
                {},
                nsmDevice});
    AsyncOperationManager::getInstance()
        ->getDispatcher(adminProfileIntf->getInventoryObjPath())
        ->addAsyncSetOperation(
            adminProfileIntf->AdminPowerProfileIntf::interface,
            "PrimaryFloorActivationWindowMultiplier",
            AsyncSetOperationInfo{
                std::bind_front(&OemAdminProfileIntfV2::
                                    setPrimaryFloorActivationWindowMultiplier,
                                adminProfileIntf),
                {},
                nsmDevice});
    AsyncOperationManager::getInstance()
        ->getDispatcher(adminProfileIntf->getInventoryObjPath())
        ->addAsyncSetOperation(
            adminProfileIntf->AdminPowerProfileIntf::interface,
            "PrimaryFloorTargetWindowMultiplier",
            AsyncSetOperationInfo{
                std::bind_front(&OemAdminProfileIntfV2::
                                    setPrimaryFloorTargetWindowMultiplier,
                                adminProfileIntf),
                {},
                nsmDevice});
    AsyncOperationManager::getInstance()
        ->getDispatcher(adminProfileIntf->getInventoryObjPath())
        ->addAsyncSetOperation(
            adminProfileIntf->AdminPowerProfileIntf::interface,
            "PrimaryFloorActivationOffset",
            AsyncSetOperationInfo{
                std::bind_front(
                    &OemAdminProfileIntfV2::setPrimaryFloorActivationOffset,
                    adminProfileIntf),
                {},
                nsmDevice});

    auto adminProfileSensor =
        std::make_shared<NsmPowerSmoothingAdminOverrideV2>(
            name, type, adminProfileIntf, inventoryObjPath, nsmDevice);
    nsmDevice->addDeviceSensors(adminProfileSensor);

    auto getAllPowerProfileSensor =
        std::make_shared<NsmPowerProfileCollectionV2>(
            name, type, inventoryObjPath, nsmDevice);
    nsmDevice->addDeviceSensors(getAllPowerProfileSensor);

    std::shared_ptr<OemCurrentPowerProfileIntf> pwrSmoothingCurProfileIntf =
        std::make_shared<OemCurrentPowerProfileIntf>(
            bus, inventoryObjPath, adminProfileIntf->getInventoryObjPath(),
            nsmDevice);

    auto currentProfileSensor =
        std::make_shared<NsmCurrentPowerSmoothingProfileV2>(
            name, type, inventoryObjPath, pwrSmoothingCurProfileIntf,
            getAllPowerProfileSensor, adminProfileSensor, nsmDevice);

    std::shared_ptr<NsmPowerSmoothingAction> pwrSmoothingAction =
        std::make_shared<NsmPowerSmoothingAction>(
            bus, name, type, inventoryObjPath, currentProfileSensor, nsmDevice);

    nsmDevice->addDeviceSensors(pwrSmoothingAction);
    // power smoothing supported version
    std::string revisionPath = inventoryObjPath + "/power_smoothing_metadata";
    std::shared_ptr<RevisionIntf> revisionIntf =
        std::make_shared<RevisionIntf>(bus, revisionPath.c_str());
    auto pwrSmoothingSupportedVersionSensor =
        std::make_shared<NsmPowerSmoothingSupportedVersion>(
            name, type, revisionPath, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
            revisionIntf);
    revisionIntf->version("nan");
    nsmDevice->addStaticSensor(pwrSmoothingSupportedVersionSensor);
    nsmDevice->addSensor(getAllPowerProfileSensor, priority);
    nsmDevice->addSensor(adminProfileSensor, priority);
    nsmDevice->addSensor(controlSensor, priority);
    nsmDevice->addSensor(lifetimeCicuitrySensor, priority);
    nsmDevice->addSensor(currentProfileSensor, priority);
}

void createMNNVLTopology(std::shared_ptr<NsmDevice> nsmDevice,
                         sdbusplus::bus::bus& bus, std::string& name,
                         std::string& type, std::string& inventoryObjPath)
{
    // create the interface
    auto mnnvlinkTopologyIntf = std::make_shared<NsmMNNVLinkTopologyIntf>(
        bus, inventoryObjPath.c_str());
    // create the interface object
    auto assetMNNVLinkTopologyObject =
        NsmInterfaceProvider<NsmMNNVLinkTopologyIntf>(
            name, type, inventoryObjPath, mnnvlinkTopologyIntf);

    // create MNNVLinkTopology sensors
    nsmDevice->addStaticSensor(
        std::make_shared<NsmInventoryProperty<NsmMNNVLinkTopologyIntf>>(
            assetMNNVLinkTopologyObject, GPU_IBGUID));
    nsmDevice->addStaticSensor(
        std::make_shared<NsmInventoryProperty<NsmMNNVLinkTopologyIntf>>(
            assetMNNVLinkTopologyObject, CHASSIS_SERIAL_NUMBER));
    nsmDevice->addStaticSensor(
        std::make_shared<NsmInventoryProperty<NsmMNNVLinkTopologyIntf>>(
            assetMNNVLinkTopologyObject, TRAY_SLOT_NUMBER));
    nsmDevice->addStaticSensor(
        std::make_shared<NsmInventoryProperty<NsmMNNVLinkTopologyIntf>>(
            assetMNNVLinkTopologyObject, TRAY_SLOT_INDEX));
    nsmDevice->addStaticSensor(
        std::make_shared<NsmInventoryProperty<NsmMNNVLinkTopologyIntf>>(
            assetMNNVLinkTopologyObject, GPU_HOST_ID));
    nsmDevice->addStaticSensor(
        std::make_shared<NsmInventoryProperty<NsmMNNVLinkTopologyIntf>>(
            assetMNNVLinkTopologyObject, GPU_MODULE_ID));
    nsmDevice->addStaticSensor(
        std::make_shared<NsmInventoryProperty<NsmMNNVLinkTopologyIntf>>(
            assetMNNVLinkTopologyObject, GPU_NVLINK_PEER_TYPE));
}

void createPCIe(std::shared_ptr<NsmDevice> nsmDevice, sdbusplus::bus::bus& bus,
                std::string& name, std::string& type,
                std::string& inventoryObjPath,
                dbus::PropertyMap& allCurrentIfaceProperties)
{
    bool priority = false;

    uint64_t deviceId{};
    if (allCurrentIfaceProperties.count("DeviceId"))
    {
        deviceId = std::get<uint64_t>(allCurrentIfaceProperties.at("DeviceId"));
    }
    int count{};
    if (allCurrentIfaceProperties.count("Count"))
    {
        count = std::get<uint64_t>(allCurrentIfaceProperties.at("Count"));
    }

    auto pcieECCIntf = std::make_shared<PCieEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    auto pcieDeviceProvider = NsmInterfaceProvider(name, type, inventoryObjPath,
                                                   pcieECCIntf);
    nsmDevice->addSensor(std::make_shared<NsmPCIeLinkSpeed<PCIeEccIntf>>(
                             pcieDeviceProvider, deviceId, false),
                         priority);
    for (auto idx = 0; idx < count; idx++)
    {
        auto pcieObjPath = inventoryObjPath + "/Ports/PCIe_" +
                           std::to_string(idx); // port metrics dbus path
        auto pciePortIntf = std::make_shared<PCieEccIntf>(bus,
                                                          pcieObjPath.c_str());
        auto pciPortSensor = std::make_shared<NsmPciePortIntf>(bus, name, type,
                                                               pcieObjPath);
        auto sensorGroup2 = std::make_shared<NsmPciGroup2>(
            name, type, pcieECCIntf, pciePortIntf, deviceId, inventoryObjPath);
        auto sensorGroup3 = std::make_shared<NsmPciGroup3>(
            name, type, pcieECCIntf, pciePortIntf, deviceId, inventoryObjPath);
        auto sensorGroup4 = std::make_shared<NsmPciGroup4>(
            name, type, pcieECCIntf, pciePortIntf, deviceId, inventoryObjPath);
        nsmDevice->addDeviceSensors(pciPortSensor);
        nsmDevice->addSensor(sensorGroup2, priority);
        nsmDevice->addSensor(sensorGroup3, priority);
        nsmDevice->addSensor(sensorGroup4, priority);
    }
}

void createProcessorPerformance(std::shared_ptr<NsmDevice> nsmDevice,
                                sdbusplus::bus::bus& bus, std::string& name,
                                std::string& type,
                                std::string& inventoryObjPath,
                                dbus::PropertyMap& allCurrentIfaceProperties)
{
    bool priority = false;

    uint64_t deviceId{};
    if (allCurrentIfaceProperties.count("DeviceId"))
    {
        deviceId = std::get<uint64_t>(allCurrentIfaceProperties.at("DeviceId"));
    }

    bool isLongRunning = true;

    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus, inventoryObjPath.c_str());

    auto throttleReasonSensor = std::make_shared<NsmProcessorThrottleReason>(
        name, type, processorPerfIntf, inventoryObjPath);

    auto throttleDurationSensor =
        std::make_shared<NsmProcessorThrottleDuration>(
            name, type, processorPerfIntf, inventoryObjPath, isLongRunning,
            nsmDevice);

    auto gpuUtilSensor = std::make_shared<NsmAccumGpuUtilTime>(
        name, type, processorPerfIntf, inventoryObjPath);
    auto pciRxTxSensor = std::make_shared<NsmPciGroup5>(
        name, type, processorPerfIntf, deviceId, inventoryObjPath);

    nsmDevice->addSensor(gpuUtilSensor, priority);
    nsmDevice->addSensor(pciRxTxSensor, priority);
    nsmDevice->addSensor(throttleReasonSensor, priority);
    nsmDevice->addSensor(throttleDurationSensor, priority, isLongRunning);
}

void createPowerCap(std::shared_ptr<NsmDevice> nsmDevice,
                    sdbusplus::bus::bus& bus, std::string& name,
                    std::string& type, std::string& inventoryObjPath,
                    dbus::PropertyMap& allCurrentIfaceProperties,
                    SensorManager& manager)
{
    std::vector<std::string> candidateForList{};
    if (allCurrentIfaceProperties.count("CompositeNumericSensors"))
    {
        candidateForList = std::get<std::vector<std::string>>(
            allCurrentIfaceProperties.at("CompositeNumericSensors"));
    }

    bool priority = false;

    // create power cap , clear power cap and power limit interface
    auto powerCapIntf = std::make_shared<NsmPowerCapIntf>(
        bus, inventoryObjPath.c_str(), name, candidateForList, nsmDevice);

    AsyncOperationManager::getInstance()
        ->getDispatcher(inventoryObjPath)
        ->addAsyncSetOperation(
            powerCapIntf->interface, "PowerCap",
            AsyncSetOperationInfo{
                std::bind_front(&NsmPowerCapIntf::setPowerCap, powerCapIntf),
                {},
                nsmDevice});

    auto clearPowerCapIntf =
        std::make_shared<NsmClearPowerCapIntf>(bus, inventoryObjPath.c_str());

    auto clearPowerCapAsyncIntf = std::make_shared<NsmClearPowerCapAsyncIntf>(
        bus, inventoryObjPath.c_str(), nsmDevice, powerCapIntf,
        clearPowerCapIntf);

    auto powerLimitIntf =
        std::make_shared<PowerLimitIface>(bus, inventoryObjPath.c_str());

    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus, inventoryObjPath.c_str());

    // create sensors for power cap properties
    auto powerCap = std::make_shared<NsmPowerCap>(
        name, type, powerCapIntf, candidateForList, persistencyIntf,
        inventoryObjPath);
    nsmDevice->addSensor(powerCap, priority);
    nsmDevice->addCapabilityRefreshSensor(powerCap);
    manager.powerCapList.emplace_back(powerCap);

    auto defaultPowerCap = std::make_shared<NsmDefaultPowerCap>(
        name, type, clearPowerCapIntf, clearPowerCapAsyncIntf);
    manager.defaultPowerCapList.emplace_back(defaultPowerCap);

    auto maxPowerCap = std::make_shared<NsmMaxPowerCap>(
        name, type, powerCapIntf, powerLimitIntf, inventoryObjPath);
    manager.maxPowerCapList.emplace_back(maxPowerCap);

    auto minPowerCap = std::make_shared<NsmMinPowerCap>(
        name, type, powerCapIntf, powerLimitIntf, inventoryObjPath);
    manager.minPowerCapList.emplace_back(minPowerCap);

    nsmDevice->addStaticSensor(defaultPowerCap);
    nsmDevice->addStaticSensor(maxPowerCap);
    nsmDevice->addStaticSensor(minPowerCap);
}

void createReconfigPermissions(std::shared_ptr<NsmDevice> nsmDevice,
                               sdbusplus::bus::bus& bus, std::string& name,
                               [[maybe_unused]] std::string& type,
                               std::string& inventoryObjPath,
                               dbus::PropertyMap& allCurrentIfaceProperties)
{
    bool priority = false;

    std::vector<std::string> featuresNames{};
    if (allCurrentIfaceProperties.count("Features"))
    {
        featuresNames = std::get<std::vector<std::string>>(
            allCurrentIfaceProperties.at("Features"));
    }

    std::map<ReconfigSettingsIntf::FeatureType, std::string> features;
    for (auto& featureName : featuresNames)
    {
        auto feature = ReconfigSettingsIntf::convertFeatureTypeFromString(
            "com.nvidia.InbandReconfigSettings.FeatureType." + featureName);
        features[feature] = featureName;
    }
    for (auto [feature, featureName] : features)
    {
        auto hostConfigPdiPath = inventoryObjPath +
                                 "/InbandReconfigPermissions/" + featureName;
        auto doeConfigPdiPath = inventoryObjPath + "/DOEReconfigPermissions/" +
                                featureName;

        auto hostConfigIntf = std::make_shared<ReconfigSettingsIntf>(
            bus, hostConfigPdiPath.c_str());
        auto doeConfigIntf = std::make_shared<ReconfigSettingsIntf>(
            bus, doeConfigPdiPath.c_str());
        auto sensor = std::make_shared<NsmReconfigPermissions>(
            name, featureName, hostConfigPdiPath, doeConfigPdiPath, feature,
            hostConfigIntf, doeConfigIntf);
        nsmDevice->addSensor(sensor, priority);
        // Patch AllowOneShotConfig for HOST
        nsm::AsyncSetOperationHandler patchHostOneShotConfig =
            std::bind(&NsmReconfigPermissions::patchHostOneShotConfig, sensor,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(hostConfigPdiPath)
            ->addAsyncSetOperation("com.nvidia.InbandReconfigSettings",
                                   "AllowOneShotConfig",
                                   AsyncSetOperationInfo{patchHostOneShotConfig,
                                                         sensor, nsmDevice});

        // Patch AllowOneShotConfig for DOE
        nsm::AsyncSetOperationHandler patchDOEOneShotConfig =
            std::bind(&NsmReconfigPermissions::patchDOEOneShotConfig, sensor,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(doeConfigPdiPath)
            ->addAsyncSetOperation("com.nvidia.InbandReconfigSettings",
                                   "AllowOneShotConfig",
                                   AsyncSetOperationInfo{patchDOEOneShotConfig,
                                                         sensor, nsmDevice});

        // Patch AllowPersistentConfig for HOST
        nsm::AsyncSetOperationHandler patchHostPersistentConfig =
            std::bind(&NsmReconfigPermissions::patchHostPersistentConfig,
                      sensor, std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(hostConfigPdiPath)
            ->addAsyncSetOperation(
                "com.nvidia.InbandReconfigSettings", "AllowPersistentConfig",
                AsyncSetOperationInfo{patchHostPersistentConfig, sensor,
                                      nsmDevice});

        // Patch AllowPersistentConfig for DOE
        nsm::AsyncSetOperationHandler patchDOEPersistentConfig =
            std::bind(&NsmReconfigPermissions::patchDOEPersistentConfig, sensor,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(doeConfigPdiPath)
            ->addAsyncSetOperation(
                "com.nvidia.InbandReconfigSettings", "AllowPersistentConfig",
                AsyncSetOperationInfo{patchDOEPersistentConfig, sensor,
                                      nsmDevice});

        // Patch AllowFLRPersistentConfig for HOST
        nsm::AsyncSetOperationHandler patchHostFLRPersistentConfig =
            std::bind(&NsmReconfigPermissions::patchHostFLRPersistentConfig,
                      sensor, std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(hostConfigPdiPath)
            ->addAsyncSetOperation(
                "com.nvidia.InbandReconfigSettings", "AllowFLRPersistentConfig",
                AsyncSetOperationInfo{patchHostFLRPersistentConfig, sensor,
                                      nsmDevice});

        // Patch AllowFLRPersistentConfig for DOE
        nsm::AsyncSetOperationHandler patchDOEFLRPersistentConfig =
            std::bind(&NsmReconfigPermissions::patchDOEFLRPersistentConfig,
                      sensor, std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3);
        AsyncOperationManager::getInstance()
            ->getDispatcher(doeConfigPdiPath)
            ->addAsyncSetOperation(
                "com.nvidia.InbandReconfigSettings", "AllowFLRPersistentConfig",
                AsyncSetOperationInfo{patchDOEFLRPersistentConfig, sensor,
                                      nsmDevice});
    }
}

void createWorkloadPowerProfile(std::shared_ptr<NsmDevice> nsmDevice,
                                sdbusplus::bus::bus& bus, std::string& name,
                                std::string& type,
                                std::string& inventoryObjPath,
                                dbus::PropertyMap& allCurrentIfaceProperties)
{
    lg2::info("NSM_WorkloadPowerProfile added");
    bool priority = false;

    std::vector<std::string> profileIdMap{};
    if (allCurrentIfaceProperties.count("ProfileIdMap"))
    {
        profileIdMap = std::get<std::vector<std::string>>(
            allCurrentIfaceProperties.at("ProfileIdMap"));
    }

    auto profileMapper = std::make_shared<NsmWorkLoadProfileEnum>(name, type,
                                                                  profileIdMap);
    nsmDevice->addStaticSensor(profileMapper);

    std::shared_ptr<OemProfileInfoIntf> profileStatusInfoIntf =
        std::make_shared<OemProfileInfoIntf>(bus, inventoryObjPath, nsmDevice);

    std::shared_ptr<NsmWorkloadProfileInfoAsyncIntf> profileInfoAsyncIntf =
        std::make_shared<NsmWorkloadProfileInfoAsyncIntf>(
            bus, inventoryObjPath.c_str(), nsmDevice);
    profileInfoAsyncIntf->workloadProfileStatusSensor =
        std::make_shared<NsmWorkLoadProfileStatus>(name, type, inventoryObjPath,
                                                   profileStatusInfoIntf,
                                                   profileInfoAsyncIntf);
    nsmDevice->addSensor(profileInfoAsyncIntf->workloadProfileStatusSensor,
                         priority);

    auto getAllPowerProfileSensor =
        std::make_shared<NsmWorkloadPowerProfileCollection>(
            name, type, inventoryObjPath, nsmDevice);
    nsmDevice->addStaticSensor(getAllPowerProfileSensor);

    auto workloadPowerProfilePageCollection =
        std::make_shared<NsmWorkloadPowerProfilePageCollection>(
            name, type, inventoryObjPath, nsmDevice);
    nsmDevice->addStaticSensor(workloadPowerProfilePageCollection);

    uint16_t firstPageIndex = 0;
    auto firstPage = std::make_shared<NsmWorkloadPowerProfilePage>(
        name, type, inventoryObjPath, nsmDevice, getAllPowerProfileSensor,
        workloadPowerProfilePageCollection, profileMapper, firstPageIndex);
    nsmDevice->addSensor(firstPage, priority);
}
NsmAcceleratorIntf::NsmAcceleratorIntf(sdbusplus::bus::bus& bus,
                                       std::string& name, std::string& type,
                                       std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    acceleratorIntf =
        std::make_unique<AcceleratorIntf>(bus, inventoryObjPath.c_str());
    acceleratorIntf->type(accelaratorType::GPU);
}

#ifdef NVIDIA_RESET_METRICS
NsmResetCountersSupportedIntf::NsmResetCountersSupportedIntf(
    sdbusplus::bus::bus& bus, std::string& name, std::string& type,
    std::string& inventoryObjPath) : NsmObject(name, type)
{
    resetMetricsSupportedIntf =
        std::make_unique<resetMetricsSupported>(bus, inventoryObjPath.c_str());
}
#endif

NsmProcessorAssociation::NsmProcessorAssociation(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    const std::string& inventoryObjPath,
    const std::vector<utils::Association>& associations) : NsmObject(name, type)
{
    associationDef = std::make_unique<AssociationDefinitionsIntf>(
        bus, inventoryObjPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDef->associations(associations_list);
}

NsmUuidIntf::NsmUuidIntf(sdbusplus::bus::bus& bus, std::string& name,
                         std::string& type, std::string& inventoryObjPath,
                         uuid_t uuid) :
    NsmObject(name, type), inventoryObjPath(inventoryObjPath)
{
    uuidIntf = std::make_unique<UuidIntf>(bus, inventoryObjPath.c_str());
    uuidIntf->uuid(uuid);
}

void NsmUuidIntf::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(uuidIntf->interface);
    nv::sensor_aggregation::DbusVariantType valueVariant{uuidIntf->uuid()};
    std::vector<uint8_t> smbusData = {};
    std::string propName = "UUID";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, valueVariant);
#endif
}

requester::Coroutine NsmUuidIntf::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    mctp::MctpDiscovery& mctpDiscovery = mctp::MctpDiscovery::getInstance();
    uuid_t deviceUuid;
    auto rc = co_await getDeviceUUID(nsmDevice, mctpDiscovery, deviceUuid);
    if (rc == NSM_SW_SUCCESS && !deviceUuid.empty())
    {
        uuidIntf->uuid(deviceUuid);
        updateMetricOnSharedMemory();
        co_return NSM_SUCCESS;
    }
    co_return NSM_ERROR;
}

#ifdef ENABLE_SYSTEM_GUID
NsmSysGuidIntf::NsmSysGuidIntf(sdbusplus::bus::bus& bus, std::string& name,
                               std::string& type,
                               std::string& inventoryObjPath) :
    NsmObject(name, type), inventoryObjPath(inventoryObjPath)
{
    sysguidIntf = std::make_unique<SysGuidIntf>(bus, inventoryObjPath.c_str());
    sysguidIntf->sysGUID();
}

uint8_t NsmSysGuidIntf::sysGUID[8] = {0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00};
bool NsmSysGuidIntf::sysGuidGenerated = false;

requester::Coroutine
    NsmSysGuidIntf::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request readSysGuid(sizeof(nsm_msg_hdr) +
                        sizeof(struct nsm_get_system_guid_req));
    auto readSysGuidMsg = reinterpret_cast<struct nsm_msg*>(readSysGuid.data());

    auto rc = encode_get_system_guid_req(0, readSysGuidMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmGetSysGuid encode_get_system_guid_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> readSysGuidResponseMsg;
    size_t readSysGuidResponseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), readSysGuid,
                                      readSysGuidResponseMsg,
                                      readSysGuidResponseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmGetSysGuid SendRecvNsmMsg failed with RC={RC}, eid={EID}", "RC",
            rc, "EID", nsmDevice->getEid());
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint8_t data[8] = {0};
    uint16_t dataLen = 8;

    rc = decode_get_system_guid_resp(readSysGuidResponseMsg.get(),
                                     readSysGuidResponseLen, &cc, &reasonCode,
                                     data, dataLen);

    LG2_ERROR_FLT(
        "NsmGetSysGuid decode_get_system_guid_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        bool sysGuidAllZeros = true;
        for (auto i = 0; i < 8; i++)
        {
            if (data[i] != 0x00)
            {
                sysGuidAllZeros = false;
            }
        }

        if (!sysGuidGenerated)
        {
            lg2::info("First instance, generating SysGUID");
            if (sysGuidAllZeros)
            {
                lg2::info("GPU returned 0x00's, generating random SysGUID");

                for (auto i = 0; i < 8; i++)
                {
                    auto sysrc = getrandom(sysGUID, 8, 0);

                    if (sysrc != 8)
                    {
                        lg2::error("getrandom failed. Return={RC}", "RC",
                                   sysrc);
                    }
                }
            }
            else
            {
                lg2::info("GPU returned a SysGUID, using it");
                for (auto i = 0; i < 8; i++)
                {
                    sysGUID[i] = data[i];
                }
            }
            sysGuidGenerated = true;
        }

        bool setSysGuidNeeded = false;
        for (auto i = 0; i < 8; i++)
        {
            if (data[i] != sysGUID[i])
            {
                setSysGuidNeeded = true;
                break;
            }
        }

        if (setSysGuidNeeded)
        {
            Request setSysGuid(sizeof(nsm_msg_hdr) +
                               sizeof(struct nsm_set_system_guid_req));
            auto setSysGuidMsg =
                reinterpret_cast<struct nsm_msg*>(setSysGuid.data());

            rc = encode_set_system_guid_req(0, setSysGuidMsg, sysGUID, 8);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::debug(
                    "NsmGetSysGuid encode_set_system_guid_req failed. eid={EID} rc={RC}",
                    "EID", nsmDevice->getEid(), "RC", rc);
                co_return rc;
            }

            std::shared_ptr<const nsm_msg> setSysGuidResponseMsg;
            size_t setSysGuidResponseLen = 0;
            rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), setSysGuid,
                                              setSysGuidResponseMsg,
                                              setSysGuidResponseLen, false);
            if (rc)
            {
                lg2::debug(
                    "NsmGetSysGuid SendRecvNsmMsg failed with RC={RC}, eid={EID}",
                    "RC", rc, "EID", nsmDevice->getEid());
                co_return rc;
            }

            Request reReadSysGuid(sizeof(nsm_msg_hdr) +
                                  sizeof(struct nsm_get_system_guid_req));
            auto reReadSysGuidMsg =
                reinterpret_cast<struct nsm_msg*>(reReadSysGuid.data());

            rc = encode_get_system_guid_req(0, reReadSysGuidMsg);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::debug(
                    "NsmGetSysGuid encode_get_system_guid_req failed. eid={EID} rc={RC}",
                    "EID", nsmDevice->getEid(), "RC", rc);
                co_return rc;
            }

            std::shared_ptr<const nsm_msg> reReadSysGuidResponseMsg;
            size_t reReadSysGuidResponseLen = 0;
            rc = co_await nsmDevice->sensorIO(
                nsmDevice->getEid(), reReadSysGuid, reReadSysGuidResponseMsg,
                reReadSysGuidResponseLen, false);
            if (rc)
            {
                lg2::debug(
                    "NsmGetSysGuid SendRecvNsmMsg failed with RC={RC}, eid={EID}",
                    "RC", rc, "EID", nsmDevice->getEid());
                co_return rc;
            }

            uint8_t cc = ERR_NULL;
            reasonCode = ERR_NULL;
            dataLen = 8;

            rc = decode_get_system_guid_resp(reReadSysGuidResponseMsg.get(),
                                             readSysGuidResponseLen, &cc,
                                             &reasonCode, data, dataLen);
        }

        // convert it to a string
        std::ostringstream oss;
        for (auto& guidtoken : data)
        {
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(guidtoken);
        }
        sysguidIntf->sysGUID(oss.str());
    }

    co_return cc ? cc : rc;
}
#endif

NsmLocationIntfProcessor::NsmLocationIntfProcessor(
    sdbusplus::bus::bus& bus, std::string& name, std::string& type,
    std::string& inventoryObjPath, std::string& locationType) :
    NsmObject(name, type)
{
    locationIntf =
        std::make_unique<LocationIntfProcessor>(bus, inventoryObjPath.c_str());
    locationIntf->locationType(
        LocationIntfProcessor::convertLocationTypesFromString(locationType));
}

NsmLocationCodeIntfProcessor::NsmLocationCodeIntfProcessor(
    sdbusplus::bus::bus& bus, std::string& name, std::string& type,
    std::string& inventoryObjPath, std::string& locationCode) :
    NsmObject(name, type)
{
    locationCodeIntf = std::make_unique<LocationCodeIntfProcessor>(
        bus, inventoryObjPath.c_str());
    locationCodeIntf->locationCode(locationCode);
}

NsmMigMode::NsmMigMode(sdbusplus::bus::bus& bus, std::string& name,
                       std::string& type, std::string& inventoryObjPath,
                       [[maybe_unused]] std::shared_ptr<NsmDevice> device,
                       bool isLongRunning) :
    NsmLongRunningSensor(name, type, isLongRunning, device,
                         NSM_TYPE_PLATFORM_ENVIRONMENTAL, NSM_GET_MIG_MODE),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmMigMode: create sensor:{NAME}", "NAME", name.c_str());
    migModeIntf = std::make_unique<MigModeIntf>(bus, inventoryObjPath.c_str());
    updateMetricOnSharedMemory();
}

void NsmMigMode::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(migModeIntf->interface);
    std::vector<uint8_t> smbusData = {};
    nv::sensor_aggregation::DbusVariantType migModeEnabled{
        migModeIntf->MigModeIntf::migModeEnabled()};
    std::string propName = "MIGModeEnabled";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, migModeEnabled);
#endif
}

void NsmMigMode::updateReading(bitfield8_t flags)
{
    migModeIntf->MigModeIntf::migModeEnabled(flags.bits.bit0);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmMigMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_MIG_mode_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_MIG_mode_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmMigMode::handleResponseMsg(const struct nsm_msg* responseMsg,
                                      size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    bitfield8_t flags;
    uint16_t dataSize = 0;
    uint16_t reasonCode = ERR_NULL;

    auto handleFunctionName = isLongRunning ? "decode_get_MIG_mode_event_resp"
                                            : "decode_get_MIG_mode_resp";
    auto rc = isLongRunning
                  ? decode_get_MIG_mode_event_resp(responseMsg, responseLen,
                                                   &cc, &reasonCode, &flags)
                  : decode_get_MIG_mode_resp(responseMsg, responseLen, &cc,
                                             &dataSize, &reasonCode, &flags);

    LG2_ERROR_FLT(
        "{FUNCNAME} failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "FUNCNAME", handleFunctionName, "REASONCODE", reasonCode, "CC", cc,
        "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(flags);
    }

    return cc ? cc : rc;
}

NsmEccMode::NsmEccMode(std::string& name, std::string& type,
                       std::shared_ptr<EccModeIntf> eccIntf,
                       std::string& inventoryObjPath, bool isLongRunning,
                       std::shared_ptr<NsmDevice> device) :
    NsmLongRunningSensor(name, type, isLongRunning, device,
                         NSM_TYPE_PLATFORM_ENVIRONMENTAL, NSM_GET_ECC_MODE),
    inventoryObjPath(inventoryObjPath)

{
    eccModeIntf = eccIntf;
    updateMetricOnSharedMemory();
}

void NsmEccMode::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(eccModeIntf->interface);
    std::vector<uint8_t> smbusData = {};

    nv::sensor_aggregation::DbusVariantType eccModeEnabled{
        eccModeIntf->EccModeIntf::eccModeEnabled()};
    std::string propName = "ECCModeEnabled";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, eccModeEnabled);

    propName = "PendingECCState";
    nv::sensor_aggregation::DbusVariantType pendingECCState{
        eccModeIntf->EccModeIntf::pendingECCState()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, pendingECCState);
#endif
}

std::optional<std::vector<uint8_t>>
    NsmEccMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_ECC_mode_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_ECC_mode_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmEccMode::handleResponseMsg(const struct nsm_msg* responseMsg,
                                      size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    bitfield8_t flags;
    uint16_t dataSize = 0;
    uint16_t reasonCode = ERR_NULL;

    auto handleFunctionName = isLongRunning ? "decode_get_ECC_mode_event_resp"
                                            : "decode_get_ECC_mode_resp";
    auto rc = isLongRunning
                  ? decode_get_ECC_mode_event_resp(responseMsg, responseLen,
                                                   &cc, &reasonCode, &flags)
                  : decode_get_ECC_mode_resp(responseMsg, responseLen, &cc,
                                             &dataSize, &reasonCode, &flags);

    LG2_ERROR_FLT(
        "{FUNCNAME} failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "FUNCNAME", handleFunctionName, "REASONCODE", reasonCode, "CC", cc,
        "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(flags);
    }
    return cc ? cc : rc;
}

void NsmEccMode::updateReading(bitfield8_t flags)
{
    eccModeIntf->EccModeIntf::eccModeEnabled(flags.bits.bit0);
    eccModeIntf->EccModeIntf::pendingECCState(flags.bits.bit1);
    updateMetricOnSharedMemory();
}

NsmEccErrorCounts::NsmEccErrorCounts(std::string& name, std::string& type,
                                     std::shared_ptr<EccModeIntf> eccIntf,
                                     std::string& inventoryObjPath) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmEccErrorCounts: create sensor:{NAME}", "NAME", name.c_str());
    eccErrorCountIntf = eccIntf;
    updateMetricOnSharedMemory();
}
void NsmEccErrorCounts::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(eccErrorCountIntf->interface);
    std::vector<uint8_t> smbusData = {};

    nv::sensor_aggregation::DbusVariantType ceCountOnSharedMem{
        static_cast<int64_t>(eccErrorCountIntf->ceCount())};
    std::string propName = "ceCount";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, ceCountOnSharedMem);

    propName = "ueCount";
    nv::sensor_aggregation::DbusVariantType ueCountOnSharedMem{
        static_cast<int64_t>(eccErrorCountIntf->ueCount())};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, ueCountOnSharedMem);

    propName = "isThresholdExceeded";
    nv::sensor_aggregation::DbusVariantType isThresholdExceeded{
        static_cast<bool>(eccErrorCountIntf->isThresholdExceeded())};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, isThresholdExceeded);
#endif
}

void NsmEccErrorCounts::updateReading(struct nsm_ECC_error_counts errorCounts)
{
    eccErrorCountIntf->ceCount(errorCounts.sram_corrected);
    int64_t ueCount = errorCounts.sram_uncorrected_secded +
                      errorCounts.sram_uncorrected_parity;
    eccErrorCountIntf->ueCount(ueCount);

    eccErrorCountIntf->isThresholdExceeded(errorCounts.flags.bits.bit0);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmEccErrorCounts::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_ECC_error_counts_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_ECC_error_counts_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmEccErrorCounts::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    struct nsm_ECC_error_counts errorCounts;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decode_get_ECC_error_counts_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode, &errorCounts);

    LG2_ERROR_FLT(
        "decode_get_ECC_error_counts_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(errorCounts);
    }
    return cc ? cc : rc;
}

// NsmPciePortIntf moved to nsmCommon/nsmPciePortIntf.*

// NsmPcieGroup definitions moved to nsmCommon/nsmPcieGroup.cpp

// NsmPciGroup2/3/4 moved to nsmCommon/nsmPciGroups.*

NsmPciGroup5::NsmPciGroup5(
    const std::string& name, const std::string& type,
    std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
    uint8_t deviceId, std::string& inventoryObjPath) :
    NsmPcieGroup(name, type, deviceId, GROUP_ID_5),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmPciGroup5: create sensor:{NAME}", "NAME", name.c_str());
    processorPerformanceIntf = processorPerfIntf;
    updateMetricOnSharedMemory();
}

void NsmPciGroup5::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(processorPerformanceIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "PCIeRXBytes";
    nv::sensor_aggregation::DbusVariantType pcIeRXBytesVal{
        processorPerformanceIntf->pcIeRXBytes()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, pcIeRXBytesVal);

    propName = "PCIeTXBytes";
    nv::sensor_aggregation::DbusVariantType pcIeTXBytesVal{
        processorPerformanceIntf->pcIeTXBytes()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, pcIeTXBytesVal);

#endif
}

void NsmPciGroup5::updateReading(
    const nsm_query_scalar_group_telemetry_group_5& data)
{
    uint64_t pcIeRXBytes = static_cast<uint64_t>(data.PCIeRXDwords) *
                           BYTES_PER_DWORD;
    uint64_t pcIeTXBytes = static_cast<uint64_t>(data.PCIeTXDwords) *
                           BYTES_PER_DWORD;

    processorPerformanceIntf->pcIeRXBytes(pcIeRXBytes);
    processorPerformanceIntf->pcIeTXBytes(pcIeTXBytes);
    updateMetricOnSharedMemory();
}

uint8_t NsmPciGroup5::handleResponseMsg(const struct nsm_msg* responseMsg,
                                        size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    struct nsm_query_scalar_group_telemetry_group_5 data;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decode_query_scalar_group_telemetry_v1_group5_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode, &data);

    LG2_ERROR_FLT(
        "decode_query_scalar_group_telemetry_v1_group5_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(data);
    }

    return cc ? cc : rc;
}

NsmEDPpScalingFactor::NsmEDPpScalingFactor(
    std::string& name, std::string& type, std::string& inventoryObjPath,
    std::shared_ptr<EDPpLocal> eDPpIntf,
    std::shared_ptr<NsmResetEdppAsyncIntf> resetEdppAsyncIntf) :
    NsmSensor(name, type), eDPpIntf(eDPpIntf),
    resetEdppAsyncIntf(resetEdppAsyncIntf), inventoryObjPath(inventoryObjPath)
{
    lg2::info("NsmEDPpScalingFactor: create sensor:{NAME}", "NAME",
              name.c_str());
    persistence = false;
    updateMetricOnSharedMemory();
}
void NsmEDPpScalingFactor::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(eDPpIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "AllowableMax";
    nv::sensor_aggregation::DbusVariantType allowableMaxVal{
        eDPpIntf->allowableMax()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, allowableMaxVal);

    propName = "AllowableMin";
    nv::sensor_aggregation::DbusVariantType allowableMinVal{
        eDPpIntf->allowableMin()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, allowableMinVal);
#endif
}

void NsmEDPpScalingFactor::updateReading(
    struct nsm_EDPp_scaling_factors scaling_factors)
{
    eDPpIntf->setPoint(
        std::tuple(scaling_factors.enforced_scaling_factor, persistence));
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmEDPpScalingFactor::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_programmable_EDPp_scaling_factor_req(instanceId,
                                                              requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_programmable_EDPp_scaling_factor_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmEDPpScalingFactor::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    struct nsm_EDPp_scaling_factors scaling_factors;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decode_get_programmable_EDPp_scaling_factor_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode,
        &scaling_factors);

    LG2_ERROR_FLT(
        "decode_get_programmable_EDPp_scaling_factor_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(scaling_factors);
    }
    return cc ? cc : rc;
}

requester::Coroutine NsmEDPpScalingFactor::patchSetPoint(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const std::tuple<bool, uint32_t>* reqSetPoint =
        std::get_if<std::tuple<bool, uint32_t>>(&value);
    if (reqSetPoint == NULL)
    {
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_DATA;
    }
    auto eid = device->getEid();
    lg2::info("patch EDPp setpoint On Device for EID: {EID}", "EID", eid);

    uint32_t allowableMin = eDPpIntf->allowableMin();
    uint32_t allowableMax = eDPpIntf->allowableMax();

    uint32_t reqLimit = std::get<1>(*reqSetPoint);
    bool reqPersistence = std::get<0>(*reqSetPoint);

    if (allowableMin > reqLimit || allowableMax < reqLimit)
    {
        lg2::error("req SetPoint Limit not in allowed range");
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_DATA;
    }

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_programmable_EDPp_scaling_factor_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_set_programmable_EDPp_scaling_factor_req(
        0, NEW_SCALING_FACTOR, reqPersistence, static_cast<uint8_t>(reqLimit),
        requestMsg);

    if (rc)
    {
        lg2::debug(
            "NsmEDPpScalingFactor::patchSetPoint  failed. eid={EID} rc={RC}",
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
            "NsmEDPpScalingFactor::patchSetPoint postPatchIO failed for while setting edpp setpoint "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc_));
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t data_size = 0;
    rc = decode_set_programmable_EDPp_scaling_factor_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &data_size);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        lg2::info(
            "NsmEDPpScalingFactor::patchSetPoint for EID: {EID} completed",
            "EID", eid);
    }
    else
    {
        lg2::error(
            "NsmEDPpScalingFactor::patchSetPoint decode_set_programmable_EDPp_scaling_factor_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reasonCode, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    persistence = reqPersistence;

    co_return NSM_SW_SUCCESS;
}

NsmMaxEDPpLimit::NsmMaxEDPpLimit(std::string& name, std::string& type,
                                 std::shared_ptr<EDPpLocal> eDPpIntf) :
    NsmObject(name, type), eDPpIntf(eDPpIntf)
{
    lg2::info("NsmMaxEDPpLimit: create sensor:{NAME}", "NAME", name.c_str());
}

requester::Coroutine
    NsmMaxEDPpLimit::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MAXIMUM_EDPP_SCALING_FACTOR;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmMaxEDPpLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmMaxEDPpLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint8_t value;
    std::vector<uint8_t> data(1, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog("NsmMaxEDPpLimit decode_get_inventory_information_resp",
                  reasonCode, cc, rc, dataSize != sizeof(value)))
    {
        LG2_ERROR(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        eDPpIntf->allowableMax(value);
    }
    co_return cc ? cc : rc;
}

NsmMinEDPpLimit::NsmMinEDPpLimit(std::string& name, std::string& type,
                                 std::shared_ptr<EDPpLocal> eDPpIntf) :
    NsmObject(name, type), eDPpIntf(eDPpIntf)
{
    lg2::info("NsmMinEDPpLimit: create sensor:{NAME}", "NAME", name.c_str());
}

requester::Coroutine
    NsmMinEDPpLimit::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MINIMUM_EDPP_SCALING_FACTOR;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmMinEDPpLimit encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmMinEDPpLimit SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint8_t value;
    std::vector<uint8_t> data(1, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog("NsmMinEDPpLimit decode_get_inventory_information_resp",
                  reasonCode, cc, rc, dataSize != sizeof(value)))
    {
        LG2_ERROR(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        eDPpIntf->allowableMin(value);
    }
    co_return cc ? cc : rc;
}

NsmClockLimitGraphics::NsmClockLimitGraphics(
    const std::string& name, const std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmClockLimitGraphics: create sensor:{NAME}", "NAME",
              name.c_str());
    cpuOperatingConfigIntf = cpuConfigIntf;
    updateMetricOnSharedMemory();
}

void NsmClockLimitGraphics::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(cpuOperatingConfigIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "SpeedLocked";
    nv::sensor_aggregation::DbusVariantType speedLockedVal{
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::speedLocked()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, speedLockedVal);

    propName = "SpeedConfig";
    nv::sensor_aggregation::DbusVariantType speedConfigVal{
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::speedConfig()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, speedConfigVal);

    propName = "SpeedLimit";
    nv::sensor_aggregation::DbusVariantType speedLimitVal{
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::speedLimit()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, speedLimitVal);
#endif
}

void NsmClockLimitGraphics::updateReading(
    const struct nsm_clock_limit& clockLimit)
{
    cpuOperatingConfigIntf->speedLimit(clockLimit.present_limit_max);
    if (clockLimit.present_limit_max == clockLimit.present_limit_min)
    {
        cpuOperatingConfigIntf->speedLocked(true);
        cpuOperatingConfigIntf->speedConfig(
            std::make_tuple(true, (uint32_t)clockLimit.present_limit_max));
    }
    else
    {
        cpuOperatingConfigIntf->speedLocked(false);
        cpuOperatingConfigIntf->speedConfig(
            std::make_tuple(false, (uint32_t)clockLimit.present_limit_max),
            true);
    }
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmClockLimitGraphics::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_clock_limit_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    uint8_t clock_id = GRAPHICS_CLOCK;
    auto rc = encode_get_clock_limit_req(instanceId, clock_id, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_clock_limit_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmClockLimitGraphics::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    struct nsm_clock_limit clockLimit;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;

    auto rc = decode_get_clock_limit_resp(responseMsg, responseLen, &cc,
                                          &data_size, &reasonCode, &clockLimit);
    LG2_ERROR_FLT(
        "decode_get_clock_limit_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(clockLimit);
    }
    return cc ? cc : rc;
}

NsmCurrClockFreq::NsmCurrClockFreq(
    const std::string& name, const std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmCurrClockFreq: create sensor:{NAME}", "NAME", name.c_str());
    cpuOperatingConfigIntf = cpuConfigIntf;
    updateMetricOnSharedMemory();
}

void NsmCurrClockFreq::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(cpuOperatingConfigIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "OperatingSpeed";
    nv::sensor_aggregation::DbusVariantType operatingSpeedVal{
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::operatingSpeed()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, operatingSpeedVal);

#endif
}

void NsmCurrClockFreq::updateReading(const uint32_t& clockFreq)
{
    cpuOperatingConfigIntf->CpuOperatingConfigIntf::operatingSpeed(clockFreq);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmCurrClockFreq::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_curr_clock_freq_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    uint8_t clock_id = GRAPHICS_CLOCK;
    auto rc = encode_get_curr_clock_freq_req(instanceId, clock_id, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_curr_clock_freq_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmCurrClockFreq::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    uint32_t clockFreq = 1;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;

    auto rc = decode_get_curr_clock_freq_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode, &clockFreq);

    LG2_ERROR_FLT(
        "decode_get_curr_clock_freq_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(clockFreq);
    }
    return cc ? cc : rc;
}

NsmDefaultBaseClockSpeed::NsmDefaultBaseClockSpeed(
    std::string& name, std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf) :
    NsmObject(name, type), cpuOperatingConfigIntf(cpuConfigIntf)
{
    lg2::info("NsmDefaultBaseClockSpeed: create sensor:{NAME}", "NAME",
              name.c_str());
}

requester::Coroutine
    NsmDefaultBaseClockSpeed::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = DEFAULT_BASE_CLOCKS;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmDefaultBaseClockSpeed: encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmDefaultBaseClockSpeed SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog(
            "NsmDefaultBaseClockSpeed decode_get_inventory_information_resp",
            reasonCode, cc, rc, dataSize != sizeof(value)))
    {
        LG2_ERROR(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        cpuOperatingConfigIntf->CpuOperatingConfigIntf::baseSpeed(value);
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmDefaultBoostClockSpeed::NsmDefaultBoostClockSpeed(
    std::string& name, std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf) :
    NsmObject(name, type), cpuOperatingConfigIntf(cpuConfigIntf)
{
    lg2::info("NsmDefaultBoostClockSpeed: create sensor:{NAME}", "NAME",
              name.c_str());
}

requester::Coroutine
    NsmDefaultBoostClockSpeed::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = DEFAULT_BOOST_CLOCKS;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmDefaultBoostClockSpeed: encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmDefaultBoostClockSpeed: sensorIO failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog("decode_get_inventory_information_resp", reasonCode, cc, rc,
                  dataSize != sizeof(value)))
    {
        LG2_ERROR(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        cpuOperatingConfigIntf
            ->CpuOperatingConfigIntf::defaultBoostClockSpeedMHz(value);
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmCurrentUtilization::NsmCurrentUtilization(
    const std::string& name, const std::string& type,
    std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
    std::shared_ptr<SMUtilizationIntf> smUtilizationIntf,
    std::string& inventoryObjPath, bool isLongRunning,
    std::shared_ptr<NsmDevice> device) :
    NsmLongRunningSensor(name, type, isLongRunning, device,
                         NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                         NSM_GET_CURRENT_UTILIZATION),
    cpuOperatingConfigIntf(cpuConfigIntf), smUtilizationIntf(smUtilizationIntf),
    inventoryObjPath(inventoryObjPath),
    smUtilizationIntfName(smUtilizationIntf->interface),
    smUtilizationPropertyName("SMUtilization")
{
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmCurrentUtilization::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_get_current_utilization_req(instanceId, requestPtr);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_current_utilization_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

void NsmCurrentUtilization::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(cpuOperatingConfigIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "Utilization";
    nv::sensor_aggregation::DbusVariantType utilizationVal{
        cpuOperatingConfigIntf->utilization()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, utilizationVal);
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, smUtilizationIntfName, smUtilizationPropertyName,
        smbusData, utilizationVal);
#endif
}

uint8_t
    NsmCurrentUtilization::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    nsm_get_current_utilization_data data;
    uint16_t dataSize = 0;
    uint16_t reasonCode = ERR_NULL;

    auto handleFunctionName = isLongRunning
                                  ? "decode_get_current_utilization_event_resp"
                                  : "decode_get_current_utilization_resp";
    auto rc = isLongRunning
                  ? decode_get_current_utilization_event_resp(
                        responseMsg, responseLen, &cc, &reasonCode, &data)
                  : decode_get_current_utilization_resp(
                        responseMsg, responseLen, &cc, &dataSize, &reasonCode,
                        &data);

    LG2_ERROR_FLT(
        "{FUNCNAME} failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "FUNCNAME", handleFunctionName, "REASONCODE", reasonCode, "CC", cc,
        "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        cpuOperatingConfigIntf->utilization(data.gpu_utilization);
        smUtilizationIntf->smUtilization(data.gpu_utilization);
        updateMetricOnSharedMemory();
    }

    return cc ? cc : rc;
}

NsmProcessorThrottleReason::NsmProcessorThrottleReason(
    std::string& name, std::string& type,
    std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmProcessorThrottleReason: create sensor:{NAME}", "NAME",
              name.c_str());
    processorPerformanceIntf = processorPerfIntf;
    std::vector<ThrottleReasons> throttleReasons;
    throttleReasons.push_back(ThrottleReasons::None);
    processorPerformanceIntf->throttleReason(throttleReasons);
    updateMetricOnSharedMemory();
}
void NsmProcessorThrottleReason::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(processorPerformanceIntf->interface);
    std::vector<uint8_t> smbusData = {};
    std::vector<std::string> throttleReasonsForShmem;

    for (auto tr : processorPerformanceIntf->throttleReason())
    {
        throttleReasonsForShmem.push_back(
            processorPerformanceIntf->convertThrottleReasonsToString(tr));
    }

    std::string propName = "ThrottleReason";
    nv::sensor_aggregation::DbusVariantType throttleReasonVal{
        throttleReasonsForShmem};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, throttleReasonVal);

#endif
}

void NsmProcessorThrottleReason::updateReading(bitfield32_t flags)
{
    std::vector<ThrottleReasons> throttleReasons;

    if (flags.bits.bit0)
    {
        throttleReasons.push_back(ThrottleReasons::SWPowerCap);
    }
    if (flags.bits.bit1)
    {
        throttleReasons.push_back(ThrottleReasons::HWSlowdown);
    }
    if (flags.bits.bit2)
    {
        throttleReasons.push_back(ThrottleReasons::HWThermalSlowdown);
    }
    if (flags.bits.bit3)
    {
        throttleReasons.push_back(ThrottleReasons::HWPowerBrakeSlowdown);
    }
    if (flags.bits.bit4)
    {
        throttleReasons.push_back(ThrottleReasons::SyncBoost);
    }
    if (flags.bits.bit5)
    {
        throttleReasons.push_back(
            ThrottleReasons::ClockOptimizedForThermalEngage);
    }
    if (throttleReasons.size() == 0)
    {
        throttleReasons.push_back(ThrottleReasons::None);
    }
    processorPerformanceIntf->throttleReason(throttleReasons);
    processorPerformanceIntf->throttleReasonHWSlowdown(flags.bits.bit1);
    processorPerformanceIntf->throttleReasonHWThermalSlowdown(flags.bits.bit2);
    processorPerformanceIntf->throttleReasonHWPowerBrakeSlowdown(
        flags.bits.bit3);
    processorPerformanceIntf->throttleReasonSyncBoost(flags.bits.bit4);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmProcessorThrottleReason::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_current_clock_event_reason_code_req(instanceId,
                                                             requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_current_clock_event_reason_code_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmProcessorThrottleReason::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    bitfield32_t data;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decode_get_current_clock_event_reason_code_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode, &data);

    LG2_ERROR_FLT(
        "decode_get_current_clock_event_reason_code_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(data);
    }
    return cc ? cc : rc;
}

NsmAccumGpuUtilTime::NsmAccumGpuUtilTime(
    const std::string& name, const std::string& type,
    std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmAccumGpuUtilTime: create sensor:{NAME}", "NAME",
              name.c_str());
    processorPerformanceIntf = processorPerfIntf;
    updateMetricOnSharedMemory();
}

void NsmAccumGpuUtilTime::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(processorPerformanceIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "AccumulatedGPUContextUtilizationDuration";
    nv::sensor_aggregation::DbusVariantType
        accumulatedGPUContextUtilizationDurationVal{
            processorPerformanceIntf
                ->accumulatedGPUContextUtilizationDuration()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData,
        accumulatedGPUContextUtilizationDurationVal);

    propName = "AccumulatedSMUtilizationDuration";
    nv::sensor_aggregation::DbusVariantType accumulatedSMUtilizationDurationVal{
        processorPerformanceIntf->accumulatedSMUtilizationDuration()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData,
        accumulatedSMUtilizationDurationVal);

#endif
}

void NsmAccumGpuUtilTime::updateReading(const uint32_t& context_util_time,
                                        const uint32_t& SM_util_time)
{
    // Convert from milliseconds to nanoseconds
    std::chrono::milliseconds contextUtilTimeMs(context_util_time);
    std::chrono::milliseconds smUtilTimeMs(SM_util_time);

    std::chrono::nanoseconds contextUtilTimeNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(contextUtilTimeMs);
    std::chrono::nanoseconds smUtilTimeNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(smUtilTimeMs);

    processorPerformanceIntf->accumulatedGPUContextUtilizationDuration(
        contextUtilTimeNs.count());
    processorPerformanceIntf->accumulatedSMUtilizationDuration(
        smUtilTimeNs.count());

    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmAccumGpuUtilTime::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_accum_GPU_util_time_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_accum_GPU_util_time_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmAccumGpuUtilTime::handleResponseMsg(const struct nsm_msg* responseMsg,
                                           size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    uint32_t context_util_time;
    uint32_t SM_util_time;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;
    auto rc = decode_get_accum_GPU_util_time_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode,
        &context_util_time, &SM_util_time);

    LG2_ERROR_FLT(
        "decode_get_accum_GPU_util_time_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(context_util_time, SM_util_time);
    }
    return cc ? cc : rc;
}

NsmTotalMemorySize::NsmTotalMemorySize(
    std::string& name, std::string& type,
    std::shared_ptr<PersistentMemoryInterface> persistentMemoryInterface) :
    NsmObject(name, type), persistentMemoryInterface(persistentMemoryInterface)
{
    lg2::info("NsmTotalMemorySize: create sensor:{NAME}", "NAME", name.c_str());
}

requester::Coroutine
    NsmTotalMemorySize::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MAXIMUM_MEMORY_CAPACITY;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "NsmTotalMemorySize: encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmTotalMemorySize: SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog("decode_get_inventory_information_resp", reasonCode, cc, rc,
                  dataSize != sizeof(value)))
    {
        LG2_ERROR(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        persistentMemoryInterface->volatileSizeInKiB(value * 1024);
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmTotalNvLinks::NsmTotalNvLinks(
    const std::string& name, const std::string& type,
    std::shared_ptr<TotalNvLinkInterface> totalNvLinkInterface,
    std::string& inventoryObjPath) :
    NsmSensor(name, type), totalNvLinkInterface(totalNvLinkInterface),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmTotalNvLinks: create sensor:{NAME}", "NAME", name.c_str());
    updateMetricOnSharedMemory();
}

void NsmTotalNvLinks::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(totalNvLinkInterface->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "TotalNvLinksCount";
    nv::sensor_aggregation::DbusVariantType totalNvLinksCount{
        totalNvLinkInterface->totalNumberNVLinks()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, totalNvLinksCount);

#endif
}

std::optional<std::vector<uint8_t>>
    NsmTotalNvLinks::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_query_ports_available_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_ports_available_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_query_ports_available_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmTotalNvLinks::handleResponseMsg(const struct nsm_msg* responseMsg,
                                           size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    uint8_t totalNvLinks;
    uint16_t data_size;
    uint16_t reasonCode = 0;

    auto rc = decode_query_ports_available_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode, &totalNvLinks);

    LG2_ERROR_FLT(
        "decode_query_ports_available_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        totalNvLinkInterface->totalNumberNVLinks(totalNvLinks);
        updateMetricOnSharedMemory();
    }
    return cc ? cc : rc;
}

NsmProcessorRevision::NsmProcessorRevision(sdbusplus::bus::bus& bus,
                                           const std::string& name,
                                           const std::string& type,
                                           std::string& inventoryObjPath) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmProcessorRevision: create sensor:{NAME}", "NAME",
              name.c_str());
    revisionIntf = std::make_unique<RevisionIntf>(bus,
                                                  inventoryObjPath.c_str());
    updateMetricOnSharedMemory();
}

void NsmProcessorRevision::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(revisionIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "Version";
    nv::sensor_aggregation::DbusVariantType versionVal{revisionIntf->version()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, versionVal);

#endif
}

std::optional<std::vector<uint8_t>>
    NsmProcessorRevision::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_inventory_information_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(
        instanceId, DEVICE_PART_NUMBER, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_inventory_information_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmProcessorRevision::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    std::vector<uint8_t> data(65535, 0);
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;

    auto rc = decode_get_inventory_information_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode, data.data());

    LG2_ERROR_FLT(
        "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        std::string revision(data.begin(), data.end());
        revisionIntf->version(revision);
        updateMetricOnSharedMemory();
    }
    return cc ? cc : rc;
}

NsmGpuHealth::NsmGpuHealth(sdbusplus::bus::bus& bus, std::string& name,
                           std::string& type, std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    healthIntf = std::make_unique<GpuHealthIntf>(bus, inventoryObjPath.c_str());
    healthIntf->health(GpuHealthType::OK);
}

NsmPowerCap::NsmPowerCap(
    std::string& name, std::string& type,
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
    const std::vector<std::string>& parents,
    const std::shared_ptr<PowerPersistencyIntf> persistencyIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type), powerCapIntf(powerCapIntf), parents(parents),
    persistencyIntf(persistencyIntf), inventoryObjPath(inventoryObjPath)
{
    persistencyIntf->persistency(false);
    persistencyIntf->persistentPowerLimit(std::nan(""));
    persistencyIntf->oneShotPowerLimit(std::nan(""));
}

void NsmPowerCap::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(powerCapIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "PowerCap";
    nv::sensor_aggregation::DbusVariantType powerCapVal{
        powerCapIntf->PowerCapIntf::powerCap()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, powerCapVal);

    propName = "PowerCapEnable";
    nv::sensor_aggregation::DbusVariantType powerCapEnableVal{
        powerCapIntf->PowerCapIntf::powerCapEnable()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, powerCapEnableVal);

#endif
}

void NsmPowerCap::updateReading(uint32_t value)
{
    // calling parent powercap to update the value on dbus
    powerCapIntf->PowerCapIntf::powerCap(value);
    powerCapIntf->PowerCapIntf::powerCapEnable(true);
    updateMetricOnSharedMemory();

    // updating Total Power
    SensorManager& manager = SensorManager::getInstance();
    for (auto it = parents.begin(); it != parents.end();)
    {
        auto sensorIt = manager.objectPathToSensorMap.find(*it);
        if (sensorIt != manager.objectPathToSensorMap.end())
        {
            auto sensor = sensorIt->second;
            if (sensor)
            {
                sensorCache.emplace_back(
                    std::dynamic_pointer_cast<NsmPowerControl>(sensor));
                it = parents.erase(it);
                continue;
            }
        }
        ++it;
    }
    // update each cached sensor
    for (const auto& sensor : sensorCache)
    {
        sensor->updatePowerCapValue(getName(), value);
    }
}

std::optional<std::vector<uint8_t>>
    NsmPowerCap::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(struct nsm_get_power_limit_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_device_power_limit_req(instanceId, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_get_device_power_limit_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmPowerCap::handleResponseMsg(const struct nsm_msg* responseMsg,
                                       size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t requested_persistent_limit_in_miliwatts = 0;
    uint32_t requested_oneshot_limit_in_miliwatts = 0;
    uint32_t enforced_limit_in_miliwatts = 0;

    auto rc = decode_get_power_limit_resp(
        responseMsg, responseLen, &cc, &dataSize, &reasonCode,
        &requested_persistent_limit_in_miliwatts,
        &requested_oneshot_limit_in_miliwatts, &enforced_limit_in_miliwatts);

    LG2_ERROR_FLT(
        "decode_get_power_limit_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        // check if device returned invalid power limit, report invalid
        // value as is on dbus
        uint32_t reading = (enforced_limit_in_miliwatts == INVALID_POWER_LIMIT)
                               ? INVALID_POWER_LIMIT
                               : enforced_limit_in_miliwatts / 1000;
        updateReading(reading);

        if (requested_persistent_limit_in_miliwatts == INVALID_POWER_LIMIT)
        {
            persistencyIntf->persistency(false);
            persistencyIntf->persistentPowerLimit(std::nan(""));
        }
        else
        {
            double reading =
                static_cast<double>(requested_persistent_limit_in_miliwatts) /
                1000;
            persistencyIntf->persistency(true);
            persistencyIntf->persistentPowerLimit(reading);
        }

        if (requested_oneshot_limit_in_miliwatts == INVALID_POWER_LIMIT)
        {
            persistencyIntf->oneShotPowerLimit(std::nan(""));
        }
        else
        {
            double reading =
                static_cast<double>(requested_oneshot_limit_in_miliwatts) /
                1000;
            persistencyIntf->oneShotPowerLimit(reading);
        }
    }
    return cc ? cc : rc;
}

NsmMaxPowerCap::NsmMaxPowerCap(std::string& name, std::string& type,
                               std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
                               std::shared_ptr<PowerLimitIface> powerLimitIntf,
                               std::string& inventoryObjPath) :
    NsmObject(name, type), powerCapIntf(powerCapIntf),
    powerLimitIntf(powerLimitIntf), inventoryObjPath(inventoryObjPath)
{
    updateMetricOnSharedMemory();
}

void NsmMaxPowerCap::updateValue(uint32_t value)
{
    powerCapIntf->maxPowerCapValue(value);
    powerLimitIntf->maxPowerWatts(value);
    lg2::debug("NsmMaxPowerCap::updateValue {VALUE}", "VALUE", value);
}

void NsmMaxPowerCap::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(powerLimitIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "MaxPowerWatts";
    nv::sensor_aggregation::DbusVariantType maxPowerVal{
        powerLimitIntf->maxPowerWatts()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, maxPowerVal);
#endif
}

requester::Coroutine
    NsmMaxPowerCap::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MAXIMUM_DEVICE_POWER_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmMaxPowerCap SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog("decode_get_inventory_information_resp", reasonCode, cc, rc,
                  dataSize != sizeof(value)))
    {
        LG2_ERROR(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        // miliwatts to Watts
        // check if device returned invalid power limit, report invalid
        // value as is on dbus
        uint32_t reading = (value == INVALID_POWER_LIMIT) ? INVALID_POWER_LIMIT
                                                          : value / 1000;
        updateValue(reading);
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmMinPowerCap::NsmMinPowerCap(std::string& name, std::string& type,
                               std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
                               std::shared_ptr<PowerLimitIface> powerLimitIntf,
                               std::string& inventoryObjPath) :
    NsmObject(name, type), powerCapIntf(powerCapIntf),
    powerLimitIntf(powerLimitIntf), inventoryObjPath(inventoryObjPath)
{
    updateMetricOnSharedMemory();
}

void NsmMinPowerCap::updateValue(uint32_t value)
{
    powerCapIntf->minPowerCapValue(value);
    powerLimitIntf->minPowerWatts(value);
    lg2::debug("NsmMinPowerCap::updateValue {VALUE}", "VALUE", value);
}

void NsmMinPowerCap::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(powerLimitIntf->interface);
    std::vector<uint8_t> smbusData = {};

    std::string propName = "MinPowerWatts";
    nv::sensor_aggregation::DbusVariantType minPowerVal{
        powerLimitIntf->minPowerWatts()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, minPowerVal);
#endif
}

requester::Coroutine
    NsmMinPowerCap::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = MINIMUM_DEVICE_POWER_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug(
            "NsmMinPowerCap SendRecvNsmMsg failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog("decode_get_inventory_information_resp", reasonCode, cc, rc,
                  dataSize != sizeof(value)))
    {
        LG2_ERROR(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        // miliwatts to Watts
        // check if device returned invalid power limit, report invalid
        // value as is on dbus
        uint32_t reading = (value == INVALID_POWER_LIMIT) ? INVALID_POWER_LIMIT
                                                          : value / 1000;
        updateValue(reading);
    }
    else
    {
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmDefaultPowerCap::NsmDefaultPowerCap(
    std::string& name, std::string& type,
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf,
    std::shared_ptr<NsmClearPowerCapAsyncIntf> clearPowerCapAsyncIntf) :
    NsmObject(name, type), defaultPowerCapIntf(clearPowerCapIntf),
    clearPowerCapAsyncIntf(clearPowerCapAsyncIntf)
{}

void NsmDefaultPowerCap::updateValue(uint32_t value)
{
    defaultPowerCapIntf->defaultPowerCap(value);
    lg2::debug("NsmDefaultPowerCap::updateValue {VALUE}", "VALUE", value);
}

requester::Coroutine
    NsmDefaultPowerCap::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    uint8_t propertyIdentifier = RATED_DEVICE_POWER_LIMIT;
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug(
            "encode_get_inventory_information_req failed. eid={EID} rc={RC}",
            "EID", nsmDevice->getEid(), "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, responseMsg,
                                      responseLen, false);
    if (rc)
    {
        lg2::debug("NsmDefaultPowerCap sensorIO failed with RC={RC}, eid={EID}",
                   "RC", rc, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = ERR_NULL;
    uint16_t reasonCode = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);

    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reasonCode, &dataSize,
                                               data.data());

    if (shouldLog("decode_get_inventory_information_resp", reasonCode, cc, rc,
                  dataSize != sizeof(value)))
    {
        LG2_ERROR(
            "decode_get_inventory_information_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}, size: {SIZE}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SIZE", dataSize);
    }
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        value = le32toh(value);
        // miliwatts to Watts
        // check if device returned invalid power limit, report invalid
        // value as is on dbus
        uint32_t reading = (value == INVALID_POWER_LIMIT) ? INVALID_POWER_LIMIT
                                                          : value / 1000;
        updateValue(reading);
    }
    // coverity[missing_return]
    co_return cc ? cc : rc;
}

NsmProcessorThrottleDuration::NsmProcessorThrottleDuration(
    std::string& name, std::string& type,
    std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
    std::string& inventoryObjPath, bool isLongRunning,
    std::shared_ptr<NsmDevice> device) :
    NsmLongRunningSensor(name, type, isLongRunning, device,
                         NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                         NSM_GET_VIOLATION_DURATION),
    processorPerformanceIntf(processorPerfIntf),
    inventoryObjPath(inventoryObjPath)

{
    lg2::info("NsmProcessorThrottleDuration: create sensor:{NAME}", "NAME",
              name.c_str());
    updateMetricOnSharedMemory();
}
void NsmProcessorThrottleDuration::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM

    auto ifaceName = std::string(processorPerformanceIntf->interface);
    std::vector<uint8_t> smbusData = {};
    std::string propName = "PowerLimitThrottleDuration";
    nv::sensor_aggregation::DbusVariantType powerLimitThrottleDuration{
        processorPerformanceIntf->powerLimitThrottleDuration()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData,
        powerLimitThrottleDuration);

    propName = "ThermalLimitThrottleDuration";
    nv::sensor_aggregation::DbusVariantType thermalLimitThrottleDuration{
        processorPerformanceIntf->thermalLimitThrottleDuration()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData,
        thermalLimitThrottleDuration);

    propName = "GlobalSoftwareViolationThrottleDuration";
    nv::sensor_aggregation::DbusVariantType
        globalSoftwareViolationThrottleDuration{
            processorPerformanceIntf
                ->globalSoftwareViolationThrottleDuration()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData,
        globalSoftwareViolationThrottleDuration);

    propName = "HardwareViolationThrottleDuration";
    nv::sensor_aggregation::DbusVariantType hardwareViolationThrottleDuration{
        processorPerformanceIntf->hardwareViolationThrottleDuration()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData,
        hardwareViolationThrottleDuration);

#endif
}

void NsmProcessorThrottleDuration::updateReading(
    const nsm_violation_duration& data)
{
    processorPerformanceIntf->powerLimitThrottleDuration(
        data.power_violation_duration);
    processorPerformanceIntf->thermalLimitThrottleDuration(
        data.thermal_violation_duration);
    processorPerformanceIntf->hardwareViolationThrottleDuration(
        data.hw_violation_duration);
    processorPerformanceIntf->globalSoftwareViolationThrottleDuration(
        data.global_sw_violation_duration);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmProcessorThrottleDuration::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));

    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_violation_duration_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_violation_duration_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmProcessorThrottleDuration::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    nsm_violation_duration data;
    uint16_t dataSize = 0;
    uint16_t reasonCode = ERR_NULL;

    auto handleFunctionName = isLongRunning
                                  ? "decode_get_violation_duration_event_resp"
                                  : "decode_get_violation_duration_resp";
    auto rc = isLongRunning
                  ? decode_get_violation_duration_event_resp(
                        responseMsg, responseLen, &cc, &reasonCode, &data)
                  : decode_get_violation_duration_resp(responseMsg, responseLen,
                                                       &cc, &dataSize,
                                                       &reasonCode, &data);

    LG2_ERROR_FLT(
        "{FUNCNAME} failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "FUNCNAME", handleFunctionName, "REASONCODE", reasonCode, "CC", cc,
        "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(data);
    }
    return cc ? cc : rc;
}

NsmConfidentialCompute::NsmConfidentialCompute(
    const std::string& name, const std::string& type,
    std::shared_ptr<ConfidentialComputeIntf> confidentialComputeIntf,
    std::string& inventoryObjPath) :
    NsmSensor(name, type), confidentialComputeIntf(confidentialComputeIntf),
    inventoryObjPath(inventoryObjPath)
{
    updateMetricOnSharedMemory();
}

void NsmConfidentialCompute::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(confidentialComputeIntf->interface);
    std::vector<uint8_t> smbusData = {};

    nv::sensor_aggregation::DbusVariantType ccModeEnabled{
        confidentialComputeIntf->ccModeEnabled()};
    std::string propName = "CCModeEnabled";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, ccModeEnabled);

    propName = "PendingCCModeState";
    nv::sensor_aggregation::DbusVariantType pendingCCModeState{
        confidentialComputeIntf->pendingCCModeState()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, pendingCCModeState);

    nv::sensor_aggregation::DbusVariantType ccDevModeEnabled{
        confidentialComputeIntf->ccDevModeEnabled()};
    propName = "CCDevModeEnabled";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, ccDevModeEnabled);

    propName = "PendingCCDevModeState";
    nv::sensor_aggregation::DbusVariantType pendingCCDevModeState{
        confidentialComputeIntf->pendingCCDevModeState()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData,
        pendingCCDevModeState);

#endif
}

std::optional<std::vector<uint8_t>>
    NsmConfidentialCompute::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_confidential_compute_mode_v1_req(instanceId,
                                                          requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_get_confidential_compute_mode_v1_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t
    NsmConfidentialCompute::handleResponseMsg(const struct nsm_msg* responseMsg,
                                              size_t responseLen)
{
    uint8_t cc = ERR_NULL;
    uint8_t current_mode;
    uint8_t pending_mode;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;

    auto rc = decode_get_confidential_compute_mode_v1_resp(
        responseMsg, responseLen, &cc, &data_size, &reasonCode, &current_mode,
        &pending_mode);

    LG2_ERROR_FLT(
        "decode_get_confidential_compute_mode_v1_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(current_mode, pending_mode);
    }
    return cc ? cc : rc;
}

void NsmConfidentialCompute::updateReading(uint8_t current_mode,
                                           uint8_t pending_mode)
{
    switch (current_mode)
    {
        case NO_MODE:
            confidentialComputeIntf->ccModeEnabled(false);
            confidentialComputeIntf->ccDevModeEnabled(false);
            break;
        case PRODUCTION_MODE:
            confidentialComputeIntf->ccModeEnabled(true);
            confidentialComputeIntf->ccDevModeEnabled(false);
            break;
        case DEVTOOLS_MODE:
            confidentialComputeIntf->ccModeEnabled(false);
            confidentialComputeIntf->ccDevModeEnabled(true);
            break;
    }

    switch (pending_mode)
    {
        case NO_MODE:
            confidentialComputeIntf->pendingCCModeState(false);
            confidentialComputeIntf->pendingCCDevModeState(false);
            break;
        case PRODUCTION_MODE:
            confidentialComputeIntf->pendingCCModeState(true);
            confidentialComputeIntf->pendingCCDevModeState(false);
            break;
        case DEVTOOLS_MODE:
            confidentialComputeIntf->pendingCCModeState(false);
            confidentialComputeIntf->pendingCCDevModeState(true);
            break;
    }

    updateMetricOnSharedMemory();
}

requester::Coroutine NsmConfidentialCompute::patchCCMode(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const bool* reqSetting = std::get_if<bool>(&value);
    if (reqSetting == NULL)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("set Confidential Compute Mode On Device for EID: {EID}", "EID",
              eid);

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_confidential_compute_mode_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    uint8_t mode;
    if (*reqSetting == true)
    {
        mode = PRODUCTION_MODE;
    }
    else
    {
        if (confidentialComputeIntf->pendingCCDevModeState())
        {
            mode = DEVTOOLS_MODE;
        }
        else
        {
            mode = NO_MODE;
        }
    }

    auto rc = encode_set_confidential_compute_mode_v1_req(0, mode, requestMsg);

    if (rc)
    {
        lg2::error(
            "NsmConfidentialCompute :: patchCCMode encode_set_confidential_compute_mode_v1_req failed. eid = {EID} rc = { RC } ",
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
        lg2::error("NsmConfidentialCompute :: patchCCMode postPatchIO failed"
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", utils::nsmSwCodeToString(rc_));

        *status = AsyncOperationStatusType::WriteFailure;

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t data_size = 0;
    rc = decode_set_confidential_compute_mode_v1_resp(
        responseMsg.get(), responseLen, &cc, &data_size, &reasonCode);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        lg2::info(
            "NsmConfidentialCompute :: patchCCMode for EID: {EID} completed",
            "EID", eid);
    }
    else
    {
        lg2::error(
            "NsmConfidentialCompute :: patchCCMode decode_set_confidential_compute_mode_v1_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reasonCode, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmConfidentialCompute::patchCCDevMode(
    const AsyncSetOperationValueType& value,
    [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const bool* reqSetting = std::get_if<bool>(&value);
    if (reqSetting == NULL)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("set Confidential Compute Devtools Mode On Device for EID: {EID}",
              "EID", eid);

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_confidential_compute_mode_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    uint8_t mode;
    if (*reqSetting == true)
    {
        mode = DEVTOOLS_MODE;
    }
    else
    {
        if (confidentialComputeIntf->pendingCCModeState())
        {
            mode = PRODUCTION_MODE;
        }
        else
        {
            mode = NO_MODE;
        }
    }

    auto rc = encode_set_confidential_compute_mode_v1_req(0, mode, requestMsg);

    if (rc)
    {
        lg2::error(
            "NsmConfidentialCompute :: patchCCDevMode encode_set_confidential_compute_mode_v1_req failed. eid = {EID} rc = { RC } ",
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
        lg2::error("NsmConfidentialCompute :: patchCCDevMode postPatchIO failed"
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", utils::nsmSwCodeToString(rc_));

        *status = AsyncOperationStatusType::WriteFailure;

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t data_size = 0;
    rc = decode_set_confidential_compute_mode_v1_resp(
        responseMsg.get(), responseLen, &cc, &data_size, &reasonCode);

    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        lg2::info(
            "NsmConfidentialCompute :: patchCCDevMode for EID: {EID} completed",
            "EID", eid);
    }
    else
    {
        lg2::error(
            "NsmConfidentialCompute :: patchCCDevMode decode_set_confidential_compute_mode_v1_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reasonCode, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;

        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    co_return NSM_SW_SUCCESS;
}

NsmEgmMode::NsmEgmMode(sdbusplus::bus::bus& bus, std::string& name,
                       std::string& type, std::string& inventoryObjPath) :
    NsmSensor(name, type), inventoryObjPath(inventoryObjPath)
{
    lg2::info("NsmEgmMode: create sensor:{NAME}", "NAME", name.c_str());
    egmModeIntf = std::make_unique<EgmModeIntf>(bus, inventoryObjPath.c_str());
    updateMetricOnSharedMemory();
}

void NsmEgmMode::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    auto ifaceName = std::string(egmModeIntf->interface);
    std::vector<uint8_t> smbusData = {};
    nv::sensor_aggregation::DbusVariantType egmModeEnabled{
        egmModeIntf->EgmModeIntf::egmModeEnabled()};
    std::string propName = "EGMModeEnabled";
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, egmModeEnabled);

    propName = "PendingEGMModeState";
    nv::sensor_aggregation::DbusVariantType pendingEgmModeState{
        egmModeIntf->EgmModeIntf::pendingEGMModeState()};
    nsm_shmem_utils::SharedMemoryManager::cacheTALData(
        inventoryObjPath, ifaceName, propName, smbusData, pendingEgmModeState);

#endif
}

void NsmEgmMode::updateReading(bitfield8_t flags)
{
    egmModeIntf->EgmModeIntf::egmModeEnabled(flags.bits.bit0);
    egmModeIntf->EgmModeIntf::pendingEGMModeState(flags.bits.bit1);
    updateMetricOnSharedMemory();
}

std::optional<std::vector<uint8_t>>
    NsmEgmMode::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_EGM_mode_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_EGM_mode_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmEgmMode::handleResponseMsg(const struct nsm_msg* responseMsg,
                                      size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    bitfield8_t flags;
    uint16_t data_size;
    uint16_t reasonCode = ERR_NULL;

    auto rc = decode_get_EGM_mode_resp(responseMsg, responseLen, &cc,
                                       &data_size, &reasonCode, &flags);

    LG2_ERROR_FLT(
        "decode_get_EGM_mode_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)
    {
        updateReading(flags);
    }

    return cc ? cc : rc;
}

requester::Coroutine createNsmProcessorSensor(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();

    dbus::PropertyMap allBaseIfaceProperties;
    auto rc = co_await utils::coGetCachedBaseProperties(
        objPath, PROCESSOR_INTERFACE, allBaseIfaceProperties);
    if (rc != NSM_SUCCESS)
    {
        co_return rc;
    }
    dbus::PropertyMap allCurrentIfaceProperties =
        co_await utils::coGetAllDbusProperty(utils::entityManagerServiceStr,
                                             objPath, interface);

    std::string name{};
    uuid_t uuid{};
    std::string type{};
    std::string inventoryObjPath{};

    if (allBaseIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allBaseIfaceProperties.at("Name"));
    }
    if (allBaseIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
    }
    if (allCurrentIfaceProperties.count("Type"))
    {
        type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
    }
    if (allBaseIfaceProperties.count("InventoryObjPath"))
    {
        inventoryObjPath = std::get<std::string>(
            allBaseIfaceProperties.at("InventoryObjPath"));
    }

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);

    if (type == "NSM_Processor")
    {
        std::vector<utils::Association> associations{};
        co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                          associations);
        auto associationSensor = std::make_shared<NsmProcessorAssociation>(
            bus, name, type, inventoryObjPath, associations);
        nsmDevice->addDeviceSensors(associationSensor);

#ifdef ACCELERATOR_DBUS
        auto sensor = std::make_shared<NsmAcceleratorIntf>(bus, name, type,
                                                           inventoryObjPath);
        nsmDevice->addDeviceSensors(sensor);
#endif

#ifdef NVIDIA_RESET_METRICS
        auto resetSupportSensor =
            std::make_shared<NsmResetCountersSupportedIntf>(bus, name, type,
                                                            inventoryObjPath);
        nsmDevice->addDeviceSensors(resetSupportSensor);
#endif

        uuid_t deviceUuid{};
        if (allBaseIfaceProperties.count("DEVICE_UUID"))
        {
            deviceUuid =
                std::get<uuid_t>(allBaseIfaceProperties.at("DEVICE_UUID"));
        }

        auto uuidSensor = std::make_shared<NsmUuidIntf>(
            bus, name, type, inventoryObjPath, deviceUuid);
        nsmDevice->addStaticSensor(uuidSensor);

#ifdef ENABLE_SYSTEM_GUID
        auto sysGuidSensor = std::make_shared<NsmSysGuidIntf>(bus, name, type,
                                                              inventoryObjPath);

        // Note: Setting this to addSensor as opposed to addStaticSensor:
        //       Ideally this would be a static sensor, but the GPU changes
        //       state with the host. A static sensor does not update so
        //       the System GUID gets wiped. Polling here resolves the issue
        //       and ensures the GUID is always correctly loaded.
        // TBD:  A more optimal solution would be to update static sensors
        //       when nsmObjects go online/offline but that may have other
        //       impacts so needs some further investigation.
        nsmDevice->addSensor(sysGuidSensor, false, true);
#endif

#if defined(ENABLE_DEBUG_INFO)
        size_t pos = inventoryObjPath.find_last_of('/');
        std::string basePath = inventoryObjPath;
        std::string processorName = name;
        if (pos != std::string::npos)
        {
            basePath = inventoryObjPath.substr(0, pos + 1);
            processorName = inventoryObjPath.substr(pos + 1);
        }

#if NETIR_DUMP_ENABLED
        // NetIR dump for Processor
        auto processorDebugInfoObject = std::make_shared<NsmDebugInfoObject>(
            bus, processorName, basePath, type, uuid, DebugDumpType::Network);
        nsmDevice->addStaticSensor(processorDebugInfoObject);
#endif

        auto processorEraseTraceObject = std::make_shared<NsmEraseTraceObject>(
            bus, processorName, basePath, type, uuid);
        nsmDevice->addStaticSensor(processorEraseTraceObject);

        auto processorLogInfoObject = std::make_shared<NsmLogInfoObject>(
            bus, processorName, basePath, type, uuid);
        nsmDevice->addStaticSensor(processorLogInfoObject);

#if DIAGNOSTIC_DUMP_ENABLED
        // Device Diagnostics(MSE DUMP) for Processor
        auto processorDiagnosticsObject = std::make_shared<NsmDebugInfoObject>(
            bus, processorName, basePath, type, uuid,
            DebugDumpType::Diagnostics);
        nsmDevice->addStaticSensor(processorDiagnosticsObject);
#endif
#endif

        auto gpuRevisionSensor = std::make_shared<NsmProcessorRevision>(
            bus, name, type, inventoryObjPath);
        nsmDevice->addStaticSensor(gpuRevisionSensor);

        auto persistentMemoryIntf = std::make_shared<PersistentMemoryInterface>(
            bus, inventoryObjPath.c_str());

        auto totalMemorySizeSensor = std::make_shared<NsmTotalMemorySize>(
            name, type, persistentMemoryIntf);
        nsmDevice->addStaticSensor(totalMemorySizeSensor);

        auto healthSensor = std::make_shared<NsmGpuHealth>(bus, name, type,
                                                           inventoryObjPath);
        nsmDevice->addDeviceSensors(healthSensor);

        createNsmErrorInjectionSensors(manager, nsmDevice, inventoryObjPath);
        auto confidentialComputeIntf =
            std::make_shared<ConfidentialComputeIntf>(bus,
                                                      inventoryObjPath.c_str());
        auto confidentialComputeSensor =
            std::make_shared<NsmConfidentialCompute>(
                name, type, confidentialComputeIntf, inventoryObjPath);
        nsmDevice->addSensor(confidentialComputeSensor,
                             CONFIDENTIAL_COMPUTE_MODE_PRIORITY);
        nsm::AsyncSetOperationHandler setconfidentialComputeHandler =
            std::bind(&NsmConfidentialCompute::patchCCMode,
                      confidentialComputeSensor, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);

        AsyncOperationManager::getInstance()
            ->getDispatcher(inventoryObjPath)
            ->addAsyncSetOperation(
                "com.nvidia.CCMode", "CCModeEnabled",
                AsyncSetOperationInfo{setconfidentialComputeHandler,
                                      confidentialComputeSensor, nsmDevice});

        nsm::AsyncSetOperationHandler setconfidentialComputeDevtoolHandler =
            std::bind(&NsmConfidentialCompute::patchCCDevMode,
                      confidentialComputeSensor, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);

        AsyncOperationManager::getInstance()
            ->getDispatcher(inventoryObjPath)
            ->addAsyncSetOperation(
                "com.nvidia.CCMode", "CCDevModeEnabled",
                AsyncSetOperationInfo{setconfidentialComputeDevtoolHandler,
                                      confidentialComputeSensor, nsmDevice});
    }
    else if (type == "NSM_Processor_Attributes")
    {
        // Handle Asset (always NVIDIA manufacturer)
        auto assetIntf =
            std::make_shared<NsmAssetIntf>(bus, inventoryObjPath.c_str());
        std::string manufacturer = MANUFACTURER_NVIDIA;

        auto assetObject = NsmInterfaceProvider<NsmAssetIntf>(
            name, type, inventoryObjPath, assetIntf);
        assetObject.invoke(pdiMethod(manufacturer), manufacturer);
        nsmDevice->addStaticSensor(
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
                assetObject, DEVICE_PART_NUMBER));
        nsmDevice->addStaticSensor(
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
                assetObject, SERIAL_NUMBER));
        const auto modelProperty = getModelInventoryProperty(
            nsmDevice->getDeviceType(), nsmDevice->getDeviceRole());
        nsmDevice->addStaticSensor(
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
                assetObject, modelProperty));

        // Handle Location and LocationCode from ProcessorAttributes
        if (allCurrentIfaceProperties.count("LocationType"))
        {
            std::string locationType = std::get<std::string>(
                allCurrentIfaceProperties.at("LocationType"));
            auto sensor = std::make_shared<NsmLocationIntfProcessor>(
                bus, name, type, inventoryObjPath, locationType);
            nsmDevice->addDeviceSensors(sensor);
        }
        if (allCurrentIfaceProperties.count("LocationCode"))
        {
            std::string locationCode = std::get<std::string>(
                allCurrentIfaceProperties.at("LocationCode"));
            auto sensor = std::make_shared<NsmLocationCodeIntfProcessor>(
                bus, name, type, inventoryObjPath, locationCode);
            nsmDevice->addDeviceSensors(sensor);
        }

        if (allCurrentIfaceProperties.count("MIGModeSupported") &&
            std::get<bool>(allCurrentIfaceProperties.at("MIGModeSupported")))
        {
            createMIGMode(nsmDevice, bus, name, type, inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count("PortDisableFutureSupported") &&
            std::get<bool>(
                allCurrentIfaceProperties.at("PortDisableFutureSupported")))
        {
            createPortDisableFuture(nsmDevice, name, type, inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count("ECCModeSupported") &&
            std::get<bool>(allCurrentIfaceProperties.at("ECCModeSupported")))
        {
            createECCMode(nsmDevice, bus, name, type, inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count("EDPpScalingFactorSupported") &&
            std::get<bool>(
                allCurrentIfaceProperties.at("EDPpScalingFactorSupported")))
        {
            createEDPpScalingFactor(nsmDevice, bus, name, type,
                                    inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count("PowerSmoothingSupported") &&
            std::get<bool>(
                allCurrentIfaceProperties.at("PowerSmoothingSupported")))
        {
            createPowerSmoothing(nsmDevice, bus, name, type, inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count("SoCPowerSmoothingSupported") &&
            std::get<bool>(
                allCurrentIfaceProperties.at("SoCPowerSmoothingSupported")))
        {
            createSoCPowerSmoothing(nsmDevice, bus, name, type,
                                    inventoryObjPath, SoCDeviceType::GPU);
        }
        if (allCurrentIfaceProperties.count("CpuOperatingConfigSupported") &&
            std::get<bool>(
                allCurrentIfaceProperties.at("CpuOperatingConfigSupported")))
        {
            createCpuOperatingConfig(nsmDevice, bus, name, type,
                                     inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count("MemCapacityUtilSupported") &&
            std::get<bool>(
                allCurrentIfaceProperties.at("MemCapacityUtilSupported")))
        {
            createMemCapacityUtil(nsmDevice, name, type, inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count("TotalNvLinksCountSupported") &&
            std::get<bool>(
                allCurrentIfaceProperties.at("TotalNvLinksCountSupported")))
        {
            createTotalNvLinksCount(nsmDevice, bus, name, type,
                                    inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count("EGMModeSupported") &&
            std::get<bool>(allCurrentIfaceProperties.at("EGMModeSupported")))
        {
            createEGMMode(nsmDevice, bus, name, type, inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count("MNNVLTopologySupported") &&
            std::get<bool>(
                allCurrentIfaceProperties.at("MNNVLTopologySupported")))
        {
            createMNNVLTopology(nsmDevice, bus, name, type, inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count(
                "MctpNsmOperationalStatusSupported") &&
            std::get<bool>(allCurrentIfaceProperties.at(
                "MctpNsmOperationalStatusSupported")))
        {
            createGpuOperationalStatus(nsmDevice, bus, name, type,
                                       inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count("GPUBasePowerLimitSupported") &&
            std::get<bool>(
                allCurrentIfaceProperties.at("GPUBasePowerLimitSupported")))
        {
            createGPUPowerLimit(nsmDevice, bus, name,
                                "NSM_GPU_BASE_POWER_LIMIT", inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count("GPUCopyCPUPowerLimitSupported") &&
            std::get<bool>(
                allCurrentIfaceProperties.at("GPUCopyCPUPowerLimitSupported")))
        {
            createGPUPowerLimit(nsmDevice, bus, name,
                                "NSM_GPU_COPY_CPU_POWER_LIMIT",
                                inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count(
                "GPUCopySwitchPowerLimitSupported") &&
            std::get<bool>(allCurrentIfaceProperties.at(
                "GPUCopySwitchPowerLimitSupported")))
        {
            createGPUPowerLimit(nsmDevice, bus, name,
                                "NSM_GPU_COPY_SWITCH_POWER_LIMIT",
                                inventoryObjPath);
        }
        if (allCurrentIfaceProperties.count("CPEREventSupported") &&
            std::get<bool>(allCurrentIfaceProperties.at("CPEREventSupported")))
        {
            createNsmCPEREvent(nsmDevice, name, "NSM_Event_CPER");
        }
    }
    else if (type == "NSM_PCIe")
    {
        createPCIe(nsmDevice, bus, name, type, inventoryObjPath,
                   allCurrentIfaceProperties);
    }
    else if (type == "NSM_ProcessorPerformance")
    {
        createProcessorPerformance(nsmDevice, bus, name, type, inventoryObjPath,
                                   allCurrentIfaceProperties);
    }
    else if (type == "NSM_PowerCap")
    {
        createPowerCap(nsmDevice, bus, name, type, inventoryObjPath,
                       allCurrentIfaceProperties, manager);
    }
    else if (type == "NSM_ReconfigPermissions")
    {
        createReconfigPermissions(nsmDevice, bus, name, type, inventoryObjPath,
                                  allCurrentIfaceProperties);
    }
    else if (type == "NSM_WorkloadPowerProfile")
    {
        createWorkloadPowerProfile(nsmDevice, bus, name, type, inventoryObjPath,
                                   allCurrentIfaceProperties);
    }
#ifdef NVIDIA_RESET_METRICS
    // Fetch the priority property
    auto resetMetricsPriority = false;
    // Fetch the ResetStatistics name property
    auto resetMetricsName = "ResetMetrics";
    auto resetPath = inventoryObjPath + "/ResetStatistics";
    // Create the Reset Statistics D-Bus Interface
    using ResetCountersServer = sdbusplus::server::object_t<
        sdbusplus::com::nvidia::ResetCounters::server::ResetCounterMetrics>;
    auto resetCountersObj =
        std::make_shared<ResetCountersServer>(bus, resetPath.c_str());
    // Add association
    //  Define associations
    std::vector<utils::Association> associations{
        {"parent", "reset_statistics", inventoryObjPath}};
    // Add associations
    auto resetMetricsAssociationDef =
        std::make_unique<AssociationDefinitionsIntf>(bus, resetPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    for (const auto& association : associations)
    {
        associationsList.emplace_back(association.forward, association.backward,
                                      association.absolutePath);
    }
    resetMetricsAssociationDef->associations(associationsList);
    // Initialize all properties to their default values
    resetCountersObj->lastResetType(
        sdbusplus::com::nvidia::ResetCounters::server::ResetCounterMetrics::
            ResetTypes::Conventional);
    resetCountersObj->pfflrResetEntryCount(std::nan(""));
    resetCountersObj->pfflrResetExitCount(std::nan(""));
    resetCountersObj->conventionalResetEntryCount(std::nan(""));
    resetCountersObj->conventionalResetExitCount(std::nan(""));
    resetCountersObj->fundamentalResetEntryCount(std::nan(""));
    resetCountersObj->fundamentalResetExitCount(std::nan(""));
    resetCountersObj->iRoTResetExitCount(std::nan(""));
    // Create the ResetStatistics sensor
    auto resetStatisticsSensor = std::make_shared<ResetStatisticsAggregator>(
        resetMetricsName, "NSM_ResetStatistics", resetPath, resetCountersObj,
        std::move(resetMetricsAssociationDef));
    nsmDevice->addDeviceSensors(resetStatisticsSensor);
    // Add sensor to the device with priority
    nsmDevice->addSensor(resetStatisticsSensor, resetMetricsPriority);
#endif

    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

dbus::Interfaces nsmProcessorInterfaces = {
    "xyz.openbmc_project.Configuration.NSM_Processor",
    "xyz.openbmc_project.Configuration.NSM_Processor.ProcessorAttributes",
    "xyz.openbmc_project.Configuration.NSM_Processor.PCIe",
    "xyz.openbmc_project.Configuration.NSM_Processor.ProcessorPerformance",
    "xyz.openbmc_project.Configuration.NSM_Processor.PowerCap",
    "xyz.openbmc_project.Configuration.NSM_Processor.ReconfigPermissions",
    "xyz.openbmc_project.Configuration.NSM_Processor.PCIeDevice",
    "xyz.openbmc_project.Configuration.NSM_Processor.WorkloadPowerProfile",
    "xyz.openbmc_project.Configuration.NSM_Processor.ResetStatistics"};

REGISTER_NSM_CREATION_FUNCTION(createNsmProcessorSensor, nsmProcessorInterfaces)

} // namespace nsm
