// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "fakeFpga.hpp"

#include <unistd.h>

#include <filesystem>
#include <fstream>

using namespace nsm;
using namespace ::testing;

namespace
{

struct NsmFpgaControllerTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    FakeFpga fpga;
    std::vector<std::weak_ptr<NsmFpgaStatus>> statuses;

    NsmFpgaControllerTest() : SensorManagerTest(devices)
    {
        NsmFpgaController::epochPath =
            (std::filesystem::temp_directory_path() /
             ("fpga-bank-epoch-" + std::to_string(getpid())))
                .string();
        std::filesystem::remove(NsmFpgaController::epochPath);
    }

    ~NsmFpgaControllerTest()
    {
        NsmFpgaController::clear();
        NsmFpgaStatus::clear();
        cleanupDeviceSensors(devices);
        drainFpgaWatchers(statuses);
    }

    std::shared_ptr<NsmFpgaStatus> makeStatus(bool enabled)
    {
        auto status = std::make_shared<NsmFpgaStatus>(
            "HGX_FPGA_0",
            "/xyz/openbmc_project/inventory/system/processors/FPGA_0");
        status->setOnline(enabled);
        status->setTypeCheck(enabled);
        status->setDigestCheck(enabled);
        statuses.push_back(status);
        return status;
    }

    std::shared_ptr<MockNsmDevice> makeDevice()
    {
        auto device = std::make_shared<MockNsmDevice>(3, 0, "MCTP_EID", "17",
                                                      0);
        ON_CALL(*device, sensorIO(_, _, _, _, _))
            .WillByDefault(Invoke(&fpga, &FakeFpga::io));
        return device;
    }

    // The state word on station 31, CSR 0x05000400
    const nsm_fpga_rule* calib()
    {
        return &stateRule;
    }
};

TEST_F(NsmFpgaControllerTest, SessionCounterIsNonzeroAndIncreases)
{
    NsmFpgaController controller(makeStatus(true));
    EXPECT_EQ(controller.nextSession(), 1u);
    EXPECT_EQ(controller.nextSession(), 2u);
    EXPECT_EQ(nsm_fpga_map_p0()->window.completion_read, 0x1060u);
    EXPECT_FALSE(controller.sessionOpen());
}

TEST_F(NsmFpgaControllerTest, RegistryReturnsOneControllerPerUuid)
{
    auto status = makeStatus(true);
    auto a = NsmFpgaController::getOrCreate("STATIC:3:0:MCTP_EID:17", status);
    auto b = NsmFpgaController::getOrCreate("STATIC:3:0:MCTP_EID:17", status);
    EXPECT_EQ(a, b);
    EXPECT_EQ(NsmFpgaController::get("STATIC:3:1:MCTP_EID:19"), nullptr);
}

TEST_F(NsmFpgaControllerTest, FirstReadOpensTheSessionAndReleaseEndsIt)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto device = makeDevice();
    uint32_t value = 0;
    auto coro = controller->read(device, calib(), value);
    EXPECT_EQ(value, 1u);
    // authority status 1, ACQUIRE 2, word 2; the session stays open
    EXPECT_EQ(fpga.exchanges, 5u);
    EXPECT_EQ(controller->lastExchanges(), 5u);
    EXPECT_TRUE(controller->sessionOpen());
    EXPECT_EQ(fpga.acquires, 1u);
    EXPECT_EQ(fpga.releases, 0u);
    // The word record named station 31 and the CSR
    EXPECT_EQ(fpga.header[5], 31);
    EXPECT_EQ(fpga.wordAddress, 0x05000400u);

    auto done = controller->release(device);
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_EQ(fpga.exchanges, 7u);
    EXPECT_EQ(fpga.releases, 1u);
    EXPECT_EQ(fpga.control[4], NSM_FPGA_CTRL_RELEASE);
    EXPECT_EQ(fpga.control[28], 1);
    EXPECT_EQ(fpga.control[24], 2);
}

