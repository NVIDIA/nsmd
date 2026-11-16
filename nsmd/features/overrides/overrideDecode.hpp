/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

#include "features/feature.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <coroutine>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace nsm::features
{

constexpr auto telemetryInterface =
    "xyz.openbmc_project.Configuration.NSM_Telemetry";
constexpr auto telemetryOverridesInterface =
    "xyz.openbmc_project.Configuration.NSM_Telemetry.FeatureOverrides";
constexpr auto policyInterface = "xyz.openbmc_project.Configuration.NSM_Policy";
/** @brief Prefix of the Policy<N> entry interfaces of an NSM_Policy record */
constexpr auto policyEntryPrefix =
    "xyz.openbmc_project.Configuration.NSM_Policy.Policy";

/** @brief Reserved key of a feature's a{sv}: enables or disables the feature */
constexpr auto enabledKey = "Enabled";
/** @brief Reserved key of a Policy<N> entry: a{sv} of device variables for
 *  the "{...}" templates of the feature list
 */
constexpr auto templateValuesKey = "TemplateValues";

/** @brief One feature's a{sv}: parameter -> value */
using FeatureOverrides = std::map<std::string, dbus::Value>;

/** @brief Every property of a FeatureOverrides interface has type a{sv}.
 *  Kept local so dbus::Value, which cannot contain itself, is unchanged.
 */
using FeaturesMap = std::map<std::string, std::variant<FeatureOverrides>>;

/** @brief Properties of a Policy<N> entry: Devices (as) and feature ids
 *  (a{sv})
 */
using PolicyEntryMap =
    std::map<std::string,
             std::variant<std::vector<std::string>, FeatureOverrides>>;

template <typename Variant, typename... Extra>
struct VariantAppend;

template <typename... Ts, typename... Extra>
struct VariantAppend<std::variant<Ts...>, Extra...>
{
    using type = std::variant<Ts..., Extra...>;
};

/** @brief InterfacesAdded payload of EM records that carry a{sv} values */
using AddedValue = VariantAppend<dbus::Value, FeatureOverrides>::type;
using AddedInterfaces =
    std::map<std::string, std::map<std::string, AddedValue>>;

/** @brief Decode feature id -> a{sv} into overrides
 *
 *  "Enabled" becomes Enable or Disable; every other key becomes a Set.
 *  A non-bool "Enabled" is logged and dropped; strict mode aborts.
 *
 *  @param[in] feature - feature id
 *  @param[in] values - the feature's a{sv}
 *  @param[in] source - record path, for logs
 *  @param[out] out - decoded overrides, appended
 */
void decodeFeatureOverrides(const std::string& feature,
                            const FeatureOverrides& values,
                            const std::string& source,
                            std::vector<Override>& out);

/** @brief Decode every property of a FeatureOverrides interface */
std::vector<Override> decodeFeatureOverrides(const FeaturesMap& features,
                                             const std::string& source);

/** @struct coGetAllAs
 *
 *  GetAll of one interface, decoded into Map. Used where the interface has
 *  a{sv} properties, which dbus::PropertyMap cannot decode. On error the
 *  result is empty and the error is logged.
 */
template <typename Map>
struct coGetAllAs : Map
{
    const std::string service;
    const std::string objectPath;
    const std::string interface;

    bool await_ready() noexcept
    {
        return false;
    }

    bool await_suspend(std::coroutine_handle<> handle) noexcept
    {
        auto& asioConnection = utils::DBusHandler::getAsioConnection();
        asioConnection->async_method_call(
            [resumeHandle = handle, this](boost::system::error_code ec,
                                          const Map& value) {
            if (ec)
            {
                lg2::error(
                    "GetAll {IFACE} failed for path={OBJECT_PATH}: {ERROR_MESSAGE}",
                    "IFACE", interface, "OBJECT_PATH", objectPath,
                    "ERROR_MESSAGE", ec.message());
            }
            else
            {
                static_cast<Map&>(*this) = value;
            }
            resumeHandle();
        },
            service.c_str(), objectPath.c_str(),
            "org.freedesktop.DBus.Properties", "GetAll", interface.c_str());
        return true;
    }

    Map await_resume() const noexcept
    {
        return static_cast<Map>(*this);
    }

    coGetAllAs(const std::string& service, const std::string& objectPath,
               const std::string& interface) :
        Map(), service(service), objectPath(objectPath), interface(interface)
    {}
};

} // namespace nsm::features
