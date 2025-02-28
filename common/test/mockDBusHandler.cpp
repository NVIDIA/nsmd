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

#include "mockDBusHandler.hpp"

#include <stdexcept>

namespace utils
{
IDBusHandler& DBusHandler()
{
    return MockDBusHandler::instance();
}

const PropertyValuesCollection::value_type
    DBusTest::get(const PropertyValuesCollection& properties,
                  const DbusProp& name)
{
    auto it =
        std::find_if(properties.begin(), properties.end(),
                     [&name](const auto& pair) { return pair.first == name; });
    if (it == properties.end())
        throw std::out_of_range("Property " + name +
                                " not found in collection");
    return *it;
}

} // namespace utils
