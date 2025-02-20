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

#include "base.h"

#include "utils.hpp"

#include <tuple>
#include <unordered_map>
namespace nsm
{

class NsmObject;
class NsmEvent;

template <typename T>
concept StateChangeLoggerAllowedType =
    std::same_as<std::decay_t<T>, nsm_reason_codes> ||
    std::same_as<std::decay_t<T>, nsm_sw_codes> ||
    std::same_as<std::decay_t<T>, nsm_completion_codes> ||
    std::same_as<std::decay_t<T>, bool>;

template <typename... Args>
concept AllStateChangeLoggerAllowedTypes =
    (StateChangeLoggerAllowedType<Args> && ...);

class StateChangeLogger
{
  public:
    virtual ~StateChangeLogger() = default;

    /**
     * @brief Determines if the state of the provided arguments has changed and
     * logs the cleared codes if applicable.
     *
     * @tparam Args Types of the arguments to be checked.
     * @note Only following types will be checked for success:
     * - bool
     * - nsm_sw_codes
     * - nsm_reason_codes
     * - nsm_completion_codes
     * @param loggerName The name of the logger to be stored in the map. This
     * name will be used to display a success message when all arguments become
     * successful after previously being unsuccessful.
     * @param args The arguments whose states are to be checked.
     * @return true if any of the arguments have changed state and a message
     * should be logged.
     */
    template <typename... Args>
    bool shouldLog(const std::string& loggerName, Args&&... args)
        requires AllStateChangeLoggerAllowedTypes<Args...>
    {
        auto loggerExists = loggers.find(loggerName) != loggers.end();
        bool allSuccess = (isSuccess(args) && ...);
        if (!loggerExists && allSuccess)
        {
            // if all arguments are successful and logger not exists, return
            // false
            return false;
        }
        else if (!loggerExists)
        {
            // logger not exist, create a new logger
            loggers[loggerName] = createStateArgs(args...);
        }

        auto& stateChangeArgs = loggers[loggerName];
        if (stateChangeArgs.size() != sizeof...(args))
        {
            lg2::error("{FUNCNAME} ERROR | Argument size mismatch", "FUNCNAME",
                       loggerName);
            return true;
        }

        bool stateChanged = shouldLogAndUpdate(stateChangeArgs, args...);
        if (!stateChanged && allSuccess)
        {
            logClearedCodes(loggerName, stateChangeArgs, args...);
            loggers.erase(loggerName);
        }

        return stateChanged;
    }
    /**
     * @brief Specifig use case for shouldLog function.
     */
    template <typename T>
    bool shouldLog(const std::string& loggerName, uint16_t reasonCode,
                   uint8_t cc, T rc)
    {
        return shouldLog(loggerName, nsm_reason_codes(reasonCode),
                         nsm_completion_codes(cc), nsm_sw_codes(rc));
    }
    /**
     * @brief Specifig use case for shouldLog function.
     */
    template <typename T>
    bool shouldLog(const std::string& loggerName, uint16_t reasonCode,
                   uint8_t cc, T rc, bool sizeMismatch)
    {
        return shouldLog(loggerName, nsm_reason_codes(reasonCode),
                         nsm_completion_codes(cc), nsm_sw_codes(rc),
                         sizeMismatch);
    }

  private:
    using StateChangeArg = std::variant<utils::Bitfield256, bool>;
    using StateChangeArgs = std::vector<StateChangeArg>;
    std::unordered_map<std::string, StateChangeArgs> loggers;

    template <typename... Args>
    StateChangeArgs createStateArgs(Args&&... args)
    {
        StateChangeArgs stateArgs;
        (stateArgs.emplace_back(initializeArg(args)), ...);
        return stateArgs;
    }

    template <typename T>
    StateChangeArg initializeArg(T)
    {
        if constexpr (std::is_same_v<std::decay_t<T>, nsm_reason_codes> ||
                      std::is_same_v<std::decay_t<T>, nsm_sw_codes> ||
                      std::is_same_v<std::decay_t<T>, nsm_completion_codes>)
        {
            return utils::Bitfield256{};
        }
        else if constexpr (std::is_same_v<std::decay_t<T>, bool>)
        {
            return bool{};
        }
    }

    template <typename T>
    bool isSuccess(T&& arg)
    {
        if constexpr (std::is_same_v<std::decay_t<T>, nsm_reason_codes>)
        {
            return arg == ERR_NULL;
        }
        else if constexpr (std::is_same_v<std::decay_t<T>, nsm_sw_codes>)
        {
            return arg == NSM_SW_SUCCESS;
        }
        else if constexpr (std::is_same_v<std::decay_t<T>,
                                          nsm_completion_codes>)
        {
            return arg == NSM_SUCCESS || arg == NSM_ACCEPTED;
        }
        else if constexpr (std::is_same_v<std::decay_t<T>, bool>)
        {
            return !arg;
        }
        // should not be reached, because of checking allowed types during
        // compilation
        return true;
    }