TEST_F(NsmFpgaControllerTest, ReadsOfOneRoundShareTheSession)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto device = makeDevice();
    uint32_t value = 0;
    auto a = controller->read(device, calib(), value);
    auto b = controller->read(device, calib(), value);
    EXPECT_EQ(fpga.exchanges, 7u);
    EXPECT_EQ(fpga.acquires, 1u);
    // Data sequences count up inside the session
    EXPECT_EQ(fpga.header[24], 2);
    auto done = controller->release(device);
    EXPECT_EQ(fpga.control[28], 2);
}

TEST_F(NsmFpgaControllerTest, GrantComesFromTheAuthorityStatus)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    fpga.authorityEpoch = 0x1122334455667788ULL;
    fpga.authorityGeneration = 41;
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), calib(), value);
    EXPECT_EQ(controller->lastAuthority().epoch, 0x1122334455667788ULL);
    EXPECT_EQ(controller->lastAuthority().generation, 41u);
    EXPECT_EQ(controller->currentGrant().generation, 42u);
    // The records carried the activated epoch, session 1 and generation 42
    EXPECT_EQ(getLe(fpga.header.data() + 8, 8), 0x1122334455667788ULL);
    EXPECT_EQ(getLe(fpga.header.data() + 16, 4), 1u);
    EXPECT_EQ(getLe(fpga.header.data() + 20, 4), 42u);
}

TEST_F(NsmFpgaControllerTest, UnavailableBankIsNeverAcquired)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    fpga.authorityFlags |= NSM_FPGA_AUTH_QUARANTINED;
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), calib(), value);
    // Only the status read went out, no record was written
    EXPECT_EQ(fpga.exchanges, 1u);
    EXPECT_EQ(fpga.acquires, 0u);
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_THAT(controller->lastError(), HasSubstr("bank not acquirable"));
    // A blocked bank is internal, the Processor stays Enabled and OK
    EXPECT_TRUE(status->enabled());
    EXPECT_EQ(status->healthIntf->health(), FpgaHealthIntf::HealthType::OK);

    // Within the retry window a later read fails without a status read
    auto quiet = controller->read(makeDevice(), calib(), value);
    EXPECT_EQ(fpga.exchanges, 1u);

    // The bank comes back: the next round acquires and clears the warning
    controller->openFailedAtUsec = 0;
    controller->fencedUntilUsec = 0;
    fpga.authorityFlags = NSM_FPGA_AUTH_INITIALIZED | NSM_FPGA_AUTH_AVAILABLE;
    fpga.authorityEpoch = 0x68e7a1c0;
    auto back = controller->read(makeDevice(), calib(), value);
    EXPECT_EQ(fpga.acquires, 1u);
    EXPECT_EQ(status->healthIntf->health(), FpgaHealthIntf::HealthType::OK);
}

TEST_F(NsmFpgaControllerTest, OwnLapsedLeaseIsRecoveredThenAcquired)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    // The bank as a lapsed nsmd lease leaves it: quarantined, quiescent,
    // released, owner still nsmd
    fpga.authorityFlags = NSM_FPGA_AUTH_INITIALIZED |
                          NSM_FPGA_AUTH_QUARANTINED | NSM_FPGA_AUTH_RING_ARMED |
                          NSM_FPGA_AUTH_QUIESCENT | NSM_FPGA_AUTH_RELEASED;
    fpga.authorityOwner = NSM_FPGA_OWNER_NSM;
    const uint64_t stale = fpga.authorityEpoch;
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), calib(), value);
    // status, RECOVER and its receipt, status again, ACQUIRE and its
    // receipt, then the two frames of the word read
    EXPECT_EQ(fpga.recovers, 1u);
    EXPECT_EQ(fpga.acquires, 1u);
    EXPECT_EQ(fpga.exchanges, 8u);
    EXPECT_TRUE(controller->sessionOpen());
    EXPECT_EQ(controller->lastError(), "");
    EXPECT_GT(fpga.authorityEpoch, stale);
    EXPECT_EQ(controller->currentGrant().epoch, fpga.authorityEpoch);
    // The epoch is persisted so the next one stays above it
    uint64_t persisted = 0;
    std::ifstream in(NsmFpgaController::epochPath);
    in >> persisted;
    EXPECT_EQ(persisted, fpga.authorityEpoch);
    EXPECT_TRUE(status->enabled());
}

