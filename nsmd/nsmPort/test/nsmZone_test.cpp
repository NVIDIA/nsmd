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

#include "nsmZone.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmZones(SensorManager& manager,
                                    const std::string& interface,
                                    const std::string& objPath);

}; // namespace nsm

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
};

TEST_F(NsmZoneFactoryTest, CreateNsmZonesSuccess)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FabricsZone";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/zone0";
    const std::string name = "Zone0";
    const std::string zoneType =
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfZones";
    const std::string fabricsObjPath =
        "/xyz/openbmc_project/inventory/system/fabric";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["ZoneType"] = zoneType;
    propertyMap["FabricsObjPath"] = fabricsObjPath;
    propertyMap["UUID"] = gpuUuid;

    createNsmZones(mockManager, interface, objPath);

    EXPECT_EQ(2, gpu->deviceSensors.size());
    auto zone = std::dynamic_pointer_cast<NsmZone>(gpu->deviceSensors[1]);
    EXPECT_NE(nullptr, zone);
    EXPECT_EQ(name, zone->getName());
}

TEST_F(NsmZoneFactoryTest, CreateNsmZonesWithZoneOfEndpoints)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FabricsZone";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/zone1";
    const std::string name = "Zone1";
    const std::string zoneType =
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfEndpoints";
    const std::string fabricsObjPath =
        "/xyz/openbmc_project/inventory/system/fabric";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["ZoneType"] = zoneType;
    propertyMap["FabricsObjPath"] = fabricsObjPath;
    propertyMap["UUID"] = gpuUuid;

    createNsmZones(mockManager, interface, objPath);

    EXPECT_EQ(2, gpu->deviceSensors.size());
    auto zone = std::dynamic_pointer_cast<NsmZone>(gpu->deviceSensors[1]);
    EXPECT_NE(nullptr, zone);
    EXPECT_EQ(name, zone->getName());
}

TEST_F(NsmZoneFactoryTest, CreateNsmZonesInvalidUUID)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FabricsZone";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/zone_invalid";
    const std::string name = "ZoneInvalid";
    const std::string zoneType =
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfZones";
    const std::string fabricsObjPath =
        "/xyz/openbmc_project/inventory/system/fabric";
    const uuid_t invalidUuid = "INVALID:UUID:DOES:NOT:EXIST";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["ZoneType"] = zoneType;
    propertyMap["FabricsObjPath"] = fabricsObjPath;
    propertyMap["UUID"] = invalidUuid;

    EXPECT_THROW_COROUTINE(createNsmZones(mockManager, interface, objPath),
                           std::runtime_error);

    // Should have only the automatic first sensor, not the zone
    EXPECT_EQ(1, gpu->deviceSensors.size());
    auto zone = std::dynamic_pointer_cast<NsmZone>(gpu->deviceSensors[0]);
    EXPECT_EQ(nullptr, zone);
}
