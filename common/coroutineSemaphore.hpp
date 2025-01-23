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
    {
        lg2::debug("CoroutineSemaphore initialized");
    }

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
        {
            lg2::debug(
                "Awaiter created: {AWAITER_ID}, eid: {EID}, address: {ADDRESS}",
                "AWAITER_ID", awaiterId, "EID", eid, "ADDRESS",
                static_cast<void*>(this));
        }

        /**
         * @brief Destructor for the Awaiter object.
         */
        ~Awaiter()
        {
            lg2::debug(
                "Awaiter destroyed: {AWAITER_ID}, eid: {EID}, address: {ADDRESS}",
                "AWAITER_ID", awaiterId, "EID", eid, "ADDRESS",
                static_cast<void*>(this));
        }

        /**
         * @brief Checks if the coroutine can proceed without suspension.
         * @return true if the semaphore is available; false otherwise.
         */
        bool await_ready() const noexcept
        {
            bool ready = semaphore.binarySem.try_acquire();
            lg2::debug("Awaiter {AWAITER_ID} await_ready: {READY}, eid: {EID}",
                       "AWAITER_ID", awaiterId, "READY",
                       ready ? "true" : "false", "EID", eid);
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
            lg2::debug(
                "Awaiter {AWAITER_ID} suspended. Queue size: {QUEUE_SIZE}, eid: {EID}, address: {ADDRESS}",
                "AWAITER_ID", awaiterId, "QUEUE_SIZE",
                semaphore.suspendedQueue.size(), "EID", eid, "ADDRESS",
                static_cast<void*>(this));
        }

        /**
         * @brief Called when the coroutine is resumed.
         */
        void await_resume() const noexcept
        {
            lg2::debug(
                "Awaiter {AWAITER_ID} resumed execution, eid: {EID}, address: {ADDRESS}",
                "AWAITER_ID", awaiterId, "EID", eid, "ADDRESS",
                static_cast<void*>(const_cast<Awaiter*>(this)));
        }
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
        lg2::debug(
            "Semaphore acquire called for eid: {EID}, Awaiter ID: {AWAITER_ID}",
            "EID", eid, "AWAITER_ID", awaiterId);
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
        lg2::debug("Semaphore release called");

        std::shared_ptr<Awaiter> nextAwaiter;

        {
            // Lock the queue to safely access and modify it.
            std::unique_lock<std::mutex> lock(mutex);

            lg2::debug("Queue size before processing: {QUEUE_SIZE}",
                       "QUEUE_SIZE", suspendedQueue.size());

            if (!suspendedQueue.empty())
            {
                nextAwaiter = suspendedQueue.front();
                suspendedQueue.pop_front();
                lg2::debug(
                    "Dequeued coroutine for resumption. Remaining queue size: {QUEUE_SIZE}, eid: {EID}, Awaiter ID: {AWAITER_ID}, address: {ADDRESS}",
                    "QUEUE_SIZE", suspendedQueue.size(), "EID",
                    nextAwaiter->eid, "AWAITER_ID", nextAwaiter->awaiterId,
                    "ADDRESS", static_cast<void*>(nextAwaiter.get()));
            }
            else
            {
                lg2::debug("No suspended coroutines in the queue");
            }
        }

        if (nextAwaiter)
        {
            // Schedule the resumption of the next coroutine in the event loop.
            lg2::debug(
                "Deferring coroutine resumption for eid: {EID}, Awaiter ID: {AWAITER_ID}",
                "EID", nextAwaiter->eid, "AWAITER_ID", nextAwaiter->awaiterId);

            if (sd_event_add_defer(
                    event.get(), nullptr,
                    [](sd_event_source*, void* userdata) -> int {
                auto nextAwaiter =
                    static_cast<std::shared_ptr<Awaiter>*>(userdata);
                if (!nextAwaiter || !(*nextAwaiter))
                {
                    lg2::error(
                        "Deferred callback userdata is null or invalid!");
                    return -1;
                }
                lg2::debug(
                    "Deferred callback executed for Awaiter {AWAITER_ID}, eid: {EID}",
                    "AWAITER_ID", (*nextAwaiter)->awaiterId, "EID",
                    (*nextAwaiter)->eid);
                (*nextAwaiter)->handle.resume();
                lg2::debug(
                    "Awaiter {AWAITER_ID} resumed successfully, eid: {EID}",
                    "AWAITER_ID", (*nextAwaiter)->awaiterId, "EID",
                    (*nextAwaiter)->eid);
                delete nextAwaiter; // Free memory after use
                return 0;
            },
                    new std::shared_ptr<Awaiter>(nextAwaiter)) < 0)
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
            lg2::debug("Semaphore released with no waiting coroutines");
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
