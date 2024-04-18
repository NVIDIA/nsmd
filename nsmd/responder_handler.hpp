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

#include "libnsm/base.h"

#include <cassert>
#include <functional>
#include <map>
#include <vector>

using NsmCommand = uint8_t;

namespace responder
{

using Response = std::vector<uint8_t>;
class CmdHandler;
using HandlerFunc =
    std::function<Response(const nsm_msg* request, size_t reqMsgLen)>;

class CmdHandler
{
  public:
    virtual ~CmdHandler() = default;
    /** @brief Invoke a NSM command handler
     *
     *  @param[in] command - command code
     *  @param[in] request - request message
     *  @param[in] reqMsgLen - request message size
     *  @return NSM response message
     */
    Response handle(NsmCommand Command, const nsm_msg* request,
                    size_t reqMsgLen)
    {
        return handlers.at(Command)(request, reqMsgLen);
    }

    /** @brief Create a response message containing only cc
     *
     *  @param[in] request - NSM request message
     *  @param[in] cc - Completion Code
     *  @return NSM response message
     */
    std::vector<uint8_t> ccOnlyResponse(const nsm_msg* request, uint8_t cc);

  protected:
    /** @brief map of NSM command code to handler - to be populated by derived
     *         classes.
     */
    std::map<NsmCommand, HandlerFunc> handlers;
};

} // namespace responder
