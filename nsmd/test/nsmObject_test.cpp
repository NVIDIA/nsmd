/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmObject.hpp"

using namespace nsm;

class TestNsmObject : public NsmObject
{
  public:
    TestNsmObject(const std::string& name, const std::string& type) :
        NsmObject(name, type)
    {}
};

struct NsmObjectTest : public ::testing::Test
{
    const std::string name = "TestObject";
    const std::string type = "TestType";
};

TEST_F(NsmObjectTest, goodTestConstructor)
{
    TestNsmObject obj(name, type);
    EXPECT_EQ(obj.getName(), name);
    EXPECT_EQ(obj.getType(), type);
}

TEST_F(NsmObjectTest, goodTestCopyConstructor)
{
    TestNsmObject obj1(name, type);
    TestNsmObject obj2(obj1);

    EXPECT_EQ(obj2.getName(), name);
    EXPECT_EQ(obj2.getType(), type);
}

TEST_F(NsmObjectTest, goodTestGetName)
{
    TestNsmObject obj("MyName", type);
    EXPECT_EQ(obj.getName(), "MyName");
}

TEST_F(NsmObjectTest, goodTestGetType)
{
    TestNsmObject obj(name, "MyType");
    EXPECT_EQ(obj.getType(), "MyType");
}

TEST_F(NsmObjectTest, goodTestGetDeviceIdentifier)
{
    TestNsmObject obj(name, type);
    obj.deviceIdentifier = "Device123";
    EXPECT_EQ(obj.getDeviceIdentifier(), "Device123");
}

TEST_F(NsmObjectTest, goodTestSetLastUpdatedTimeStamp)
{
    TestNsmObject obj(name, type);
    uint64_t timestamp = 1000000;

    obj.setLastUpdatedTimeStamp(timestamp);
    EXPECT_EQ(obj.lastUpdatedTimeStampInUsec, timestamp);
}

TEST_F(NsmObjectTest, goodTestNeedsUpdate)
{
    TestNsmObject obj(name, type);
    obj.refreshLimitInUsec = 1000;

    // Set initial timestamp
    obj.setLastUpdatedTimeStamp(0);

    // Test when update is needed
    EXPECT_TRUE(obj.needsUpdate(2000)); // delta = 2000 > 1000

    // Test when update is not needed
    EXPECT_FALSE(obj.needsUpdate(500)); // delta = 500 < 1000
}

TEST_F(NsmObjectTest, goodTestNeedsUpdateEdgeCase)
{
    TestNsmObject obj(name, type);
    obj.refreshLimitInUsec = 1000;
    obj.setLastUpdatedTimeStamp(5000);

    // Exactly at limit
    EXPECT_FALSE(obj.needsUpdate(6000)); // delta = 1000, not > 1000

    // Just over limit
    EXPECT_TRUE(obj.needsUpdate(6001)); // delta = 1001 > 1000
}

TEST_F(NsmObjectTest, goodTestIsRefreshed)
{
    TestNsmObject obj(name, type);
    EXPECT_FALSE(obj.isRefreshed);

    obj.isRefreshed = true;
    EXPECT_TRUE(obj.isRefreshed);
}

TEST_F(NsmObjectTest, goodTestInitialTimestamp)
{
    TestNsmObject obj(name, type);
    EXPECT_EQ(obj.lastUpdatedTimeStampInUsec, INIT_TIMESTAMP);
}

TEST_F(NsmObjectTest, goodTestDefaultRefreshLimit)
{
    TestNsmObject obj(name, type);
    EXPECT_EQ(obj.refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

TEST_F(NsmObjectTest, goodTestRefreshLimitConstants)
{
    EXPECT_GT(RR_REFRESH_LIMIT_IN_USEC, 0);
    EXPECT_GT(PRIORITY_REFRESH_LIMIT_IN_USEC, 0);
    EXPECT_GT(GPM_REFRESH_LIMIT_IN_USEC, 0);
    EXPECT_GT(LONG_RUNNING_REFRESH_LIMIT_IN_USEC, 0);
}

TEST_F(NsmObjectTest, goodTestMultipleObjects)
{
    TestNsmObject obj1("Object1", "Type1");
    TestNsmObject obj2("Object2", "Type2");
    TestNsmObject obj3("Object3", "Type3");

    EXPECT_EQ(obj1.getName(), "Object1");
    EXPECT_EQ(obj2.getName(), "Object2");
    EXPECT_EQ(obj3.getName(), "Object3");

    EXPECT_EQ(obj1.getType(), "Type1");
    EXPECT_EQ(obj2.getType(), "Type2");
    EXPECT_EQ(obj3.getType(), "Type3");
}
