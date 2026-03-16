#include "test/mockDBusHandler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmInventoryProperty.hpp"
#include "nsmPCIeLinkSpeed.hpp"

class NsmSensorsTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
};

// NsmPCIeLinkSpeedBase static method tests
TEST_F(NsmSensorsTest, PCIeLinkSpeed_GenerationMapping)
{
    // Test generation mapping: value 0 or > 6 should return Unknown
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::generation(0),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeSlot::Generations::Unknown);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::generation(7),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeSlot::Generations::Unknown);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::generation(100),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeSlot::Generations::Unknown);

    // Test valid generations (1-6 map to Gen1-Gen6)
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::generation(1),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeSlot::Generations::Gen1);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::generation(2),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeSlot::Generations::Gen2);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::generation(3),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeSlot::Generations::Gen3);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::generation(4),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeSlot::Generations::Gen4);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::generation(5),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeSlot::Generations::Gen5);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::generation(6),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeSlot::Generations::Gen6);
}

TEST_F(NsmSensorsTest, PCIeLinkSpeed_PCIeTypeMapping)
{
    // Test PCIe type mapping: value 0 or > 6 should return Unknown
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::pcieType(0),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeDevice::PCIeTypes::Unknown);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::pcieType(7),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeDevice::PCIeTypes::Unknown);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::pcieType(100),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeDevice::PCIeTypes::Unknown);

    // Test valid PCIe types (1-6 map to Gen1-Gen6)
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::pcieType(1),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeDevice::PCIeTypes::Gen1);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::pcieType(2),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeDevice::PCIeTypes::Gen2);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::pcieType(3),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeDevice::PCIeTypes::Gen3);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::pcieType(4),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeDevice::PCIeTypes::Gen4);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::pcieType(5),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeDevice::PCIeTypes::Gen5);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::pcieType(6),
              sdbusplus::xyz::openbmc_project::Inventory::Item::server::
                  PCIeDevice::PCIeTypes::Gen6);
}

TEST_F(NsmSensorsTest, PCIeLinkSpeed_LinkWidthCalculation)
{
    // Test link width calculation: 2^(value-1) for value > 0, else 0
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::linkWidth(0), 0);
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::linkWidth(1), 1);   // 2^0 = 1
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::linkWidth(2), 2);   // 2^1 = 2
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::linkWidth(3), 4);   // 2^2 = 4
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::linkWidth(4), 8);   // 2^3 = 8
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::linkWidth(5), 16);  // 2^4 = 16
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::linkWidth(6), 32);  // 2^5 = 32
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::linkWidth(7), 64);  // 2^6 = 64
    EXPECT_EQ(nsm::NsmPCIeLinkSpeedBase::linkWidth(8), 128); // 2^7 = 128
}
