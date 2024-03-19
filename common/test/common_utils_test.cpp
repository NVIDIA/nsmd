#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::ElementsAre;

#include "utils.hpp"
#include <sdbusplus/bus.hpp>

TEST(ConvertUUIDToString, testGoodConversionToString)
{
    std::vector<uint8_t> intUUID{0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                 0x0c, 0x0d, 0x0e, 0x0f};

    // convert intUUID to stringUUID
    uuid_t stringUUID = utils::convertUUIDToString(intUUID);

    EXPECT_STREQ(stringUUID.c_str(), "00010203-0405-0607-0809-0a0b0c0d0e0f\0");
}

TEST(ConvertUUIDToString, testGBadConversionToString)
{
    std::vector<uint8_t> intUUID{0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                 0x06};

    // convert intUUID to stringUUID
    uuid_t stringUUID = utils::convertUUIDToString(intUUID);

    EXPECT_STREQ(stringUUID.c_str(), "");
}