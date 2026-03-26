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

// =============================================================================
// FALSE-branch coverage: count() checks in createNsmZones
// =============================================================================

// Name absent → name="" → NsmZone created with empty name → sensor IS added
TEST_F(NsmZoneFactoryTest, CreateZones_MissingName_SensorCreated)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FabricsZone";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/zone_no_name";
    const std::string fabricsObjPath =
        "/xyz/openbmc_project/inventory/test/zone/fabric_no_name";
    const std::string zoneType =
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfZones";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["ZoneType"] = zoneType;
    propertyMap["FabricsObjPath"] = fabricsObjPath;
    propertyMap["UUID"] = gpuUuid;
    // "Name" intentionally omitted → FALSE branch → name="" → sensor added

    const size_t before = gpu->deviceSensors.size();
    createNsmZones(mockManager, interface, objPath);
    // NsmZone constructed with empty name → no throw in test env → sensor added
    //
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ZoneType absent → zoneType="" → convertZoneTypeFromString("") throws in
// NsmZone constructor → propagates (no try/catch in factory)
TEST_F(NsmZoneFactoryTest, CreateZones_MissingZoneType_Throws)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FabricsZone";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/zone_no_zonetype";
    const std::string fabricsObjPath =
        "/xyz/openbmc_project/inventory/test/zone/fabric_no_zonetype";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = std::string("Zone_NoType");
    propertyMap["FabricsObjPath"] = fabricsObjPath;
    propertyMap["UUID"] = gpuUuid;
    // "ZoneType" intentionally omitted → zoneType="" →
    // convertZoneTypeFromString("") throws

    EXPECT_THROW_COROUTINE(createNsmZones(mockManager, interface, objPath),
                           std::exception);
}

// FabricsObjPath absent → fabricsObjPath="" → inventoryObjPath="/zones/0"
// (valid D-Bus path starting with "/") → sensor IS created
TEST_F(NsmZoneFactoryTest, CreateZones_MissingFabricsObjPath_SensorCreated)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FabricsZone";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/zone_no_fabrics";
    const std::string zoneType =
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfZones";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = std::string("Zone_NoFabrics");
    propertyMap["ZoneType"] = zoneType;
    propertyMap["UUID"] = gpuUuid;
    // "FabricsObjPath" intentionally omitted → fabricsObjPath="" →
    // inventoryObjPath="/zones/0" → valid path → sensor IS created

    const size_t before = gpu->deviceSensors.size();
    createNsmZones(mockManager, interface, objPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// UUID absent → uuid="" → parseStaticUuid("") throws std::runtime_error
// before NsmZone is created
TEST_F(NsmZoneFactoryTest, CreateZones_MissingUUID_Throws)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FabricsZone";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/zone_no_uuid";
    const std::string fabricsObjPath =
        "/xyz/openbmc_project/inventory/test/zone/fabric_no_uuid";
    const std::string zoneType =
        "xyz.openbmc_project.Inventory.Item.Zone.ZoneType.ZoneOfZones";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = std::string("Zone_NoUUID");
    propertyMap["ZoneType"] = zoneType;
    propertyMap["FabricsObjPath"] = fabricsObjPath;
    // "UUID" intentionally omitted → uuid="" → parseStaticUuid("") throws

    EXPECT_THROW_COROUTINE(createNsmZones(mockManager, interface, objPath),
                           std::exception);
}
