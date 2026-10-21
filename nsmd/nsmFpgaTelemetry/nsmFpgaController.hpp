// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#pragma once

#include "fpga/fpga-controller.h"
#include "fpga/fpga-run.h"
#include "fpga/fpga-tables.h"

#include "common/coroutineSemaphore.hpp"
#include "common/event.hpp"
#include "nsmDevice.hpp"
#include "nsmFpgaStatus.hpp"
#include "utils.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>

namespace nsm
{

using MmioRecord = std::array<uint8_t, NSM_FPGA_RECORD_SIZE>;

// Session owner of one FPGA. It opens, renews and releases the session and
// drives the libnsm row runner: ask for the next MMIO frame, send it, hand
// the reply back, sleep when told to. Every row decision is libnsm's.
class NsmFpgaController : public std::enable_shared_from_this<NsmFpgaController>
{
  public:
    NsmFpgaController(std::shared_ptr<NsmFpgaStatus> status,
                      const nsm_fpga_map* map = nsm_fpga_map_p0());

    // One rule alone: a round of one item with no group rows
    requester::Coroutine read(std::shared_ptr<NsmDevice> nsmDevice,
                              const nsm_fpga_rule* rule, uint32_t& value);
    // One round of a group inside one episode: the group's setup rows, then
    // body (the items, each through readItem), then its cleanup rows
    requester::Coroutine runGroup(std::shared_ptr<NsmDevice> nsmDevice,
                                  const nsm_fpga_group* group,
                                  const char* target,
                                  std::function<requester::Coroutine()> body);
    // One item inside runGroup: its rows over the round's group buffer
    requester::Coroutine readItem(std::shared_ptr<NsmDevice> nsmDevice,
                                  const nsm_fpga_rule* rule, uint32_t& value);
    // The group buffer of the current round
    std::span<const uint8_t> roundBuffer() const
    {
        return {run.group, run.group_len};
    }
    // Bytes the last read captured for its item
    std::span<const uint8_t> lastCapture() const
    {
        return {run.item, run.item_len};
    }
    // How the last read ended the group, when an expect row asked for it
    bool lastEndedGroup() const
    {
        return run.end_group;
    }
    uint8_t lastGroupState() const
    {
        return run.group_state;
    }
    uint8_t lastGroupHealth() const
    {
        return run.group_health;
    }
    // The current round was ended by the group's setup rows
    bool roundEndedEarly() const
    {
        return roundEnded;
    }

    // Ends the session now; the idle watcher calls it after the round
    requester::Coroutine release(std::shared_ptr<NsmDevice> nsmDevice);

    // Next nonzero session number for an ACQUIRE
    uint32_t nextSession();

    bool sessionOpen() const
    {
        return open;
    }
    const std::string& lastError() const
    {
        return error;
    }
    size_t lastExchanges() const
    {
        return exchanges;
    }
    const nsm_fpga_authority_status& lastAuthority() const
    {
        return authority;
    }
    const nsm_fpga_grant& currentGrant() const
    {
        return session.grant;
    }
    // The FPGA status object allows polling
    bool enabled() const
    {
        const auto fpgaStatus = statusObject();
        return fpgaStatus && fpgaStatus->enabled();
    }
    // The status the discovery check entry created, looked up by device
    std::shared_ptr<NsmFpgaStatus> statusObject() const;

    // One controller per FPGA device, keyed by its STATIC UUID.
    static std::shared_ptr<NsmFpgaController>
        getOrCreate(const uuid_t& uuid, std::shared_ptr<NsmFpgaStatus> status);
    static std::shared_ptr<NsmFpgaController> get(const uuid_t& uuid);
    static void clear();

    // Shutdown drain: stop new episodes and release every open session so the
    // bank is not left owned, which the authority quarantines. Bounded.
    static requester::Coroutine shutdownRelease(common::Event& loopEvent);

    static constexpr uint64_t usecPerMillisecond = 1000;
    static constexpr uint64_t usecPerSecond = 1000 * usecPerMillisecond;
    // Tuning comes from the meson options fpga-* (defaults 100 ms, 5 s, 16)
    static constexpr uint64_t idleReleaseUsec = uint64_t{FPGA_IDLE_RELEASE_MS} *
                                                usecPerMillisecond;
    static constexpr uint64_t renewAfterUsec = uint64_t{FPGA_RENEW_AFTER_MS} *
                                               usecPerMillisecond;
    // After a refused session the round fails fast without new status reads
    static constexpr uint64_t openRetryUsec = usecPerSecond;
    // After a fence or quarantine, only re-read the status word this often
    // instead of hammering ACQUIRE, until it shows available with owner 0
    static constexpr uint64_t fenceBackoffUsec = 60 * usecPerSecond;
    // After the bank refused an ACTIVATE or RECOVER, leave it alone this long
    static constexpr uint64_t recoverHoldUsec = 600 * usecPerSecond;
    // The shutdown drain waits this often, this many times, for a busy episode
    static constexpr uint64_t drainPollUsec = 5 * usecPerMillisecond;
    static constexpr uint32_t drainPollLimit = 400;
    // Where the last RECOVER epoch is kept so later ones stay above it
    static std::string epochPath;
    // Per-episode wall-clock budget, well inside the lease; past it the job
    // stops between frames and the grant is kept for RELEASE while the lease
    // holds, fenced only once the lease has lapsed
    uint64_t episodeDeadlineUsec = 4 * usecPerSecond;
    // A round that has used this much of the episode leaves its remaining
    // items to the next round, which is due at once
    uint64_t episodeSliceUsec = usecPerSecond;
    // The grant lease the FPGA honours; a RELEASE after it lapses is rejected
    uint64_t leaseUsec = 10 * usecPerSecond;
    // The current episode has used its slice
    bool episodeOverSlice() const
    {
        return episodeStartUsec != 0 &&
               nowUsec() - episodeStartUsec >= episodeSliceUsec;
    }
    // Monotonic microseconds, the clock the sensor manager schedules with
    static uint64_t nowUsec()
    {
        return utils::getCurrentSteadyClockTimestampUs();
    }
    static constexpr uint32_t maxPendingRepeats = FPGA_PENDING_REPEATS;
    // Row poll defaults when the row carries none: 5 ms step, about 1 s
    static constexpr uint32_t defaultPollPeriodUsec = 5000;
    static constexpr uint32_t defaultPollLimit = 200;

