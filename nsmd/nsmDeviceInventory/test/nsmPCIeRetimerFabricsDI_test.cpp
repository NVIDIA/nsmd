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
// createPCIeRetimerFabricsFeature coverage
// =============================================================================

#include "features/creators.hpp"

struct NsmPCIeRetimerFabricsDIFactoryTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
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

    nsm::features::DeviceContext makeDev()
    {
        nsm::features::DeviceContext dev;
        dev.name = "HGX_PCIeRetimer_0";
        dev.kind = "pcie_retimer";
        dev.staticUuid = retimerUuid;
        dev.platformPrefix = "HGX_";
        dev.device = retimer;
        return dev;
    }

    requester::Coroutine create(const dbus::PropertyMap& params)
    {
        return nsm::features::createPCIeRetimerFabricsFeature(
            mockManager, makeDev(), params);
    }
};

// All params present → sensor created and added
TEST_F(NsmPCIeRetimerFabricsDIFactoryTest, Feature_AllParams_SensorCreated)
{
    const size_t before = retimer->deviceSensors.size();
    auto rc = create({{"Name", std::string("FabricsDI_0")},
                      {"FabricType", std::string(fabricTypePCIe)}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(retimer->deviceSensors.size(), before + 1);
}

// Missing Name → name="" → D-Bus path ends with "/" (invalid) →
// sdbusplus throws → no sensor
TEST_F(NsmPCIeRetimerFabricsDIFactoryTest, Feature_MissingName_NoSensor)
{
    const size_t before = retimer->deviceSensors.size();
    EXPECT_THROW_COROUTINE(
        create({{"FabricType", std::string(fabricTypePCIe)}}), std::exception);
    EXPECT_EQ(before, retimer->deviceSensors.size());
}

// Missing FabricType → fabricType="" → convertFabricTypeFromString("")
// throws → no sensor
TEST_F(NsmPCIeRetimerFabricsDIFactoryTest, Feature_MissingFabricType_NoSensor)
{
    const size_t before = retimer->deviceSensors.size();
    EXPECT_THROW_COROUTINE(create({{"Name", std::string("FabricsDI_NoType")}}),
                           std::exception);
    EXPECT_EQ(before, retimer->deviceSensors.size());
}

// NVLink fabric type → sensor created
TEST_F(NsmPCIeRetimerFabricsDIFactoryTest,
       Feature_NVLinkFabricType_SensorCreated)
{
    const size_t before = retimer->deviceSensors.size();
    auto rc = create({{"Name", std::string("FabricsDI_NVLink")},
                      {"FabricType", std::string(fabricTypeNVLink)}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(retimer->deviceSensors.size(), before + 1);
}

// Ethernet fabric type → sensor created
TEST_F(NsmPCIeRetimerFabricsDIFactoryTest,
       Feature_EthernetFabricType_SensorCreated)
{
    const size_t before = retimer->deviceSensors.size();
    auto rc = create({{"Name", std::string("FabricsDI_Eth")},
                      {"FabricType", std::string(fabricTypeEthernet)}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(retimer->deviceSensors.size(), before + 1);
}
