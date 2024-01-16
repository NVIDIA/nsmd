#include "base.h"
#include "platform-environmental.h"

#include "mockupResponder.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;

class MockupResponderTest : public testing::Test
{
  protected:
    MockupResponderTest() :
        event(sdeventplus::Event::get_default()), mockupResponder(false, event)
    {}

    sdeventplus::Event event;
    MockupResponder::MockupResponder mockupResponder;
};

TEST_F(MockupResponderTest, getPropertyTest)
{
    std::string expectedBoardPartNumber("MCX750500B-0D00_DK");
    std::string expectedSerialNumber("SN123456789");

    uint32_t propertyIdentifier = BOARD_PART_NUMBER;

    //get first property
    auto res = mockupResponder.getProperty(propertyIdentifier);
    EXPECT_NE(res.size(), 0);

    //verify board part number property
    std::string returnedBoardPartNumber((char*)res.data(), res.size());
    EXPECT_EQ(returnedBoardPartNumber, expectedBoardPartNumber);

    //get second property
    propertyIdentifier = SERIAL_NUMBER;
    res = mockupResponder.getProperty(propertyIdentifier);
    EXPECT_NE(res.size(), 0);

    //verify serial number property
    std::string returnedSerialNumber((char*)res.data(), res.size());
    EXPECT_EQ(returnedSerialNumber, expectedSerialNumber);
}