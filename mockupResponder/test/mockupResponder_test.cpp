#include "base.h"
#include "platform-environmental.h"

#include "mockupResponder.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;

class MockupResponderTest : public testing::Test
{
  protected:
    MockupResponderTest() : event(sdeventplus::Event::get_default())

    {
        systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        mockupResponder = std::make_shared<MockupResponder::MockupResponder>(
            false, event, *objServer, 30, NSM_DEV_ID_GPU, 0);
    }

    sdeventplus::Event event;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<MockupResponder::MockupResponder> mockupResponder;
};

TEST_F(MockupResponderTest, getPropertyTest)
{
    std::string expectedBoardPartNumber("MCX750500B-0D00_DK");
    std::string expectedSerialNumber("SN123456789");

    uint32_t propertyIdentifier = BOARD_PART_NUMBER;

    // get first property
    auto res = mockupResponder->getProperty(propertyIdentifier);
    EXPECT_NE(res.size(), 0);

    // verify board part number property
    std::string returnedBoardPartNumber((char*)res.data(), res.size());
    EXPECT_EQ(returnedBoardPartNumber, expectedBoardPartNumber);

    // get second property
    propertyIdentifier = SERIAL_NUMBER;
    res = mockupResponder->getProperty(propertyIdentifier);
    EXPECT_NE(res.size(), 0);

    // verify serial number property
    std::string returnedSerialNumber((char*)res.data(), res.size());
    EXPECT_EQ(returnedSerialNumber, expectedSerialNumber);
}