TEST_F(NsmFpgaControllerTest, RefusedRecoverHoldsOffAndHostLeaseIsLeftAlone)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    fpga.authorityFlags = NSM_FPGA_AUTH_INITIALIZED |
                          NSM_FPGA_AUTH_QUARANTINED | NSM_FPGA_AUTH_RING_ARMED |
                          NSM_FPGA_AUTH_QUIESCENT | NSM_FPGA_AUTH_RELEASED;
    fpga.authorityOwner = NSM_FPGA_OWNER_NSM;
    fpga.recoverResult = NSM_FPGA_CTRL_DENIED;
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), calib(), value);
    EXPECT_EQ(fpga.recovers, 1u);
    EXPECT_EQ(fpga.acquires, 0u);
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_THAT(controller->lastError(), HasSubstr("bank not acquirable"));
    // Within the hold the status is re-read but no RECOVER is sent again
    controller->openFailedAtUsec = 0;
    controller->fencedUntilUsec = 0;
    auto again = controller->read(makeDevice(), calib(), value);
    EXPECT_EQ(fpga.recovers, 1u);
    EXPECT_EQ(fpga.exchanges, 4u);

    // A quarantined bank the host owned is never recovered by nsmd
    fpga.recoverResult = NSM_FPGA_CTRL_OK;
    fpga.authorityOwner = NSM_FPGA_OWNER_NATIVE;
    controller->recoverHoldUntilUsec = 0;
    controller->openFailedAtUsec = 0;
    controller->fencedUntilUsec = 0;
    auto host = controller->read(makeDevice(), calib(), value);
    EXPECT_EQ(fpga.recovers, 1u);
    EXPECT_EQ(fpga.acquires, 0u);
    EXPECT_EQ(fpga.exchanges, 5u);
}

TEST_F(NsmFpgaControllerTest, RejectedAcquireIsReportedByName)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    fpga.acquireResult = NSM_FPGA_CTRL_STALE;
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), calib(), value);
    EXPECT_EQ(fpga.exchanges, 3u);
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_THAT(controller->lastError(), HasSubstr("STALE"));

    fpga.acquireResult = NSM_FPGA_CTRL_BUSY;
    // Advance past the refused-acquire retry window before the next attempt.
    controller->openFailedAtUsec = 0;
    auto again = controller->read(makeDevice(), calib(), value);
    EXPECT_EQ(fpga.exchanges, 6u);
    EXPECT_THAT(controller->lastError(), HasSubstr("BUSY"));
}

TEST_F(NsmFpgaControllerTest, MailboxWritesThenPollsStatusThenReadsData)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    // A mailbox read: write the command, poll a done bit, read the result
    static const nsm_fpga_command mailboxCmds[] = {
        {.op = NSM_FPGA_OP_WRITE32,
         .station = 29,
         .u = {.csr = {0x05000420ULL, 0xabc}}},
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .poll = NSM_FPGA_POLL_SET,
         .poll_mask = 0x1,
         .u = {.csr = {0x0500045cULL, 0}}},
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .u = {.csr = {0x05000800ULL, 0}}},
    };
    static const nsm_fpga_rule mailboxRule{
        .name = "Mailbox_Test",
        .commands = mailboxCmds,
        .command_count = 3,
        .kind = NSM_FPGA_RULE_STATE,
        .group = "Test_Group",
        .states = testStates,
        .state_count = 5,
    };
    fpga.words[0x0500045cULL] = 0x1; // response_status: done bit set
    fpga.words[0x05000800ULL] = 42;  // data word
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), &mailboxRule, value);
    EXPECT_EQ(value, 42u); // the last read is the result
    EXPECT_EQ(fpga.acquires, 1u);
    EXPECT_TRUE(controller->sessionOpen());
}

