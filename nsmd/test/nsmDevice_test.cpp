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
	uint8_t eid = 30;

	nsm::NsmDevice nsmDevice(eid);

    EXPECT_EQ(nsmDevice.eid, eid);

}
