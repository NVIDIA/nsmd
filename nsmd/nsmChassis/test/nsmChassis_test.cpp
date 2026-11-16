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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"

#include "../../common/nsmPropertySupport.hpp"
#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmChassis.hpp"
#include "nsmDebugInfo.hpp"
#include "nsmErrorInjection/nsmErrorInjectionCommon.hpp"
#include "nsmGPIO/nsmGPIOStateCommon.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmPowerSupplyStatus.hpp"
#include "nsmProcessor/nsmOemResetStatistics.hpp"
#include "nsmWriteProtectedJumper.hpp"

using namespace nsm;
using namespace std::string_literals;

struct NsmChassisTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string name = "HGX_GPU_SXM_1";

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";
    const uuid_t gpuDeviceUuid = "000b3ec1-0068-0045-0086-000009062aa8";
    const uuid_t gpuMctpUuid = "00000000-0000-0000-0000-0000000000a4";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmChassisTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_EQ(2, devices.size());
        EXPECT_NE(gpu, nullptr);
        EXPECT_NE(fpga, nullptr);
        EXPECT_EQ(NSM_DEV_ID_GPU, gpu->getDeviceType());
        EXPECT_EQ(NSM_DEV_ID_BASEBOARD, fpga->getDeviceType());
    }

    ~NsmChassisTest()
    {
        cleanupDeviceSensors(devices);
        gpu.reset();
        fpga.reset();
    }

    /** @brief Chassis ChassisAttributes params of a GPU device list */
    const dbus::PropertyMap asset = {
        {"AssetInformationAvailable", true},
        {"ChassisType",
         "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module"s},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"s},
        {"LocationCode", "SXM2"s},
    };

    /** @brief Run a chassis creator for `device` as its device list would;
     *  the params get the chassis Name unless they set one
     */
    requester::Coroutine run(nsm::features::CreatorFn create,
                             const std::shared_ptr<MockNsmDevice>& device,
                             dbus::PropertyMap params,
                             std::optional<uint8_t> deviceType = std::nullopt)
    {
        nsm::features::DeviceContext dev;
        dev.name = name;
        dev.kind = "chassis";
        dev.platformPrefix = "HGX_";
        dev.device = device;
        if (device)
        {
            dev.deviceType = device->getDeviceType();
            dev.deviceRole = device->getDeviceRole();
        }
        if (deviceType)
        {
            dev.deviceType = *deviceType;
        }
        params.try_emplace("Name", name);
        return create(mockManager, dev, params);
    }

    requester::Coroutine
        createChassis(const std::shared_ptr<MockNsmDevice>& device,
                      dbus::PropertyMap params = {})
    {
        return run(nsm::features::createChassisFeature, device,
                   std::move(params));
    }

    requester::Coroutine
        createAttributes(const std::shared_ptr<MockNsmDevice>& device,
                         dbus::PropertyMap params,
                         std::optional<uint8_t> deviceType = std::nullopt)
    {
        return run(nsm::features::createChassisAttributesFeature, device,
                   std::move(params), deviceType);
    }

    requester::Coroutine
        createAssociations(const std::shared_ptr<MockNsmDevice>& device,
                           dbus::PropertyMap params)
    {
        return run(nsm::features::createChassisAssociationsFeature, device,
                   std::move(params));
    }

    /** @brief Sensors of type T among `sensors` */
    template <typename T, typename Sensors>
    static std::vector<std::shared_ptr<T>> find(const Sensors& sensors)
    {
        std::vector<std::shared_ptr<T>> found;
        for (const auto& sensor : sensors)
        {
            if (auto typed = std::dynamic_pointer_cast<T>(sensor))
            {
                found.push_back(typed);
            }
        }
        return found;
    }
};

// ============================================================================
// createChassisFeature / createChassisAssociationsFeature /
// createChassisAttributesFeature: chassis sensors from the feature params
// ============================================================================

TEST_F(NsmChassisTest, goodTestCreateGpuChassis)
{
    gpu->deviceUuid = gpuDeviceUuid;
    gpu->uuid = gpuMctpUuid;
    const size_t before = gpu->deviceSensors.size();
    const size_t staticBefore = gpu->staticSensors.size();
    const size_t roundRobinBefore = gpu->roundRobinSensors.size();

    auto rc = createChassis(gpu);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    rc = createAssociations(
        gpu, {{"ParentChassisAssociationPaths",
               std::vector<std::string>{"chassis/HGX_Baseboard_0"}}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    rc = createAttributes(gpu, asset);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());

    // UUID, MCTP UUID, associations; asset part number, serial number, model,
    // SKU, location, location code, chassis type, health
    EXPECT_EQ(staticBefore + 11, gpu->staticSensors.size());
    EXPECT_EQ(roundRobinBefore, gpu->roundRobinSensors.size());
    EXPECT_EQ(before + 11, gpu->deviceSensors.size());

    auto sensors = before;
    auto chassisUuid = dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
        gpu->deviceSensors[sensors++]);
    ASSERT_NE(nullptr, chassisUuid);
    EXPECT_EQ(gpuDeviceUuid, chassisUuid->invoke(pdiMethod(uuid)));

    auto mctpUuid = dynamic_pointer_cast<NsmInterfaceProvider<MctpUuidIntf>>(
        gpu->deviceSensors[sensors++]);
    ASSERT_NE(nullptr, mctpUuid);
    EXPECT_EQ(gpuMctpUuid, mctpUuid->invoke(pdiMethod(uuid)));

    auto associations =
        dynamic_pointer_cast<NsmInterfaceProvider<AssociationDefinitionsInft>>(
            gpu->deviceSensors[sensors++]);
    ASSERT_NE(nullptr, associations);
    EXPECT_EQ(1, associations->invoke(pdiMethod(associations)).size());

    // Asset inventory property sensors (part number, serial number, model)
    for (auto property : {FRU_PART_NUMBER, SERIAL_NUMBER, MARKETING_NAME})
    {
        auto assetProperty =
            dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                gpu->deviceSensors[sensors++]);
        ASSERT_NE(nullptr, assetProperty);
        EXPECT_EQ(property, assetProperty->property);
        EXPECT_EQ(MANUFACTURER_NVIDIA,
                  assetProperty->invoke(pdiMethod(manufacturer)));
    }

    auto sku = dynamic_pointer_cast<NsmInterfaceProvider<NsmApSkuIdIntf>>(
        gpu->deviceSensors[sensors++]);
    ASSERT_NE(nullptr, sku);
    // A GPU has a SKU of its own: not tombstoned
    EXPECT_NE(std::string(propertyNotSupported), sku->invoke(pdiMethod(sku)));

    auto location = dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
        gpu->deviceSensors[sensors++]);
    ASSERT_NE(nullptr, location);
    EXPECT_EQ(LocationIntf::LocationTypes::Embedded,
              location->invoke(pdiMethod(locationType)));

    auto locationCode =
        dynamic_pointer_cast<NsmInterfaceProvider<LocationCodeIntf>>(
            gpu->deviceSensors[sensors++]);
    ASSERT_NE(nullptr, locationCode);
    EXPECT_EQ("SXM2", locationCode->invoke(pdiMethod(locationCode)));

    auto chassis = dynamic_pointer_cast<NsmInterfaceProvider<ChassisIntf>>(
        gpu->deviceSensors[sensors++]);
    ASSERT_NE(nullptr, chassis);
    EXPECT_EQ(ChassisIntf::ChassisType::Module,
              chassis->invoke(pdiMethod(type)));

    auto health = dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
        gpu->deviceSensors[sensors++]);
    ASSERT_NE(nullptr, health);
    EXPECT_EQ(HealthIntf::HealthType::OK, health->invoke(pdiMethod(health)));
}

