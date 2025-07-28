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

#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/event.hpp>

#include <atomic>
#include <coroutine>
#include <deque>
#include <memory>
#include <mutex>
#include <semaphore>

namespace common
{
/**
 * @brief A coroutine-compatible semaphore for managing coroutine suspension and
 * resumption.
 *
 * This implementation allows coroutines to wait for semaphore availability and
 * suspends them in an explicit queue until the semaphore is released. The
 * release process ensures that suspended coroutines are resumed in a FIFO order
 * while avoiding nested coroutine calls and unnecessary chain reactions.
 */
class CoroutineSemaphore
{
  public:
    /**
     * @brief Constructs a binary semaphore.
     */
    CoroutineSemaphore() :
        binarySem(1), event(sdeventplus::Event::get_default()),
        awaiterIdGenerator(0)
    {}

    /**
     * @brief An Awaiter object to manage coroutine suspension and resumption.
     */
    struct Awaiter
    {
        CoroutineSemaphore& semaphore;  // Reference to the semaphore
        std::coroutine_handle<> handle; // Coroutine handle to be resumed
        int eid;                        // Identifier for the coroutine
        int awaiterId;                  // Unique ID for the awaiter

        /**
         * @brief Constructor for the Awaiter object.
         * @param semaphoreRef Reference to the semaphore managing the
         * coroutine.
         * @param eidValue Identifier for the coroutine.
         * @param id Unique identifier for the awaiter.
         */
        Awaiter(CoroutineSemaphore& semaphoreRef, int eidValue, int id) :
            semaphore(semaphoreRef), handle(nullptr), eid(eidValue),
            awaiterId(id)
        {}

        /**
         * @brief Destructor for the Awaiter object.
         */
        ~Awaiter() {}

        /**
         * @brief Checks if the coroutine can proceed without suspension.
         * @return true if the semaphore is available; false otherwise.
         */
        bool await_ready() const noexcept
        {
            bool ready = semaphore.binarySem.try_acquire();
            return ready;
        }

        /**
         * @brief Suspends the coroutine if the semaphore is unavailable.
         *        The coroutine is added to the suspended queue for later
         * resumption.
         * @param h Coroutine handle to be stored for resumption.
         */
        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            handle = h;

            // Capture the current Awaiter object in a shared pointer for safe
            // resumption.
            auto awaiter = std::make_shared<Awaiter>(*this);

            std::unique_lock<std::mutex> lock(semaphore.mutex);
            semaphore.suspendedQueue.push_back(awaiter);
        }

        /**
         * @brief Called when the coroutine is resumed.
         */
        void await_resume() const noexcept {}
    };

    /**
     * @brief Acquires the semaphore asynchronously.
     *        If the semaphore is unavailable, suspends the coroutine until it
     * is released.
     * @param eid Identifier for the coroutine.
     * @return An Awaiter object to manage suspension and resumption.
     */
    Awaiter acquire(int eid)
    {
        int awaiterId = ++awaiterIdGenerator;
        return Awaiter{*this, eid, awaiterId};
    }

    /**
     * @brief Releases the semaphore and schedules the next coroutine in the
     * queue for resumption.
     *
     * The release process does not immediately resume the next coroutine.
     * Instead, it defers the resumption to the next tick of the event loop.
     * This avoids nested coroutine calls, prevents chain reactions, and ensures
     * fairness by allowing new requests to join the queue.
     */
    void release()
    {
        std::shared_ptr<Awaiter> nextAwaiter;

        {
            // Lock the queue to safely access and modify it.
            std::unique_lock<std::mutex> lock(mutex);
            if (!suspendedQueue.empty())
            {
                nextAwaiter = suspendedQueue.front();
                suspendedQueue.pop_front();
            }
        }

        if (nextAwaiter)
        {
            // Schedule the resumption of the next coroutine in the event loop.

            if (sd_event_add_defer(event.get(), nullptr,
                                   [](sd_event_source*, void* userdata) -> int {
                auto nextAwaiter =
                    static_cast<std::shared_ptr<Awaiter>*>(userdata);
                if (!nextAwaiter || !(*nextAwaiter))
                {
                    return -1;
                }
                (*nextAwaiter)->handle.resume();
                delete nextAwaiter; // Free memory after use
                return 0;
            }, new std::shared_ptr<Awaiter>(nextAwaiter)) < 0)
            {
                lg2::error(
                    "Failed to schedule deferred coroutine resumption for eid: {EID}, Awaiter ID: {AWAITER_ID}",
                    "EID", nextAwaiter->eid, "AWAITER_ID",
                    nextAwaiter->awaiterId);
            }
        }
        else
        {
            // If no coroutines are waiting, simply release the semaphore.
            binarySem.release();
        }
    }

  private:
    std::binary_semaphore binarySem; // Binary semaphore
    sdeventplus::Event event;        // Event loop for resumption
    std::deque<std::shared_ptr<Awaiter>>
        suspendedQueue;              // Explicit queue for suspended coroutines
    std::mutex mutex;                // Protects access to suspendedQueue
    std::atomic<int> awaiterIdGenerator; // Generates unique IDs for Awaiters
};

} // namespace common
