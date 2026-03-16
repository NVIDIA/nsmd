#include "test/mockDBusHandler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmKeyMgmt.hpp"

class NsmFirmwareUtilsTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                       0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
};

// NsmKeyMgmt Constructor Test
TEST_F(NsmFirmwareUtilsTest, KeyMgmt_ConstructorBasic)
{
    std::string chassisName = "GPU0";
    std::string type = "GPU";
    uint16_t componentClassification = 1;
    uint16_t componentIdentifier = 2;
    uint8_t componentClassificationIndex = 0;

    // Create progress interface
    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               chassisName + "/Progress";
    auto progressIntf =
        std::make_shared<nsm::ProgressIntf>(bus, progressPath.c_str());

    nsm::NsmKeyMgmt keyMgmt(bus, chassisName, type, testUuid, progressIntf,
                            componentClassification, componentIdentifier,
                            componentClassificationIndex);

    EXPECT_EQ(keyMgmt.getName(), chassisName);
    EXPECT_EQ(keyMgmt.getType(), type);
    EXPECT_EQ(keyMgmt.componentClassification, componentClassification);
    EXPECT_EQ(keyMgmt.componentIdentifier, componentIdentifier);
    EXPECT_EQ(keyMgmt.componentClassificationIndex,
              componentClassificationIndex);
}

// NsmKeyMgmt addSlotObject Test
TEST_F(NsmFirmwareUtilsTest, KeyMgmt_AddSlotObject)
{
    std::string chassisName = "GPU0";
    std::string type = "GPU";
    uint16_t componentClassification = 1;
    uint16_t componentIdentifier = 2;
    uint8_t componentClassificationIndex = 0;

    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               chassisName + "/Progress";
    auto progressIntf =
        std::make_shared<nsm::ProgressIntf>(bus, progressPath.c_str());

    nsm::NsmKeyMgmt keyMgmt(bus, chassisName, type, testUuid, progressIntf,
                            componentClassification, componentIdentifier,
                            componentClassificationIndex);

    EXPECT_EQ(keyMgmt.fwSlotObjects.size(), 0);

    // Create and add slot objects
    std::string chassisPath = std::string(chassisInventoryBasePath);
    std::vector<utils::Association> associations;
    auto slot1 = std::make_shared<nsm::NsmFirmwareSlot>(
        bus, chassisPath, associations, 0, nsm::SlotIntf::FirmwareType::AP,
        chassisName);

    keyMgmt.addSlotObject(slot1);
    EXPECT_EQ(keyMgmt.fwSlotObjects.size(), 1);

    auto slot2 = std::make_shared<nsm::NsmFirmwareSlot>(
        bus, chassisPath, associations, 1, nsm::SlotIntf::FirmwareType::EC,
        chassisName);

    keyMgmt.addSlotObject(slot2);
    EXPECT_EQ(keyMgmt.fwSlotObjects.size(), 2);
}

// NsmKeyMgmt getPath Test
TEST_F(NsmFirmwareUtilsTest, KeyMgmt_GetPathMethod)
{
    std::string chassisName = "GPU0";
    std::string type = "GPU";

    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               chassisName + "/Progress";
    auto progressIntf =
        std::make_shared<nsm::ProgressIntf>(bus, progressPath.c_str());

    nsm::NsmKeyMgmt keyMgmt(bus, chassisName, type, testUuid, progressIntf, 1,
                            2, 0);

    std::string expectedPath = std::string(chassisInventoryBasePath) + "/" +
                               chassisName;
    EXPECT_EQ(keyMgmt.getPath(chassisName), expectedPath);
}
