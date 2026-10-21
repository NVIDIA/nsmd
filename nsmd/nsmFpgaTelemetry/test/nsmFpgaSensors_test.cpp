// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "nsmFpgaSensorsFixture.hpp"

#include <cmath>

TEST_F(NsmFpgaSensorsTest, SessionsAreNonzeroAndIncrease)
{
    NsmFpgaController controller(makeStatus(true));
    EXPECT_EQ(controller.nextSession(), 1u);
    EXPECT_EQ(controller.nextSession(), 2u);
    EXPECT_EQ(nsm_fpga_map_p0()->window.completion_read, 0x1060u);
    EXPECT_FALSE(controller.sessionOpen());
}

TEST_F(NsmFpgaSensorsTest, RegistryReturnsOneControllerPerUuid)
{
    auto status = makeStatus(true);
    auto a = NsmFpgaController::getOrCreate("STATIC:3:0:MCTP_EID:17", status);
    auto b = NsmFpgaController::getOrCreate("STATIC:3:0:MCTP_EID:17", status);
    EXPECT_EQ(a, b);
    EXPECT_EQ(NsmFpgaController::get("STATIC:3:1:MCTP_EID:19"), nullptr);
}

TEST_F(NsmFpgaSensorsTest, ControllerFindsTheStatusCreatedAfterIt)
{
    // Items may reach nsmd before the discovery check entry: the controller
    // starts without a status and picks up the one the check creates later
    const uuid_t uuid = "STATIC:3:5:MCTP_EID:57";
    auto controller = NsmFpgaController::getOrCreate(uuid, nullptr);
    EXPECT_EQ(controller->statusObject(), nullptr);
    EXPECT_FALSE(controller->enabled());
    bool created = false;
    auto status = NsmFpgaStatus::getOrCreate(
        uuid, "HGX_FPGA_5",
        "/xyz/openbmc_project/inventory/system/accelerator/FPGA_5", created);
    EXPECT_TRUE(created);
    EXPECT_EQ(controller->statusObject(), status);
    EXPECT_EQ(controller->statusObject()->getInventoryPath(),
              "/xyz/openbmc_project/inventory/system/accelerator/FPGA_5");
}

TEST_F(NsmFpgaSensorsTest, GroupReadsItsItemsInSeriesInOneSession)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto group = std::make_shared<NsmFpgaGroup>("Test_Group");
    auto a = makeState("Test_State", status, controller, &stateRule);
    auto b = makeState("Test_State_B", status, controller, &secondStateRule);
    group->addItem(a);
    group->addItem(b);
    EXPECT_EQ(group->itemCount(), 2u);
    auto device = makeDevice();
    auto coro = group->update(device);
    // Both items read, one ACQUIRE shared across the group round
    EXPECT_TRUE(a->hasReading());
    EXPECT_TRUE(b->hasReading());
    EXPECT_EQ(fpga.acquires, 1u);
    EXPECT_TRUE(controller->sessionOpen());
}

TEST_F(NsmFpgaSensorsTest, GroupIsDueAtTheShortestIntervalOfItsItems)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto group = std::make_shared<NsmFpgaGroup>("Test_Group");
    // A first item slower than the round robin default sets the group pace
    group->addItem(makeState("Test_State", status, controller, &stateRule,
                             RR_REFRESH_LIMIT_IN_USEC * 2));
    EXPECT_EQ(group->refreshIntervalUsec(), RR_REFRESH_LIMIT_IN_USEC * 2);
    group->addItem(makeState("Test_State_B", status, controller,
                             &secondStateRule, 1000000));
    EXPECT_EQ(group->refreshIntervalUsec(), 1000000u);
    auto device = makeDevice();
    // Never read: due at once; after a round: due again one interval later
    EXPECT_TRUE(group->needsUpdate(NsmFpgaController::nowUsec()));
    auto coro = group->update(device);
    const uint64_t now = NsmFpgaController::nowUsec();
    EXPECT_FALSE(group->needsUpdate(now));
    EXPECT_TRUE(group->needsUpdate(now + 1000001));
}

TEST_F(NsmFpgaSensorsTest, RoundReadsOnlyTheItemsDueAtTheirOwnInterval)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto group = std::make_shared<NsmFpgaGroup>("Test_Group");
    group->bind(controller, nullptr);
    // One item due again after a microsecond, one at the default interval
    auto a = makeState("Test_State", status, controller, &stateRule, 1);
    auto b = makeState("Test_State_B", status, controller, &secondStateRule);
    group->addItem(a);
    group->addItem(b);
    auto device = makeDevice();
    auto first = group->update(device);
    // authority status 1, ACQUIRE 2, one word per item
    EXPECT_EQ(fpga.exchanges, 7u);
    EXPECT_TRUE(a->hasReading());
    EXPECT_TRUE(b->hasReading());
    auto second = group->update(device);
    // Only the first item is due; the other keeps its reading unread
    EXPECT_EQ(fpga.exchanges, 9u);
    EXPECT_TRUE(b->hasReading());
}