TEST_F(NsmFpgaControllerTest, AckClearWritesBackThePolledWordWithTheBitClear)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    // A mailbox read whose cleanup acknowledges the polled status word
    static const nsm_fpga_command rows[] = {
        {.op = NSM_FPGA_OP_WRITE32,
         .station = 29,
         .u = {.csr = {0x0500043cULL, 0x1}}},
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .poll = NSM_FPGA_POLL_SET,
         .poll_mask = 0x1,
         .u = {.csr = {0x0500045cULL, 0}}},
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .capture = NSM_FPGA_BUF_ITEM,
         .u = {.csr = {0x05000800ULL, 0}}}};
    static const nsm_fpga_command ack[] = {{.op = NSM_FPGA_OP_ACK_CLEAR,
                                            .station = 29,
                                            .poll_mask = 0x1,
                                            .u = {.csr = {0x0500045cULL, 0}}}};
    static const nsm_fpga_rule rule{.name = "Acked",
                                    .commands = rows,
                                    .command_count = 3,
                                    .cleanup = ack,
                                    .cleanup_count = 1,
                                    .kind = NSM_FPGA_RULE_STATE,
                                    .group = "Test_Group",
                                    .states = testStates,
                                    .state_count = 5};
    // Idle status; the command write makes it ready with another flag set
    fpga.words[0x0500045cULL] = 0x0;
    fpga.words[0x05000800ULL] = 0x274;
    fpga.onWrite = [this](uint64_t address, uint32_t) {
        if (address == 0x0500043cULL)
        {
            fpga.words[0x0500045cULL] = 0x41;
        }
    };
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), &rule, value);
    EXPECT_EQ(value, 0x274u);
    ASSERT_EQ(controller->lastCapture().size(), 4u);
    EXPECT_EQ(controller->lastCapture()[0], 0x74u);
    // The cleanup wrote the polled word back with only the ready bit clear
    EXPECT_EQ(fpga.words[0x0500045cULL], 0x40u);
    EXPECT_TRUE(controller->sessionOpen());
}

TEST_F(NsmFpgaControllerTest, PendingCompletionsAreReadAgain)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    fpga.pendingCompletions = 2;
    fpga.words[0x05000400ULL] = 2;
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), calib(), value);
    EXPECT_EQ(fpga.exchanges, 7u);
    EXPECT_EQ(value, 2u);
}

TEST_F(NsmFpgaControllerTest, CompletionPendingForeverStopsTheRead)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    fpga.pendingCompletions = 100;
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), calib(), value);
    EXPECT_EQ(fpga.exchanges, 5u + NsmFpgaController::maxPendingRepeats);
    EXPECT_THAT(controller->lastError(), HasSubstr("pending"));
}

TEST_F(NsmFpgaControllerTest, LostAnswerToAWriteDropsTheSession)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    // Exchange 4 is the word record publish
    fpga.loseExchange = 4;
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), calib(), value);
    // A lost reply drops the session and retries next round, no latching fault
    EXPECT_TRUE(status->enabled());
    EXPECT_FALSE(controller->sessionOpen());
}

TEST_F(NsmFpgaControllerTest, LostAnswerToAReadIsNotAmbiguous)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    // Exchange 3 is the ACQUIRE receipt read; the publish itself was acked
    fpga.loseExchange = 3;
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), calib(), value);
    EXPECT_FALSE(controller->sessionOpen());
}

TEST_F(NsmFpgaControllerTest, NoSessionCompletionFencesTheSession)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    // Exchange 5 is the completion read of the first word
    fpga.noSessionExchange = 5;
    auto device = makeDevice();
    uint32_t value = 0;
    auto coro = controller->read(device, calib(), value);
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_EQ(controller->lastError(), "FPGA reports no session");
    // A lost session fences the bank: the next read backs off, no re-acquire
    auto fenced = controller->read(device, calib(), value);
    EXPECT_EQ(fpga.exchanges, 5u);
    EXPECT_EQ(fpga.acquires, 1u);
    // Once the backoff clears, the next read acquires again with the next
    // generation
    controller->fencedUntilUsec = 0;
    auto again = controller->read(device, calib(), value);
    EXPECT_EQ(fpga.exchanges, 10u);
    EXPECT_EQ(fpga.acquires, 2u);
    EXPECT_EQ(getLe(fpga.control.data() + 16, 4), 2u);
}

