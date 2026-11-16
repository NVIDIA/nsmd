/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmZone.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

using namespace nsm;

auto bus = sdbusplus::bus::new_default();

TEST(NsmZone, Constructor)
{
    std::string name = "Zone0";
    std::string type = "NSM_FabricsZone";
    std::string fabricObjPath = "/xyz/openbmc_project/inventory/system/fabric";
    std::string zoneType =
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfZones";

    NsmZone zone(bus, name, type, fabricObjPath, zoneType);

    EXPECT_EQ(zone.getName(), name);
    EXPECT_EQ(zone.getType(), type);
}

TEST(NsmZone, ConstructorWithDifferentZoneTypes)
{
    std::string name = "Zone1";
    std::string type = "NSM_FabricsZone";
    std::string fabricObjPath = "/xyz/openbmc_project/inventory/system/fabric";

    std::vector<std::string> zoneTypes = {
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfZones",
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfEndpoints"};

    for (const auto& zoneType : zoneTypes)
    {
        NsmZone zone(bus, name, type, fabricObjPath, zoneType);
        EXPECT_EQ(zone.getName(), name);
    }
}

TEST(NsmZone, ConstructorWithDifferentFabricPaths)
{
    std::string name = "Zone2";
    std::string type = "NSM_FabricsZone";
    std::string zoneType =
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfEndpoints";

    std::vector<std::string> fabricPaths = {
        "/xyz/openbmc_project/inventory/system/fabric0",
        "/xyz/openbmc_project/inventory/system/fabric1",
        "/xyz/openbmc_project/inventory/system/fabric2"};

    for (const auto& fabricPath : fabricPaths)
    {
        NsmZone zone(bus, name, type, fabricPath, zoneType);
        EXPECT_EQ(zone.getType(), type);
    }
}

TEST(NsmZone, ConstructorWithMultipleInstances)
{
    std::string type = "NSM_FabricsZone";
    std::string fabricObjPath = "/xyz/openbmc_project/inventory/system/fabric";
    std::string zoneType =
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfZones";

    for (int i = 0; i < 3; i++)
    {
        std::string name = "Zone" + std::to_string(i);
        NsmZone zone(bus, name, type, fabricObjPath, zoneType);
        EXPECT_EQ(zone.getName(), name);
        EXPECT_EQ(zone.getType(), type);
    }
}

TEST(NsmZone, ConstructorWithDifferentNames)
{
    std::string type = "NSM_FabricsZone";
    std::string fabricObjPath = "/xyz/openbmc_project/inventory/system/fabric";
    std::string zoneType =
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfEndpoints";

    std::vector<std::string> names = {"PrimaryZone", "SecondaryZone",
                                      "BackupZone"};

    for (const auto& name : names)
    {
        NsmZone zone(bus, name, type, fabricObjPath, zoneType);
        EXPECT_EQ(zone.getName(), name);
    }
}

struct NsmZoneFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";
    const std::string zoneOfZones =
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfZones";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmZoneFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmZoneFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    /** @brief Run the FabricsZone feature of a CX device on the mock device */
    requester::Coroutine createZone(const dbus::PropertyMap& params,
                                    bool withDevice = true)
    {
        nsm::features::DeviceContext dev;
        dev.name = "HGX_CX_0";
        dev.kind = "cx_io";
        dev.staticUuid = gpuUuid;
        dev.platformPrefix = "HGX_";
        if (withDevice)
        {
            dev.device = gpu;
        }
        return nsm::features::createFabricsZoneFeature(mockManager, dev,
                                                       params);
    }
};

TEST_F(NsmZoneFactoryTest, CreateFabricsZoneSuccess)
{
    const std::string name = "Zone0";
    dbus::PropertyMap params{
        {"Name", name},
        {"ZoneType", zoneOfZones},
        {"FabricName", std::string("HGX_PCIeTopology_CX_0")}};

    auto rc = createZone(params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    ASSERT_EQ(2, gpu->deviceSensors.size());
    auto zone = std::dynamic_pointer_cast<NsmZone>(gpu->deviceSensors[1]);
    ASSERT_NE(nullptr, zone);
    EXPECT_EQ(name, zone->getName());
    EXPECT_EQ("NSM_FabricsZone", zone->getType());
}

TEST_F(NsmZoneFactoryTest, CreateFabricsZoneWithZoneOfEndpoints)
{
    const std::string name = "Zone1";
    dbus::PropertyMap params{
        {"Name", name},
        {"ZoneType",
         std::string(
             "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfEndpoints")},
        {"FabricName", std::string("HGX_PCIeTopology_CX_1")}};

    auto rc = createZone(params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(2, gpu->deviceSensors.size());
    auto zone = std::dynamic_pointer_cast<NsmZone>(gpu->deviceSensors[1]);
    ASSERT_NE(nullptr, zone);
    EXPECT_EQ(name, zone->getName());
}

TEST_F(NsmZoneFactoryTest, CreateFabricsZoneNoNsmDevice)
{
    dbus::PropertyMap params{
        {"Name", std::string("ZoneNoDevice")},
        {"ZoneType", zoneOfZones},
        {"FabricName", std::string("HGX_PCIeTopology_CX_2")}};

    auto rc = createZone(params, false);

    EXPECT_EQ(rc.data(), NSM_ERROR);
    // Only the automatic first sensor of the mock device, not the zone
    ASSERT_EQ(1, gpu->deviceSensors.size());
    EXPECT_EQ(nullptr,
              std::dynamic_pointer_cast<NsmZone>(gpu->deviceSensors[0]));
}

// Name absent: the zone is created with an empty name
TEST_F(NsmZoneFactoryTest, CreateFabricsZone_MissingName_SensorCreated)
{
    dbus::PropertyMap params{
        {"ZoneType", zoneOfZones},
        {"FabricName", std::string("HGX_PCIeTopology_CX_3")}};

    const size_t before = gpu->deviceSensors.size();
    auto rc = createZone(params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before + 1);
}

// ZoneType absent: convertZoneTypeFromString("") throws in the NsmZone
// constructor and the creator does not catch it
TEST_F(NsmZoneFactoryTest, CreateFabricsZone_MissingZoneType_Throws)
{
    dbus::PropertyMap params{
        {"Name", std::string("Zone_NoType")},
        {"FabricName", std::string("HGX_PCIeTopology_CX_4")}};

    EXPECT_THROW_COROUTINE(createZone(params), std::exception);
}