TEST_F(NsmFpgaSensorsTest, GroupSetupRowsRunOncePerRound)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    static const nsm_fpga_command setup[] = {
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .capture = NSM_FPGA_BUF_GROUP,
         .u = {.csr = {0x05000400ULL, 0}}}};
    static const nsm_fpga_group definition{
        .name = "Test_Group", .setup = setup, .setup_count = 1};
    // Row-less items: both publish the word the setup row captured
    static const nsm_fpga_rule presence{
        .name = "Presence",
        .kind = NSM_FPGA_RULE_STATE,
        .numeric = {.buffer = NSM_FPGA_BUF_GROUP, .offset = 0},
        .group = "Test_Group",
        .states = testStates,
        .state_count = 5};
    auto group = std::make_shared<NsmFpgaGroup>("Test_Group");
    group->bind(controller, &definition);
    auto a = makeState("Presence_A", status, controller, &presence);
    auto b = makeState("Presence_B", status, controller, &presence);
    group->addItem(a);
    group->addItem(b);
    auto coro = group->update(makeDevice());
    // One session, one setup read, no per-item exchange
    EXPECT_EQ(fpga.exchanges, 5u);
    EXPECT_TRUE(a->hasReading());
    EXPECT_TRUE(b->hasReading());
    EXPECT_EQ(a->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Enabled);
    EXPECT_TRUE(controller->sessionOpen());
}

TEST_F(NsmFpgaSensorsTest, GroupEndedBySetupPublishesTheStateOnEveryItem)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    static const nsm_fpga_command setup[] = {
        {.op = NSM_FPGA_OP_READ32,
         .station = 29,
         .capture = NSM_FPGA_BUF_GROUP,
         .u = {.csr = {0x05000400ULL, 0}},
         .expect = {.mask = 0x7, .value = 0},
         .on_expect_fail = NSM_FPGA_END_GROUP,
         .group_state = NSM_FPGA_STATE_ABSENT}};
    static const nsm_fpga_group definition{
        .name = "Test_Group", .setup = setup, .setup_count = 1};
    auto group = std::make_shared<NsmFpgaGroup>("Test_Group");
    group->bind(controller, &definition);
    auto a = makeState("Test_State", status, controller, &stateRule);
    auto b = makeState("Test_State_B", status, controller, &secondStateRule);
    group->addItem(a);
    group->addItem(b);
    auto coro = group->update(makeDevice());
    // The state word is 1, so the group ended as Absent before any item
    EXPECT_EQ(fpga.exchanges, 5u);
    EXPECT_FALSE(a->hasReading());
    EXPECT_EQ(a->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Absent);
    EXPECT_EQ(b->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Absent);
    // The group's state is not a decoded reading of the item
    EXPECT_FALSE(a->availabilityIntf->available());
    EXPECT_TRUE(controller->sessionOpen());
}

TEST_F(NsmFpgaSensorsTest, UnavailableBankMakesEveryGroupItemUnavailable)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto group = std::make_shared<NsmFpgaGroup>("Test_Group");
    group->bind(controller, nullptr);
    auto a = makeState("Test_State", status, controller, &stateRule);
    auto b = makeState("Test_State_B", status, controller, &secondStateRule);
    group->addItem(a);
    group->addItem(b);
    auto device = makeDevice();
    // A good round first, so both items hold a reading
    auto good = group->update(device);
    EXPECT_TRUE(a->hasReading());
    EXPECT_TRUE(b->hasReading());
    auto released = controller->release(device);
    // The bank is quarantined: one status read, no ACQUIRE, no stale value
    fpga.authorityFlags |= NSM_FPGA_AUTH_QUARANTINED;
    const auto before = fpga.exchanges;
    auto blocked = group->update(device);
    EXPECT_EQ(fpga.exchanges, before + 1);
    EXPECT_EQ(fpga.acquires, 1u);
    EXPECT_FALSE(a->hasReading());
    EXPECT_FALSE(b->hasReading());
    EXPECT_EQ(a->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);
    EXPECT_EQ(b->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);
}

TEST_F(NsmFpgaSensorsTest, FencedBankMakesEveryGroupItemUnavailableQuietly)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto group = std::make_shared<NsmFpgaGroup>("Test_Group");
    group->bind(controller, nullptr);
    auto a = makeState("Test_State", status, controller, &stateRule);
    auto b = makeState("Test_State_B", status, controller, &secondStateRule);
    group->addItem(a);
    group->addItem(b);
    auto device = makeDevice();
    auto good = group->update(device);
    EXPECT_TRUE(a->hasReading());
    // Fenced within the backoff: nothing goes to the FPGA, nothing stays stale
    controller->sessionLost("test fence", true);
    const auto before = fpga.exchanges;
    auto fenced = group->update(device);
    EXPECT_EQ(fpga.exchanges, before);
    EXPECT_FALSE(a->hasReading());
    EXPECT_FALSE(b->hasReading());
    EXPECT_EQ(a->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);
}

TEST_F(NsmFpgaSensorsTest, GroupRegistryReturnsOnePerUuidAndGroup)
{
    bool created = false;
    auto g1 = NsmFpgaGroup::getOrCreate("STATIC:3:0:MCTP_EID:17", "Test_Group",
                                        created);
    EXPECT_TRUE(created);
    auto g2 = NsmFpgaGroup::getOrCreate("STATIC:3:0:MCTP_EID:17", "Test_Group",
                                        created);
    EXPECT_FALSE(created);
    EXPECT_EQ(g1, g2);
    auto g3 = NsmFpgaGroup::getOrCreate("STATIC:3:1:MCTP_EID:19", "Test_Group",
                                        created);
    EXPECT_TRUE(created);
    EXPECT_NE(g1, g3);
}

