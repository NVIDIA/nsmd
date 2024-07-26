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

#include "asyncOperationManager.hpp"

using namespace nsm;

TEST(asyncOperationManager, GoodTest)
{
    AsyncOperationManager manager{8, AsyncOperationResultObjPath};

    for (size_t i{0}; i < 4; ++i)
    {
        const auto [path, statusInterface] = manager.getNewStatusInterface();

        const std::string objPath = std::string{AsyncOperationResultObjPath} +
                                    "/" + std::to_string(i);

        EXPECT_EQ(path, objPath);
        EXPECT_NE(statusInterface.get(), nullptr);
    }

    for (size_t i{4}; i < 8; ++i)
    {
        const auto [path, statusInterface,
                    valueInterface] = manager.getNewStatusValueInterface();

        const std::string objPath = std::string{AsyncOperationResultObjPath} +
                                    "/" + std::to_string(i);

        EXPECT_EQ(path, objPath);
        EXPECT_NE(statusInterface.get(), nullptr);
        EXPECT_NE(valueInterface.get(), nullptr);
    }

    for (size_t i{0}; i < 32; ++i)
    {
        const auto [path, statusInterface] = manager.getNewStatusInterface();
        EXPECT_EQ(path, "");
        EXPECT_EQ(statusInterface.get(), nullptr);
    }

    for (size_t i{0}; i < 32; ++i)
    {
        const auto [path, statusInterface,
                    valueInterface] = manager.getNewStatusValueInterface();

        EXPECT_EQ(path, "");
        EXPECT_EQ(statusInterface.get(), nullptr);
        EXPECT_EQ(valueInterface.get(), nullptr);
    }

    manager.statusInterfaces[2]->status(AsyncOperationStatusType::Success);
    manager.statusInterfaces[7]->status(AsyncOperationStatusType::WriteFailure);

    {
        const auto [path, statusInterface,
                    valueInterface] = manager.getNewStatusValueInterface();

        const std::string objPath = std::string{AsyncOperationResultObjPath} +
                                    "/" + std::to_string(2);

        EXPECT_EQ(path, objPath);
        EXPECT_NE(statusInterface.get(), nullptr);
        EXPECT_NE(valueInterface.get(), nullptr);
    }

    {
        const auto [path, statusInterface,
                    valueInterface] = manager.getNewStatusValueInterface();

        const std::string objPath = std::string{AsyncOperationResultObjPath} +
                                    "/" + std::to_string(7);

        EXPECT_EQ(path, objPath);
        EXPECT_NE(statusInterface.get(), nullptr);
        EXPECT_NE(valueInterface.get(), nullptr);
    }

    manager.statusInterfaces[5]->status(
        AsyncOperationStatusType::UnsupportedRequest);

    {
        const auto [path, statusInterface] = manager.getNewStatusInterface();

        const std::string objPath = std::string{AsyncOperationResultObjPath} +
                                    "/" + std::to_string(5);

        EXPECT_EQ(path, objPath);
        EXPECT_NE(statusInterface.get(), nullptr);
    }
}
