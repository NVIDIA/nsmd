/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmCommon/nsmPciePortIntf.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();

class NsmPciePortIntfTest : public ::testing::Test
{};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(NsmPciePortIntfTest, ConstructorCreatesObjectSuccessfully)
{
    std::string name = "TestPciePort";
    std::string type = "PCIePort";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test";

    EXPECT_NO_THROW(
        { NsmPciePortIntf pciePort(bus, name, type, inventoryPath); });
}

TEST_F(NsmPciePortIntfTest, ConstructorSetsNameCorrectly)
{
    std::string name = "TestPciePort";
    std::string type = "PCIePort";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test";

    NsmPciePortIntf pciePort(bus, name, type, inventoryPath);

    EXPECT_EQ(pciePort.getName(), name);
}

TEST_F(NsmPciePortIntfTest, ConstructorSetsTypeCorrectly)
{
    std::string name = "TestPciePort";
    std::string type = "PCIePort";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test";

    NsmPciePortIntf pciePort(bus, name, type, inventoryPath);

    EXPECT_EQ(pciePort.getType(), type);
}

TEST_F(NsmPciePortIntfTest, ConstructorCreatesPciePortIntf)
{
    std::string name = "TestPciePort";
    std::string type = "PCIePort";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test";

    NsmPciePortIntf pciePort(bus, name, type, inventoryPath);

    // Verify pciePortIntf is created (not nullptr)
    EXPECT_NE(pciePort.pciePortIntf, nullptr);
}

TEST_F(NsmPciePortIntfTest, ConstructorWithDifferentNames)
{
    std::string type = "PCIePort";

    std::string name1 = "PCIePort_0";
    std::string name2 = "PCIePort_1";
    std::string name3 = "PCIePort_GPU";
    std::string path1 = "/xyz/openbmc_project/inventory/system/test_name0";
    std::string path2 = "/xyz/openbmc_project/inventory/system/test_name1";
    std::string path3 = "/xyz/openbmc_project/inventory/system/test_name2";

    NsmPciePortIntf port1(bus, name1, type, path1);
    NsmPciePortIntf port2(bus, name2, type, path2);
    NsmPciePortIntf port3(bus, name3, type, path3);

    EXPECT_EQ(port1.getName(), name1);
    EXPECT_EQ(port2.getName(), name2);
    EXPECT_EQ(port3.getName(), name3);
}

TEST_F(NsmPciePortIntfTest, ConstructorWithDifferentTypes)
{
    std::string name = "TestPort";

    std::string type1 = "PCIePort";
    std::string type2 = "PCIeX16";
    std::string type3 = "PCIeGen4";
    std::string path1 = "/xyz/openbmc_project/inventory/system/test_type0";
    std::string path2 = "/xyz/openbmc_project/inventory/system/test_type1";
    std::string path3 = "/xyz/openbmc_project/inventory/system/test_type2";

    NsmPciePortIntf port1(bus, name, type1, path1);
    NsmPciePortIntf port2(bus, name, type2, path2);
    NsmPciePortIntf port3(bus, name, type3, path3);

    EXPECT_EQ(port1.getType(), type1);
    EXPECT_EQ(port2.getType(), type2);
    EXPECT_EQ(port3.getType(), type3);
}

TEST_F(NsmPciePortIntfTest, ConstructorWithDifferentInventoryPaths)
{
    std::string name = "TestPort";
    std::string type = "PCIePort";

    std::string path1 = "/xyz/openbmc_project/inventory/system/chassis";
    std::string path2 = "/xyz/openbmc_project/inventory/system/board";
    std::string path3 = "/xyz/openbmc_project/inventory/system/gpu0";

    NsmPciePortIntf port1(bus, name, type, path1);
    NsmPciePortIntf port2(bus, name, type, path2);
    NsmPciePortIntf port3(bus, name, type, path3);

    // All should create successfully with different paths
    EXPECT_NE(port1.pciePortIntf, nullptr);
    EXPECT_NE(port2.pciePortIntf, nullptr);
    EXPECT_NE(port3.pciePortIntf, nullptr);
}

TEST_F(NsmPciePortIntfTest, ConstructorWithEmptyName)
{
    std::string name = "";
    std::string type = "PCIePort";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test";

    EXPECT_NO_THROW(
        { NsmPciePortIntf pciePort(bus, name, type, inventoryPath); });
}

TEST_F(NsmPciePortIntfTest, ConstructorWithEmptyType)
{
    std::string name = "TestPort";
    std::string type = "";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test";

    EXPECT_NO_THROW(
        { NsmPciePortIntf pciePort(bus, name, type, inventoryPath); });
}

TEST_F(NsmPciePortIntfTest, ConstructorWithLongStrings)
{
    std::string name = "VeryLongPCIePortNameWithLotsOfCharacters";
    std::string type = "VeryLongPCIeTypeDescriptionWithDetails";
    std::string inventoryPath =
        "/xyz/openbmc_project/inventory/system/chassis/board/component/subcomponent/test";

    EXPECT_NO_THROW(
        { NsmPciePortIntf pciePort(bus, name, type, inventoryPath); });
}

TEST_F(NsmPciePortIntfTest, ConstructorWithSpecialCharactersInName)
{
    std::string name = "PCIe-Port_0.1";
    std::string type = "PCIePort";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test";

    EXPECT_NO_THROW({
        NsmPciePortIntf pciePort(bus, name, type, inventoryPath);
        EXPECT_EQ(pciePort.getName(), name);
    });
}

// ============================================================================
// Inheritance Tests
// ============================================================================

TEST_F(NsmPciePortIntfTest, InheritsFromNsmObject)
{
    std::string name = "TestPort";
    std::string type = "PCIePort";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test";

    NsmPciePortIntf pciePort(bus, name, type, inventoryPath);

    // Test that it can be used as NsmObject
    NsmObject* nsmObj = &pciePort;
    EXPECT_NE(nsmObj, nullptr);
    EXPECT_EQ(nsmObj->getName(), name);
    EXPECT_EQ(nsmObj->getType(), type);
}

TEST_F(NsmPciePortIntfTest, MultipleInstancesIndependent)
{
    std::string name1 = "Port1";
    std::string name2 = "Port2";
    std::string type = "PCIePort";
    std::string path1 = "/xyz/openbmc_project/inventory/system/port1";
    std::string path2 = "/xyz/openbmc_project/inventory/system/port2";

    NsmPciePortIntf port1(bus, name1, type, path1);
    NsmPciePortIntf port2(bus, name2, type, path2);

    // Verify they are independent instances
    EXPECT_NE(port1.pciePortIntf, port2.pciePortIntf);
    EXPECT_EQ(port1.getName(), name1);
    EXPECT_EQ(port2.getName(), name2);
}