TEST_F(NsmFpgaSensorsTest, FirstReadOpensTheSessionAndReleaseEndsIt)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Starting);
    EXPECT_FALSE(sensor->availabilityIntf->available());
    ASSERT_NE(sensor->associationIntf, nullptr);
    auto device = makeDevice();
    auto coro = sensor->update(device);
    EXPECT_TRUE(sensor->hasReading());
    EXPECT_EQ(sensor->lastRaw(), 1u);
    // authority status 1, ACQUIRE 2, word 2; the session stays open
    EXPECT_EQ(fpga.exchanges, 5u);
    EXPECT_EQ(controller->lastExchanges(), 5u);
    EXPECT_TRUE(controller->sessionOpen());
    EXPECT_EQ(fpga.acquires, 1u);
    EXPECT_EQ(fpga.releases, 0u);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Enabled);
    EXPECT_EQ(sensor->healthIntf->health(), FpgaHealthIntf::HealthType::OK);
    EXPECT_TRUE(sensor->availabilityIntf->available());
    // The word record named station 31 and the CSR
    EXPECT_EQ(fpga.header[5], 31);
    EXPECT_EQ(fpga.wordAddress, 0x05000400u);

    auto done = controller->release(device);
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_EQ(fpga.exchanges, 7u);
    EXPECT_EQ(fpga.releases, 1u);
    // RELEASE carried the verified data sequence 1 and control sequence 2
    EXPECT_EQ(fpga.control[4], NSM_FPGA_CTRL_RELEASE);
    EXPECT_EQ(fpga.control[28], 1);
    EXPECT_EQ(fpga.control[24], 2);
}

TEST_F(NsmFpgaSensorsTest, PowerStateFollowsTheStateWhenAsked)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto plain = makeState("Test_State", status, controller, &stateRule);
    EXPECT_EQ(plain->powerStateIntf, nullptr);
    auto sensor = makeState("Test_State_B", status, controller, &stateRule,
                            RR_REFRESH_LIMIT_IN_USEC, true);
    ASSERT_NE(sensor->powerStateIntf, nullptr);
    EXPECT_EQ(sensor->powerStateIntf->powerState(),
              FpgaPowerStateIntf::State::Unknown);
    auto coro = sensor->update(makeDevice());
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Enabled);
    EXPECT_EQ(sensor->powerStateIntf->powerState(),
              FpgaPowerStateIntf::State::On);
    sensor->publishState(NSM_FPGA_STATE_STANDBY_OFFLINE, NSM_FPGA_HEALTH_OK);
    EXPECT_EQ(sensor->powerStateIntf->powerState(),
              FpgaPowerStateIntf::State::Off);
    sensor->publishState(NSM_FPGA_STATE_STARTING, NSM_FPGA_HEALTH_OK);
    EXPECT_EQ(sensor->powerStateIntf->powerState(),
              FpgaPowerStateIntf::State::PoweringOn);
    sensor->markUnavailable();
    EXPECT_EQ(sensor->powerStateIntf->powerState(),
              FpgaPowerStateIntf::State::Unknown);
}

TEST_F(NsmFpgaSensorsTest, ReadsOfOneRoundShareTheSession)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto first = makeState("Test_State", status, controller, &stateRule);
    auto second = makeState("Test_State_B", status, controller,
                            &secondStateRule);
    auto device = makeDevice();
    auto a = first->update(device);
    auto b = second->update(device);
    EXPECT_TRUE(second->hasReading());
    EXPECT_EQ(fpga.exchanges, 7u);
    EXPECT_EQ(fpga.acquires, 1u);
    EXPECT_EQ(fpga.header[5], 34);
    // Data sequences count up inside the session
    EXPECT_EQ(fpga.header[24], 2);
    auto done = controller->release(device);
    EXPECT_EQ(fpga.control[28], 2);
}

TEST_F(NsmFpgaSensorsTest, GrantComesFromTheAuthorityStatus)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    fpga.authorityEpoch = 0x1122334455667788ULL;
    fpga.authorityGeneration = 41;
    auto coro = sensor->update(makeDevice());
    EXPECT_TRUE(sensor->hasReading());
    EXPECT_EQ(controller->lastAuthority().epoch, 0x1122334455667788ULL);
    EXPECT_EQ(controller->lastAuthority().generation, 41u);
    EXPECT_EQ(controller->currentGrant().generation, 42u);
    // The records carried the activated epoch, session 1 and generation 42
    EXPECT_EQ(getLe(fpga.header.data() + 8, 8), 0x1122334455667788ULL);
    EXPECT_EQ(getLe(fpga.header.data() + 16, 4), 1u);
    EXPECT_EQ(getLe(fpga.header.data() + 20, 4), 42u);
}

