#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::ElementsAre;

#include "base.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmDevice.hpp"

TEST(nsmDevice, GoodTest)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";

    nsm::NsmDevice nsmDevice(uuid);
    EXPECT_EQ(nsmDevice.uuid, uuid);

    uint8_t setMode = 2;
    nsmDevice.setEventMode(2);
    auto getMode = nsmDevice.getEventMode();
    EXPECT_EQ(setMode, getMode);
}
