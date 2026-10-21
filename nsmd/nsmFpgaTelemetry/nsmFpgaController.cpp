// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "nsmFpgaController.hpp"

#include "device-configuration.h"

#include "common/sleep.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>

namespace nsm
{

bool NsmFpgaController::shuttingDown = false;

namespace
{
constexpr size_t mmioRequestSize =
    sizeof(nsm_msg_hdr) + sizeof(nsm_common_req_v2) + sizeof(uint32_t) +
    sizeof(nsm_mmio_access_query);
constexpr uint16_t recordDwords = NSM_FPGA_RECORD_SIZE / sizeof(uint32_t);

static std::map<uuid_t, std::shared_ptr<NsmFpgaController>>& registry()
{
    static std::map<uuid_t, std::shared_ptr<NsmFpgaController>> controllers;
    return controllers;
}

// Drains every session, then ends the event loop the daemon runs on
requester::Coroutine drainAndExit(common::Event& loopEvent)
{
    co_await NsmFpgaController::shutdownRelease(loopEvent);
    static_cast<sdeventplus::Event&>(loopEvent).exit(0);
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

int onShutdownSignal(sd_event_source* /*source*/,
                     const struct signalfd_siginfo* /*si*/, void* userdata)
{
    lg2::info("FPGA telemetry received a shutdown signal, draining sessions");
    drainAndExit(*static_cast<common::Event*>(userdata)).detach();
    return 0;
}

// Takes SIGTERM and SIGINT on the default event loop so open sessions are
// released before exit; without it the bank is left owned and quarantined
void installShutdownHandler()
{
    static common::Event loopEvent;
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, nullptr);
    sd_event* loop = static_cast<sdeventplus::Event&>(loopEvent).get();
    if (sd_event_add_signal(loop, nullptr, SIGTERM, onShutdownSignal,
                            &loopEvent) < 0 ||
        sd_event_add_signal(loop, nullptr, SIGINT, onShutdownSignal,
                            &loopEvent) < 0)
    {
        lg2::error("FPGA telemetry could not take the shutdown signals");
        sigprocmask(SIG_UNBLOCK, &mask, nullptr);
    }
}
} // namespace

NsmFpgaController::NsmFpgaController(std::shared_ptr<NsmFpgaStatus> status,
                                     const nsm_fpga_map* map) :
    status(std::move(status)), map(map),
    configurationType(map->window.configuration_type)
{
    const nsm_fpga_run_limits limits{defaultPollPeriodUsec, defaultPollLimit,
                                     maxPendingRepeats};
    nsm_fpga_run_init(&run, map, &session, &limits);
}

std::shared_ptr<NsmFpgaStatus> NsmFpgaController::statusObject() const
{
    if (status)
    {
        return status;
    }
    return uuid.empty() ? nullptr : NsmFpgaStatus::get(uuid);
}

std::string NsmFpgaController::statusName() const
{
    const auto fpgaStatus = statusObject();
    return fpgaStatus ? fpgaStatus->getName() : std::string();
}

std::string NsmFpgaController::statusPath() const
{
    const auto fpgaStatus = statusObject();
    return fpgaStatus ? fpgaStatus->getInventoryPath() : std::string();
}

void NsmFpgaController::noteSession(enum nsm_fpga_session_state state)
{
    // Quarantined or uninitialized blocks every item until an operator acts
    const bool blocked = state == NSM_FPGA_SESSION_QUARANTINED ||
                         state == NSM_FPGA_SESSION_UNINITIALIZED;
    const auto fpgaStatus = statusObject();
    if (blocked && !sessionBlocked && fpgaStatus)
    {
        const std::string reason =
            state == NSM_FPGA_SESSION_QUARANTINED
                ? "management session quarantined, recover required"
                : "management session uninitialized, activate pending";
        lg2::error("FPGA session blocked: {NAME} {REASON}", "NAME",
                   fpgaStatus->getName(), "REASON", reason,
                   "REDFISH_MESSAGE_ID",
                   std::string("ResourceEvent.1.0.ResourceErrorsDetected"),
                   "REDFISH_MESSAGE_ARGS",
                   fpgaStatus->getInventoryPath() + "," + reason);
    }
    sessionBlocked = blocked;
}

requester::Coroutine NsmFpgaController::exchange(
    std::shared_ptr<NsmDevice> nsmDevice, const nsm_fpga_step& step,
    MmioRecord& reply, bool& lost, bool& noSession, std::string& failure)
{
    lost = false;
    noSession = false;
    const bool write = step.kind == NSM_FPGA_STEP_WRITE;
    Request request(mmioRequestSize + (write ? reply.size() : 0), 0);
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    int rc = write
                 ? encode_set_device_config_mmio_req(0, configurationType,
                                                     step.address, recordDwords,
                                                     step.record, msg)
                 : encode_get_device_config_mmio_req(
                       0, configurationType, step.address, recordDwords, msg);
    if (rc != NSM_SW_SUCCESS)
    {
        failure = "MMIO request encode failed";
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    std::shared_ptr<const nsm_msg> response;
    size_t responseLen = 0;
    rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), request, response,
                                      responseLen);
    if (rc != NSM_SW_SUCCESS || !response)
    {
        lost = true;
        failure = "transport failed rc=" + std::to_string(rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    uint8_t cc = NSM_ERROR;
    uint16_t reason = ERR_NULL;
    if (write)
    {
        rc = decode_set_device_config_v2_resp(response.get(), responseLen, &cc,
                                              &reason);
    }
    else
    {
        rc = decode_get_device_config_mmio_resp(response.get(), responseLen,
                                                recordDwords, &cc, &reason,
                                                reply.data());
    }
    if (rc != NSM_SW_SUCCESS)
    {
        // An undecodable answer to a SET leaves the publication unknown
        lost = write;
        failure = "MMIO response decode failed rc=" + std::to_string(rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    if (cc == NSM_ERROR && reason == NSM_FPGA_REASON_NO_SESSION)
    {
        noSession = true;
        failure = "FPGA reports no session";
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    if (cc != NSM_SUCCESS)
    {
        failure = "MMIO completion cc=" + std::to_string(cc) +
                  " reason=" + std::to_string(reason);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmFpgaController::drive(std::shared_ptr<NsmDevice> nsmDevice)
{
    if (!nsmDevice)
    {
        error = "no device";
        // coverity[missing_return]
        co_return NSM_SW_ERROR_NULL;
    }
    for (;;)
    {
        // Checked between frames, so nothing is in flight: past the deadline
        // the job stops and the grant stays for RELEASE while the lease holds;
        // once the lease has lapsed a RELEASE would be rejected, so fence.
        // Cleanup rows and control-only paths (no episode) are exempt.
        if (episodeStartUsec != 0 && !inCleanup &&
            nowUsec() - episodeStartUsec >= episodeDeadlineUsec)
        {
            error = "episode deadline exceeded";
            lg2::error(
                "FPGA episode deadline exceeded during {ITEM} after {N} exchanges",
                "ITEM", currentItem ? currentItem : "group rows", "N",
                exchanges);
            if (nowUsec() - acquiredAtUsec < leaseUsec)
            {
                nsm_fpga_run_abort(&run, error.c_str());
                // coverity[missing_return]
                co_return NSM_SW_ERROR;
            }
            sessionLost("episode deadline exceeded, lease lapsed, bank fenced",
                        true);
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
        }
        const nsm_fpga_step* step = nullptr;
        const auto action = nsm_fpga_run_next(&run, &step);
        if (action == NSM_FPGA_RUN_DONE)
        {
            // coverity[missing_return]
            co_return NSM_SW_SUCCESS;
        }
        if (action == NSM_FPGA_RUN_FAILED)
        {
            error = run.error;
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
        }
        if (action == NSM_FPGA_RUN_WAIT)
        {
            co_await common::Sleep(event, run.wait_us, common::NonPriority);
            continue;
        }
        MmioRecord reply{};
        bool lost = false;
        bool noSession = false;
        std::string failure;
        auto rc = co_await exchange(nsmDevice, *step, reply, lost, noSession,
                                    failure);
        exchanges++;
        if (rc != NSM_SW_SUCCESS && !lost && !noSession)
        {
            nsm_fpga_run_abort(&run, failure.c_str());
            continue;
        }
        nsm_fpga_run_reply(
            &run, step->kind == NSM_FPGA_STEP_READ ? reply.data() : nullptr,
            lost, noSession);
    }
}

void NsmFpgaController::settle()
{
    if (run.fence)
    {
        sessionLost(error, true);
    }
}

uint32_t NsmFpgaController::nextSession()
{
    if (++sessionCounter == 0 || sessionCounter > NSM_FPGA_MAX_SEQUENCE)
    {
        sessionCounter = 1;
    }
    return sessionCounter;
}

void NsmFpgaController::sessionLost(const std::string& why, bool fenced)
{
    if (open)
    {
        lg2::warning("FPGA session ended early: {WHY}", "WHY", why);
    }
    open = false;
    if (fenced)
    {
        // The bank is fenced: do not RELEASE and do not re-acquire until the
        // status word shows it available again, re-read only every backoff
        fencedUntilUsec = nowUsec() + fenceBackoffUsec;
    }
}

requester::Coroutine
    NsmFpgaController::control(std::shared_ptr<NsmDevice> nsmDevice,
                               uint8_t opcode, uint32_t argument)
{
    nsm_fpga_run_control(&run, opcode, argument);
    // coverity[missing_return]
    co_return co_await drive(nsmDevice);
}

std::string NsmFpgaController::epochPath = FPGA_EPOCH_PATH;

uint64_t NsmFpgaController::nextEpoch() const
{
    uint64_t epoch = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    uint64_t last = 0;
    std::ifstream in(epochPath);
    in >> last;
    epoch = std::max({epoch, authority.epoch + 1, last + 1});
    return epoch;
}

requester::Coroutine
    NsmFpgaController::recoverOwnLease(std::shared_ptr<NsmDevice> nsmDevice)
{
    const uint64_t previous = authority.epoch;
    const uint64_t epoch = nextEpoch();
    nsm_fpga_run_recover(&run, epoch);
    auto rc = co_await drive(nsmDevice);
    if (rc != NSM_SW_SUCCESS)
    {
        recoverHoldUntilUsec = nowUsec() + recoverHoldUsec;
        lg2::error(
            "FPGA bank recover refused for {NAME}: {ERROR}, left alone for {MINUTES} min",
            "NAME", statusName(), "ERROR", error, "MINUTES",
            recoverHoldUsec / usecPerSecond / 60, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    persistEpoch(epoch);
    nsm_fpga_run_status(&run);
    rc = co_await drive(nsmDevice);
    if (rc != NSM_SW_SUCCESS)
    {
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    authority = run.authority;
    lg2::warning(
        "FPGA bank {NAME} recovered after a lapsed lease: epoch {EPOCH} replaces {PREVIOUS}",
        "NAME", statusName(), "EPOCH", epoch, "PREVIOUS", previous, "EID",
        nsmDevice->getEid(), "REDFISH_MESSAGE_ID",
        std::string("ResourceEvent.1.0.ResourceStatusChangedOK"),
        "REDFISH_MESSAGE_ARGS", statusPath() + ",management session recovered");
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmFpgaController::activateBank(std::shared_ptr<NsmDevice> nsmDevice)
{
    const uint64_t epoch = nextEpoch();
    nsm_fpga_run_activate(&run, epoch);
    auto rc = co_await drive(nsmDevice);
    if (rc != NSM_SW_SUCCESS)
    {
        recoverHoldUntilUsec = nowUsec() + recoverHoldUsec;
        lg2::error(
            "FPGA bank activate refused for {NAME}: {ERROR}, left alone for {MINUTES} min",
            "NAME", statusName(), "ERROR", error, "MINUTES",
            recoverHoldUsec / usecPerSecond / 60, "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    persistEpoch(epoch);
    nsm_fpga_run_status(&run);
    rc = co_await drive(nsmDevice);
    if (rc != NSM_SW_SUCCESS)
    {
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    authority = run.authority;
    // a fresh authority means the core may have changed: check it again
    if (const auto fpgaStatus = statusObject())
    {
        fpgaStatus->requestRecheck();
    }
    lg2::info(
        "FPGA bank {NAME} activated after a controller reset or core load: epoch {EPOCH}",
        "NAME", statusName(), "EPOCH", epoch, "EID", nsmDevice->getEid());
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

void NsmFpgaController::persistEpoch(uint64_t epoch) const
{
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(epochPath).parent_path(), ec);
    std::ofstream out(epochPath, std::ios::trunc);
    out << epoch;
}

requester::Coroutine
    NsmFpgaController::openSession(std::shared_ptr<NsmDevice> nsmDevice)
{
    nsm_fpga_run_status(&run);
    auto rc = co_await drive(nsmDevice);
    if (rc != NSM_SW_SUCCESS)
    {
        noteSession(NSM_FPGA_SESSION_UNREACHABLE);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    authority = run.authority;
    // A bank nobody activated since its controller came up is activated
    // here; one that is initialized, owned or quarantined is never touched
    if (nsm_fpga_authority_activatable(&authority) &&
        nowUsec() >= recoverHoldUntilUsec)
    {
        co_await activateBank(nsmDevice);
    }
    // nsmd's own lapsed lease is recovered here, with no client on the bus;
    // a bank held by the host or faulted is never touched
    if (nsm_fpga_authority_recoverable(&authority) &&
        nowUsec() >= recoverHoldUntilUsec)
    {
        co_await recoverOwnLease(nsmDevice);
    }
    // The bank's own state, sampled before ACQUIRE makes nsmd the owner
    noteSession(nsm_fpga_session_classify(&authority));
    nsm_fpga_grant next{};
    next.session = nextSession();
    const bool acquirable = nsm_fpga_authority_acquirable(&authority);
    if (!acquirable ||
        nsm_fpga_grant_from_authority(&authority, next.session, &next.epoch,
                                      &next.generation) != NSM_SW_SUCCESS)
    {
        error =
            "bank not acquirable: flags=" + std::to_string(authority.flags) +
            " owner=" + std::to_string(authority.owner);
        // Back off for 60 s; the next read re-reads this status and re-fences
        // if the bank is still quarantined or owned, so ACQUIRE is not hammered
        fencedUntilUsec = nowUsec() + fenceBackoffUsec;
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    session.grant = next;
    session.control_sequence = 0;
    session.data_sequence = 0;
    rc = co_await control(nsmDevice, NSM_FPGA_CTRL_ACQUIRE, 0);
    if (rc != NSM_SW_SUCCESS)
    {
        // coverity[missing_return]
        co_return rc;
    }
    open = true;
    sessionDevice = nsmDevice;
    fencedUntilUsec = 0;
    acquiredAtUsec = nowUsec();
    lastActivityUsec = acquiredAtUsec;
    if (!watcherRunning)
    {
        idleWatcher(nsmDevice).detach();
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmFpgaController::readItem(std::shared_ptr<NsmDevice> nsmDevice,
                                const nsm_fpga_rule* rule, uint32_t& value)
{
    nsm_fpga_run_begin_item(&run);
    if (rule == nullptr)
    {
        error = "no rule";
        // coverity[missing_return]
        co_return NSM_SW_ERROR_NULL;
    }
    if (roundEnded)
    {
        // The group's setup check ended the round; every item takes its state
        run.end_group = true;
        run.group_state = roundState;
        run.group_health = roundHealth;
        error = "group ended by its setup check";
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    if (rule->command_count == 0)
    {
        // A row-less item publishes a word the group's setup rows captured
        uint32_t word = 0;
        if (!nsm_fpga_rule_word(rule, nullptr, 0, run.group, run.group_len,
                                &word))
        {
            error = "group buffer holds no word for the item";
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
        }
        run.last_word = word;
        run.any_read = true;
        value = word;
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    nsm_fpga_run_rows(&run, rule->commands, rule->command_count);
    currentItem = rule->name;
    uint8_t rc = co_await drive(nsmDevice);
    currentItem = nullptr;
    settle();
    const bool anyRead = run.any_read;
    if (run.wrote_any && rule->cleanup_count > 0 && open)
    {
        // Leave the station clean whatever happened; the first error stands
        const std::string firstError = error;
        nsm_fpga_run_rows(&run, rule->cleanup, rule->cleanup_count);
        inCleanup = true;
        auto cleanupRc = co_await drive(nsmDevice);
        inCleanup = false;
        settle();
        if (rc == NSM_SW_SUCCESS)
        {
            rc = cleanupRc;
        }
        else
        {
            error = firstError;
        }
    }
    if (rc != NSM_SW_SUCCESS)
    {
        // coverity[missing_return]
        co_return rc;
    }
    if (!anyRead)
    {
        error = "command sequence has no read";
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    // Rules without a capture publish the last word read
    nsm_fpga_run_default_capture(&run);
    value = run.last_word;
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmFpgaController::runGroup(std::shared_ptr<NsmDevice> nsmDevice,
                                const nsm_fpga_group* group, const char* target,
                                std::function<requester::Coroutine()> body)
{
    if (!nsmDevice)
    {
        error = "no device";
        // coverity[missing_return]
        co_return NSM_SW_ERROR_NULL;
    }
    auto round = [this, nsmDevice, group, body]() -> requester::Coroutine {
        nsm_fpga_run_begin_round(&run);
        roundEnded = false;
        roundState = 0;
        roundHealth = 0;
        int rc = NSM_SW_SUCCESS;
        bool setupStarted = false;
        if (group != nullptr && group->setup_count > 0)
        {
            nsm_fpga_run_rows(&run, group->setup, group->setup_count);
            rc = co_await drive(nsmDevice);
            settle();
            setupStarted = run.any_read || run.wrote_any;
            if (run.end_group)
            {
                // An ended group is a normal outcome: the items publish its
                // state and the round goes on to the cleanup rows
                roundEnded = true;
                roundState = run.group_state;
                roundHealth = run.group_health;
                rc = NSM_SW_SUCCESS;
            }
        }
        if (rc == NSM_SW_SUCCESS)
        {
            rc = co_await body();
        }
        if (group != nullptr && group->cleanup_count > 0 && setupStarted &&
            open)
        {
            // Restore what the setup rows changed, whatever the items did
            const std::string firstError = error;
            nsm_fpga_run_begin_item(&run);
            nsm_fpga_run_rows(&run, group->cleanup, group->cleanup_count);
            inCleanup = true;
            auto cleanupRc = co_await drive(nsmDevice);
            inCleanup = false;
            settle();
            if (rc == NSM_SW_SUCCESS)
            {
                rc = cleanupRc;
            }
            else
            {
                error = firstError;
            }
        }
        // coverity[missing_return]
        co_return rc;
    };
    // coverity[missing_return]
    co_return co_await episode(nsmDevice, target, round);
}

requester::Coroutine
    NsmFpgaController::release(std::shared_ptr<NsmDevice> nsmDevice)
{
    if (!open || !nsmDevice)
    {
        open = false;
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    // Serialize with reads so a poll in flight finishes before the grant drops
    co_await ioLock.acquire(nsmDevice->getEid());
    busy = true;
    auto rc = co_await control(nsmDevice, NSM_FPGA_CTRL_RELEASE,
                               session.data_sequence);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("FPGA session release failed: {ERROR}", "ERROR", error,
                   "EID", nsmDevice->getEid());
    }
    open = false;
    busy = false;
    ioLock.release();
    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine
    NsmFpgaController::idleWatcher(std::weak_ptr<NsmDevice> weakDevice)
{
#ifdef COVERAGE_DISABLE_COROUTINES
    // Inline coroutines cannot wait for idle; the session is released directly
    (void)weakDevice;
    co_return NSM_SW_SUCCESS;
#else
    auto self = shared_from_this();
    watcherRunning = true;
    while (open)
    {
        co_await common::Sleep(event, idleReleaseUsec, common::NonPriority);
        if (!open)
        {
            break;
        }
        if (busy || nowUsec() - lastActivityUsec < idleReleaseUsec)
        {
            continue;
        }
        auto nsmDevice = weakDevice.lock();
        if (!nsmDevice)
        {
            open = false;
            break;
        }
        co_await release(nsmDevice);
    }
    watcherRunning = false;
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
#endif
}

requester::Coroutine
    NsmFpgaController::read(std::shared_ptr<NsmDevice> nsmDevice,
                            const nsm_fpga_rule* rule, uint32_t& value)
{
    if (rule == nullptr)
    {
        error = "no rule";
        // coverity[missing_return]
        co_return NSM_SW_ERROR_NULL;
    }
    auto body = [this, nsmDevice, rule, &value]() -> requester::Coroutine {
        // coverity[missing_return]
        co_return co_await readItem(nsmDevice, rule, value);
    };
    // coverity[missing_return]
    co_return co_await runGroup(nsmDevice, nullptr, rule->name, body);
}

requester::Coroutine
    NsmFpgaController::episode(std::shared_ptr<NsmDevice> nsmDevice,
                               const char* target,
                               std::function<requester::Coroutine()> body)
{
    // Hold the controller lock for the whole episode; a poll that arrives while
    // another episode or the idle RELEASE is in flight waits here rather than
    // failing the round
    co_await ioLock.acquire(nsmDevice->getEid());
    exchanges = 0;
    // The guards are decided under the lock so an episode or the shutdown
    // drain that ran meanwhile is seen; these rounds fail quietly
    const uint64_t now = nowUsec();
    const char* skipReason = nullptr;
    const auto fpgaStatus = statusObject();
    if (shuttingDown)
    {
        skipReason = "shutting down";
    }
    else if (!fpgaStatus || !fpgaStatus->enabled())
    {
        skipReason = "FPGA status not enabled";
        if (fpgaStatus && !fpgaStatus->isOnline())
        {
            // An offline FPGA holds no session nsmd can vouch for
            noteSession(NSM_FPGA_SESSION_UNREACHABLE);
        }
    }
    else if (!open && now < fencedUntilUsec)
    {
        // Fenced or quarantined: skip this round until the 60 s backoff expires
        skipReason = "bank fenced, backing off";
    }
    else if (!open && openFailedAtUsec != 0 &&
             now - openFailedAtUsec < openRetryUsec)
    {
        skipReason = "bank refused a session moments ago";
    }
    if (skipReason != nullptr)
    {
        error = skipReason;
        ioLock.release();
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    busy = true;
    error.clear();
    int rc = NSM_SW_SUCCESS;
    if (!open)
    {
        rc = co_await openSession(nsmDevice);
        openFailedAtUsec = rc == NSM_SW_SUCCESS ? 0 : now;
    }
    else if (nowUsec() - acquiredAtUsec >= renewAfterUsec)
    {
        // Renew an aged lease before any data op so the episode cannot outlive
        // it
        rc = co_await control(nsmDevice, NSM_FPGA_CTRL_RENEW, 0);
        if (rc != NSM_SW_SUCCESS)
        {
            sessionLost("renew failed: " + error, true);
        }
        else
        {
            acquiredAtUsec = nowUsec();
        }
    }
    if (rc == NSM_SW_SUCCESS)
    {
        // Start the episode clock once the session is held so acquire latency
        // never counts against the deadline
        episodeStartUsec = nowUsec();
        rc = co_await body();
    }
    lastActivityUsec = nowUsec();
    episodeStartUsec = 0;
    busy = false;
    ioLock.release();
    // Once per read target and change of outcome, not once per round
    auto lastLogged = lastLoggedErrors.find(target);
    if (rc == NSM_SW_SUCCESS)
    {
        if (lastLogged != lastLoggedErrors.end())
        {
            lg2::info("FPGA read of {TARGET} recovered", "TARGET", target,
                      "EID", nsmDevice->getEid());
            lastLoggedErrors.erase(lastLogged);
        }
    }
    else if (lastLogged == lastLoggedErrors.end() ||
             lastLogged->second != error)
    {
        lg2::error("FPGA read of {TARGET} failed: {ERROR}", "ERROR", error,
                   "TARGET", target, "N", exchanges, "OPEN", open, "EID",
                   nsmDevice->getEid());
        lastLoggedErrors[target] = error;
    }
    // coverity[missing_return]
    co_return rc == NSM_SW_SUCCESS ? NSM_SW_SUCCESS : NSM_SW_ERROR;
}

std::shared_ptr<NsmFpgaController>
    NsmFpgaController::getOrCreate(const uuid_t& uuid,
                                   std::shared_ptr<NsmFpgaStatus> status)
{
    // The first controller takes over the shutdown signals for the drain
    static bool handlerInstalled = false;
    if (!handlerInstalled)
    {
        installShutdownHandler();
        handlerInstalled = true;
    }
    auto& controllers = registry();
    auto it = controllers.find(uuid);
    if (it != controllers.end())
    {
        return it->second;
    }
    auto controller = std::make_shared<NsmFpgaController>(std::move(status));
    controller->uuid = uuid;
    controllers.emplace(uuid, controller);
    return controller;
}

std::shared_ptr<NsmFpgaController> NsmFpgaController::get(const uuid_t& uuid)
{
    auto& controllers = registry();
    auto it = controllers.find(uuid);
    return it == controllers.end() ? nullptr : it->second;
}

void NsmFpgaController::clear()
{
    registry().clear();
}

requester::Coroutine
    NsmFpgaController::shutdownRelease(common::Event& loopEvent)
{
    static bool started = false;
    if (started)
    {
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    started = true;
    // From here on no controller opens a new session; in-flight reads finish
    shuttingDown = true;
    for (auto& entry : registry())
    {
        auto& controller = entry.second;
        if (!controller || !controller->open)
        {
            continue;
        }
        auto device = controller->sessionDevice.lock();
        if (!device)
        {
            controller->open = false;
            continue;
        }
        // Let an in-flight episode finish before releasing the grant
        uint32_t waits = 0;
        while (controller->busy && waits++ < drainPollLimit)
        {
            co_await common::Sleep(loopEvent, drainPollUsec,
                                   common::NonPriority);
        }
        if (controller->busy)
        {
            // The drain stays bounded: a stalled exchange is left to the
            // authority rather than holding up the exit
            lg2::error("FPGA session still busy at shutdown, not released",
                       "EID", device->getEid());
            continue;
        }
        auto rc = co_await controller->release(device);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "FPGA session release on shutdown failed, the bank may need recover before the next start: {ERROR}",
                "ERROR", controller->lastError(), "EID", device->getEid());
        }
        else
        {
            lg2::info("FPGA session released on shutdown", "EID",
                      device->getEid());
        }
    }
    lg2::info("FPGA shutdown drain complete");
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

} // namespace nsm
