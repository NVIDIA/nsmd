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

#include "asyncOperationManager.hpp"
#include "nsmSensor.hpp"

namespace nsm
{
class NsmAsyncSensor : public NsmSensor
{
  public:
    using NsmSensor::NsmSensor;

    requester::Coroutine set(const AsyncSetOperationValueType& value,
                             AsyncOperationStatusType* status,
                             std::shared_ptr<NsmDevice> device);

  protected:
    AsyncOperationStatusType* status = nullptr;
    template <typename T>
    T getValue()
    {
        return std::get<T>(*value);
    }

    virtual requester::Coroutine update(SensorManager& manager,
                                        eid_t eid) override;

  private:
    const AsyncSetOperationValueType* value = nullptr;
};
} // namespace nsm
