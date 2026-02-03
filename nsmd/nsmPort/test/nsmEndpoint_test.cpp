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

#include "nsmEndpoint.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

using namespace nsm;

auto bus = sdbusplus::bus::new_default();

TEST(NsmEndpoint, Constructor)
{
    std::string name = "Endpoint0";
    std::string type = "NSM_Endpoint";
    std::string fabricObjPath = "/xyz/openbmc_project/inventory/system/fabric";

    std::vector<utils::Association> associations;
    associations.push_back({"parent_fabric", "endpoint", fabricObjPath});

    NsmEndpoint endpoint(bus, name, type, associations, fabricObjPath);

    EXPECT_EQ(endpoint.getName(), name);
    EXPECT_EQ(endpoint.getType(), type);
    // Interfaces are created successfully (private fields, tested via no-crash)
}

TEST(NsmEndpoint, ConstructorWithMultipleAssociations)
{
    std::string name = "Endpoint1";
    std::string type = "NSM_Endpoint";
    std::string fabricObjPath = "/xyz/openbmc_project/inventory/system/fabric";

    std::vector<utils::Association> associations;
    associations.push_back({"parent_fabric", "endpoint", fabricObjPath});
    associations.push_back({"connected_port", "endpoint",
                            "/xyz/openbmc_project/inventory/system/port0"});

    NsmEndpoint endpoint(bus, name, type, associations, fabricObjPath);

    EXPECT_EQ(endpoint.getName(), name);
    EXPECT_EQ(endpoint.getType(), type);
}

TEST(NsmEndpoint, ConstructorWithEmptyAssociations)
{
    std::string name = "Endpoint2";
    std::string type = "NSM_Endpoint";
    std::string fabricObjPath = "/xyz/openbmc_project/inventory/system/fabric2";

    std::vector<utils::Association> associations; // Empty

    NsmEndpoint endpoint(bus, name, type, associations, fabricObjPath);

    EXPECT_EQ(endpoint.getName(), name);
    // Endpoint interface created successfully (tested via no-crash)
}

namespace nsm
{
requester::Coroutine createNsmEndpoints(SensorManager& manager,
                                        const std::string& interface,
                                        const std::string& objPath);

}; // namespace nsm

struct NsmEndpointFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmEndpointFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmEndpointFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmEndpointFactoryTest, CreateNsmEndpointsSuccess)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FabricsEndpoint";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/endpoint0";
    const std::string name = "Endpoint0";
    const std::string fabricsObjPath =
        "/xyz/openbmc_project/inventory/system/fabric";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["FabricsObjPath"] = fabricsObjPath;
    propertyMap["UUID"] = gpuUuid;

    // No associations - don't set up any .Associations interfaces
    createNsmEndpoints(mockManager, interface, objPath);

    EXPECT_EQ(2, gpu->deviceSensors.size());
    // Skip first sensor (new sensor added automatically)
    auto endpoint =
        std::dynamic_pointer_cast<NsmEndpoint>(gpu->deviceSensors[1]);
    EXPECT_NE(nullptr, endpoint);
    EXPECT_EQ(name, endpoint->getName());
}

TEST_F(NsmEndpointFactoryTest, CreateNsmEndpointsWithAssociations)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FabricsEndpoint";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/endpoint1";
    const std::string name = "Endpoint1";
    const std::string fabricsObjPath =
        "/xyz/openbmc_project/inventory/system/fabric";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["FabricsObjPath"] = fabricsObjPath;
    propertyMap["UUID"] = gpuUuid;

    dbus::PropertyMap association = {{"Forward", "parent_fabric"},
                                     {"Backward", "endpoint"},
                                     {"AbsolutePath", fabricsObjPath}};
    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        objPath, interface + ".Associations0");
    propertyMapAssociation0 = association;

    createNsmEndpoints(mockManager, interface, objPath);

    EXPECT_EQ(2, gpu->deviceSensors.size());
    // Skip first sensor (new sensor added automatically)
    auto endpoint =
        std::dynamic_pointer_cast<NsmEndpoint>(gpu->deviceSensors[1]);
    EXPECT_NE(nullptr, endpoint);
    EXPECT_EQ(name, endpoint->getName());
}

TEST_F(NsmEndpointFactoryTest, CreateNsmEndpointsInvalidUUID)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FabricsEndpoint";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/endpoint_invalid";
    const std::string name = "EndpointInvalid";
    const std::string fabricsObjPath =
        "/xyz/openbmc_project/inventory/system/fabric";
    const uuid_t invalidUuid = "INVALID:UUID:DOES:NOT:EXIST";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["FabricsObjPath"] = fabricsObjPath;
    propertyMap["UUID"] = invalidUuid;

    // No associations - don't set up any .Associations interfaces
    EXPECT_THROW_COROUTINE(createNsmEndpoints(mockManager, interface, objPath),
                           std::runtime_error);

    // Should have only the automatic first sensor, not the endpoint
    EXPECT_EQ(1, gpu->deviceSensors.size());
    // Verify the sensor is NOT an NsmEndpoint
    auto endpoint =
        std::dynamic_pointer_cast<NsmEndpoint>(gpu->deviceSensors[0]);
    EXPECT_EQ(nullptr, endpoint);
}

TEST_F(NsmEndpointFactoryTest, CreateNsmEndpointsMultipleAssociations)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FabricsEndpoint";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/endpoint_multi";
    const std::string name = "EndpointMulti";
    const std::string fabricsObjPath =
        "/xyz/openbmc_project/inventory/system/fabric";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["FabricsObjPath"] = fabricsObjPath;
    propertyMap["UUID"] = gpuUuid;

    dbus::PropertyMap association0 = {{"Forward", "parent_fabric"},
                                      {"Backward", "endpoint"},
                                      {"AbsolutePath", fabricsObjPath}};
    dbus::PropertyMap association1 = {
        {"Forward", "connected_port"},
        {"Backward", "endpoint"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system/port0"}};
    dbus::PropertyMap association2 = {
        {"Forward", "connected_device"},
        {"Backward", "endpoint"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system/device0"}};

    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        objPath, interface + ".Associations0");
    propertyMapAssociation0 = association0;
    auto& propertyMapAssociation1 = utils::MockDbusAsync::propertyMap(
        objPath, interface + ".Associations1");
    propertyMapAssociation1 = association1;
    auto& propertyMapAssociation2 = utils::MockDbusAsync::propertyMap(
        objPath, interface + ".Associations2");
    propertyMapAssociation2 = association2;

    createNsmEndpoints(mockManager, interface, objPath);

    EXPECT_EQ(2, gpu->deviceSensors.size());
    // Skip first sensor (new sensor added automatically)
    auto endpoint =
        std::dynamic_pointer_cast<NsmEndpoint>(gpu->deviceSensors[1]);
    EXPECT_NE(nullptr, endpoint);
    EXPECT_EQ(name, endpoint->getName());
}