TEST_F(NsmFpgaSensorsTest, UnavailableBankIsNeverAcquired)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    fpga.authorityFlags |= NSM_FPGA_AUTH_QUARANTINED;
    auto coro = sensor->update(makeDevice());
    EXPECT_FALSE(sensor->hasReading());
    // Only the status read went out, no record was written
    EXPECT_EQ(fpga.exchanges, 1u);
    EXPECT_EQ(fpga.acquires, 0u);
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_THAT(controller->lastError(), HasSubstr("bank not acquirable"));
    // A blocked bank is internal, the Processor stays Enabled and OK
    EXPECT_TRUE(status->enabled());
    EXPECT_EQ(status->healthIntf->health(), FpgaHealthIntf::HealthType::OK);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);

    // Within the retry window the other sensors fail without a status read
    auto second = makeState("Test_State_C", status, controller,
                            &secondStateRule);
    auto quiet = second->update(makeDevice());
    EXPECT_EQ(fpga.exchanges, 1u);
    EXPECT_FALSE(second->hasReading());

    // After the window the status is read again; still not acquirable
    controller->openFailedAtUsec = 0;
    controller->fencedUntilUsec = 0;
    fpga.authorityFlags = NSM_FPGA_AUTH_QUARANTINED;
    fpga.authorityEpoch = 0;
    auto again = sensor->update(makeDevice());
    EXPECT_EQ(fpga.exchanges, 2u);
    EXPECT_EQ(fpga.acquires, 0u);

    // The bank comes back: the next round acquires and clears the warning
    controller->openFailedAtUsec = 0;
    controller->fencedUntilUsec = 0;
    fpga.authorityFlags = NSM_FPGA_AUTH_INITIALIZED | NSM_FPGA_AUTH_AVAILABLE;
    fpga.authorityEpoch = 0x68e7a1c0;
    auto back = sensor->update(makeDevice());
    EXPECT_TRUE(sensor->hasReading());
    EXPECT_EQ(status->healthIntf->health(), FpgaHealthIntf::HealthType::OK);
}

TEST_F(NsmFpgaSensorsTest, RejectedAcquireIsReportedByName)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    fpga.acquireResult = NSM_FPGA_CTRL_STALE;
    auto coro = sensor->update(makeDevice());
    EXPECT_FALSE(sensor->hasReading());
    EXPECT_EQ(fpga.exchanges, 3u);
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_THAT(controller->lastError(), HasSubstr("STALE"));

    fpga.acquireResult = NSM_FPGA_CTRL_BUSY;
    controller->openFailedAtUsec = 0;
    auto again = sensor->update(makeDevice());
    EXPECT_EQ(fpga.exchanges, 6u);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);
    EXPECT_THAT(controller->lastError(), HasSubstr("BUSY"));
}

TEST_F(NsmFpgaSensorsTest, PendingCompletionsAreReadAgain)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    fpga.pendingCompletions = 2;
    fpga.words[0x05000400ULL] = 2;
    auto coro = sensor->update(makeDevice());
    EXPECT_EQ(fpga.exchanges, 7u);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Fault);
    EXPECT_EQ(sensor->healthIntf->health(),
              FpgaHealthIntf::HealthType::Critical);
    EXPECT_FALSE(sensor->operationalStatusIntf->functional());
}

TEST_F(NsmFpgaSensorsTest, CompletionPendingForeverStopsTheRead)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    fpga.pendingCompletions = 100;
    auto coro = sensor->update(makeDevice());
    EXPECT_FALSE(sensor->hasReading());
    EXPECT_EQ(fpga.exchanges, 5u + NsmFpgaController::maxPendingRepeats);
    EXPECT_THAT(controller->lastError(), HasSubstr("pending"));
}

TEST_F(NsmFpgaSensorsTest, LostAnswerToAWriteDropsTheSession)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    // Exchange 4 is the word record publish
    fpga.loseExchange = 4;
    auto coro = sensor->update(makeDevice());
    // A lost reply publishes NaN this round and keeps polling, no latching
    // fault
    EXPECT_FALSE(sensor->hasReading());
    EXPECT_TRUE(status->enabled());
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);
    EXPECT_FALSE(sensor->availabilityIntf->available());
}

TEST_F(NsmFpgaSensorsTest, LostAnswerToAReadIsNotAmbiguous)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    // Exchange 3 is the ACQUIRE receipt read; the publish itself was acked
    fpga.loseExchange = 3;
    auto coro = sensor->update(makeDevice());
    EXPECT_FALSE(sensor->hasReading());
    EXPECT_FALSE(controller->sessionOpen());
}

TEST_F(NsmFpgaSensorsTest, NoSessionCompletionClosesTheSession)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    // Exchange 5 is the completion read of the first word
    fpga.noSessionExchange = 5;
    auto device = makeDevice();
    auto coro = sensor->update(device);
    EXPECT_FALSE(sensor->hasReading());
    EXPECT_FALSE(controller->sessionOpen());
    EXPECT_EQ(controller->lastError(), "FPGA reports no session");
    // A lost session fences the bank until the backoff has elapsed.
    auto fenced = sensor->update(device);
    EXPECT_FALSE(sensor->hasReading());
    EXPECT_EQ(fpga.exchanges, 5u);
    controller->fencedUntilUsec = 0;
    // The next read acquires again with the next generation.
    auto again = sensor->update(device);
    EXPECT_TRUE(sensor->hasReading());
    EXPECT_EQ(fpga.exchanges, 10u);
    EXPECT_EQ(fpga.acquires, 2u);
    EXPECT_EQ(getLe(fpga.control.data() + 16, 4), 2u);
}

TEST_F(NsmFpgaSensorsTest, RenewOnceTheLeaseHasAged)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    auto device = makeDevice();
    auto coro = sensor->update(device);
    EXPECT_EQ(fpga.renews, 0u);
    controller->acquiredAtUsec = 0;
    auto again = sensor->update(device);
    EXPECT_EQ(fpga.renews, 1u);
    EXPECT_EQ(fpga.exchanges, 5u + 2u + 2u);
    EXPECT_TRUE(controller->sessionOpen());
    EXPECT_TRUE(sensor->hasReading());
}

