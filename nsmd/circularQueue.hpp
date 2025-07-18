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

#pragma once

#include <stdexcept>
#include <vector>

namespace nsm
{

template <typename T>
class CircularQueue : public std::vector<T>
{
  private:
    size_t currentIndex = 0;

  public:
    CircularQueue() : std::vector<T>() {}

    void push(const T& value)
    {
        std::vector<T>::push_back(value);
    }
    void push(T&& value)
    {
        std::vector<T>::push_back(value);
    }

    T& current()
    {
        if (size() == 0)
        {
            throw std::runtime_error(
                "CircularQueue::current() called on empty queue");
        }
        if (currentIndex >= size())
        {
            // Some sensors were removed from the queue, so we need to reset the
            // current index to 0
            currentIndex = 0;
        }
        return std::vector<T>::at(currentIndex);
    }

    void next()
    {
        if (size() > 0)
        {
            currentIndex = (currentIndex + 1) % size();
        }
        else
        {
            currentIndex = 0;
        }
    }

    using std::vector<T>::size;
    using std::vector<T>::empty;

    using std::vector<T>::begin;
    using std::vector<T>::end;
    using std::vector<T>::cbegin;
    using std::vector<T>::cend;
};

} // namespace nsm
