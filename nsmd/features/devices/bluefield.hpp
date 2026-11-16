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

#include "features/creators.hpp"
#include "features/feature.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nsm::features
{

/** @brief Features of a BlueField NIC, in creation order */
inline FeatureList bluefieldFeatures()
{
    using namespace std::string_literals;

    FeatureList features;

    // NVLink management NIC chassis, on chassis/<device>
    features.push_back({.id = "NVLinkMgmtNicChassis",
                        .create = createNVLinkMgmtNicChassisFeature,
                        .params = {{"Name", "{device}"s}}});
    features.push_back(
        {.id = "NVLinkMgmtNicChassisAttributes",
         .create = createNVLinkMgmtNicChassisAttributesFeature,
         .params = {
             {"Name", "{device}"s},
             {"ChassisType",
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module"s},
             {"LocationType",
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Bay"s},
             {"LocationCode", optVar<std::string>("{LocationCode}")},
             {"LocationContext", "{parentChassisName}"s},
             {"FieldReplaceable", true}}});
    // bridged_chassis / bridging_chassis to the SMA bridging this device
    features.push_back({.id = "ChassisAssociations",
                        .create = createChassisAssociationsFeature,
                        .params = {{"Name", "{device}"s},
                                   {"BridgingChassisAssociationPath",
                                    optVar<std::string>("{BridgeName}")}}});
    // Assembly with its attributes, on chassis/<device>/Assembly<N>, numbered
    // with the device's other assemblies
    features.push_back(
        {.id = "NVLinkMgmtNicChassisAssembly",
         .create = createNVLinkMgmtNicChassisAssemblyFeature,
         .params = {
             {"Name", "Device Assembly"s},
             {"PhysicalContext",
              "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.Board"s},
             {"LocationType",
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Slot"s}}});
    // Network adapter, on chassis/<device>/NetworkAdapters/<Name>; mode
    // objects only for DeviceModesSupported entries other than -1
    features.push_back(
        {.id = "NetworkAdapter",
         .create = createNetworkAdapterFeature,
         .params = {{"Name", var<std::string>("{NetworkAdapterName}")},
                    {"ParentChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}},
                    {"DeviceModesSupported",
                     std::vector<int64_t>{
                         -1, -1, -1, 1,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,  -1, 1}}}});
    // NVLink ports with network addresses, on <adapter>/Ports/Port_<n>
    features.push_back(
        {.id = "Port",
         .create = createNVLinkFeature,
         .params = {
             {"Name", "Port"s},
             {"ParentObjPath",
              "/xyz/openbmc_project/inventory/system/chassis/{device}/NetworkAdapters/{NetworkAdapterName}"s},
             {"NetworkPortAddressesEnabled", true},
             {"SupportFECHistogram", true},
             {"OpticalModuleTelemetrySupported", true},
             {"Priority", false},
             {"Count", var<uint64_t>("{PortCount}")}}});
    // PCIe device objects, all on chassis/<device>/PCIeDevices/<device>
    features.push_back({.id = "ChassisPCIeDeviceUUID",
                        .create = createChassisPCIeDeviceUUIDFeature,
                        .params = {{"Name", "{device}"s}}});
    features.push_back(
        {.id = "ChassisPCIeDeviceAssociations",
         .create = createChassisPCIeDeviceAssociationsFeature,
         .params = {{"Name", "{device}"s},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}}}});
    features.push_back({.id = "ChassisPCIeDeviceAsset",
                        .create = createChassisPCIeDeviceAssetFeature,
                        .params = {{"Name", "{device}"s}}});
    features.push_back({.id = "ChassisPCIeDeviceHealth",
                        .create = createChassisPCIeDeviceHealthFeature,
                        .params = {{"Name", "{device}"s}}});
    // PCIe device; the AER error status is created with it
    features.push_back(
        {.id = "ChassisPCIeDeviceMultiPortPCIeDevice",
         .create = createChassisPCIeDeviceMultiPortPCIeDeviceFeature,
         .params = {{"Name", "{device}"s}}});
    features.push_back(
        {.id = "ChassisPCIeDeviceAERErrorStatus",
         .create = createChassisPCIeDeviceRetimerAERErrorStatusFeature,
         .params = {{"Name", "{device}"s}}});
    // PCIe fabric, on fabrics/<Name>
    features.push_back(
        {.id = "PCIeRetimerFabrics",
         .create = createPCIeRetimerFabricsFeature,
         .params = {
             {"Name", "{platformPrefix}PCIeTopology_{deviceBase}"s},
             {"FabricType",
              "xyz.openbmc_project.Inventory.Item.Fabric.FabricType.PCIe"s},
             {"ChassisAssociationPaths",
              std::vector<std::string>{"chassis/{device}"}}}});
    // PCIe switch, on fabrics/<FabricName>/Switches/<Name>
    features.push_back(
        {.id = "MultiPortPCIeSwitchDevice",
         .create = createMultiPortPCIeSwitchDeviceFeature,
         .params = {
             {"Name", var<std::string>("{SwitchName}")},
             {"FabricName", "{platformPrefix}PCIeTopology_{deviceBase}"s},
             {"DeviceInstance", u64(0)},
             {"ParentChassisAssociationPaths",
              std::vector<std::string>{"chassis/{device}"}},
             {"FabricAssociationPaths",
              std::vector<std::string>{
                  "fabrics/{platformPrefix}PCIeTopology_{deviceBase}"}}}});
    // PCIe links, ports discovered under <switch>/Ports/
    features.push_back(
        {.id = "MultiPCIeLink",
         .create = createMultiPCIeLinkFeature,
         .params = {
             {"Name", "PCIePorts"s},
             {"FabricName", "{platformPrefix}PCIeTopology_{deviceBase}"s},
             {"SwitchName", var<std::string>("{SwitchName}")},
             {"UpstreamPortName", "UP"s},
             {"DownstreamPortName", "DOWN"s},
             {"ParentDeviceAssociationsEnabled", true}}});

    // IRoT chassis with its slots; one array per slot field, element i is
    // slot i + 1
    features.push_back(
        {.id = "RoT",
         .create = createRoTFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"SlotChassisName",
              std::vector<std::string>{"{device}", "{device}"}},
             {"SlotFirmwareType", std::vector<std::string>{"AP", "AP"}},
             {"SlotType", std::vector<std::string>{"Active", "InActive"}},
             {"SlotComponentClassification", std::vector<uint64_t>{10, 10}},
             {"SlotComponentIdentifier", std::vector<uint64_t>{1, 1}},
             {"SlotComponentIndex", std::vector<uint64_t>{0, 0}},
             {"SlotSoftwareName",
              std::vector<std::string>{"{platformPrefix}FW_{deviceBase}",
                                       "{platformPrefix}FW_{deviceBase}"}}}});

    // IRoT responder objects, all on chassis/<platformPrefix>IRoT_<deviceBase>
    // UUID object; its update adds SPDMResponder once the device UUID is read
    features.push_back(
        {.id = "IRoTSPDMResponder",
         .create = createIRoTSPDMResponderFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s}}});
    features.push_back(
        {.id = "IRoTAssociations",
         .create = createIRoTAssociationsFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"ParentChassisAssociationPaths",
              std::vector<std::string>{"chassis/{device}"}},
             // BF4 IRoT is intentionally not a host trusted component; no
             // InventoryChassisAssociationPaths, since inventory_chassis /
             // associated_chassis to BlueField_0 would make bmcweb advertise a
             // (phantom) TrustedComponents link, collection, DOT and
             // ComponentIntegrity cert surfaces.
             {"RoTInventoryAssociationPaths",
              std::vector<std::string>{
                  "chassis/{device}/NetworkAdapters/{NetworkAdapterName}"}}}});
    features.push_back(
        {.id = "IRoTAsset",
         .create = createIRoTAssetFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"AssetName", "{device} IRoT"s}}});
    features.push_back(
        {.id = "IRoTHealth",
         .create = createIRoTHealthFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s}}});
    features.push_back(
        {.id = "IRoTLocationType",
         .create = createIRoTLocationTypeFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"LocationType",
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"s}}});
    features.push_back(
        {.id = "IRoTChassisType",
         .create = createIRoTChassisTypeFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"ChassisType",
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"s}}});
    // Entity decorators of <platformPrefix>IRoT_<deviceBase>, owned by nsmd
    features.push_back(
        {.id = "IRoTLocationCode",
         .create = createIRoTLocationCodeFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"LocationCode", "IRoT_0"s}}});
    features.push_back(
        {.id = "IRoTLocationContext",
         .create = createIRoTLocationContextFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"LocationContext", "{parentChassisName}/{device}"s}}});
    features.push_back(
        {.id = "IRoTFieldReplaceable",
         .create = createIRoTFieldReplaceableFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"FieldReplaceable", false}}});

#ifdef ENABLE_DEBUG_TOKEN
    features.push_back(
        {.id = "DebugToken",
         .create = createDebugTokenFeature,
         .params = {{"ChassisName", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"DebugTokenType", std::vector<std::string>{"Unified"}},
                    {"DebugTokenDeviceType", "NIC"s}}});
#endif

    // Firmware component type: no nsmd consumer found, placeholder
    features.push_back({.id = "FWComponentType",
                        .create = createFWComponentTypeFeature,
                        .params = {{"Name", "AP"s}, {"ComponentType", "AP"s}}});

    return features;
}

} // namespace nsm::features
