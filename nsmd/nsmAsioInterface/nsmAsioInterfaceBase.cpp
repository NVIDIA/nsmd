/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#include "nsmAsioInterfaceBase.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmAsioInterfaceBase::NsmAsioInterfaceBase(
    sdbusplus::asio::object_server& objServer, const std::string& objectPath,
    const std::string& interfaceName) :
    objServer(objServer), objectPath(objectPath), interfaceName(interfaceName)
{
    // Create the D-Bus interface object
    dbusInterface = objServer.add_interface(objectPath, interfaceName);

    if (!dbusInterface)
    {
        lg2::error(
            "Failed to create D-Bus interface: path={PATH}, interface={IFACE}",
            "PATH", objectPath, "IFACE", interfaceName);
    }
}

} // namespace nsm