    template <typename... Args>
    bool shouldLogAndUpdate(StateChangeArgs& stateArgs, Args&&... args)
    {
        size_t i = 0;
        bool stateChanged = false;
        (shouldLogAndUpdate(stateChanged, stateArgs[i++], args), ...);
        return stateChanged;
    }
    template <typename T>
    void shouldLogAndUpdate(bool& stateChanged, StateChangeArg& storedValue,
                            T&& arg)
    {
        stateChanged |= std::visit(
            [&](auto&& value) -> bool {
            using StoredType = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<StoredType, utils::Bitfield256>)
            {
                return !isSuccess(arg) && value.setBit(arg);
            }
            else if constexpr (std::is_same_v<StoredType, bool>)
            {
                bool result = !isSuccess(arg) && value != arg;
                if (result)
                {
                    value = arg; // Update value in variant
                }
                return result;
            }
            // should not be reached, because of checking allowed types during
            // compilation
            return false;
        },
            storedValue);
    }

    template <typename... Args>
    void logClearedCodes(const std::string& loggerName,
                         StateChangeArgs& stateArgs, Args&&... args)
    {
        std::string clearedCodes;
        size_t i = 0;
        (appendClearedCodes(clearedCodes, stateArgs[i++], args), ...);
        if (!clearedCodes.empty())
        {
            lg2::info("{FUNCNAME} SUCCESSFUL | Cleared Codes : {CLEAREDCODES}",
                      "FUNCNAME", loggerName, "CLEAREDCODES", clearedCodes);
        }
    }

    template <typename T>
    void appendClearedCodes(std::string& clearedCodes,
                            const StateChangeArg& value, T&&)
    {
        std::string codesText = std::visit(
            [](auto& storedValue) -> std::string {
            using StoredType = std::decay_t<decltype(storedValue)>;
            if constexpr (std::is_same_v<StoredType, utils::Bitfield256>)
            {
                return storedValue.getSetBits();
            }
            else if constexpr (std::is_same_v<StoredType, bool>)
            {
                return storedValue ? "1" : "";
            }
            // should not be reached, because of checking allowed types during
            // compilation
            return "";
        },
            value);

        if (!codesText.empty())
        {
            codesText =
                (std::is_same_v<std::decay_t<T>, nsm_reason_codes>)
                    ? "ReasonCodes=[" + codesText + "]"
                : (std::is_same_v<std::decay_t<T>, nsm_sw_codes>)
                    ? "ResultCodes=[" + codesText + "]"
                : (std::is_same_v<std::decay_t<T>, nsm_completion_codes>)
                    ? "CompletionCodes=[" + codesText + "]"
                    : "Flag=[" + codesText + "]";

            if (!clearedCodes.empty())
            {
                clearedCodes += ", ";
            }
            clearedCodes += codesText;
        }
    }

    // Type list of allowed types to be extracted
    using AllowedTypesToExtract =
        std::tuple<bool, uint8_t, uint16_t, int, nsm_sw_codes, nsm_reason_codes,
                   nsm_completion_codes>;

    template <typename T>
    static bool constexpr IsAllowedTypeToExtract =
        std::disjunction_v<std::is_same<T, AllowedTypesToExtract>>;

    // Helper function to create allowed tuple, including only allowed types
    template <typename Arg>
    static auto extractAllowedTuple(const Arg& arg)
    {
        if constexpr (IsAllowedTypeToExtract<std::decay_t<Arg>>)
        {
            return std::make_tuple(arg);
        }
        else
        {
            return std::tuple<>();
        }
    }

    template <std::size_t... Indices, typename... Args>
    static auto extractOddArgsHelper(std::index_sequence<Indices...>,
                                     const Args&... args)
    {
        return std::tuple_cat(extractAllowedTuple(
            std::get<Indices * 2 + 1>(std::forward_as_tuple(args...)))...);
    }

  public:
    template <typename... Args>
    static auto extractOddArgs(const Args&... args)
    {
        static_assert(sizeof...(Args) % 2 == 0,
                      "Arguments must be in key-value pairs.");
        constexpr std::size_t numPairs = sizeof...(Args) / 2;
        return extractOddArgsHelper(std::make_index_sequence<numPairs>(),
                                    args...);
    }
};

} // namespace nsm
