#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::ElementsAre;

#include "base.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "PCIeRetimerFWInventory.hpp"

static auto& bus = utils::DBusHandler::getBus();
static const std::string sensorName("dummy_sensor");
static const std::string sensorType("dummy_type");
static const std::string manufacturer("dummy_manufacturer");
static const std::vector<utils::Association>
    associations({{"chassis", "all_sensors",
                   "/xyz/openbmc_project/inventory/dummy_device"}});

TEST(NsmPCIeRetimerFirmwareVersion, GoodTest)
{
    nsm::NsmPCIeRetimerFirmwareVersion FWSensor(bus, sensorName, associations,
                                                sensorType, manufacturer, 0);

    EXPECT_EQ(FWSensor.name, sensorName);
    EXPECT_EQ(FWSensor.type, sensorType);

    std::string version = "Mock";
    EXPECT_EQ(FWSensor.softwareVer_->version(), version);
}

TEST(NsmPCIeRetimerFirmwareVersion, HandleResponseMsgSuccess)
{
    nsm::NsmPCIeRetimerFirmwareVersion FWSensor(bus, sensorName, associations,
                                                sensorType, manufacturer, 0);

    std::vector<uint8_t> responseMsg{
        0x10,
        0xDE,                            // PCI VID: NVIDIA 0x10DE
        0x00,                            // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                            // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
        NSM_GET_INVENTORY_INFORMATION,   // command
        0,                               // completion code
        0,
        0,
        8,
        0, // data size
        0x01,
        0x00,
        0x1a,
        0x00,
        0x00,
        0x00,
        0x0a,
        0x00};

    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    size_t msg_len = responseMsg.size();

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint16_t data_size = 0;
    uint8_t inventory_information[8];

    auto rc = decode_get_inventory_information_resp(response, msg_len, &cc,
                                                    &reason_code, &data_size,
                                                    inventory_information);

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(8, data_size);

    std::stringstream iss;
    iss << int(inventory_information[0]) << '.'
        << int(inventory_information[2]);
    iss << '.';
    iss << int(((inventory_information[4] << 8) | inventory_information[6]));

    std::string firmwareVersion = iss.str();
    updateValue(firmwareVersion);
    EXPECT_EQ(FWSensor.softwareVer_->version(), firmwareVersion);
}