TEST_F(NsmFpgaSensorsTest, ItemsPollRoundRobin)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Telemetry_State", status, controller,
                            &telemetryRule);
    auto device = makeDevice();
    // The factory schedules every FPGA group round robin
    const auto before = device->roundRobinSensors.size();
    device->addSensor(std::static_pointer_cast<NsmObject>(sensor),
                      PollingType::RoundRobin);
    EXPECT_EQ(device->roundRobinSensors.size(), before + 1);
    const uint64_t now = NsmFpgaController::nowUsec();
    auto first = sensor->update(device);
    EXPECT_EQ(fpga.exchanges, 5u);
    // Polled again inside the refresh period the sensor is not due
    sensor->setLastUpdatedTimeStamp(now);
    EXPECT_FALSE(sensor->needsUpdate(now + PRIORITY_REFRESH_LIMIT_IN_USEC));
    EXPECT_TRUE(sensor->needsUpdate(now + RR_REFRESH_LIMIT_IN_USEC + 1));
}

TEST_F(NsmFpgaSensorsTest, NumericSensorScalesTheWord)
{
    EXPECT_EQ(NsmFpgaSensor::unitForSensorType("temperature"),
              SensorUnit::DegreesC);
    EXPECT_EQ(NsmFpgaSensor::unitForSensorType("pressure"), std::nullopt);
    EXPECT_DOUBLE_EQ(NsmFpgaSensor::decode(numericRule, wordBytes(85)).value(),
                     42.5);
    // Above the rule's maximum the reading is invalid
    EXPECT_EQ(NsmFpgaSensor::decode(numericRule, wordBytes(1000)),
              std::nullopt);

    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    NsmFpgaSensor sensor(utils::DBusHandler::getBus(),
                         makeInfo("Test_Temp_0", &numericRule), status,
                         controller);
    fpga.words[0x05000420ULL] = 85;
    auto device = makeDevice();
    auto coro = sensor.update(device);
    EXPECT_EQ(fpga.exchanges, 5u);
    EXPECT_EQ(fpga.wordAddress, 0x05000420u);
    EXPECT_EQ(controller->lastError(), "");
}

TEST_F(NsmFpgaSensorsTest, DisabledStatusBlocksPolling)
{
    auto status = makeStatus(false);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    auto coro = sensor->update(makeDevice());
    EXPECT_FALSE(sensor->hasReading());
    EXPECT_EQ(fpga.exchanges, 0u);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);
}

TEST_F(NsmFpgaSensorsTest, FailedReadShowsUnavailableUntilTheNextGoodOne)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    auto device = makeDevice();
    auto first = sensor->update(device);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Enabled);
    // The completion read of the second word is lost
    fpga.loseExchange = 7;
    auto second = sensor->update(device);
    EXPECT_FALSE(sensor->hasReading());
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);
    EXPECT_EQ(sensor->healthIntf->health(), FpgaHealthIntf::HealthType::OK);
    // A lost reply must keep later reads fenced until the retry window ends.
    auto fenced = sensor->update(device);
    EXPECT_FALSE(sensor->hasReading());
    EXPECT_EQ(fpga.exchanges, 7u);
    controller->fencedUntilUsec = 0;
    auto third = sensor->update(device);
    EXPECT_TRUE(sensor->hasReading());
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Enabled);
}

TEST_F(NsmFpgaSensorsTest, PublishMapsEveryStateCode)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    sensor->publish(0);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Absent);
    sensor->publish(1);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Enabled);
    sensor->publish(2);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Fault);
    EXPECT_EQ(sensor->healthIntf->health(),
              FpgaHealthIntf::HealthType::Critical);
    sensor->publish(4);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Starting);
    EXPECT_EQ(sensor->healthIntf->health(), FpgaHealthIntf::HealthType::OK);
    // A code the map does not name is unavailable with a warning
    sensor->publish(3);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);
    EXPECT_EQ(sensor->healthIntf->health(),
              FpgaHealthIntf::HealthType::Warning);
}

TEST_F(NsmFpgaSensorsTest, SentinelWordPublishesUnavailableNotDecodedBits)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    // An absent or unpowered slice answers the sentinel on every CSR
    fpga.words[0x05000400ULL] = NSM_FPGA_WORD_UNAVAILABLE;
    auto coro = sensor->update(makeDevice());
    EXPECT_FALSE(sensor->hasReading());
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);
    EXPECT_EQ(sensor->healthIntf->health(), FpgaHealthIntf::HealthType::OK);
}

TEST_F(NsmFpgaSensorsTest, IdentityItemPublishesItsFieldsAndReadsOnce)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto group = std::make_shared<NsmFpgaGroup>("Test_Group");
    group->bind(controller, nullptr);
    auto sensor = makeState("Test_Inventory", status, controller,
                            &identityRule);
    group->addItem(sensor);
    ASSERT_NE(sensor->assetIntf, nullptr);
    EXPECT_EQ(sensor->revisionIntf, nullptr);
    fpga.words[0x05000600ULL] = 0x0a0b0c0d;
    fpga.words[0x05000604ULL] = 0x32314241; // "AB12"
    fpga.words[0x05000608ULL] = 0x20202020; // trailing spaces
    auto device = makeDevice();
    auto first = group->update(device);
    EXPECT_TRUE(sensor->hasReading());
    EXPECT_EQ(sensor->assetIntf->serialNumber(), "0x0a0b0c0d");
    EXPECT_EQ(sensor->assetIntf->partNumber(), "AB12");
    // The next round sends nothing for an item that has its identity
    const auto before = fpga.exchanges;
    auto second = group->update(device);
    EXPECT_EQ(fpga.exchanges, before);
    EXPECT_EQ(sensor->assetIntf->partNumber(), "AB12");
    // Marked unavailable, every field is blank and the item reads again
    sensor->markUnavailable();
    EXPECT_EQ(sensor->assetIntf->serialNumber(), "");
    // The next scheduled round occurs after this item's refresh interval.
    sensor->lastReadUsec = 0;
    auto third = group->update(device);
    EXPECT_EQ(fpga.exchanges, before + 6);
    EXPECT_EQ(sensor->assetIntf->serialNumber(), "0x0a0b0c0d");
}