TEST_F(NsmChassisTest, goodTestCreateBaseboardChassis)
{
    fpga->deviceUuid = "fpga-device-uuid";
    const size_t staticBefore = fpga->staticSensors.size();

    auto rc = createChassis(fpga, {{"PCIeRefClockEnabled", true}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());

    auto chassisUuid =
        find<NsmInterfaceProvider<UuidIntf>>(fpga->staticSensors);
    ASSERT_EQ(1, chassisUuid.size());
    EXPECT_EQ("fpga-device-uuid", chassisUuid[0]->invoke(pdiMethod(uuid)));
    EXPECT_EQ(
        1,
        find<NsmInterfaceProvider<MctpUuidIntf>>(fpga->staticSensors).size());
#ifdef NVIDIA_FPGA_PCIE_REFERENCE_CLOCK_COUNT
    EXPECT_EQ(staticBefore + 3, fpga->staticSensors.size());
    EXPECT_EQ(1,
              find<NsmInterfaceProvider<PCIeRefClockIntf>>(fpga->staticSensors)
                  .size());
#else
    EXPECT_EQ(staticBefore + 2, fpga->staticSensors.size());
#endif
}

TEST_F(NsmChassisTest, goodTestCreateChassisWithoutPCIeRefClock)
{
    const size_t staticBefore = fpga->staticSensors.size();

    auto rc = createChassis(fpga);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Only the UUID and MCTP UUID: the list does not enable PCIeRefClock
    EXPECT_EQ(staticBefore + 2, fpga->staticSensors.size());
    EXPECT_TRUE(
        find<NsmInterfaceProvider<PCIeRefClockIntf>>(fpga->staticSensors)
            .empty());
}

TEST_F(NsmChassisTest, goodTestCreateChassisEmptyDeviceUuid)
{
    // The device UUID is read over NSM later; before that it is empty
    auto rc = createChassis(gpu);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    auto chassisUuid = find<NsmInterfaceProvider<UuidIntf>>(gpu->staticSensors);
    ASSERT_EQ(1, chassisUuid.size());
    EXPECT_EQ("", chassisUuid[0]->invoke(pdiMethod(uuid)));
}

TEST_F(NsmChassisTest, badTestCreateChassisNoNsmDevice)
{
    auto rc = createChassis(nullptr);
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_FALSE(rc.exception());
}

TEST_F(NsmChassisTest, badTestCreateChassisNoName)
{
    nsm::features::DeviceContext dev;
    dev.name = name;
    dev.device = gpu;
    EXPECT_THROW_COROUTINE(
        nsm::features::createChassisFeature(mockManager, dev, {}),
        std::out_of_range);
}

TEST_F(NsmChassisTest, goodTestChassisParentAndBridgingAssociations)
{
    const size_t before = gpu->staticSensors.size();

    auto rc = createAssociations(
        gpu, {{"ParentChassisAssociationPaths",
               std::vector<std::string>{"board/HGX_ProcessorModule_0", "",
                                        "chassis/HGX_Baseboard_0"}},
              {"BridgingChassisAssociationPath", "chassis/HGX_Bridge_0"s}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    ASSERT_EQ(before + 1, gpu->staticSensors.size());

    auto associations = find<NsmInterfaceProvider<AssociationDefinitionsInft>>(
        gpu->staticSensors);
    ASSERT_EQ(1, associations.size());
    const auto links = associations[0]->invoke(pdiMethod(associations));
    // The empty parent entry gives no link
    ASSERT_EQ(3, links.size());
    const std::string system = inventorySystemBasePath.string();
    EXPECT_EQ(std::make_tuple("parent_chassis"s, "all_chassis"s,
                              system + "/board/HGX_ProcessorModule_0"),
              links[0]);
    EXPECT_EQ(std::make_tuple("parent_chassis"s, "all_chassis"s,
                              system + "/chassis/HGX_Baseboard_0"),
              links[1]);
    EXPECT_EQ(std::make_tuple("bridged_chassis"s, "bridging_chassis"s,
                              system + "/chassis/HGX_Bridge_0"),
              links[2]);
}

TEST_F(NsmChassisTest, goodTestChassisAssociationsNoTargets)
{
    const size_t before = gpu->staticSensors.size();

    auto rc = createAssociations(
        gpu, {{"ParentChassisAssociationPaths", std::vector<std::string>{""}},
              {"BridgingChassisAssociationPath", ""s}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // No link: no association sensor
    EXPECT_EQ(before, gpu->staticSensors.size());

    rc = createAssociations(gpu, {});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(before, gpu->staticSensors.size());
}

TEST_F(NsmChassisTest, badTestChassisAssociationsNoNsmDevice)
{
    auto rc = createAssociations(
        nullptr, {{"ParentChassisAssociationPaths",
                   std::vector<std::string>{"chassis/HGX_Baseboard_0"}}});
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_FALSE(rc.exception());
}

TEST_F(NsmChassisTest, goodTestCreateStaticSensors)
{
    const size_t before = fpga->deviceSensors.size();
    const size_t staticBefore = fpga->staticSensors.size();
    const size_t roundRobinBefore = fpga->roundRobinSensors.size();

    auto rc = createAttributes(fpga, {{"AssetInformationAvailable", true},
                                      {"DimensionSupported", true},
                                      {"WriteProtectSupported", true}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());

    // part number, serial number, model, SKU, health, depth, width, height
    EXPECT_EQ(staticBefore + 8, fpga->staticSensors.size());
    // write protect jumper
    EXPECT_EQ(roundRobinBefore + 1, fpga->roundRobinSensors.size());
    EXPECT_EQ(before + 9, fpga->deviceSensors.size());

    auto sensors = before;
    auto partNumber = dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
        fpga->deviceSensors[sensors++]);
    auto serialNumber =
        dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
            fpga->deviceSensors[sensors++]);
    auto model = dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
        fpga->deviceSensors[sensors++]);
    auto sku = dynamic_pointer_cast<NsmInterfaceProvider<NsmApSkuIdIntf>>(
        fpga->deviceSensors[sensors++]);
    sensors++; // health
    auto depth = dynamic_pointer_cast<NsmInventoryProperty<DimensionIntf>>(
        fpga->deviceSensors[sensors++]);
    auto width = dynamic_pointer_cast<NsmInventoryProperty<DimensionIntf>>(
        fpga->deviceSensors[sensors++]);
    auto height = dynamic_pointer_cast<NsmInventoryProperty<DimensionIntf>>(
        fpga->deviceSensors[sensors++]);
    ASSERT_NE(nullptr, partNumber);
    ASSERT_NE(nullptr, serialNumber);
    ASSERT_NE(nullptr, model);
    ASSERT_NE(nullptr, sku);
    ASSERT_NE(nullptr, depth);
    ASSERT_NE(nullptr, width);
    ASSERT_NE(nullptr, height);

    EXPECT_EQ(FRU_PART_NUMBER, partNumber->property);
    EXPECT_EQ(SERIAL_NUMBER, serialNumber->property);
    EXPECT_EQ(MARKETING_NAME, model->property);
    EXPECT_EQ(PRODUCT_LENGTH, depth->property);
    EXPECT_EQ(PRODUCT_WIDTH, width->property);
    EXPECT_EQ(PRODUCT_HEIGHT, height->property);
    EXPECT_EQ(MANUFACTURER_NVIDIA, model->invoke(pdiMethod(manufacturer)));
    // A baseboard-class chassis has no SKU of its own
    EXPECT_EQ(std::string(propertyNotSupported), sku->invoke(pdiMethod(sku)));

    EXPECT_EQ(0, fpga->prioritySensors.size());
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmWriteProtectedJumper>(
                           fpga->deviceSensors.back()));
}

TEST_F(NsmChassisTest, goodTestCreateDynamicSensors)
{
    const size_t before = gpu->deviceSensors.size();
    const size_t staticBefore = gpu->staticSensors.size();
    const size_t roundRobinBefore = gpu->roundRobinSensors.size();

    auto rc = createAttributes(gpu, {{"PowerLimitSupported", true}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // SKU and health are static, min and max power limit are polled
    EXPECT_EQ(staticBefore + 2, gpu->staticSensors.size());
    EXPECT_EQ(roundRobinBefore + 2, gpu->roundRobinSensors.size());
    EXPECT_EQ(before + 4, gpu->deviceSensors.size());
    auto powerLimits =
        find<NsmInventoryProperty<PowerLimitIntf>>(gpu->deviceSensors);
    ASSERT_EQ(2, powerLimits.size());
    EXPECT_EQ(MINIMUM_DEVICE_POWER_LIMIT, powerLimits[0]->property);
    EXPECT_EQ(MAXIMUM_DEVICE_POWER_LIMIT, powerLimits[1]->property);
}

TEST_F(NsmChassisTest, goodTestPowerLimitWithPriority)
{
    const size_t priorityBefore = gpu->prioritySensors.size();

    auto rc = createAttributes(
        gpu, {{"PowerLimitSupported", true}, {"Priority", true}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(priorityBefore + 2, gpu->prioritySensors.size());
}

TEST_F(NsmChassisTest, goodTestChassisAttributesWithoutOptionalFlags)
{
    const size_t before = gpu->deviceSensors.size();
    const size_t staticBefore = gpu->staticSensors.size();

    auto rc = createAttributes(gpu, {{"AssetInformationAvailable", false},
                                     {"DimensionSupported", false},
                                     {"PowerLimitSupported", false},
                                     {"WriteProtectSupported", false},
                                     {"ResetMetricsSupported", false},
                                     {"GPIOStateSupported", false},
                                     {"ErrorInjectionSupported", false},
                                     {"DeviceDiagnosticsSupported", false},
                                     {"SoCPowerSmoothingSupported", false}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());

    // Only the SKU and health that are always created
    EXPECT_EQ(staticBefore + 2, gpu->staticSensors.size());
    EXPECT_EQ(before + 2, gpu->deviceSensors.size());
    EXPECT_EQ(
        1,
        find<NsmInterfaceProvider<NsmApSkuIdIntf>>(gpu->staticSensors).size());
    EXPECT_EQ(
        1, find<NsmInterfaceProvider<HealthIntf>>(gpu->staticSensors).size());
}

TEST_F(NsmChassisTest, badTestWriteProtectOnNonBaseboard)
{
    EXPECT_THROW_COROUTINE(
        createAttributes(gpu, {{"WriteProtectSupported", true}}),
        std::runtime_error);
}

TEST_F(NsmChassisTest, badTestWriteProtectDeviceTypeFromDeviceList)
{
    // The write protect check uses the device list's device type, not the
    // NsmDevice's
    EXPECT_THROW_COROUTINE(createAttributes(fpga,
                                            {{"WriteProtectSupported", true}},
                                            NSM_DEV_ID_GPU),
                           std::runtime_error);
}

TEST_F(NsmChassisTest, badTestCreateAttributesNoName)
{
    nsm::features::DeviceContext dev;
    dev.name = name;
    dev.device = gpu;
    EXPECT_THROW_COROUTINE(
        nsm::features::createChassisAttributesFeature(mockManager, dev, {}),
        std::out_of_range);
}

TEST_F(NsmChassisTest, goodTestCreatePrettyNameWithValue)
{
    auto rc = createAttributes(gpu,
                               {{"PrettyNameForChassis", "GPU Module 1"s}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    auto items = find<NsmInterfaceProvider<ItemIntf>>(gpu->staticSensors);
    ASSERT_EQ(1, items.size());
    EXPECT_EQ("GPU Module 1", items[0]->invoke(pdiMethod(prettyName)));
}

TEST_F(NsmChassisTest, goodTestCreatePrettyNameEmpty)
{
    auto rc = createAttributes(gpu, {{"PrettyNameForChassis", ""s}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    auto items = find<NsmInterfaceProvider<ItemIntf>>(gpu->staticSensors);
    ASSERT_EQ(1, items.size());
    EXPECT_EQ("", items[0]->invoke(pdiMethod(prettyName)));
}

TEST_F(NsmChassisTest, goodTestCreateLocationContextWithValue)
{
    auto rc = createAttributes(gpu,
                               {{"LocationContext", "Rear Panel Slot 3"s}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    auto contexts =
        find<NsmInterfaceProvider<LocationContextIntf>>(gpu->staticSensors);
    ASSERT_EQ(1, contexts.size());
    EXPECT_EQ("Rear Panel Slot 3",
              contexts[0]->invoke(pdiMethod(locationContext)));
}

TEST_F(NsmChassisTest, goodTestCreateFieldReplaceableTrue)
{
    auto rc = createAttributes(gpu, {{"FieldReplaceable", true}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    auto replaceable =
        find<NsmInterfaceProvider<ReplaceableIntf>>(gpu->staticSensors);
    ASSERT_EQ(1, replaceable.size());
    EXPECT_TRUE(replaceable[0]->invoke(pdiMethod(fieldReplaceable)));
}

TEST_F(NsmChassisTest, goodTestCreateFieldReplaceableFalse)
{
    auto rc = createAttributes(gpu, {{"FieldReplaceable", false}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    auto replaceable =
        find<NsmInterfaceProvider<ReplaceableIntf>>(gpu->staticSensors);
    ASSERT_EQ(1, replaceable.size());
    EXPECT_FALSE(replaceable[0]->invoke(pdiMethod(fieldReplaceable)));
}

TEST_F(NsmChassisTest, goodTestEmptyLocationCodeLeftOut)
{
    // EM publishes "" for a LocationCode variable the topology does not set
    auto rc = createAttributes(gpu, {{"LocationCode", ""s}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());

    EXPECT_TRUE(find<NsmInterfaceProvider<LocationCodeIntf>>(gpu->staticSensors)
                    .empty());
}

TEST_F(NsmChassisTest, badTestChassisTypeWithEmptyString)
{
    EXPECT_THROW_COROUTINE(createAttributes(gpu, {{"ChassisType", ""s}}),
                           sdbusplus::exception::InvalidEnumString);
}

TEST_F(NsmChassisTest, badTestLocationWithEmptyString)
{
    EXPECT_THROW_COROUTINE(createAttributes(gpu, {{"LocationType", ""s}}),
                           sdbusplus::exception::InvalidEnumString);
}

TEST_F(NsmChassisTest, goodTestAssetWithCustomManufacturer)
{
    auto rc = createAttributes(gpu, {{"AssetInformationAvailable", true},
                                     {"Manufacturer", "CustomManufacturer"s}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    auto assetProperties =
        find<NsmInventoryProperty<NsmAssetIntf>>(gpu->staticSensors);
    ASSERT_EQ(3, assetProperties.size());
    for (const auto& assetProperty : assetProperties)
    {
        EXPECT_EQ("CustomManufacturer",
                  assetProperty->invoke(pdiMethod(manufacturer)));
    }
}

TEST_F(NsmChassisTest, goodTestAssetNotSupportedOnHpmSma)
{
    // combined = (NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA=3 << 8) |
    // NSM_DEV_ID_MCTP_BRIDGE=5 = 773
    const uuid_t hpmSmaUuid = "STATIC:773:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto hpmSma = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(hpmSmaUuid));
    ASSERT_NE(nullptr, hpmSma);
    ASSERT_EQ(NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA, hpmSma->getDeviceRole());

    auto rc = createAttributes(hpmSma, {{"AssetInformationAvailable", true}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // No asset property is read over NSM: the asset object itself is added,
    // with the properties tombstoned
    EXPECT_TRUE(find<NsmInventoryProperty<NsmAssetIntf>>(hpmSma->staticSensors)
                    .empty());
    auto assets = find<NsmChassis<NsmAssetIntf>>(hpmSma->staticSensors);
    ASSERT_EQ(1, assets.size());
    EXPECT_EQ(std::string(propertyNotSupported),
              assets[0]->invoke(pdiMethod(partNumber)));
    EXPECT_EQ(std::string(propertyNotSupported),
              assets[0]->invoke(pdiMethod(serialNumber)));
    EXPECT_EQ(MANUFACTURER_NVIDIA, assets[0]->invoke(pdiMethod(manufacturer)));
}

TEST_F(NsmChassisTest, goodTestCreateResetMetricsSupported)
{
    const size_t before = fpga->deviceSensors.size();

    auto rc = createAttributes(fpga, {{"ResetMetricsSupported", true}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_GT(fpga->deviceSensors.size(), before);
    EXPECT_EQ(1, find<ResetStatisticsAggregator>(fpga->deviceSensors).size());
}

TEST_F(NsmChassisTest, goodTestCreateGPIOStateSupported)
{
    const size_t before = fpga->deviceSensors.size();

    auto rc = createAttributes(fpga, {{"GPIOStateSupported", true}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_GT(fpga->deviceSensors.size(), before);
    EXPECT_FALSE(find<NsmGPIOState>(fpga->deviceSensors).empty());
}

#if defined(ENABLE_DEBUG_INFO)
TEST_F(NsmChassisTest, goodTestCreateDeviceDiagnosticsSupported)
{
    auto rc = createAttributes(fpga, {{"DeviceDiagnosticsSupported", true}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    auto debugInfo = find<NsmDebugInfoObject>(fpga->staticSensors);
    ASSERT_EQ(1, debugInfo.size());
    EXPECT_EQ("NSM_DeviceDiagnostics", debugInfo[0]->getType());
}
#endif

TEST_F(NsmChassisTest, goodTestCreateMCUErrorInjectionForMctpBridge)
{
    const uuid_t mctpBridgeUuid = "STATIC:5:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto mctpBridge = dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(mctpBridgeUuid));
    ASSERT_NE(nullptr, mctpBridge);
    mctpBridge->deviceType = NSM_DEV_ID_MCTP_BRIDGE;
    const size_t before = mctpBridge->deviceSensors.size();

    auto rc = createAttributes(mctpBridge, {{"ErrorInjectionSupported", true}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // More than the SKU and health: the MCU error injection sensors
    EXPECT_GT(mctpBridge->deviceSensors.size(), before + 2);
}

TEST_F(NsmChassisTest,
       MCUErrorInjection_HPMSMARole_CoversLeakAndUSBBridgeBranches)
{
    const uuid_t hpmSmaUuid = "STATIC:773:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto hpmSma = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(hpmSmaUuid));
    ASSERT_NE(nullptr, hpmSma);
    EXPECT_EQ(NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA, hpmSma->getDeviceRole());
    const size_t before = hpmSma->deviceSensors.size();

    auto rc = createAttributes(hpmSma, {{"ErrorInjectionSupported", true}},
                               NSM_DEV_ID_MCTP_BRIDGE);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Sensors for LeakDetectionErrors + USBBridgeEmulationErrors are added
    EXPECT_GT(hpmSma->deviceSensors.size(), before + 2);
}

/** @brief Chassis attributes of an MCTP bridge device of the given role with
 *  SoCPowerSmoothingSupported set; the sensors added by the feature are the
 *  difference of the round-robin queue
 */
#define SOC_SMOOTHING_ATTRIBUTES(deviceUuid, expectedRole, supported)          \
    auto bridge = std::dynamic_pointer_cast<MockNsmDevice>(                    \
        mockManager.getNsmDeviceFromStaticUUID(deviceUuid));                   \
    ASSERT_NE(nullptr, bridge);                                                \
    ASSERT_EQ(expectedRole, bridge->getDeviceRole());                          \
    const size_t roundRobinBefore = bridge->roundRobinSensors.size();          \
    auto rc = createAttributes(bridge,                                         \
                               {{"SoCPowerSmoothingSupported", supported}},    \
                               NSM_DEV_ID_MCTP_BRIDGE);                        \
    EXPECT_EQ(rc.data(), NSM_SUCCESS);                                         \
    EXPECT_FALSE(rc.exception())

TEST_F(NsmChassisTest, goodTestSoCPowerSmoothingForHpmSma)
{
    SOC_SMOOTHING_ATTRIBUTES("STATIC:773:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                             NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA, true);

    EXPECT_GT(bridge->roundRobinSensors.size(), roundRobinBefore);
}

TEST_F(NsmChassisTest, goodTestSoCPowerSmoothingForSxmSma)
{
    SOC_SMOOTHING_ATTRIBUTES("STATIC:261:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                             NSM_MCTP_BRIDGE_DEV_ROLE_SXM_SMA, true);

    EXPECT_GT(bridge->roundRobinSensors.size(), roundRobinBefore);
}

TEST_F(NsmChassisTest, goodTestSoCPowerSmoothingNotCreatedForOtherBridgeRole)
{
    // Role 0 (no SMA role): the flag is set but the device is not an SMA
    SOC_SMOOTHING_ATTRIBUTES("STATIC:5:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                             NSM_DEV_ROLE_RESERVED, true);

    EXPECT_EQ(bridge->roundRobinSensors.size(), roundRobinBefore);
}

TEST_F(NsmChassisTest, goodTestSoCPowerSmoothingNotCreatedWhenFlagFalse)
{
    SOC_SMOOTHING_ATTRIBUTES("STATIC:773:0:NSM_DEVICE_INSTANCE_NUMBER:0",
                             NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA, false);

    EXPECT_EQ(bridge->roundRobinSensors.size(), roundRobinBefore);
}

TEST_F(NsmChassisTest, goodTestSoCPowerSmoothingNotCreatedForGpuDevice)
{
    const size_t roundRobinBefore = gpu->roundRobinSensors.size();

    auto rc = createAttributes(gpu, {{"SoCPowerSmoothingSupported", true}});

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->roundRobinSensors.size(), roundRobinBefore);
}

TEST_F(NsmChassisTest, badTestErrorInjectionNotCreatedForGpuDevice)
{
    const size_t staticBefore = gpu->staticSensors.size();
    const size_t before = gpu->deviceSensors.size();

    auto rc = createAttributes(gpu, {{"ErrorInjectionSupported", true}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Only the SKU and health: a GPU has no MCU error injection
    EXPECT_EQ(staticBefore + 2, gpu->staticSensors.size());
    EXPECT_EQ(before + 2, gpu->deviceSensors.size());
}

struct NsmInventoryPropertyTest : public NsmChassisTest
{
    NsmChassis<NsmAssetIntf> chassisAsset{name};
    NsmChassis<DimensionIntf> chassisDimension{name};
    NsmChassis<PowerLimitIntf> chassisPowerLimit{name};
    void SetUp() override
    {
        EXPECT_EQ(chassisAsset.getName(), name);
        EXPECT_EQ(chassisAsset.getType(), "NSM_Chassis");
        EXPECT_EQ(chassisDimension.getName(), name);
        EXPECT_EQ(chassisDimension.getType(), "NSM_Chassis");
        EXPECT_EQ(chassisPowerLimit.getName(), name);
        EXPECT_EQ(chassisPowerLimit.getType(), "NSM_Chassis");
    }

    std::shared_ptr<NsmInventoryPropertyBase> sensor;
    void testRequest()
    {
        auto request = sensor->genRequestMsg(eid, instanceId);
        EXPECT_TRUE(request.has_value());
        EXPECT_EQ(request.value().size(),
                  sizeof(nsm_msg_hdr) +
                      sizeof(nsm_get_inventory_information_req));
        auto requestPtr =
            reinterpret_cast<struct nsm_msg*>(request.value().data());
        uint8_t decodedProperty = 0;
        auto rc = decode_get_inventory_information_req(
            requestPtr, request.value().size(), &decodedProperty);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        EXPECT_EQ(decodedProperty, (uint8_t)sensor->property);
    }
    void testResponse(uint8_t* data, size_t valueSize)
    {
        std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                      NSM_RESPONSE_CONVENTION_LEN + valueSize);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_get_inventory_information_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, valueSize, data, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmInventoryPropertyTest, goodTestPartNumberRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestPartNumberResponse)
{
    std::string partNumber = "PN12345";
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    testResponse((uint8_t*)partNumber.c_str(), partNumber.size());
    EXPECT_EQ(chassisAsset.invoke(pdiMethod(partNumber)), partNumber);
}
TEST_F(NsmInventoryPropertyTest, goodTestSerialNumberRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, SERIAL_NUMBER);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestSerialNumberResponse)
{
    std::string serialNumber = "SN12345";
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, SERIAL_NUMBER);
    testResponse((uint8_t*)serialNumber.c_str(), serialNumber.size());
    EXPECT_EQ(chassisAsset.invoke(pdiMethod(serialNumber)), serialNumber);
}
TEST_F(NsmInventoryPropertyTest, goodTestModelRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, MARKETING_NAME);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestModelResponse)
{
    std::string model = "NV123";
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, MARKETING_NAME);
    testResponse((uint8_t*)model.c_str(), model.size());
    EXPECT_EQ(chassisAsset.invoke(pdiMethod(model)), model);
}
TEST_F(NsmInventoryPropertyTest, goodTestDepthRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_LENGTH);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestDepthResponse)
{
    uint32_t depth = 850;
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_LENGTH);
    testResponse((uint8_t*)&depth, sizeof(depth));
    EXPECT_EQ(chassisDimension.invoke(pdiMethod(depth)), (double)depth);
}
TEST_F(NsmInventoryPropertyTest, goodTestHeightRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_HEIGHT);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestHeightResponse)
{
    uint32_t height = 2100;
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_HEIGHT);
    testResponse((uint8_t*)&height, sizeof(height));
    EXPECT_EQ(chassisDimension.invoke(pdiMethod(height)), (double)height);
}
TEST_F(NsmInventoryPropertyTest, goodTestWidthRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_WIDTH);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestWidthResponse)
{
    uint32_t width = 712;
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_WIDTH);
    testResponse((uint8_t*)&width, sizeof(width));
    EXPECT_EQ(chassisDimension.invoke(pdiMethod(width)), (double)width);
}
TEST_F(NsmInventoryPropertyTest, goodTestMinPowerWattsRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit, MINIMUM_DEVICE_POWER_LIMIT);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestMinPowerWattsResponse)
{
    uint32_t minPowerMilliWatts = 20000;
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit, MINIMUM_DEVICE_POWER_LIMIT);
    testResponse((uint8_t*)&minPowerMilliWatts, sizeof(minPowerMilliWatts));
    EXPECT_EQ(chassisPowerLimit.invoke(pdiMethod(minPowerWatts)),
              (size_t)minPowerMilliWatts / 1000);
}
TEST_F(NsmInventoryPropertyTest, goodTestMaxPowerWattsRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit, MAXIMUM_DEVICE_POWER_LIMIT);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestMaxPowerWattsResponse)
{
    uint32_t maxPowerMilliWatts = 100000;
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit, MAXIMUM_DEVICE_POWER_LIMIT);
    testResponse((uint8_t*)&maxPowerMilliWatts, sizeof(maxPowerMilliWatts));
    EXPECT_EQ(chassisPowerLimit.invoke(pdiMethod(maxPowerWatts)),
              (size_t)maxPowerMilliWatts / 1000);
}
TEST_F(NsmInventoryPropertyTest, badTestRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmInventoryPropertyTest, badTestResponseSize)
{
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) - 1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, 0, nullptr, responseMsg);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(NsmInventoryPropertyTest, badTestCompletionErrorResponse)
{
    uint8_t value = 0;
    sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  NSM_RESPONSE_CONVENTION_LEN + sizeof(value));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, sizeof(value), &value, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    struct nsm_get_inventory_information_resp* resp =
        (struct nsm_get_inventory_information_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto cc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_ERROR);
}
TEST_F(NsmInventoryPropertyTest, badTestNotImplementedResponse)
{
    try
    {
        uint8_t value = 0;
        sensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
            chassisAsset, MEMORY_VENDOR);
        testResponse(&value, sizeof(value));
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& err)
    {
        EXPECT_EQ(err.what(), std::string("Not implemented PDI"));
    }
    catch (...)
    {
        FAIL() << "Expected std::runtime_error";
    }
}

struct NsmPowerSupplyStatusTest : public NsmChassisTest
{
    NsmInterfaceProvider<PowerStateIntf> chassisPowerState{
        name, "NSM_PowerState", "/xyz/openbmc_project/dummy"};

    void SetUp() override
    {
        EXPECT_EQ(chassisPowerState.getName(), name);
        EXPECT_EQ(chassisPowerState.getType(), "NSM_PowerState");
    }
    std::shared_ptr<NsmPowerSupplyStatus> sensor;
    void init(uint8_t gpuInstanceId)
    {
        eid = 12;
        sensor = std::make_shared<NsmPowerSupplyStatus>(chassisPowerState,
                                                        gpuInstanceId);
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->getType(), "NSM_PowerState");
        EXPECT_EQ(sensor->gpuInstanceId, gpuInstanceId);
    }
    void testResponse(uint8_t status)
    {
        std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_get_power_supply_status_resp));
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_get_power_supply_status_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, status, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmPowerSupplyStatusTest, goodTestRequest)
{
    init(0);
    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    fpga_diagnostics_settings_data_index data_index;
    auto rc = decode_get_fpga_diagnostics_settings_req(
        requestPtr, request.value().size(), &data_index);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(data_index, GET_POWER_SUPPLY_STATUS);
}
TEST_F(NsmPowerSupplyStatusTest, goodTestResponse)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        using PowerState =
            sdbusplus::xyz::openbmc_project::State::server::Chassis::PowerState;
        for (auto state : {PowerState::Off, PowerState::On})
        {
            uint8_t status = (state == PowerState::On) << i;
            testResponse(status);
            EXPECT_EQ(chassisPowerState.invoke(pdiMethod(currentPowerState)),
                      state);
        }
    }
}
TEST_F(NsmPowerSupplyStatusTest, badTestRequest)
{
    init(0);
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmPowerSupplyStatusTest, badTestResponseSize)
{
    init(0);
    uint8_t status = 0;
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_power_supply_status_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_power_supply_status_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, status, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor->handleResponseMsg(responseMsg, response.size() - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmPowerSupplyStatusTest, badTestCompletionErrorResponse)
{
    init(0);
    uint8_t status = 0;
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_power_supply_status_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_power_supply_status_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, status, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    struct nsm_get_power_supply_status_resp* resp =
        (struct nsm_get_power_supply_status_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto cc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_ERROR);
}

struct NsmGpuPresenceAndPowerStatusTest : public NsmChassisTest
{
    std::shared_ptr<NsmGpuPresenceAndPowerStatus> sensor,
        chassisOperationalStatus;

    /** @brief Add the operational status sensor of GPU `gpuInstance` to the
     *  baseboard as the chassis code does: one priority sensor per name and
     *  type, the other GPUs merged into it as sub sensors
     */
    void addOperationalStatus(uint8_t gpuInstance)
    {
        const std::string gpuName = "HGX_GPU_SXM_" +
                                    std::to_string(gpuInstance + 1);
        dbus::Interfaces inventoryObjPaths{
            chassisInventoryBasePath / gpuName,
            processorsInventoryBasePath /
                ("GPU_SXM_" + std::to_string(gpuInstance + 1))};
        auto operationalStatus = NsmInterfaceProvider<OperationalStatusIntf>(
            gpuName, "NSM_OperationalStatus", inventoryObjPaths);
        fpga->addSensor(std::make_shared<NsmGpuPresenceAndPowerStatus>(
                            operationalStatus, gpuInstance),
                        true);
    }

    void SetUp() override
    {
        addOperationalStatus(0);

        EXPECT_EQ(2, fpga->deviceSensors.size()); // +1 for msgTypes sensor
        EXPECT_EQ(1, fpga->prioritySensors.size());
        chassisOperationalStatus =
            std::dynamic_pointer_cast<NsmGpuPresenceAndPowerStatus>(
                fpga->deviceSensors[1]); // Skip msgTypes sensor at index 0
        ASSERT_NE(chassisOperationalStatus, nullptr);
        EXPECT_EQ(0, chassisOperationalStatus->sensors.size());

        for (uint8_t i = 1; i < 8; i++)
        {
            addOperationalStatus(i);
        }
        // Sensors shall be added as sub sensor
        EXPECT_EQ(2, fpga->deviceSensors.size());
        EXPECT_EQ(1, fpga->prioritySensors.size());
        EXPECT_EQ(7, chassisOperationalStatus->sensors.size());
    }
    void init(uint8_t gpuInstanceId)
    {
        if (gpuInstanceId == 0)
        {
            sensor = chassisOperationalStatus;
        }
        else
        {
            sensor = std::dynamic_pointer_cast<NsmGpuPresenceAndPowerStatus>(
                chassisOperationalStatus->sensors[gpuInstanceId - 1]);
        }
    }

    const Response diagHeader{
        0x10,
        0xDE,                              // PCI VID: NVIDIA 0x10DE
        0x00,                              // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                              // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_DEVICE_CONFIGURATION,     // NVIDIA_MSG_TYPE
        NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
        0,                                 // completion code
        0,
        0,
        1,
        0 // data size
    };
    void testResponse(uint8_t presence, uint8_t power_status)
    {
        const Response presenceMsg{presence};
        const Response powerStatusMsg{power_status};

        EXPECT_CALL(*fpga, sensorIO)
            .Times(2)
            .WillOnce(mockSensorIO(diagHeader, presenceMsg))
            .WillOnce(mockSensorIO(diagHeader, powerStatusMsg));
        sensor->update(fpga);
    }
};

TEST_F(NsmGpuPresenceAndPowerStatusTest, goodTestRequest)
{
    init(0);
    sensor->state = NsmGpuPresenceAndPowerStatus::State::GetPresence;

    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    fpga_diagnostics_settings_data_index data_index;
    auto rc = decode_get_fpga_diagnostics_settings_req(
        requestPtr, request.value().size(), &data_index);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(data_index, GET_GPU_PRESENCE);
    sensor->state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;

    request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_fpga_diagnostics_settings_req));
    requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    rc = decode_get_fpga_diagnostics_settings_req(
        requestPtr, request.value().size(), &data_index);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(data_index, GET_GPU_POWER_STATUS);
}

TEST_F(NsmGpuPresenceAndPowerStatusTest, goodTestGpuStatusEnabledResponse)
{
    // "State": "Enabled" if presence=active, power=active
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
            server::OperationalStatus::StateType;
        testResponse(0x01 << i, 0x01 << i);
        EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Enabled);
    }
}

TEST_F(NsmGpuPresenceAndPowerStatusTest,
       goodTestGpuStatusUnavailableOfflineResponse)
{
    //"State": "UnavailableOffline" if presence=active,
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
            server::OperationalStatus::StateType;
        testResponse(0x01 << i, 0x00);
        EXPECT_EQ(sensor->invoke(pdiMethod(state)),
                  StateType::UnavailableOffline);
    }
}

TEST_F(NsmGpuPresenceAndPowerStatusTest, goodTestGpuStatusFaultResponse)
{
    init(0);
    using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
        server::OperationalStatus::StateType;
    const Response presenceMsg{0};
    const Response powerStatusMsg{0};
    // For error responses, we need at least 4 bytes to avoid buffer
    // overflow
    const Response errorResponse{0, 0, 0,
                                 0}; // 4 bytes minimum for error response
    auto ccError = diagHeader;
    ccError[6] = NSM_ERROR;

    EXPECT_CALL(*fpga, sensorIO)
        .Times(2)
        .WillOnce(mockSensorIO(diagHeader, presenceMsg))
        .WillOnce(mockSensorIO(diagHeader, powerStatusMsg, NSM_ERROR));
    sensor->update(fpga);
    EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Fault);

    sensor->invoke(pdiMethod(state), StateType::None);
    EXPECT_CALL(*fpga, sensorIO)
        .WillOnce(mockSensorIO(diagHeader, presenceMsg, NSM_ERROR));
    sensor->update(fpga);
    EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Fault);

    sensor->invoke(pdiMethod(state), StateType::None);
    EXPECT_CALL(*fpga, sensorIO).WillOnce(mockSensorIO(ccError, presenceMsg));
    sensor->update(fpga);
    EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Fault);

    sensor->invoke(pdiMethod(state), StateType::None);
    EXPECT_CALL(*fpga, sensorIO)
        .Times(2)
        .WillOnce(mockSensorIO(diagHeader, presenceMsg))
        .WillOnce(mockSensorIO(ccError, powerStatusMsg));
    sensor->update(fpga);
    EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Fault);
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, goodTestGpuStatusAbsentResponse)
{
    // power=inactive "State": "Absent" if presence=inactive
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
            server::OperationalStatus::StateType;
        testResponse(0, 0);
        EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Absent);
    }
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, badTestRequest)
{
    init(0);
    sensor->state = NsmGpuPresenceAndPowerStatus::State::GetPresence;
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
    sensor->state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmGpuPresenceAndPowerStatusTest, badTestResponseSize)
{
    init(0);
    sensor->state = NsmGpuPresenceAndPowerStatus::State::GetPresence;
    uint8_t presence = 0;
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_gpu_presence_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_gpu_presence_resp(instanceId, NSM_SUCCESS, ERR_NULL,
                                           presence, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor->handleResponseMsg(responseMsg, response.size() - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, badTestCompletionErrorResponse)
{
    init(0);
    sensor->state = NsmGpuPresenceAndPowerStatus::State::GetPowerStatus;
    uint8_t power_status = 0;
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_gpu_power_status_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_gpu_power_status_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, power_status, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = reinterpret_cast<struct nsm_get_gpu_power_status_resp*>(
        responseMsg->payload);
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// Test default case in genRequestMsg() when state is invalid enum value
TEST_F(NsmGpuPresenceAndPowerStatusTest,
       GenRequestMsg_DefaultState_ReturnsNullopt)
{
    init(0);
    // Set state to invalid value -> hits default: case -> rc stays NSM_SW_ERROR
    // -> if (rc) -> returns nullopt
    sensor->state = static_cast<NsmGpuPresenceAndPowerStatus::State>(99);
    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_FALSE(request.has_value());
}

// Test default case in handleResponse() when state is invalid enum value
TEST_F(NsmGpuPresenceAndPowerStatusTest,
       HandleResponse_DefaultState_ReturnsFaultState)
{
    init(0);
    using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
        server::OperationalStatus::StateType;

    // Set an invalid state so handleResponse hits the default case
    // rc stays NSM_SW_ERROR -> condition (cc != NSM_SUCCESS || rc !=
    // NSM_SW_SUCCESS) is true -> sets Fault state
    sensor->state = static_cast<NsmGpuPresenceAndPowerStatus::State>(99);

    // Build a valid presence response message to avoid decode failure
    uint8_t presence = 0;
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_gpu_presence_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_gpu_presence_resp(instanceId, NSM_SUCCESS, ERR_NULL,
                                           presence, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(result, NSM_SUCCESS);
    EXPECT_EQ(sensor->invoke(pdiMethod(state)), StateType::Fault);
}

struct NsmWriteProtectedJumperTest : public NsmChassisTest
{
    std::shared_ptr<NsmWriteProtectedJumper> sensor;

    void SetUp() override
    {
        auto rc = createAttributes(fpga, {{"WriteProtectSupported", true}});
        EXPECT_EQ(rc.data(), NSM_SUCCESS);

        EXPECT_GT(fpga->deviceSensors.size(), 0);
        sensor = std::dynamic_pointer_cast<NsmWriteProtectedJumper>(
            fpga->deviceSensors.back());
        EXPECT_NE(sensor, nullptr);
    }

    void TearDown() override
    {
        sensor.reset();
        ::testing::Mock::VerifyAndClearExpectations(&mockManager);
    }

    void testResponse(uint8_t presence)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp));
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        nsm_fpga_diagnostics_settings_wp_jumper data;
        data.presence = presence;
        auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmWriteProtectedJumperTest, goodTestRequest)
{
    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    fpga_diagnostics_settings_data_index data_index;
    auto rc = decode_get_fpga_diagnostics_settings_req(
        requestPtr, request.value().size(), &data_index);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(data_index, GET_WP_JUMPER_PRESENCE);
}

TEST_F(NsmWriteProtectedJumperTest, goodTestResponsePresent)
{
    testResponse(1);
    EXPECT_EQ(sensor->invoke(pdiMethod(writeProtected)), uint8_t(1));
    EXPECT_EQ(sensor->invoke(pdiMethod(writeProtectedControl)), uint8_t(1));
}

TEST_F(NsmWriteProtectedJumperTest, goodTestResponseNotPresent)
{
    testResponse(0);
    EXPECT_EQ(sensor->invoke(pdiMethod(writeProtected)), uint8_t(0));
    EXPECT_EQ(sensor->invoke(pdiMethod(writeProtectedControl)), uint8_t(0));
}

TEST_F(NsmWriteProtectedJumperTest, badTestResponseSize)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp) - 1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmWriteProtectedJumperTest, badTestCompletionErrorResponse)
{
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_fpga_diagnostics_settings_wp_jumper data;
    data.presence = 0;
    auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto resp =
        reinterpret_cast<struct nsm_fpga_diagnostics_settings_wp_jumper_resp*>(
            responseMsg->payload);
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// ============================================================================
// Tests for NsmChassis<IntfType>::update() template instantiations
// These test the update() coroutine for various D-Bus interface types.
// For non-UuidIntf types, update() simply co_returns NSM_SUCCESS. //

// ============================================================================

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisLocationIntf)
{
    // Arrange: create NsmChassis<LocationIntf>
    auto chassisLocation = std::make_shared<NsmChassis<LocationIntf>>(name);
    ASSERT_NE(chassisLocation, nullptr);
    EXPECT_EQ(chassisLocation->getName(), name);
    EXPECT_EQ(chassisLocation->getType(), "NSM_Chassis");

    // Act: call update (non-UuidIntf path => direct co_return NSM_SUCCESS) //

    chassisLocation->update(gpu);

    // Assert: no error
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisLocationCodeIntf)
{
    auto chassisLocationCode =
        std::make_shared<NsmChassis<LocationCodeIntf>>(name);
    ASSERT_NE(chassisLocationCode, nullptr);

    chassisLocationCode->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisLocationContextIntf)
{
    auto chassisLocCtx =
        std::make_shared<NsmChassis<LocationContextIntf>>(name);
    ASSERT_NE(chassisLocCtx, nullptr);

    chassisLocCtx->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisReplaceableIntf)
{
    auto chassisReplaceable =
        std::make_shared<NsmChassis<ReplaceableIntf>>(name);
    ASSERT_NE(chassisReplaceable, nullptr);

    chassisReplaceable->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisMctpUuidIntf)
{
    auto chassisMctpUuid = std::make_shared<NsmChassis<MctpUuidIntf>>(name);
    ASSERT_NE(chassisMctpUuid, nullptr);

    chassisMctpUuid->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisAssociationDefinitionsIntf)
{
    auto chassisAssoc =
        std::make_shared<NsmChassis<AssociationDefinitionsInft>>(name);
    ASSERT_NE(chassisAssoc, nullptr);

    chassisAssoc->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisChassisIntf)
{
    auto chassis = std::make_shared<NsmChassis<ChassisIntf>>(name);
    ASSERT_NE(chassis, nullptr);

    chassis->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisHealthIntf)
{
    auto chassisHealth = std::make_shared<NsmChassis<HealthIntf>>(name);
    ASSERT_NE(chassisHealth, nullptr);

    chassisHealth->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisItemIntf)
{
    auto chassisItem = std::make_shared<NsmChassis<ItemIntf>>(name);
    ASSERT_NE(chassisItem, nullptr);

    chassisItem->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisPowerLimitIntf)
{
    auto chassisPowerLimit = std::make_shared<NsmChassis<PowerLimitIntf>>(name);
    ASSERT_NE(chassisPowerLimit, nullptr);

    chassisPowerLimit->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisDimensionIntf)
{
    auto chassisDim = std::make_shared<NsmChassis<DimensionIntf>>(name);
    ASSERT_NE(chassisDim, nullptr);

    chassisDim->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisNsmAssetIntf)
{
    auto chassisAsset = std::make_shared<NsmChassis<NsmAssetIntf>>(name);
    ASSERT_NE(chassisAsset, nullptr);

    chassisAsset->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisNsmApSkuIdIntf)
{
    auto chassisSKU = std::make_shared<NsmChassis<NsmApSkuIdIntf>>(name);
    ASSERT_NE(chassisSKU, nullptr);

    chassisSKU->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisPCIeRefClockIntf)
{
    auto chassisPCIeRefClock =
        std::make_shared<NsmChassis<PCIeRefClockIntf>>(name);
    ASSERT_NE(chassisPCIeRefClock, nullptr);
    chassisPCIeRefClock->update(gpu);
}

TEST_F(NsmChassisTest, goodTestUpdateNsmChassisSettingsIntf)
{
    auto chassisSettings = std::make_shared<NsmChassis<SettingsIntf>>(name);
    ASSERT_NE(chassisSettings, nullptr);
    chassisSettings->update(gpu);
}

// =============================================================================
// Direct tests for getErrorInjectionTypeAndSubtype
// (nsmErrorInjectionCommon.hpp) covering uncovered switch cases: MemoryErrors,
// PCIeErrors, NVLinkErrors, ThermalErrors, LeakDetectionErrors,
// USBBridgeEmulationErrors, Unknown
// =============================================================================

TEST(GetErrorInjectionTypeAndSubtype, NullFirstArg_ReturnsFalse)
{
    uint16_t subtype = 0;
    EXPECT_FALSE(nsm::getErrorInjectionTypeAndSubtype(
        ErrorInjectionCapabilityIntf::Type::MemoryErrors, nullptr, &subtype));
}

TEST(GetErrorInjectionTypeAndSubtype, NullSecondArg_ReturnsFalse)
{
    uint16_t type = 0;
    EXPECT_FALSE(nsm::getErrorInjectionTypeAndSubtype(
        ErrorInjectionCapabilityIntf::Type::MemoryErrors, &type, nullptr));
}

TEST(GetErrorInjectionTypeAndSubtype, MemoryErrors_SetsEiMemoryErrors)
{
    uint16_t type = 0xFF, subtype = 0xFF;
    EXPECT_TRUE(nsm::getErrorInjectionTypeAndSubtype(
        ErrorInjectionCapabilityIntf::Type::MemoryErrors, &type, &subtype));
    EXPECT_EQ(type, EI_MEMORY_ERRORS);
    EXPECT_EQ(subtype, 0);
}

TEST(GetErrorInjectionTypeAndSubtype, PCIeErrors_SetsEiPciErrors)
{
    uint16_t type = 0xFF, subtype = 0xFF;
    EXPECT_TRUE(nsm::getErrorInjectionTypeAndSubtype(
        ErrorInjectionCapabilityIntf::Type::PCIeErrors, &type, &subtype));
    EXPECT_EQ(type, EI_PCI_ERRORS);
    EXPECT_EQ(subtype, 0);
}

TEST(GetErrorInjectionTypeAndSubtype, NVLinkErrors_SetsEiNvlinkErrors)
{
    uint16_t type = 0xFF, subtype = 0xFF;
    EXPECT_TRUE(nsm::getErrorInjectionTypeAndSubtype(
        ErrorInjectionCapabilityIntf::Type::NVLinkErrors, &type, &subtype));
    EXPECT_EQ(type, EI_NVLINK_ERRORS);
    EXPECT_EQ(subtype, 0);
}

TEST(GetErrorInjectionTypeAndSubtype, ThermalErrors_SetsEiThermalErrors)
{
    uint16_t type = 0xFF, subtype = 0xFF;
    EXPECT_TRUE(nsm::getErrorInjectionTypeAndSubtype(
        ErrorInjectionCapabilityIntf::Type::ThermalErrors, &type, &subtype));
    EXPECT_EQ(type, EI_THERMAL_ERRORS);
    EXPECT_EQ(subtype, 0);
}

TEST(GetErrorInjectionTypeAndSubtype,
     LeakDetectionErrors_SetsEiDeviceLeakDetect)
{
    uint16_t type = 0xFF, subtype = 0xFF;
    EXPECT_TRUE(nsm::getErrorInjectionTypeAndSubtype(
        ErrorInjectionCapabilityIntf::Type::LeakDetectionErrors, &type,
        &subtype));
    EXPECT_EQ(type, EI_DEVICE_ERRORS);
    EXPECT_EQ(subtype, EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT);
}

TEST(GetErrorInjectionTypeAndSubtype,
     USBBridgeEmulationErrors_SetsEiDeviceUsbEmulation)
{
    uint16_t type = 0xFF, subtype = 0xFF;
    EXPECT_TRUE(nsm::getErrorInjectionTypeAndSubtype(
        ErrorInjectionCapabilityIntf::Type::USBBridgeEmulationErrors, &type,
        &subtype));
    EXPECT_EQ(type, EI_DEVICE_ERRORS);
    EXPECT_EQ(subtype, EI_DEVICE_ERRORS_SUBTYPE_USB_EMULATION);
}

TEST(GetErrorInjectionTypeAndSubtype, Unknown_ReturnsFalse)
{
    uint16_t type = 0xFF, subtype = 0xFF;
    EXPECT_FALSE(nsm::getErrorInjectionTypeAndSubtype(
        ErrorInjectionCapabilityIntf::Type::Unknown, &type, &subtype));
}
