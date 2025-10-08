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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
using namespace ::testing;

#include "base.h"

#include "utils.hpp"

#define private public
#define protected public

#include "stateChangeLogger.hpp"

using namespace nsm;

struct StateChangeLoggerTest : public Test, public StateChangeLogger
{
    const utils::Bitfield256& bitfieldArg(const std::string& fName,
                                          size_t index = 0)
    {
        auto& logger = loggers[fName];
        assert(logger.size() > index);
        return std::get<utils::Bitfield256>(logger[index]);
    }
    auto boolArg(const std::string& fName, size_t index)
    {
        auto& logger = loggers[fName];
        assert(logger.size() > index);
        return std::get<bool>(logger[index]);
    }
};

TEST_F(StateChangeLoggerTest, GoodTestSuccessNotStored)
{
    EXPECT_FALSE(shouldLog("GoodTestSuccessNotStored1", NSM_SW_SUCCESS,
                           nsm_completion_codes(0)));
    EXPECT_TRUE(loggers.empty());

    EXPECT_FALSE(shouldLog("GoodTestSuccessNotStored2", ERR_NULL, NSM_SUCCESS,
                           NSM_SW_SUCCESS));
    EXPECT_TRUE(loggers.empty());
    EXPECT_NO_THROW(shouldLog("GoodTestSuccessNotStored2", ERR_NULL,
                              NSM_SUCCESS, NSM_SW_SUCCESS, false));
    EXPECT_TRUE(loggers.empty());

    EXPECT_FALSE(shouldLog("GoodTestSuccessNotStored3", ERR_NULL, NSM_SUCCESS,
                           NSM_SW_SUCCESS, false));
    EXPECT_TRUE(loggers.empty());
}

TEST_F(StateChangeLoggerTest, GoodTestArgsMismatch)
{
    EXPECT_TRUE(shouldLog("GoodTestArgsMismatch", NSM_SW_ERROR,
                          nsm_completion_codes(0)));
    EXPECT_FALSE(loggers.empty());

    EXPECT_TRUE(shouldLog("GoodTestArgsMismatch", ERR_NULL, NSM_SUCCESS,
                          NSM_SW_SUCCESS));

    EXPECT_FALSE(loggers.empty());
}

TEST_F(StateChangeLoggerTest, GoodTestLogResultCode)
{
    const auto fName = "GoodTestLogResultCode";
    EXPECT_TRUE(shouldLog(fName, NSM_SW_ERROR_DATA));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName).isAnyBitSet());
    EXPECT_FALSE(shouldLog(fName, NSM_SW_ERROR_DATA));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName).isAnyBitSet());
    EXPECT_TRUE(shouldLog(fName, NSM_SW_ERROR_LENGTH));
    EXPECT_FALSE(shouldLog(fName, NSM_SW_ERROR_LENGTH));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName).isAnyBitSet());
    EXPECT_FALSE(shouldLog(fName, NSM_SW_SUCCESS));
    EXPECT_TRUE(loggers.empty());
}

TEST_F(StateChangeLoggerTest, GoodTestLogResponse)
{
    const auto fName = "GoodTestLogResponse";
    EXPECT_TRUE(shouldLog(fName, ERR_TIMEOUT, NSM_SUCCESS, NSM_SW_ERROR));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_FALSE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_TRUE(shouldLog(fName, ERR_NOT_SUPPORTED, NSM_ERR_INVALID_DATA,
                          NSM_SW_ERROR));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_TRUE(shouldLog(fName, ERR_TIMEOUT, NSM_ERR_INVALID_DATA,
                          NSM_SW_ERROR_LENGTH));
    EXPECT_FALSE(shouldLog(fName, ERR_TIMEOUT, NSM_ERR_INVALID_DATA,
                           NSM_SW_ERROR_LENGTH));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_FALSE(
        shouldLog(fName, ERR_TIMEOUT, NSM_ERR_INVALID_DATA, NSM_SW_SUCCESS));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_FALSE(shouldLog(fName, ERR_NULL, NSM_SUCCESS, NSM_SW_SUCCESS));
    EXPECT_TRUE(loggers.empty());
}

TEST_F(StateChangeLoggerTest, GoodTestLogResponseWithSize)
{
    const auto fName = "GoodTestLogResponseWithSize";
    EXPECT_TRUE(
        shouldLog(fName, ERR_TIMEOUT, NSM_ERR_NOT_READY, NSM_SW_ERROR, false));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_FALSE(boolArg(fName, 3));
    EXPECT_TRUE(shouldLog(fName, ERR_NOT_SUPPORTED, NSM_ERR_NOT_READY,
                          NSM_SW_SUCCESS, true));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_TRUE(boolArg(fName, 3));
    EXPECT_TRUE(shouldLog(fName, ERR_TIMEOUT, NSM_ERR_INVALID_DATA,
                          NSM_SW_ERROR_LENGTH, false));
    EXPECT_FALSE(shouldLog(fName, ERR_TIMEOUT, NSM_ERR_INVALID_DATA,
                           NSM_SW_ERROR_LENGTH, false));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_TRUE(boolArg(fName, 3));
    EXPECT_FALSE(shouldLog(fName, ERR_TIMEOUT, NSM_ERR_INVALID_DATA,
                           NSM_SW_SUCCESS, false));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_TRUE(boolArg(fName, 3));
    EXPECT_FALSE(
        shouldLog(fName, ERR_NULL, NSM_SUCCESS, NSM_SW_SUCCESS, false));
    EXPECT_TRUE(loggers.empty());
}

TEST_F(StateChangeLoggerTest, GoodTestBoolOnly)
{
    const auto clearLoggerMsg1 = "GoodTestBoolOnly 1";
    const auto clearLoggerMsg2 = "GoodTestBoolOnly 2";
    EXPECT_TRUE(shouldLog(clearLoggerMsg1, true));
    EXPECT_TRUE(shouldLog(clearLoggerMsg2, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg1, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg2, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg1, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg2, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg1, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg2, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg1, false));
    EXPECT_FALSE(shouldLog(clearLoggerMsg2, false));
}