TEST_F(NsmFpgaSensorsTest, ArrayItemsPublishLaneVectorsOnTheSharedObject)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto group = std::make_shared<NsmFpgaGroup>("Test_Group");
    group->bind(controller, nullptr);
    const std::string module =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_Chassis_0_FPGA_0_Module_0";
    auto make = [&](const std::string& item, const nsm_fpga_rule* rule) {
        auto info = makeInfo(item, rule);
        info.objectPath = module;
        auto sensor = std::make_shared<NsmFpgaSensor>(
            utils::DBusHandler::getBus(), info, status, controller);
        group->addItem(sensor);
        return sensor;
    };
    auto txFault = make("Test_Tx_Fault", &txFaultRule);
    auto rxLos = make("Test_Rx_Los", &rxLosRule);
    auto txPower = make("Test_Tx_Power", &txPowerRule);
    auto rxPower = make("Test_Rx_Power", &rxPowerRule);
    // Flags one bit per lane; powers big-endian 16 bit, half a mW per count
    fpga.words[0x05000610ULL] = 0x05;
    fpga.words[0x05000614ULL] = 0x80;
    fpga.words[0x05000620ULL] = 0x02000100;
    fpga.words[0x05000624ULL] = 0x02000100;
    fpga.words[0x05000628ULL] = 0x04000300;
    fpga.words[0x0500062cULL] = 0x04000300;
    auto coro = group->update(makeDevice());
    EXPECT_EQ(controller->lastError(), "");
    ASSERT_NE(txFault->laneStatusIntf, nullptr);
    using LaneStatus = FpgaLaneStatusIntf::LaneStatus;
    const LaneStatus on = LaneStatus::Asserted;
    const LaneStatus off = LaneStatus::Clear;
    EXPECT_EQ(txFault->laneStatusIntf->txFault(),
              (std::vector<LaneStatus>{on, off, on, off, off, off, off, off}));
    EXPECT_EQ(rxLos->laneStatusIntf->rxLossOfSignal(),
              (std::vector<LaneStatus>{off, off, off, off, off, off, off, on}));
    ASSERT_NE(txPower->metricsIntf, nullptr);
    EXPECT_EQ(txPower->metricsIntf->txOutputPowerMilliWatts(),
              (std::vector<double>{0.5, 1.0, 0.5, 1.0}));
    EXPECT_EQ(rxPower->metricsIntf->rxInputPowerMilliWatts(),
              (std::vector<double>{1.5, 2.0, 1.5, 2.0}));
    // Items on one object path share its interfaces
    EXPECT_EQ(txPower->metricsIntf, rxPower->metricsIntf);
    EXPECT_EQ(txFault->laneStatusIntf, rxLos->laneStatusIntf);
    // A failed round empties the vectors instead of leaving stale lanes
    txFault->markUnavailable();
    EXPECT_TRUE(txFault->laneStatusIntf->txFault().empty());
    EXPECT_FALSE(rxLos->laneStatusIntf->rxLossOfSignal().empty());
}

TEST_F(NsmFpgaSensorsTest, ArrayWithoutItsCapabilityIsEmptyNotFailed)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto group = std::make_shared<NsmFpgaGroup>("Test_Group");
    group->bind(controller, nullptr);
    // The flags apply only when bit 7 of the captured byte is set
    nsm_fpga_rule gated = txFaultRule;
    gated.valid_if = {NSM_FPGA_BUF_ITEM, NSM_FPGA_CMP_EQ, 1, 0, 0x80, 0x80};
    auto info = makeInfo("Test_Tx_Fault", &gated);
    info.objectPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_Chassis_0_FPGA_0_Module_0";
    auto txFault = std::make_shared<NsmFpgaSensor>(utils::DBusHandler::getBus(),
                                                   info, status, controller);
    group->addItem(txFault);
    fpga.words[0x05000610ULL] = 0x05;
    auto coro = group->update(makeDevice());
    EXPECT_EQ(controller->lastError(), "");
    ASSERT_NE(txFault->laneStatusIntf, nullptr);
    EXPECT_TRUE(txFault->laneStatusIntf->txFault().empty());
    EXPECT_FALSE(txFault->hasReading());
    // The read itself succeeded, so the outcome is recorded as good
    ASSERT_TRUE(txFault->lastReadOk.has_value());
    EXPECT_TRUE(*txFault->lastReadOk);
    // Once the capability appears the lanes publish
    fpga.words[0x05000610ULL] = 0x85;
    txFault->lastReadUsec = 0;
    auto again = group->update(makeDevice());
    EXPECT_EQ(txFault->laneStatusIntf->txFault().size(), 8u);
}