TEST_F(NsmFpgaControllerTest, RenewOnceTheLeaseHasAged)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto device = makeDevice();
    uint32_t value = 0;
    auto coro = controller->read(device, calib(), value);
    EXPECT_EQ(fpga.renews, 0u);
    controller->acquiredAtUsec = 0;
    auto again = controller->read(device, calib(), value);
    EXPECT_EQ(fpga.renews, 1u);
    EXPECT_EQ(fpga.exchanges, 5u + 2u + 2u);
    EXPECT_TRUE(controller->sessionOpen());
}

TEST_F(NsmFpgaControllerTest, IllegalRowIsRefusedBeforeTheWire)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    static const nsm_fpga_command misaligned[] = {
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .u = {.csr = {0x05000401ULL, 0}}}};
    static const nsm_fpga_rule rule{.name = "Misaligned",
                                    .commands = misaligned,
                                    .command_count = 1,
                                    .kind = NSM_FPGA_RULE_STATE,
                                    .group = "Test_Group",
                                    .states = testStates,
                                    .state_count = 5};
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), &rule, value);
    // The session opened (status, ACQUIRE, receipt) but no word went out
    EXPECT_EQ(fpga.exchanges, 3u);
    EXPECT_THAT(controller->lastError(), HasSubstr("blocked"));
    EXPECT_TRUE(controller->sessionOpen());
}

TEST_F(NsmFpgaControllerTest, FaultedCompletionFencesWithoutRelease)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    fpga.faultCompletions = 1;
    auto device = makeDevice();
    uint32_t value = 0;
    auto coro = controller->read(device, calib(), value);
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_EQ(fpga.releases, 0u);
    EXPECT_THAT(controller->lastError(), HasSubstr("faulted"));
    EXPECT_NE(controller->fencedUntilUsec, 0u);
    // Fenced: the next read backs off without touching the FPGA
    const auto before = fpga.exchanges;
    auto fenced = controller->read(device, calib(), value);
    EXPECT_EQ(fpga.exchanges, before);
    EXPECT_EQ(fpga.acquires, 1u);
}

TEST_F(NsmFpgaControllerTest, CleanupRowsRunOnlyAfterAWrite)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    static const nsm_fpga_command cleanup[] = {
        {.op = NSM_FPGA_OP_WRITE32,
         .station = 29,
         .u = {.csr = {0x05000438ULL, 0x55}}}};
    // A read-only precheck that fails runs no cleanup
    static const nsm_fpga_command precheck[] = {
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .u = {.csr = {0x05000400ULL, 0}},
         .expect = {.mask = 0x1, .value = 0}}};
    static const nsm_fpga_rule precheckRule{.name = "Precheck",
                                            .commands = precheck,
                                            .command_count = 1,
                                            .cleanup = cleanup,
                                            .cleanup_count = 1,
                                            .kind = NSM_FPGA_RULE_STATE,
                                            .group = "Test_Group",
                                            .states = testStates,
                                            .state_count = 5};
    auto device = makeDevice();
    uint32_t value = 0;
    auto first = controller->read(device, &precheckRule, value);
    EXPECT_EQ(fpga.words.count(0x05000438ULL), 0u);
    EXPECT_THAT(controller->lastError(), HasSubstr("expected state"));
    // After a write went out the same failure runs the cleanup row
    static const nsm_fpga_command afterWrite[] = {
        {.op = NSM_FPGA_OP_WRITE32,
         .station = 29,
         .u = {.csr = {0x05000420ULL, 0xabc}}},
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .u = {.csr = {0x05000400ULL, 0}},
         .expect = {.mask = 0x1, .value = 0}}};
    static const nsm_fpga_rule writeRule{.name = "AfterWrite",
                                         .commands = afterWrite,
                                         .command_count = 2,
                                         .cleanup = cleanup,
                                         .cleanup_count = 1,
                                         .kind = NSM_FPGA_RULE_STATE,
                                         .group = "Test_Group",
                                         .states = testStates,
                                         .state_count = 5};
    auto second = controller->read(device, &writeRule, value);
    EXPECT_EQ(fpga.words[0x05000438ULL], 0x55u);
    EXPECT_THAT(controller->lastError(), HasSubstr("expected state"));
    EXPECT_TRUE(controller->sessionOpen());
}

