#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::ElementsAre;

#include "base.h"
#include "network-ports.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmPort.hpp"

TEST(NsmPortMetrics, GoodTest)
{
    auto bus = sdbusplus::bus::new_default();
    std::string pName("dummy_port");
    uint8_t portNum = 1;
    std::string type = "DummyType";
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/dummy/dummy_device";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/dummy/dummy_device/Ports";
    std::vector<utils::Association> associations;

    nsm::NsmPortMetrics portTel(bus, pName, portNum, type, associations,
                                parentObjPath, inventoryObjPath);

    EXPECT_EQ(portTel.portName, pName);
    EXPECT_EQ(portTel.portNumber, portNum);
    EXPECT_NE(portTel.iBPortIntf, nullptr);
    EXPECT_NE(portTel.portMetricsOem2Intf, nullptr);
    EXPECT_NE(portTel.associationDefinitionsIntf, nullptr);

    std::vector<uint8_t> portData{
        0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    }; /*for counter values, 8 bytes each*/
    auto portTelData =
        reinterpret_cast<nsm_port_counter_data*>(portData.data());

    portTel.updateCounterValues(portTelData);

    EXPECT_EQ(portTel.iBPortIntf->rxPkts(), portTelData->port_rcv_pkts);
    // checking only first and last values for iBPortIntf
    EXPECT_EQ(portTel.iBPortIntf->txWait(), portTelData->xmit_wait);

    EXPECT_EQ(portTel.portMetricsOem2Intf->rxBytes(),
              portTelData->port_rcv_data);
    // checking only first and last values for portMetricsOem2Intf
    EXPECT_EQ(portTel.portMetricsOem2Intf->txBytes(),
              portTelData->port_xmit_data);
}
