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

#include <common/types.hpp>
#include <nsmd/nsmDevice.hpp>

namespace nsm
{

/**
 * @brief The GlobalPollingStateManager class provides access to the global
 * state of devices.
 *
 * This class acts as a centralized interface for retrieving and managing
 * device state.
 */
class GlobalPollingStateManager
{
  public:
    GlobalPollingStateManager(const NsmDeviceTable& nsmDevices) :
        nsmDevices(nsmDevices)
    {}
    inline PollingState getState()
    {
        for (const auto& device : nsmDevices)
        {
            if (device->getPollingState() == POLL_PRIORITY)
            {
                return POLL_PRIORITY;
            }
        }
        return POLL_NON_PRIORITY;
    }

  private:
    const NsmDeviceTable& nsmDevices;
};

} // namespace nsm