TEST_F(NsmFpgaControllerTest, PopDrainsUntilTheStatusShowsEmpty)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    static const nsm_fpga_command rows[] = {
        {.op = NSM_FPGA_OP_POP,
         .station = 29,
         .poll_mask = 0x4,
         .u = {.pop = {0x0500045cULL, 0x05000800ULL}},
         .limit = 8},
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .u = {.csr = {0x05000400ULL, 0}}}};
    static const nsm_fpga_rule rule{.name = "Drain",
                                    .commands = rows,
                                    .command_count = 2,
                                    .kind = NSM_FPGA_RULE_STATE,
                                    .group = "Test_Group"};
    fpga.words[0x0500045cULL] = 0x4;
    size_t pops = 0;
    fpga.onRead = [this, &pops](uint64_t address) {
        if (address == 0x05000800ULL && ++pops == 2)
        {
            fpga.words[0x0500045cULL] = 0x0;
        }
    };
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), &rule, value);
    EXPECT_EQ(pops, 2u);
    // open 3, two status and data pairs 8, the empty status 2, the read 2
    EXPECT_EQ(fpga.exchanges, 15u);
    EXPECT_EQ(value, 1u);
}

TEST_F(NsmFpgaControllerTest, RunIfSkipsRowsWhoseConditionFails)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    static const nsm_fpga_command rows[] = {
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .capture = NSM_FPGA_BUF_ITEM,
         .u = {.csr = {0x05000400ULL, 0}}},
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .u = {.csr = {0x05000420ULL, 0}},
         .run_if = {.buffer = NSM_FPGA_BUF_ITEM,
                    .op = NSM_FPGA_CMP_EQ,
                    .width = 1,
                    .offset = 0,
                    .mask = 0xff,
                    .value = 2}},
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .u = {.csr = {0x05000438ULL, 0}}}};
    static const nsm_fpga_rule rule{.name = "Conditional",
                                    .commands = rows,
                                    .command_count = 3,
                                    .kind = NSM_FPGA_RULE_STATE,
                                    .group = "Test_Group"};
    fpga.words[0x05000438ULL] = 7;
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), &rule, value);
    // The calibration word is 1, so the row gated on 2 never went out
    EXPECT_EQ(fpga.exchanges, 7u);
    EXPECT_EQ(value, 7u);
    EXPECT_EQ(controller->lastCapture().size(), 4u);
}

TEST_F(NsmFpgaControllerTest, EndGroupReportsTheStateWithoutFencing)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    static const nsm_fpga_command rows[] = {
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .u = {.csr = {0x05000400ULL, 0}},
         .expect = {.mask = 0x7, .value = 0},
         .on_expect_fail = NSM_FPGA_END_GROUP,
         .group_state = NSM_FPGA_STATE_ABSENT}};
    static const nsm_fpga_rule rule{.name = "Presence",
                                    .commands = rows,
                                    .command_count = 1,
                                    .kind = NSM_FPGA_RULE_STATE,
                                    .group = "Test_Group",
                                    .states = testStates,
                                    .state_count = 5};
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), &rule, value);
    EXPECT_TRUE(controller->lastEndedGroup());
    EXPECT_EQ(controller->lastGroupState(), NSM_FPGA_STATE_ABSENT);
    EXPECT_EQ(controller->lastGroupHealth(), NSM_FPGA_HEALTH_OK);
    EXPECT_TRUE(controller->sessionOpen());
}

