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

#pragma once

#include "libnsm/base.h"

#include "features/creators.hpp"
#include "features/feature.hpp"

#include <string>

namespace nsm::features
{

/** @brief Features of a GPU, in creation order */
inline FeatureList gpuFeatures()
{
    using namespace std::string_literals;

    FeatureList features;

    // Chassis: Common.UUID and MCTP.UUID
    features.push_back({.id = "Chassis",
                        .create = createChassisFeature,
                        .params = {{"Name", "{device}"s}}});
    // ContainedBy
    features.push_back(
        {.id = "ChassisAssociations",
         .create = createChassisAssociationsFeature,
         .params = {{"Name", "{device}"s},
                    {"ParentChassisAssociationPaths",
                     std::vector<std::string>{"{parentChassis}"}}}});

    // Chassis attributes; SKU and Health are always created
    features.push_back(
        {.id = "ChassisAttributes",
         .create = createChassisAttributesFeature,
         .params = {
             {"Name", "{device}"s},
             {"AssetInformationAvailable", true},
             {"Manufacturer", "NVIDIA"s},
             {"LocationType",
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"s},
             {"LocationCode", optVar<std::string>("{LocationCode}")},
             {"LocationContext", optVar<std::string>("{LocationContext}")},
             {"FieldReplaceable", false},
             {"ChassisType",
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"s},
             {"PrettyNameForChassis", "GPU_{instance}"s},
             {"DimensionSupported", true},
             {"PowerLimitSupported", true},
             {"Priority", false}}});

    // Assemblies, numbered Assembly0, Assembly1, ... over the
    // enabled ones in this order
    features.push_back(
        {.id = "ChassisAssemblyBoard",
         .create = createChassisAssemblyFeature,
         .params = {
             {"AssemblyType", "Board"s},
             {"Name", "GPU_{instance} Board Assembly"s},
             {"PhysicalContext",
              "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU"s},
             {"LocationType",
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"s}}});
    features.push_back(
        {.id = "ChassisAssemblyDevice",
         .create = createChassisAssemblyFeature,
         .params = {
             {"AssemblyType", "Device"s},
             {"Name", "GPU_{instance} Device Assembly"s},
             {"PhysicalContext",
              "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU"s},
             {"LocationType",
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"s}}});

    // PCIe device objects, all on chassis/<name>/PCIeDevices/GPU_<instance>
    features.push_back({.id = "ChassisPCIeDeviceUUID",
                        .create = createChassisPCIeDeviceUUIDFeature,
                        .params = {{"Name", "GPU_{instance}"s}}});
    features.push_back(
        {.id = "ChassisPCIeDeviceAssociations",
         .create = createChassisPCIeDeviceAssociationsFeature,
         .params = {{"Name", "GPU_{instance}"s},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}}}});
    features.push_back({.id = "ChassisPCIeDeviceAsset",
                        .create = createChassisPCIeDeviceAssetFeature,
                        .params = {{"Name", "GPU_{instance}"s}}});
    features.push_back({.id = "ChassisPCIeDeviceHealth",
                        .create = createChassisPCIeDeviceHealthFeature,
                        .params = {{"Name", "GPU_{instance}"s}}});
    features.push_back({.id = "ChassisPCIeDevicePCIeDevice",
                        .create = createChassisPCIeDevicePCIeDeviceFeature,
                        .params = {{"Name", "GPU_{instance}"s}}});
#if defined(ENABLE_CLOCK_OUTPUT_STATE)
    features.push_back(
        {.id = "ChassisPCIeDeviceClockOutputEnableState",
         .create = createChassisPCIeDeviceClockOutputEnableStateFeature,
         .params = {{"Name", "GPU_{instance}"s}}});
#endif

    features.push_back(
        {.id = "Temp_0",
         .create = createTempFeature,
         .params = {{"Name", "{device}_TEMP_0"s},
                    {"SensorId", u64(0)},
                    {"PhysicalContext", "GPU"s},
                    {"Aggregated", true},
                    {"Priority", true},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}},
                    {"ProcessorAssociationPaths",
                     std::vector<std::string>{"accelerator/{deviceBase}"}},
                    {"ThresholdsEnabled", true},
                    {"UpperFatal", 95.0},
                    {"UpperCritical", 92.0},
                    {"UpperCaution", 90.0}}});
    features.push_back(
        {.id = "Temp_1",
         .create = createTempFeature,
         .params = {
             {"Name", "{device}_TEMP_1"s},
             {"SensorId", u64(2)},
             {"PhysicalContext", "GPU"s},
             {"Implementation", "Synthesized"s},
             {"ReadingBasis", "Headroom"s},
             {"Description",
              "Thermal Limit(TLIMIT) Temperature is the distance in deg C from the GPU temperature to the first throttle limit."s},
             {"Aggregated", true},
             {"Priority", true},
             {"ChassisAssociationPaths",
              std::vector<std::string>{"chassis/{device}"}},
             {"ProcessorAssociationPaths",
              std::vector<std::string>{"accelerator/{deviceBase}"}},
             {"ThresholdsEnabled", true},
             {"LowerFatal", -5.0},
             {"LowerCritical", -4.0},
             {"LowerCaution", -2.0}}});

    features.push_back(
        {.id = "DRAM_0_Power_0",
         .create = createPowerFeature,
         .params = {{"Name", "{device}_DRAM_0_Power_0"s},
                    {"SensorId", u64(1)},
                    {"AveragingInterval", u64(0)},
                    {"PhysicalContext", "GPU"s},
                    {"Aggregated", true},
                    {"Priority", true},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}},
                    {"ProcessorAssociationPaths",
                     std::vector<std::string>{"accelerator/{deviceBase}"}},
                    {"MemoryAssociationPaths",
                     std::vector<std::string>{"dimm/{deviceBase}_DRAM_0"}}}});
    features.push_back(
        {.id = "Power_0",
         .create = createPowerFeature,
         .params = {{"Name", "{device}_Power_0"s},
                    {"SensorId", u64(0)},
                    {"AveragingInterval", u64(0)},
                    {"PhysicalContext", "GPU"s},
                    {"MaxAllowableOperatingValue", 1020.0},
                    {"Aggregated", true},
                    {"Priority", true},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}},
                    {"ProcessorAssociationPaths",
                     std::vector<std::string>{"accelerator/{deviceBase}"}},
                    {"CompositeNumericSensorsEnabled", true},
                    {"PeakValueEnabled", true},
                    {"PeakValueSensorId", u64(0)},
                    {"PeakValueAveragingInterval", u64(0)},
                    {"PeakValueAggregated", true},
                    {"PeakValuePriority", false}}});

    features.push_back(
        {.id = "Energy_0",
         .create = createEnergyFeature,
         .params = {{"Name", "{device}_Energy_0"s},
                    {"SensorId", u64(0)},
                    {"PhysicalContext", "GPU"s},
                    {"Aggregated", true},
                    {"Priority", true},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}},
                    {"ProcessorAssociationPaths",
                     std::vector<std::string>{"accelerator/{deviceBase}"}}}});

    features.push_back(
        {.id = "Voltage_0",
         .create = createVoltageFeature,
         .params = {{"Name", "{device}_Voltage_0"s},
                    {"SensorId", u64(0)},
                    {"PhysicalContext", "GPU"s},
                    {"Aggregated", true},
                    {"Priority", false},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}},
                    {"ProcessorAssociationPaths",
                     std::vector<std::string>{"accelerator/{deviceBase}"}}}});

    features.push_back({.id = "EventSetting",
                        .create = createEventSettingFeature,
                        .params = {{"Name", "GlobalEventSetting"s},
                                   {"EventGenerationSetting", u64(2)}}});

    features.push_back(
        {.id = "EventConfigDeviceCapabilityDiscovery",
         .create = createEventConfigFeature,
         .params = {{"Name", "deviceCapabilityDiscoveryEventSetting"s},
                    {"MessageType", u64(0)},
                    {"SubscribedEventIDs", std::vector<uint64_t>{1, 2}}}});
    features.push_back(
        {.id = "EventConfigNetworkPort",
         .create = createEventConfigFeature,
         .params = {{"Name", "NetworkPortEventSetting"s},
                    {"MessageType", u64(1)},
                    {"SubscribedEventIDs", std::vector<uint64_t>{0}}}});
    features.push_back(
        {.id = "EventConfigPlatformEnvironment",
         .create = createEventConfigFeature,
         .params = {{"Name", "PlatformEnvironmentEventSetting"s},
                    {"MessageType", u64(3)},
                    {"SubscribedEventIDs", std::vector<uint64_t>{0, 1, 2}}}});
    features.push_back(
        {.id = "EventConfigDiagnostics",
         .create = createEventConfigFeature,
         .params = {{"Name", "DiagnosticsEventSetting"s},
                    {"MessageType", u64(4)},
                    {"SubscribedEventIDs", std::vector<uint64_t>{4}}}});

    features.push_back(
        {.id = "GPUSWInventory",
         .create = createGPUSWInventoryFeature,
         .params = {{"Name", "{platformPrefix}Driver_{deviceBase}"s},
                    {"Manufacturer", "NVIDIA"s},
                    {"ProcessorAssociationPaths",
                     std::vector<std::string>{"accelerator/{deviceBase}"}},
                    {"SoftwareVersionAssociationsEnabled", true}}});

    features.push_back(
        {.id = "XIDEvent",
         .create = createXIDEventFeature,
         .params = {
             {"Name", "XIDEventSetting"s},
             {"MessageId", "ResourceEvent.1.0.ResourceErrorsDetected"s},
             {"Severity", "Critical"s},
             {"LoggingNamespace", "GPU_{instance} XID"s},
             {"Resolution",
              "Regarding XID documentation and further actions please refer to XID and sXID Catalog for NVIDIA Data Center Products (NVOnline: 1115699)"s},
             {"MessageArgs",
              std::vector<std::string>{
                  "GPU_{instance} Driver Event Message",
                  "[{Timestamp}][{SequenceNumber}][{Flags:x}] XID {EventMessageReason} {MessageTextString}"}},
             {"EventIds",
              std::vector<std::string>{"XID 48", "GPU-ECC-UNCORRECTABLE-ERROR",
                                       "XID 94", "GPU-ECC-CONTAINED-ERROR"}}}});

    // Runtime IST complete event; Severity comes from the event payload
    features.push_back(
        {.id = "RuntimeISTCompleteEvent",
         .create = createRuntimeISTCompleteEventFeature,
         .params = {
             {"Name", "RuntimeISTCompleteEventSetting"s},
             {"MessageId", "ResourceEvent.1.0.ResourceErrorsDetected"s},
             {"LoggingNamespace", "GPU_{instance}"s},
             {"Resolution",
              "Check RIST app output, please contact with NVIDIA for the app and further troubleshooting."s},
             {"MessageArgs",
              std::vector<std::string>{
                  "GPU_{instance}",
                  "Runtime IST event [Result={Result}][AppVersion={AppVersion}][StatusCode={StatusCode}][MaxTemperature={MaxTemperature:.2f}C][AvgTemperature={AvgTemperature:.2f}C][Timestamp={Timestamp}][UUID={GpuId}]"}},
             {"EventIds", std::vector<std::string>{"Runtime IST Complete",
                                                   "GPU-RIST-EVENT"}}}});

    features.push_back(
        {.id = "RediscoveryEvent",
         .create = createRediscoveryEventFeature,
         .params = {
             {"Name", "RediscoveryEventSetting"s},
             {"MessageId", "ResourceEvent.1.0.ResourceErrorsDetected"s},
             {"Severity", "Informational"s},
             {"LoggingNamespace", "GPU_{instance}"s},
             {"Resolution",
              "GPU capabilities changed, driver state transitioned"s},
             {"MessageArgs",
              std::vector<std::string>{"GPU_{instance}", "rediscovery"}},
             {"EventIds",
              std::vector<std::string>{"Rediscovery", "GPU-REDISCOVERY-EVENT"}},
             {"Logging", true}}});

    features.push_back(
        {.id = "ResetRequiredEvent",
         .create = createResetRequiredEventFeature,
         .params = {
             {"Name", "ResetRequiredEventSetting"s},
             {"MessageId", "Base.1.13.ResetRequired"s},
             {"Severity", "Warning"s},
             {"EventIds",
              std::vector<std::string>{"ResetRequired", "GPU-RST-REQ-EVENT"}},
             {"LoggingNamespace", "GPU_{instance}"s},
             {"Resolution", "Reset the GPU or power cycle the Baseboard."s},
             {"MessageArgs",
              std::vector<std::string>{"GPU_{instance}", "ForceRestart"}}}});

    features.push_back(
        {.id = "ThresholdEvent",
         .create = createThresholdEventFeature,
         .params = {
             {"Name", "ThresholdEventSetting"s},
             {"MessageId", "ResourceEvent.1.0.ResourceErrorsDetected"s},
             {"Severity", "Critical"s},
             {"LoggingNamespace", "{device} Threshold"s},
             {"Resolution",
              "Follow Debug and RAS Guide for NVIDIA Data Center Products (NVOnline: 1109712) for further actions."s},
             {"MessageArgs", std::vector<std::string>{"{device}", "No Error"}},
             {"EventIds",
              std::vector<std::string>{"Threshold", "GPU-THRESHOLD-EVENT"}}}});

    features.push_back(
        {.id = "NVLink",
         .create = createNVLinkFeature,
         .params = {
             {"Name", "NVLink"s},
             {"ParentObjPath",
              "/xyz/openbmc_project/inventory/system/accelerator/{deviceBase}"s},
             {"Priority", false},
             {"Count", var<uint64_t>("{NvLinkCount}")}}});

    features.push_back(
        {.id = "ClockLimit_0",
         .create = createClockLimitFeature,
         .params = {
             {"Name", "GPU_{instance} ClockLimit_0"s},
             {"PhysicalContext",
              "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU"s},
             {"ParentChassisAssociationPaths",
              std::vector<std::string>{"chassis/{device}"}},
             {"ClockMode", "com.nvidia.ClockMode.Mode.MaximumPerformance"s},
             {"Priority", false}}});

    features.push_back(
        {.id = "Processor",
         .create = createProcessorFeature,
         .params = {{"Name", "GPU_{instance} Processor"s},
                    {"ParentChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}},
                    {"BridgingChassisAssociationPath",
                     optVar<std::string>("{BridgeName}")},
                    {"TotalPowerAssociationEnabled", true}}});

    features.push_back(
        {.id = "ProcessorAttributes",
         .create = createProcessorAttributesFeature,
         .params = {
             {"Name", "GPU_{instance} Processor"s},
             {"LocationType", "xyz.openbmc_project.Inventory.Decorator."
                              "Location.LocationTypes.Embedded"s},
             {"LocationCode", optVar<std::string>("{LocationCode}")},
             {"LocationContext", optVar<std::string>("{LocationContext}")},
             {"FieldReplaceable", false},
             {"MIGModeSupported", true},
             {"PortDisableFutureSupported", true},
             {"ECCModeSupported", true},
             {"EDPpScalingFactorSupported", true},
             {"PowerSmoothingSupported", true},
             {"CpuOperatingConfigSupported", true},
             {"MemCapacityUtilSupported", true},
             {"TotalNvLinksCountSupported", true},
             {"EGMModeSupported", true},
             {"MNNVLTopologySupported", true},
             {"MctpNsmOperationalStatusSupported", true},
             {"GPUBasePowerLimitSupported", true},
             {"GPUCopyCPUPowerLimitSupported", true},
             {"GPUCopySwitchPowerLimitSupported", true},
             {"SoCPowerSmoothingSupported", true},
             {"CPEREventSupported", true}}});

    features.push_back({.id = "PowerCap",
                        .create = createPowerCapFeature,
                        .params = {{"Name", "GPU_{instance} Power Cap"s}}});

    features.push_back({.id = "WorkloadPowerProfile",
                        .create = createWorkloadPowerProfileFeature,
                        .params = {{"Name", "GPU_{instance} Processor"s}}});

    features.push_back({.id = "ProcessorPerformance",
                        .create = createProcessorPerformanceFeature,
                        .params = {{"Name", "GPU_{instance} Processor"s},
                                   {"DeviceId", u64(0)}}});

    features.push_back(
        {.id = "ReconfigPermissions",
         .create = createReconfigPermissionsFeature,
         .params = {{"Name", "GPU_{instance} Processor"s},
                    {"AvailableKnobs",
                     std::vector<std::string>{"InSystemTest",
                                              "FusingMode",
                                              "CCMode",
                                              "BAR0Firewall",
                                              "CCDevMode",
                                              "TGPCurrentLimit",
                                              "TGPRatedLimit",
                                              "TGPMaxLimit",
                                              "TGPMinLimit",
                                              "ClockLimit",
                                              "NVLinkDisable",
                                              "ECCEnable",
                                              "PCIeVFConfiguration",
                                              "RowRemappingAllowed",
                                              "RowRemappingFeature",
                                              "HBMFrequencyChange",
                                              "HULKLicenseUpdate",
                                              "ForceTestCoupling",
                                              "BAR0TypeConfig",
                                              "EDPpScalingFactor",
                                              "PowerSmoothingPrivilegeLevel1",
                                              "PowerSmoothingPrivilegeLevel2",
                                              "EGMMode",
                                              "InfoROMFileSystemRecreate",
                                              "RISTDiagnostic",
                                              "AdaptiveTGPMode"}},
                    {"DisabledKnobs", std::vector<std::string>{}}}});

    features.push_back(
        {.id = "Memory",
         .create = createMemoryFeature,
         .params = {
             {"Name", "GPU_{instance} Memory"s},
             {"ErrorCorrection",
              "xyz.openbmc_project.Inventory.Item.Dimm.Ecc.SingleBitECC"s},
             {"DeviceType",
              "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.HBM"s},
             {"ParentChassisAssociationPaths",
              std::vector<std::string>{"chassis/{device}"}},
             {"ProcessorAssociationPaths",
              std::vector<std::string>{"accelerator/{deviceBase}"}}}});

    features.push_back({.id = "MemoryAttributes",
                        .create = createMemoryAttributesFeature,
                        .params = {{"Name", "GPU_{instance} Memory"s}}});

    // Develop's static EM Dimm record
    features.push_back(
        {.id = "MemoryLocation",
         .create = createMemoryLocationFeature,
         .params = {
             {"Name", "GPU_{instance} Memory"s},
             {"LocationCode", optVar<std::string>("{MemoryLocationCode}")},
             {"LocationContext", optVar<std::string>("{LocationContext}")}}});

    features.push_back(
        {.id = "PCIe_0",
         .create = createGpuPCIeFeature,
         .params = {
             {"Name", "GPU_{instance} PCIe_0"s},
             {"Health",
              "xyz.openbmc_project.State.Decorator.Health.HealthType.OK"s},
             {"ChasisPowerState",
              "xyz.openbmc_project.State.Chassis.PowerState.On"s},
             {"DeviceIndex", u64(0)},
             {"ClearableScalarGroup", std::vector<uint64_t>{2, 3, 4}},
             {"AllStatesAssociationsEnabled", true}}});

    features.push_back(
        {.id = "PCIe_0_PortInfo",
         .create = createGpuPCIePortInfoFeature,
         .params = {
             {"Name", "GPU_{instance} PCIe_0"s},
             {"PortType",
              "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort"s},
             {"PortProtocol",
              "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe"s},
             {"DeviceIndex", u64(0)},
             {"Priority", false}}});

    // GPM metrics, PerInstanceMetrics flattened per metric
    features.push_back(
        {.id = "GPMMetrics",
         .create = createGPMMetricsFeature,
         .params = {{"Name", "GPU_{instance} GPM_Aggregated_Metrics"s},
                    {"AggregatedRetrievalSource", u64(1)},
                    {"AggregatedGpuInstance", u64(255)},
                    {"AggregatedComputeInstance", u64(255)},
                    {"AggregatedMetricsBitfield",
                     std::vector<uint64_t>{255, 255, 223, 255}},
                    {"AggregatedMemoryBandwidth", true},
                    {"NVDECPerInstanceName",
                     "GPU_{instance} GPM_NVDEC_PerInstance_Metrics"s},
                    {"NVDECPerInstanceEnabled", true},
                    {"NVDECPerInstanceRetrievalSource", u64(2)},
                    {"NVDECPerInstanceGpuInstance", u64(255)},
                    {"NVDECPerInstanceComputeInstance", u64(255)},
                    {"NVDECPerInstanceBitfield", std::vector<uint64_t>{255}},
                    {"NVJPGPerInstanceName",
                     "GPU_{instance} GPM_NVJPG_PerInstance_Metrics"s},
                    {"NVJPGPerInstanceEnabled", true},
                    {"NVJPGPerInstanceRetrievalSource", u64(2)},
                    {"NVJPGPerInstanceGpuInstance", u64(255)},
                    {"NVJPGPerInstanceComputeInstance", u64(255)},
                    {"NVJPGPerInstanceBitfield", std::vector<uint64_t>{255}},
                    {"NVENCPerInstanceName",
                     "GPU_{instance} GPM_NVENC_PerInstance_Metrics"s},
                    {"NVENCPerInstanceEnabled", false},
                    {"NVENCPerInstanceRetrievalSource", u64(2)},
                    {"NVENCPerInstanceGpuInstance", u64(255)},
                    {"NVENCPerInstanceComputeInstance", u64(255)},
                    {"NVENCPerInstanceBitfield", std::vector<uint64_t>{255}}}});

    features.push_back(
        {.id = "GPMPortMetrics",
         .create = createGPMPortMetricsFeature,
         .params = {
             {"Name", "GPU_{instance} GPM_Port_Metrics"s},
             {"ParentObjPath",
              "/xyz/openbmc_project/inventory/system/accelerator/{deviceBase}"s},
             {"RetrievalSource", u64(0)},
             {"GpuInstance", u64(255)},
             {"ComputeInstance", u64(255)},
             {"Metrics", std::vector<std::string>{"NVLinkRawTxBandwidthGbps",
                                                  "NVLinkDataTxBandwidthGbps",
                                                  "NVLinkRawRxBandwidthGbps",
                                                  "NVLinkDataRxBandwidthGbps"}},
             {"NVLinkCount", var<uint64_t>("{NvLinkCount}")},
             {"InstanceBitfield",
              std::vector<uint64_t>{255, 255, 255, 255, 15}}}});

    // IRoT chassis with its slots; one array per slot field, element i is
    // slot i + 1
    features.push_back(
        {.id = "RoT",
         .create = createRoTFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"ImageCopyEnabled", true},
             {"ImageCopyStateEnabled", false},
             {"InbandUpdatePolicyEnabled", true},
             {"SlotChassisName",
              std::vector<std::string>{"{deviceBase}", "{deviceBase}"}},
             {"SlotFirmwareType", std::vector<std::string>{"AP", "AP"}},
             {"SlotType", std::vector<std::string>{"Active", "InActive"}},
             {"SlotComponentClassification", std::vector<uint64_t>{10, 10}},
             {"SlotComponentIdentifier", std::vector<uint64_t>{49152, 49152}},
             {"SlotComponentIndex", std::vector<uint64_t>{0, 0}},
             {"SlotSoftwareName",
              std::vector<std::string>{"{platformPrefix}FW_{deviceBase}",
                                       "{platformPrefix}FW_{deviceBase}"}}}});

    // IRoT responder objects, all on chassis/<platformPrefix>IRoT_<deviceBase>
    features.push_back(
        {.id = "IRoTChassisType",
         .create = createIRoTChassisTypeFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"ChassisType",
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"s}}});
    features.push_back(
        {.id = "IRoTLocationType",
         .create = createIRoTLocationTypeFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"LocationType",
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"s}}});
    features.push_back(
        {.id = "IRoTLocationContext",
         .create = createIRoTLocationContextFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"LocationContext", optVar<std::string>("{LocationContext}")}}});
    features.push_back(
        {.id = "IRoTLocationCode",
         .create = createIRoTLocationCodeFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"LocationCode", "IRoT_{LocationCode}"s}}});
    features.push_back(
        {.id = "IRoTFieldReplaceable",
         .create = createIRoTFieldReplaceableFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"FieldReplaceable", false}}});
    // UUID object; its update adds SPDMResponder once the device UUID is read.
    // HGXR disables MctpUuidEnabled through policy
    features.push_back(
        {.id = "IRoTSPDMResponder",
         .create = createIRoTSPDMResponderFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"MctpUuidEnabled", true}}});
    features.push_back(
        {.id = "IRoTAssociations",
         .create = createIRoTAssociationsFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"ParentChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}},
                    {"RoTInventoryAssociationPaths",
                     std::vector<std::string>{"accelerator/{deviceBase}"}},
                    {"InventoryChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}}}});
    features.push_back(
        {.id = "IRoTAsset",
         .create = createIRoTAssetFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"AssetName", "{device} IRoT"s}}});
    features.push_back(
        {.id = "IRoTHealth",
         .create = createIRoTHealthFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s}}});
    // Decorator.SKU written by pldm (EC SKU, PLDMDeviceInventory
    // CreateInventoryPath)
    features.push_back(
        {.id = "IRoTSKU",
         .create = createIRoTSKUFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s}}});

#ifdef ENABLE_DEBUG_TOKEN
    features.push_back(
        {.id = "DebugToken",
         .create = createDebugTokenFeature,
         .params = {{"ChassisName", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"DebugTokenType", std::vector<std::string>{"Unified"}},
                    {"DebugTokenDeviceType", "GPU"s}}});
#endif

    // Firmware component type: no nsmd consumer found, placeholder
    features.push_back({.id = "FWComponentType",
                        .create = createFWComponentTypeFeature,
                        .params = {{"Name", "AP"s}, {"ComponentType", "AP"s}}});

    return features;
}

} // namespace nsm::features
