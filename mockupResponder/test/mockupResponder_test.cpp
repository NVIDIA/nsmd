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

    uint32_t handle = BOARD_PART_NUMBER;
    uint32_t nextHandle = 0;
    nsm_inventory_property_record* ptr = NULL;

    //get first property
    auto res = mockupResponder.getProperty(handle, nextHandle);
    EXPECT_NE(res.size(), 0);
    ptr = (nsm_inventory_property_record*)res.data();

    //verify board part number property
    EXPECT_EQ(ptr->property_id, BOARD_PART_NUMBER);
    EXPECT_EQ(ptr->data_type, NvCharArray);
    EXPECT_EQ(ptr->data_length, expectedBoardPartNumber.length());
    std::string returnedBoardPartNumber((char*)ptr->data, ptr->data_length);
    EXPECT_EQ(returnedBoardPartNumber, expectedBoardPartNumber);
    EXPECT_EQ(nextHandle, SERIAL_NUMBER);

    //get second property
    handle = nextHandle;
    res = mockupResponder.getProperty(handle, nextHandle);
    EXPECT_NE(res.size(), 0);
    ptr = (nsm_inventory_property_record*)res.data();

    //verify serial number property
    EXPECT_EQ(ptr->property_id, SERIAL_NUMBER);
    EXPECT_EQ(ptr->data_type, NvCharArray);
    EXPECT_EQ(ptr->data_length, expectedSerialNumber.length());
    std::string returnedSerialNumber((char*)ptr->data, ptr->data_length);
    EXPECT_EQ(returnedSerialNumber, expectedSerialNumber);
    EXPECT_EQ(nextHandle, 0);
}