  private:
    // One episode: preamble, open or renew the session, run body, postamble
    requester::Coroutine episode(std::shared_ptr<NsmDevice> nsmDevice,
                                 const char* target,
                                 std::function<requester::Coroutine()> body);
    requester::Coroutine openSession(std::shared_ptr<NsmDevice> nsmDevice);
    requester::Coroutine control(std::shared_ptr<NsmDevice> nsmDevice,
                                 uint8_t opcode, uint32_t argument);
    // RECOVER a bank quarantined with nsmd's own lapsed lease, then re-read
    // its status; the bank is left alone for recoverHoldUsec after a refusal
    requester::Coroutine recoverOwnLease(std::shared_ptr<NsmDevice> nsmDevice);
    // ACTIVATE of a bank nobody activated since its controller came up, with
    // a fresh epoch; a refusal leaves the bank alone for recoverHoldUsec
    requester::Coroutine activateBank(std::shared_ptr<NsmDevice> nsmDevice);
    void persistEpoch(uint64_t epoch) const;
    // The epoch a RECOVER carries: wall clock, above the bank's and the last
    uint64_t nextEpoch() const;
    // Runs the job the runner holds to its end: send, reply, wait, repeat
    requester::Coroutine drive(std::shared_ptr<NsmDevice> nsmDevice);
    // One Type 5 MMIO exchange: SET of a record or GET of one
    requester::Coroutine exchange(std::shared_ptr<NsmDevice> nsmDevice,
                                  const nsm_fpga_step& step, MmioRecord& reply,
                                  bool& lost, bool& noSession,
                                  std::string& failure);
    // A job that ended with the grant gone drops the session and fences
    void settle();
    // Releases the session once reads stop arriving; detached coroutine
    requester::Coroutine idleWatcher(std::weak_ptr<NsmDevice> weakDevice);
    // Drops the open session. When fenced, the bank is left for the authority
    // to recover and not re-acquired until the backoff; see fencedUntilUsec.
    void sessionLost(const std::string& why, bool fenced = false);

    // Logs once when the bank enters a state only an operator can clear
    void noteSession(enum nsm_fpga_session_state state);
    std::string statusName() const;
    std::string statusPath() const;

    std::shared_ptr<NsmFpgaStatus> status;
    uuid_t uuid;
    bool sessionBlocked = false;
    const nsm_fpga_map* map;
    uint32_t configurationType;
    common::Event event;
    // Serializes reads and release so a poll that arrives during the idle
    // RELEASE waits for it instead of skipping the round and publishing NaN
    common::CoroutineSemaphore ioLock{event};
    bool open = false;
    bool busy = false;
    bool watcherRunning = false;
    // The grant and sequences the runner stamps on every frame
    nsm_fpga_session session{};
    nsm_fpga_run run{};
    nsm_fpga_authority_status authority{};
    uint32_t sessionCounter = 0;
    uint64_t acquiredAtUsec = 0;
    uint64_t lastActivityUsec = 0;
    uint64_t openFailedAtUsec = 0;
    // When set in the future, the bank is fenced or quarantined; back off from
    // ACQUIRE and only re-read its status word until this time passes
    uint64_t fencedUntilUsec = 0;
    // No RECOVER before this time after the bank refused one
    uint64_t recoverHoldUntilUsec = 0;
    // Start of the current episode, non-zero only while one runs
    uint64_t episodeStartUsec = 0;
    // Cleanup rows run past the deadline so the station is left clean
    bool inCleanup = false;
    // Name of the item whose rows run now, for the deadline log
    const char* currentItem = nullptr;
    // The device this controller holds a session on, for the shutdown drain
    std::weak_ptr<NsmDevice> sessionDevice;
    // Last failure logged per read target, cleared when it recovers
    std::map<std::string, std::string> lastLoggedErrors;
    std::string error;
    // The current group round was ended by its setup rows with this state
    bool roundEnded = false;
    uint8_t roundState = 0;
    uint8_t roundHealth = 0;
    size_t exchanges = 0;
    // Set once on SIGTERM so no controller opens a new session while draining
    static bool shuttingDown;
};

} // namespace nsm
