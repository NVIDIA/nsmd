#include "base.h"

#include "nsm_discovery_cmd.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;

using ordered_json = nlohmann::ordered_json;
using namespace nsmtool::discovery;

TEST(parseBitfieldVar, GoodTest)
{
    bitfield8_t supportedTypes[8] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

    ordered_json result;
    std::string key("Supported Nvidia Message Types");
    for (int i = 0; i < 8; i++)
    {
        parseBitfieldVar(result, key, supportedTypes[i], i);
    }
    EXPECT_EQ(result[key].size(), 0);

    supportedTypes[0].byte = 0x1f;
    for (int i = 0; i < 8; i++)
    {
        parseBitfieldVar(result, key, supportedTypes[i], i);
    }
    EXPECT_EQ(result[key].size(), 5);
	EXPECT_THAT(result[key], ElementsAre(0,1,2,3,4));

    supportedTypes[0].byte = 0;
	supportedTypes[7].byte = 0xf8;
	result.clear();
    for (int i = 0; i < 8; i++)
    {
        parseBitfieldVar(result, key, supportedTypes[i], i);
    }
    EXPECT_EQ(result[key].size(), 5);
	EXPECT_THAT(result[key], ElementsAre(59,60,61,62,63));
}