TEST_F(NsmFpgaControllerTest, ByteNackFailsTheItemWithoutFencing)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    static const nsm_fpga_command rows[] = {
        {.op = NSM_FPGA_OP_BYTE,
         .station = 1,
         .u = {.i2c = {0x50, 0x00, NSM_FPGA_I2C_START}}}};
    static const nsm_fpga_rule rule{.name = "Module",
                                    .commands = rows,
                                    .command_count = 1,
                                    .kind = NSM_FPGA_RULE_STATE,
                                    .group = "Test_Group",
                                    .states = testStates,
                                    .state_count = 5};
    fpga.nackBytes = true;
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), &rule, value);
    EXPECT_THAT(controller->lastError(), HasSubstr("nack"));
    EXPECT_TRUE(controller->sessionOpen());
    EXPECT_EQ(fpga.releases, 0u);
    EXPECT_EQ(controller->fencedUntilUsec, 0u);
}

TEST_F(NsmFpgaControllerTest, ByteReadCapturesTheDataByte)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    static const nsm_fpga_command rows[] = {
        {.op = NSM_FPGA_OP_BYTE,
         .station = 1,
         .u = {.i2c = {0x50, 0x0e, NSM_FPGA_I2C_START}}},
        {.op = NSM_FPGA_OP_BYTE,
         .station = 1,
         .capture = NSM_FPGA_BUF_ITEM,
         .u = {.i2c = {0x50, 0x00,
                       NSM_FPGA_I2C_READ | NSM_FPGA_I2C_STOP |
                           NSM_FPGA_I2C_READ_NACK}}}};
    static const nsm_fpga_rule rule{.name = "Register",
                                    .commands = rows,
                                    .command_count = 2,
                                    .kind = NSM_FPGA_RULE_STATE,
                                    .group = "Test_Group",
                                    .states = testStates,
                                    .state_count = 5};
    fpga.byteData = 0x5a;
    uint32_t value = 0;
    auto coro = controller->read(makeDevice(), &rule, value);
    EXPECT_EQ(value, 0x5au);
    ASSERT_EQ(controller->lastCapture().size(), 1u);
    EXPECT_EQ(controller->lastCapture()[0], 0x5au);
    // open 3, each byte op is a header write and a completion read
    EXPECT_EQ(fpga.exchanges, 7u);
}

TEST_F(NsmFpgaControllerTest, DeadlineBetweenFramesStopsTheJobAndKeepsTheGrant)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto device = makeDevice();
    // The budget is gone before the first data frame; the lease still holds
    controller->episodeDeadlineUsec = 0;
    uint32_t value = 0;
    auto coro = controller->read(device, calib(), value);
    EXPECT_EQ(controller->lastError(), "episode deadline exceeded");
    // authority status 1, ACQUIRE 2, no word; the grant is kept, not fenced
    EXPECT_EQ(fpga.exchanges, 3u);
    EXPECT_TRUE(controller->sessionOpen());
    EXPECT_EQ(controller->fencedUntilUsec, 0u);
    auto released = controller->release(device);
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_EQ(fpga.releases, 1u);
    // With a budget again the next read is a normal one
    controller->episodeDeadlineUsec = 4 * NsmFpgaController::usecPerSecond;
    auto again = controller->read(device, calib(), value);
    EXPECT_EQ(value, 1u);
    EXPECT_EQ(fpga.acquires, 2u);
}

TEST_F(NsmFpgaControllerTest, DeadlineAfterTheLeaseLapsedFences)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto device = makeDevice();
    controller->episodeDeadlineUsec = 0;
    controller->leaseUsec = 0;
    uint32_t value = 0;
    auto coro = controller->read(device, calib(), value);
    // A RELEASE would be rejected now: the bank is left for the authority
    EXPECT_EQ(fpga.exchanges, 3u);
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_NE(controller->fencedUntilUsec, 0u);
    EXPECT_EQ(fpga.releases, 0u);
}

} // namespace
