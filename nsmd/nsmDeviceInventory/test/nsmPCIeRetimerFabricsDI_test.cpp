/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "test/mockDBusHandler.hpp"

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
