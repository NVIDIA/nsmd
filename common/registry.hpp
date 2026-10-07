// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES
/*
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

#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

namespace common
{

/** @brief The type of the id @p Record reports. */
template <typename Record>
using RegistryId =
    std::remove_cvref_t<decltype(std::declval<const Record&>().id())>;

/** @brief A record that reports, through id(), the key it is filed under. */
template <typename Record>
concept RegistryRecord = std::movable<Record> &&
                         requires(const Record& record) { record.id(); } &&
                         std::copyable<RegistryId<Record>> &&
                         std::totally_ordered<RegistryId<Record>>;

/** @brief What happened to a watched record. */
enum class RegistryEvent
{
    Added,
    Changed
};

/**
 * @brief The records of one kind nsmd created, by the id each one reports.
 *
 * Records and their users may be created in any order, so users look records
 * up on each use and watch the ones they name.
 */
template <RegistryRecord Record>
class Registry
{
  public:
    using Id = RegistryId<Record>;
    using RecordPtr = std::shared_ptr<const Record>;
    using Listener = std::function<void(const Id&, RegistryEvent)>;

  private:
    struct InterestState
    {
        std::set<Id> ids;
        Listener listener;
    };

  public:
    /** @brief Keeps a watch alive; destroying it stops the notifications. */
    class Interest
    {
      public:
        Interest() = default;
        Interest(const Interest&) = delete;
        Interest& operator=(const Interest&) = delete;
        Interest(Interest&&) noexcept = default;
        Interest& operator=(Interest&&) noexcept = default;
        ~Interest() = default;

      private:
        friend Registry;

        explicit Interest(std::shared_ptr<InterestState> state) :
            state(std::move(state))
        {}

        std::shared_ptr<InterestState> state;
    };

    Registry() = default;
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    static Registry& instance()
    {
        static Registry registry;
        return registry;
    }

    /**
     * @brief Record @p record unless a record with its id already exists.
     *
     * Records are immutable once added.
     *
     * @return Whether @p record was added.
     */
    bool add(Record&& record)
    {
        return insert(std::move(record));
    }

    bool add(const Record& record)
    {
        return insert(record);
    }

    /** @brief The record with @p id, or null; holding it keeps it alive. */
    RecordPtr find(const Id& id) const
    {
        auto found = records.find(id);
        return found == records.end() ? nullptr : found->second;
    }

    /** @brief Tell those watching @p id that its record changed. */
    void changed(const Id& id)
    {
        if (records.contains(id))
        {
            notify(id, RegistryEvent::Changed);
        }
    }

    /**
     * @brief Call @p listener on each later add or change of a record in
     * @p ids, until the returned Interest is destroyed.
     *
     * Records already present are not replayed; read them with find.
     */
    [[nodiscard]] Interest watch(std::set<Id> ids, Listener listener)
    {
        auto state = std::make_shared<InterestState>(std::move(ids),
                                                     std::move(listener));
        interests.push_back(state);
        return Interest(std::move(state));
    }

  private:
    template <typename R>
    bool insert(R&& record)
    {
        Id id = record.id();
        auto hint = records.lower_bound(id);
        if (hint != records.end() && hint->first == id)
        {
            return false;
        }
        const auto entry = records.emplace_hint(
            hint, std::move(id),
            std::make_shared<const Record>(std::forward<R>(record)));
        // Records are never erased, so the stored key outlives every listener.
        notify(entry->first, RegistryEvent::Added);
        return true;
    }

    void notify(const Id& id, RegistryEvent event)
    {
        std::vector<std::weak_ptr<InterestState>> matched;
        std::erase_if(interests, [&](const auto& weak) {
            auto interest = weak.lock();
            if (!interest)
            {
                return true;
            }
            if (interest->ids.contains(id))
            {
                matched.push_back(weak);
            }
            return false;
        });
        // A listener may destroy another's Interest; that one is skipped.
        for (const auto& weak : matched)
        {
            if (auto interest = weak.lock())
            {
                interest->listener(id, event);
            }
        }
    }

    std::map<Id, RecordPtr> records;
    std::vector<std::weak_ptr<InterestState>> interests;
};

} // namespace common