TEST_F(NsmFpgaSensorsTest, LinkItemsFeedOnePortObject)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    const std::string port =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_FPGA_0/Ports/Ethernet_0";
    auto link = makePortItem("Ethernet_0_Pcs_Ready", port, "LinkStatus", status,
                             controller, &stateRule);
    auto local = makePortItem("Ethernet_0_Local_Fault", port, "LocalFault",
                              status, controller, &secondStateRule);
    auto remote = makePortItem("Ethernet_0_Remote_Fault", port, "RemoteFault",
                               status, controller, &secondStateRule);
    auto ber = makePortItem("Ethernet_0_High_Ber", port, "HighBER", status,
                            controller, &secondStateRule);
    // Items on one port path share its object; no state object is built
    ASSERT_NE(link->port, nullptr);
    EXPECT_EQ(link->port, local->port);
    EXPECT_EQ(link->operationalStatusIntf, nullptr);
    EXPECT_EQ(link->port->width(), nullptr);
    ASSERT_NE(link->port->faults(), nullptr);
    using LinkStatus = PortStateIntf::LinkStatusType;
    using FaultState = FpgaLinkFaultIntf::FaultState;
    // Nothing read yet: unavailable, down, every fault unknown
    EXPECT_FALSE(link->port->available());
    EXPECT_EQ(link->port->linkStatus(), LinkStatus::LinkDown);
    EXPECT_EQ(link->port->faults()->localFault(), FaultState::Unknown);
    // The PCS ready word decodes Enabled: the link is up
    auto coro = link->update(makeDevice());
    EXPECT_TRUE(link->port->available());
    EXPECT_EQ(link->port->linkStatus(), LinkStatus::LinkUp);
    EXPECT_EQ(link->port->health(), FpgaHealthIntf::HealthType::OK);
    // A code outside the map reads as not ready: down, but a reading
    link->publish(3);
    EXPECT_TRUE(link->port->available());
    EXPECT_EQ(link->port->linkStatus(), LinkStatus::LinkDown);
    EXPECT_EQ(link->port->health(), FpgaHealthIntf::HealthType::Warning);
    // Faults: Fault is Asserted, any other state Clear, no reading Unknown
    local->publish(2);
    remote->publish(1);
    EXPECT_EQ(link->port->faults()->localFault(), FaultState::Asserted);
    EXPECT_EQ(link->port->faults()->remoteFault(), FaultState::Clear);
    EXPECT_EQ(link->port->faults()->highBER(), FaultState::Unknown);
    EXPECT_EQ(link->port->health(), FpgaHealthIntf::HealthType::Critical);
    ber->publish(1);
    EXPECT_EQ(link->port->faults()->highBER(), FaultState::Clear);
    ber->markUnavailable();
    EXPECT_EQ(link->port->faults()->highBER(), FaultState::Unknown);
    // The sentinel on the status word takes the port's availability away
    fpga.words[0x05000400ULL] = NSM_FPGA_WORD_UNAVAILABLE;
    auto again = link->update(makeDevice());
    EXPECT_FALSE(link->port->available());
    EXPECT_EQ(link->port->linkStatus(), LinkStatus::LinkDown);
    // The fault items still read: their values stay
    EXPECT_EQ(link->port->faults()->localFault(), FaultState::Asserted);
}

TEST_F(NsmFpgaSensorsTest, LaneItemsWidenTheirPort)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    const std::string port =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_FPGA_0/Ports/C2C_0";
    std::vector<std::shared_ptr<NsmFpgaSensor>> lanes;
    for (int i = 0; i < 4; ++i)
    {
        lanes.push_back(
            makePortItem("C2C_0_Lane_" + std::to_string(i) + "_Pcs_Ready", port,
                         "Lane", status, controller, &stateRule));
    }
    ASSERT_NE(lanes[0]->port->width(), nullptr);
    EXPECT_EQ(lanes[0]->port->faults(), nullptr);
    using LinkStatus = PortStateIntf::LinkStatusType;
    EXPECT_EQ(lanes[0]->port->width()->width(), 4u);
    EXPECT_EQ(lanes[0]->port->width()->activeWidth(), 0u);
    EXPECT_FALSE(lanes[0]->port->available());
    // Two lanes ready: a reading, but not every lane is up
    lanes[0]->publish(1);
    lanes[1]->publish(1);
    EXPECT_TRUE(lanes[0]->port->available());
    EXPECT_EQ(lanes[0]->port->width()->activeWidth(), 2u);
    EXPECT_EQ(lanes[0]->port->linkStatus(), LinkStatus::LinkDown);
    lanes[2]->publish(1);
    lanes[3]->publish(1);
    EXPECT_EQ(lanes[0]->port->width()->activeWidth(), 4u);
    EXPECT_EQ(lanes[0]->port->linkStatus(), LinkStatus::LinkUp);
    // One lane drops out of the map: down with three active lanes
    lanes[3]->publish(3);
    EXPECT_EQ(lanes[0]->port->width()->activeWidth(), 3u);
    EXPECT_EQ(lanes[0]->port->linkStatus(), LinkStatus::LinkDown);
    // Every lane without a reading: the port has none either
    for (auto& lane : lanes)
    {
        lane->markUnavailable();
    }
    EXPECT_FALSE(lanes[0]->port->available());
    EXPECT_EQ(lanes[0]->port->width()->activeWidth(), 0u);
}

