/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmPCIeRetimerFabricsDI.hpp"

using namespace nsm;
using namespace ::testing;

static auto bus = sdbusplus::bus::new_default();

static constexpr auto fabricTypePCIe =
    "xyz.openbmc_project.Inventory.Item.Fabric.FabricType.PCIe";
static constexpr auto fabricTypeNVLink =
    "xyz.openbmc_project.Inventory.Item.Fabric.FabricType.NVLink";
static constexpr auto fabricTypeEthernet =
    "xyz.openbmc_project.Inventory.Item.Fabric.FabricType.Ethernet";

// ============================================================================
// Constructor Tests
// ============================================================================

TEST(NsmPCIeRetimerFabricsDITest, ConstructorBasicPCIe)
{
    std::string name = "PCIeRetimer0";
    std::string type = "NSM_PCIeRetimer_Fabrics";
    std::vector<utils::Association> associations;

    EXPECT_NO_THROW({
        NsmPCIeRetimerFabricDI obj(bus, name, associations, type,
                                   fabricTypePCIe);
        EXPECT_EQ(obj.getName(), name);
        EXPECT_EQ(obj.getType(), type);
    });
}

TEST(NsmPCIeRetimerFabricsDITest, ConstructorNVLink)
{
    std::string name = "NVLinkRetimer0";
    std::string type = "NSM_PCIeRetimer_Fabrics";
    std::vector<utils::Association> associations;

    EXPECT_NO_THROW({
        NsmPCIeRetimerFabricDI obj(bus, name, associations, type,
                                   fabricTypeNVLink);
        EXPECT_EQ(obj.getName(), name);
    });
}

TEST(NsmPCIeRetimerFabricsDITest, ConstructorEthernet)
{
    std::string name = "EthernetRetimer0";
    std::string type = "NSM_PCIeRetimer_Fabrics";
    std::vector<utils::Association> associations;

    EXPECT_NO_THROW({
        NsmPCIeRetimerFabricDI obj(bus, name, associations, type,
                                   fabricTypeEthernet);
        EXPECT_EQ(obj.getName(), name);
    });
}

TEST(NsmPCIeRetimerFabricsDITest, ConstructorWithAssociations)
{
    std::string name = "PCIeRetimerAssoc";
    std::string type = "NSM_PCIeRetimer_Fabrics";
    std::vector<utils::Association> associations;
    associations.push_back({"parent_chassis", "pcie_retimer",
                            "/xyz/openbmc_project/inventory/system/chassis"});

    NsmPCIeRetimerFabricDI obj(bus, name, associations, type, fabricTypePCIe);

    EXPECT_EQ(obj.getName(), name);
    EXPECT_EQ(obj.getType(), type);
    EXPECT_NE(obj.associationDefIntf, nullptr);
    EXPECT_NE(obj.uuidIntf, nullptr);
    EXPECT_NE(obj.fabricIntf, nullptr);
}

TEST(NsmPCIeRetimerFabricsDITest, ConstructorWithMultipleAssociations)
{
    std::string name = "PCIeRetimerMultiAssoc";
    std::string type = "NSM_PCIeRetimer_Fabrics";
    std::vector<utils::Association> associations;
    associations.push_back({"parent_chassis", "pcie_retimer",
                            "/xyz/openbmc_project/inventory/system"});
    associations.push_back(
        {"containing", "fabric", "/xyz/openbmc_project/inventory/adapters"});

    NsmPCIeRetimerFabricDI obj(bus, name, associations, type, fabricTypePCIe);

    EXPECT_EQ(obj.getName(), name);
    EXPECT_NE(obj.associationDefIntf, nullptr);
}

TEST(NsmPCIeRetimerFabricsDITest, ConstructorInterfacesNotNull)
{
    std::string name = "PCIeRetimerIfaces";
    std::string type = "NSM_PCIeRetimer_Fabrics";
    std::vector<utils::Association> associations;

    NsmPCIeRetimerFabricDI obj(bus, name, associations, type, fabricTypePCIe);

    EXPECT_NE(obj.associationDefIntf, nullptr);
    EXPECT_NE(obj.uuidIntf, nullptr);
    EXPECT_NE(obj.fabricIntf, nullptr);
}

