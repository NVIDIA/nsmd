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

#include "nsmEvent.hpp"

namespace nsm
{

class NsmDevice;

/// Per-device handler for NSM Type 4, Event ID 0x02 (Set Diag Test Result).
/// Decodes the per-TID test result and pushes a ResultReceived App.Notify
/// carrying TID, error code, and result mask to prebootdiag.
class NsmDiagSetTestResultEvent : public NsmEvent
{
  public:
    NsmDiagSetTestResultEvent(const std::string& name, const std::string& type,
                              std::weak_ptr<NsmDevice> device);

    int handle(eid_t eid, NsmType type, NsmEventId eventId,
               const nsm_msg* event, size_t eventLen) final;

  private:
    std::weak_ptr<NsmDevice> device;
};

} // namespace nsm