TEST_F(NsmFpgaSensorsTest, RoundLeavesItsRemainingItemsToAnImmediateNextRound)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    // Every item after the first finds the episode slice used up
    controller->episodeSliceUsec = 0;
    auto group = std::make_shared<NsmFpgaGroup>("Test_Group");
    group->bind(controller, nullptr);
    auto a = makeState("Test_State", status, controller, &stateRule);
    auto b = makeState("Test_State_B", status, controller, &secondStateRule);
    group->addItem(a);
    group->addItem(b);
    auto device = makeDevice();
    auto first = group->update(device);
    // authority status 1, ACQUIRE 2, one word 2; the second item waits
    EXPECT_EQ(fpga.exchanges, 5u);
    EXPECT_TRUE(a->hasReading());
    EXPECT_FALSE(b->hasReading());
    EXPECT_TRUE(controller->sessionOpen());
    // Due at once, not after the refresh interval
    EXPECT_TRUE(group->needsUpdate(NsmFpgaController::nowUsec()));
    auto second = group->update(device);
    // The next round starts where the last one stopped
    EXPECT_EQ(fpga.exchanges, 7u);
    EXPECT_TRUE(b->hasReading());
    EXPECT_TRUE(a->hasReading());
}

// The FPGA answers values the map does not expect: nothing is published as
// a reading, nothing crashes, and the next good answer is published again

TEST_F(NsmFpgaSensorsTest, WordAboveTheRuleRangePublishesNaN)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    NsmFpgaSensor sensor(utils::DBusHandler::getBus(),
                         makeInfo("Test_Temp_0", &numericRule), status,
                         controller);
    auto* dbusValue = dynamic_cast<NsmNumericSensorDbusValue*>(
        sensor.sensorValue->objects.back().get());
    ASSERT_NE(dbusValue, nullptr);
    auto device = makeDevice();
    // 1000 scaled by 0.5 is 500 C, above the rule's 100 C maximum
    fpga.words[0x05000420ULL] = 1000;
    auto bad = sensor.update(device);
    EXPECT_TRUE(std::isnan(dbusValue->valueIntf.value()));
    // The transport was fine, only the value was refused
    EXPECT_EQ(controller->lastError(), "");
    EXPECT_TRUE(controller->sessionOpen());
    fpga.words[0x05000420ULL] = 85;
    auto good = sensor.update(device);
    EXPECT_DOUBLE_EQ(dbusValue->valueIntf.value(), 42.5);
}

TEST_F(NsmFpgaSensorsTest, StateWordOutsideItsMapPublishesUnavailableWarning)
{
    // A state map without a catch all entry: unknown words match nothing
    static const nsm_fpga_state_entry strictStates[] = {
        {0x7, 0, NSM_FPGA_STATE_ABSENT, NSM_FPGA_HEALTH_OK},
        {0x7, 1, NSM_FPGA_STATE_ENABLED, NSM_FPGA_HEALTH_OK}};
    static const nsm_fpga_rule strictRule{.name = "Strict_State",
                                          .commands = stateCmds,
                                          .command_count = 1,
                                          .kind = NSM_FPGA_RULE_STATE,
                                          .group = "Test_Group",
                                          .states = strictStates,
                                          .state_count = 2};
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Strict_State", status, controller, &strictRule);
    auto device = makeDevice();
    fpga.words[0x05000400ULL] = 0x5;
    auto coro = sensor->update(device);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);
    EXPECT_EQ(sensor->healthIntf->health(),
              FpgaHealthIntf::HealthType::Warning);
    EXPECT_FALSE(sensor->operationalStatusIntf->functional());
    fpga.words[0x05000400ULL] = 0x1;
    auto again = sensor->update(device);
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Enabled);
}

TEST_F(NsmFpgaSensorsTest, IdentityBytesThatAreNotTextPublishBlankText)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_Inventory", status, controller,
                            &identityRule);
    // Control bytes where the part text should be, the serial still a number
    fpga.words[0x05000600ULL] = 0x0a0b0c0d;
    fpga.words[0x05000604ULL] = 0x01020304;
    fpga.words[0x05000608ULL] = 0x05060708;
    auto coro = sensor->update(makeDevice());
    EXPECT_TRUE(sensor->hasReading());
    EXPECT_EQ(sensor->assetIntf->serialNumber(), "0x0a0b0c0d");
    EXPECT_EQ(sensor->assetIntf->partNumber(), "");
}

TEST_F(NsmFpgaSensorsTest, GarbageCompletionRecordFailsTheReadThenRecovers)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    auto device = makeDevice();
    fpga.corruptCompletions = 1;
    auto bad = sensor->update(device);
    EXPECT_FALSE(sensor->hasReading());
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::UnavailableOffline);
    EXPECT_THAT(controller->lastError(), HasSubstr("malformed"));
    EXPECT_TRUE(status->enabled());
    // The next completion is a record again and the item reads
    auto good = sensor->update(device);
    EXPECT_TRUE(sensor->hasReading());
    EXPECT_EQ(sensor->operationalStatusIntf->state(),
              FpgaOperationalStatusIntf::StateType::Enabled);
}

TEST_F(NsmFpgaSensorsTest, ShortReplyFailsTheReadAndKeepsTheSession)
{
    auto status = makeStatus(true);
    auto controller = std::make_shared<NsmFpgaController>(status);
    auto sensor = makeState("Test_State", status, controller, &stateRule);
    auto device = makeDevice();
    // Exchange 5 is the completion read of the first word
    fpga.shortReplyExchange = 5;
    auto bad = sensor->update(device);
    EXPECT_FALSE(sensor->hasReading());
    EXPECT_THAT(controller->lastError(), HasSubstr("decode failed"));
    // Nothing was published to the bank that went unanswered: no fence
    EXPECT_TRUE(controller->sessionOpen());
    EXPECT_EQ(controller->fencedUntilUsec, 0u);
    auto good = sensor->update(device);
    EXPECT_TRUE(sensor->hasReading());
    EXPECT_EQ(fpga.acquires, 1u);
}