TEST(NsmPCIeRetimerFabricsDITest, ConstructorEmptyAssociations)
{
    std::string name = "PCIeRetimerEmpty";
    std::string type = "NSM_PCIeRetimer_Fabrics";
    std::vector<utils::Association> emptyAssociations;

    NsmPCIeRetimerFabricDI obj(bus, name, emptyAssociations, type,
                               fabricTypePCIe);

    EXPECT_EQ(obj.getName(), name);
    EXPECT_EQ(obj.getType(), type);
}

// =============================================================================
// createNSMPCIeRetimerFabrics factory branch coverage
// (exercised via NsmObjectFactory dispatch — static linkage workaround)
// =============================================================================

#include "nsmObjectFactory.hpp"

struct NsmPCIeRetimerFabricsDIFactoryTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_Fabrics";
    // UUID that maps to a MockNsmDevice (created on demand by
    // MockMctpDiscovery)
    const uuid_t retimerUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> retimer;

    NsmPCIeRetimerFabricsDIFactoryTest() : SensorManagerTest(devices)
    {
        retimer = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(retimerUuid));
        EXPECT_NE(retimer, nullptr);
    }

    ~NsmPCIeRetimerFabricsDIFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// All three properties present → all TRUE branches → sensor created and added
TEST_F(NsmPCIeRetimerFabricsDIFactoryTest, Factory_AllProperties_SensorCreated)
{
    const std::string testPath = "/xyz/test/fabrics/all_props";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("FabricsDI_0");
    pm["UUID"] = retimerUuid;
    pm["FabricType"] = std::string(fabricTypePCIe);

    const size_t before = retimer->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, testPath);
    EXPECT_GT(retimer->deviceSensors.size(), before);
}

// Missing UUID → FALSE branch for count("UUID") → uuid="" →
// parseStaticUuid("") throws → caught by createObjects → no sensor
TEST_F(NsmPCIeRetimerFabricsDIFactoryTest, Factory_MissingUUID_NoSensor)
{
    const std::string testPath = "/xyz/test/fabrics/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("FabricsDI_NoUUID");
    pm["FabricType"] = std::string(fabricTypePCIe);
    // "UUID" intentionally omitted → uuid remains ""

    const size_t before = retimer->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, testPath);
    EXPECT_EQ(before, retimer->deviceSensors.size());
}

// Missing Name → FALSE branch for count("Name") → name="" →
// D-Bus path ends with "/" (invalid) → sdbusplus throws → no sensor
TEST_F(NsmPCIeRetimerFabricsDIFactoryTest, Factory_MissingName_NoSensor)
{
    const std::string testPath = "/xyz/test/fabrics/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["UUID"] = retimerUuid;
    pm["FabricType"] = std::string(fabricTypePCIe);
    // "Name" intentionally omitted → name=""

    const size_t before = retimer->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, testPath);
    EXPECT_EQ(before, retimer->deviceSensors.size());
}

// Missing FabricType → FALSE branch for count("FabricType") → fabricType="" →
// convertFabricTypeFromString("") throws → caught → no sensor
TEST_F(NsmPCIeRetimerFabricsDIFactoryTest, Factory_MissingFabricType_NoSensor)
{
    const std::string testPath = "/xyz/test/fabrics/no_fabrictype";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("FabricsDI_NoType");
    pm["UUID"] = retimerUuid;
    // "FabricType" intentionally omitted → fabricType=""

    const size_t before = retimer->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, testPath);
    EXPECT_EQ(before, retimer->deviceSensors.size());
}

// NVLink fabric type → TRUE branch for count("FabricType"), NVLink value →
// sensor created
TEST_F(NsmPCIeRetimerFabricsDIFactoryTest,
       Factory_NVLinkFabricType_SensorCreated)
{
    const std::string testPath = "/xyz/test/fabrics/nvlink";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("FabricsDI_NVLink");
    pm["UUID"] = retimerUuid;
    pm["FabricType"] = std::string(fabricTypeNVLink);

    const size_t before = retimer->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, testPath);
    EXPECT_GT(retimer->deviceSensors.size(), before);
}

// Ethernet fabric type → sensor created (covers third FabricType TRUE branch)
TEST_F(NsmPCIeRetimerFabricsDIFactoryTest,
       Factory_EthernetFabricType_SensorCreated)
{
    const std::string testPath = "/xyz/test/fabrics/ethernet";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("FabricsDI_Eth");
    pm["UUID"] = retimerUuid;
    pm["FabricType"] = std::string(fabricTypeEthernet);

    const size_t before = retimer->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, testPath);
    EXPECT_GT(retimer->deviceSensors.size(), before);
}
