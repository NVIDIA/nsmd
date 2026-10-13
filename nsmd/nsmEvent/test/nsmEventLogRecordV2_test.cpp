// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

/*
 * Unit tests for NsmEventLogRecordV2, the CPER record chunk collector.
 *
 * The collector polls Get Event Log Record V2 to fetch a CPER record chunk
 * by chunk and then acknowledges the event. Both legs carry one
 * application-level retry, which these tests drive at the response-handler
 * boundary:
 *
 *   - record collection (handleGetModeResponseMsg): a VBIOS error or a
 *     malformed response discards the partial record and restarts from
 *     transfer handle 0; a second consecutive failure stops reading and
 *     acknowledges the event instead.
 *   - acknowledgment (handleAckModeResponseMsg): a failed ACK is retried
 *     once; a second failure stops polling for the event.
 *
 * Responses are encoded with libnsm and fed to handleResponseMsg(), which
 * is what NsmSensor::update() does after a sensorIO round trip; the
 * NsmEventLogRecordV2UpdateTest fixture additionally drives update() itself
 * through a MockNsmDevice. The owning NsmCPEREvent is held as a weak_ptr,
 * so most tests run the collector standalone and only the hand-off tests
 * attach a real owner.
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <coroutine>
#include <cstddef>
#include <cstring>
#include <memory>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-capability-discovery.h"
#include "platform-environmental.h"

#include "nsmCPEREvent.hpp"
#include "nsmEventLogRecordV2.hpp"

using namespace nsm;

namespace
{

constexpr eid_t kEid = 11;
constexpr uint16_t kEventHandle = 5;
constexpr uint16_t kReasonCode = 0x0042;
// Any version the device may report; distinct from the owner's default so
// the hand-off is observable on NsmCPEREvent::cperEventFormatVersion.
constexpr uint8_t kRecordVersion = NSM_EVENT_VERSION + 1;

using Bytes = std::vector<uint8_t>;

const nsm_msg* asMsg(const Bytes& buf)
{
    return reinterpret_cast<const nsm_msg*>(buf.data());
}

// Device rejected the request: reason-code response with cc != NSM_SUCCESS.
// First- and continuation-segment requests share this error encoding.
Bytes errorResponse(uint8_t cc)
{
    Bytes buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto rc = encode_nsm_get_event_log_record_v2_resp_first_handle(
        0, cc, kReasonCode, nullptr, reinterpret_cast<nsm_msg*>(buf.data()));
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
    return buf;
}

// First segment of a record (answers a request with transfer_handle == 0).
Bytes firstSegment(uint16_t nextTransferHandle, uint16_t nextEventHandle,
                   const Bytes& data, uint8_t eventVersion = NSM_EVENT_VERSION)
{
    Bytes buf(sizeof(nsm_msg_hdr) +
                  offsetof(nsm_get_event_log_record_v2_resp_first_handle,
                           event_data) +
                  data.size(),
              0);
    nsm_event_log_record_v2_first_fields fields{};
    fields.next_transfer_handle = nextTransferHandle;
    fields.event_handle = nextEventHandle;
    fields.nvidia_message_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
    fields.event_version = eventVersion;
    fields.event_id = NSM_CPER_EVENT;
    fields.event_class = NSM_POLLED_EVENT_CLASS;
    fields.event_data = data.empty() ? nullptr
                                     : const_cast<uint8_t*>(data.data());
    fields.event_data_len = static_cast<uint16_t>(data.size());
    auto rc = encode_nsm_get_event_log_record_v2_resp_first_handle(
        0, NSM_SUCCESS, 0, &fields, reinterpret_cast<nsm_msg*>(buf.data()));
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
    return buf;
}

// Continuation segment (answers a request with transfer_handle != 0). A
// successful ACK response has the same shape and carries no data.
Bytes nextSegment(uint16_t nextTransferHandle, uint16_t nextEventHandle,
                  const Bytes& data)
{
    Bytes buf(
        sizeof(nsm_msg_hdr) +
            offsetof(nsm_get_event_log_record_v2_resp_next_handle, event_data) +
            data.size(),
        0);
    nsm_event_log_record_v2_next_fields fields{};
    fields.next_transfer_handle = nextTransferHandle;
    fields.event_handle = nextEventHandle;
    fields.event_data = data.empty() ? nullptr
                                     : const_cast<uint8_t*>(data.data());
    fields.event_data_len = static_cast<uint16_t>(data.size());
    auto rc = encode_nsm_get_event_log_record_v2_resp_next_handle(
        0, NSM_SUCCESS, 0, &fields, reinterpret_cast<nsm_msg*>(buf.data()));
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
    return buf;
}

Bytes ackResponse(uint16_t nextEventHandle)
{
    return nextSegment(0, nextEventHandle, {});
}

uint8_t feed(NsmEventLogRecordV2& collector, const Bytes& response)
{
    return collector.handleResponseMsg(asMsg(response), response.size());
}

struct PendingRequest
{
    uint8_t mode = 0xFF;
    uint16_t eventHandle = 0xFFFF;
    uint16_t transferHandle = 0xFFFF;
};

// What the collector would send on its next poll.
PendingRequest nextRequest(NsmEventLogRecordV2& collector)
{
    PendingRequest req;
    auto msg = collector.genRequestMsg(kEid, 0);
    EXPECT_TRUE(msg.has_value());
    if (msg.has_value())
    {
        EXPECT_EQ(NSM_SUCCESS, decode_nsm_get_event_log_record_v2_req(
                                   asMsg(*msg), msg->size(), &req.mode,
                                   &req.eventHandle, &req.transferHandle));
    }
    return req;
}

// The unit-test DBusHandler has no asio connection, so the owner's record
// logger would dereference a null connection on its first D-Bus call. Park a
// never-finishing coroutine in the owner's logger slot: Coroutine::assign()
// then refuses to start the logger, while logRecordOnRf() still performs the
// observable hand-off (format version, event-handle bookkeeping, staged
// record). In the coverage build co_await/co_return are stripped, this
// becomes a plain function returning a finished Coroutine, and the logger
// runs synchronously against the mock D-Bus and completes harmlessly.
class ParkedRecordLogger
{
  public:
    explicit ParkedRecordLogger(NsmCPEREvent& owner) :
        slot(owner.cperRecordLoggerHandle)
    {
        requester::Coroutine::assign(slot, []() -> requester::Coroutine {
            (void)co_await std::suspend_always{};
            co_return NSM_SW_SUCCESS;
        });
    }

    ~ParkedRecordLogger()
    {
        if (slot && !slot.done())
        {
            slot.destroy();
        }
        slot = nullptr;
    }

  private:
    std::coroutine_handle<>& slot;
};

} // namespace

class NsmEventLogRecordV2Test : public Test
{
  protected:
    static std::shared_ptr<NsmEventLogRecordV2>
        makeCollector(std::shared_ptr<NsmCPEREvent> owner = nullptr)
    {
        auto collector = std::make_shared<NsmEventLogRecordV2>(
            "GPU_0_CPER_EventLogRecordV2", "NSM_CPER",
            NSM_EVENT_LOG_V2_MODE_GET_DATA, 0, owner);
        collector->triggerRecordChunkCollection(kEventHandle, 0);
        return collector;
    }

    // Collect a single-chunk record so the ACK request is the next poll.
    static void collectRecord(NsmEventLogRecordV2& collector,
                              const Bytes& record)
    {
        ASSERT_EQ(NSM_SW_SUCCESS,
                  feed(collector, firstSegment(0, NO_MORE_HANDLES, record)));
        ASSERT_EQ(NSM_EVENT_LOG_V2_MODE_ACKNOWLEDGEMENT, collector.mode);
    }
};

// ============================================================================
// Record chunk collection: VBIOS error and malformed response handling
// ============================================================================

TEST_F(NsmEventLogRecordV2Test, GetMode_ErrorResponse_RetriesOnceFromFirstChunk)
{
    auto collector = makeCollector();

    EXPECT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));

    EXPECT_EQ(1, collector->recordReadAttempts);
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_GET_DATA, collector->mode);
    EXPECT_TRUE(collector->eventData.empty());
    EXPECT_TRUE(collector->needsUpdate(0));
    EXPECT_TRUE(collector->isRecordCollectionInProgress());

    auto req = nextRequest(*collector);
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_GET_DATA, req.mode);
    EXPECT_EQ(kEventHandle, req.eventHandle);
    EXPECT_EQ(0, req.transferHandle);
}

TEST_F(NsmEventLogRecordV2Test,
       GetMode_ErrorOnContinuationChunk_DiscardsPartialRecordAndRestarts)
{
    auto collector = makeCollector();
    const Bytes firstChunk{0x01, 0x02, 0x03};

    ASSERT_EQ(
        NSM_SW_SUCCESS,
        feed(*collector, firstSegment(1, PENDING_HANDLE_VALUE, firstChunk)));
    ASSERT_EQ(firstChunk, collector->eventData);
    ASSERT_EQ(1, collector->transferHandle);

    EXPECT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_INVALID_DATA)));

    // Nothing of the half-collected record survives; the retry starts over.
    EXPECT_EQ(1, collector->recordReadAttempts);
    EXPECT_TRUE(collector->eventData.empty());
    EXPECT_EQ(0, collector->transferHandle);
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_GET_DATA, collector->mode);
    EXPECT_EQ(0, nextRequest(*collector).transferHandle);
}

TEST_F(NsmEventLogRecordV2Test, GetMode_MalformedResponse_CountsAsReadFailure)
{
    auto collector = makeCollector();
    auto response = firstSegment(0, PENDING_HANDLE_VALUE, {0xAA});

    // Truncated below the first-segment minimum: decode fails with cc == 0.
    auto rc = collector->handleResponseMsg(
        asMsg(response), sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));

    EXPECT_EQ(NSM_SW_SUCCESS, rc);
    EXPECT_EQ(1, collector->recordReadAttempts);
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_GET_DATA, collector->mode);
    EXPECT_TRUE(collector->eventData.empty());
    EXPECT_TRUE(collector->needsUpdate(0));
}

TEST_F(NsmEventLogRecordV2Test,
       GetMode_RetrySucceeds_RecordCollectedThenAcknowledged)
{
    auto collector = makeCollector();
    const Bytes record{0x10, 0x20, 0x30, 0x40};

    ASSERT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));
    ASSERT_EQ(1, collector->recordReadAttempts);

    // The retried first-chunk request succeeds and completes the record.
    EXPECT_EQ(NSM_SW_SUCCESS,
              feed(*collector, firstSegment(0, NO_MORE_HANDLES, record)));

    EXPECT_EQ(record, collector->eventData);
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_ACKNOWLEDGEMENT, collector->mode);
    EXPECT_TRUE(collector->needsUpdate(0));

    auto req = nextRequest(*collector);
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_ACKNOWLEDGEMENT, req.mode);
    EXPECT_EQ(kEventHandle, req.eventHandle);
    EXPECT_EQ(0, req.transferHandle);
}

TEST_F(NsmEventLogRecordV2Test,
       GetMode_SecondConsecutiveError_StopsReadingAndAcknowledges)
{
    auto collector = makeCollector();

    ASSERT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));
    EXPECT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));

    // Exactly one retry: the second failure exhausts the read budget and the
    // collector moves on to acknowledge the event instead of reading again.
    EXPECT_EQ(MAX_RECORD_READ_ATTEMPTS, collector->recordReadAttempts);
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_ACKNOWLEDGEMENT, collector->mode);
    EXPECT_TRUE(collector->eventData.empty());
    EXPECT_TRUE(collector->needsUpdate(0));
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_ACKNOWLEDGEMENT,
              nextRequest(*collector).mode);
}

TEST_F(NsmEventLogRecordV2Test, GetMode_ReadBudgetResetsForNextRecord)
{
    auto collector = makeCollector();
    ASSERT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));
    ASSERT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));
    ASSERT_EQ(MAX_RECORD_READ_ATTEMPTS, collector->recordReadAttempts);

    collector->triggerRecordChunkCollection(kEventHandle + 1, 0);

    EXPECT_EQ(0, collector->recordReadAttempts);
    EXPECT_EQ(0, collector->recordAckAttempts);
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_GET_DATA, collector->mode);
    auto req = nextRequest(*collector);
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_GET_DATA, req.mode);
    EXPECT_EQ(kEventHandle + 1, req.eventHandle);
    EXPECT_EQ(0, req.transferHandle);
}

// ============================================================================
// Event acknowledgment: ACK failure handling
// ============================================================================

TEST_F(NsmEventLogRecordV2Test, AckMode_ErrorResponse_RetriesAckOnce)
{
    auto collector = makeCollector();
    const Bytes record{0xA1, 0xB2};
    collectRecord(*collector, record);

    EXPECT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));

    EXPECT_EQ(1, collector->recordAckAttempts);
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_ACKNOWLEDGEMENT, collector->mode);
    // The record is kept for logging and polling continues for the retry.
    EXPECT_EQ(record, collector->eventData);
    EXPECT_TRUE(collector->needsUpdate(0));

    auto req = nextRequest(*collector);
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_ACKNOWLEDGEMENT, req.mode);
    EXPECT_EQ(kEventHandle, req.eventHandle);
    EXPECT_EQ(0, req.transferHandle);
}

TEST_F(NsmEventLogRecordV2Test, AckMode_RetrySucceeds_StopsPolling)
{
    auto collector = makeCollector();
    collectRecord(*collector, {0xA1});
    ASSERT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));

    EXPECT_EQ(NSM_SW_SUCCESS, feed(*collector, ackResponse(NO_MORE_HANDLES)));

    EXPECT_EQ(1, collector->recordAckAttempts);
    EXPECT_FALSE(collector->needsUpdate(0));
    EXPECT_EQ(NO_MORE_HANDLES, collector->nextEventHandle);
}

TEST_F(NsmEventLogRecordV2Test,
       AckMode_SecondConsecutiveError_GivesUpAndStopsPolling)
{
    auto collector = makeCollector();
    collectRecord(*collector, {0xA1});

    ASSERT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));
    ASSERT_TRUE(collector->needsUpdate(0));
    EXPECT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));

    // Exactly one retry: the second ACK failure ends polling for this event.
    EXPECT_EQ(MAX_RECORD_ACK_ATTEMPTS, collector->recordAckAttempts);
    EXPECT_FALSE(collector->needsUpdate(0));
}

// ============================================================================
// Hand-off to the owning NsmCPEREvent
// ============================================================================

TEST_F(NsmEventLogRecordV2Test, Owner_RetriedRecordIsHandedOverOnceAcknowledged)
{
    auto owner = std::make_shared<NsmCPEREvent>(nullptr, "GPU_0_CPER",
                                                "NSM_CPER");
    auto collector = makeCollector(owner);
    owner->setEventLogRecordChunkCollector(collector);
    ParkedRecordLogger parked(*owner);
    const Bytes record{0xC0, 0xFF, 0xEE};

    // Read fails once and is retried; the ACK fails once and is retried.
    ASSERT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));
    ASSERT_EQ(NSM_SW_SUCCESS,
              feed(*collector,
                   firstSegment(0, NO_MORE_HANDLES, record, kRecordVersion)));
    EXPECT_EQ(NSM_EVENT_VERSION, owner->cperEventFormatVersion);
    ASSERT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));
    // Nothing is handed over until the ACK completes.
    EXPECT_EQ(NSM_EVENT_VERSION, owner->cperEventFormatVersion);

    EXPECT_EQ(NSM_SW_SUCCESS, feed(*collector, ackResponse(NO_MORE_HANDLES)));

    EXPECT_FALSE(collector->needsUpdate(0));
    EXPECT_EQ(kRecordVersion, owner->cperEventFormatVersion);
    EXPECT_TRUE(owner->eventHandles.empty());
#ifndef COVERAGE_DISABLE_COROUTINES
    // The logger is parked, so the record staged for the CPER logger is
    // still visible: data header followed by the collected payload.
    ASSERT_EQ(sizeof(nsm_cper_event_data_header) + record.size(),
              owner->cperRecordData.size());
    nsm_cper_event_data_header hdr{};
    std::memcpy(&hdr, owner->cperRecordData.data(), sizeof(hdr));
    EXPECT_EQ(kRecordVersion, hdr.format_version);
    EXPECT_EQ(DEFAULT_CPER_FORMAT, hdr.format_type);
    EXPECT_EQ(record.size(), static_cast<size_t>(hdr.event_data_length));
    EXPECT_EQ(record, Bytes(owner->cperRecordData.begin() + sizeof(hdr),
                            owner->cperRecordData.end()));
#endif
}

TEST_F(NsmEventLogRecordV2Test,
       Owner_AckFailureIsReportedWithoutAdvancingEventQueue)
{
    auto owner = std::make_shared<NsmCPEREvent>(nullptr, "GPU_0_CPER",
                                                "NSM_CPER");
    auto collector = makeCollector(owner);
    owner->setEventLogRecordChunkCollector(collector);
    ParkedRecordLogger parked(*owner);
    const Bytes record{0xC0, 0xFF, 0xEE};

    // Record collected with another event pending; both ACK attempts fail.
    ASSERT_EQ(NSM_SW_SUCCESS,
              feed(*collector, firstSegment(0, PENDING_HANDLE_VALUE, record,
                                            kRecordVersion)));
    ASSERT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));
    ASSERT_EQ(NSM_EVENT_VERSION, owner->cperEventFormatVersion);
    EXPECT_EQ(NSM_SW_SUCCESS,
              feed(*collector, errorResponse(NSM_ERR_NOT_READY)));

    // The owner is still notified so the record reaches the logger, but a
    // failed ACK does not queue the pending handle (a successful one would).
    EXPECT_FALSE(collector->needsUpdate(0));
    EXPECT_EQ(kRecordVersion, owner->cperEventFormatVersion);
    EXPECT_TRUE(owner->eventHandles.empty());
}

// ============================================================================
// Retry reached through the real polling entry point (NsmSensor::update)
// ============================================================================

class NsmEventLogRecordV2UpdateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<NsmEventLogRecordV2> collector;

    NsmEventLogRecordV2UpdateTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        collector = std::make_shared<NsmEventLogRecordV2>(
            "GPU_0_CPER_EventLogRecordV2", "NSM_CPER",
            NSM_EVENT_LOG_V2_MODE_GET_DATA, 0, nullptr);
        collector->triggerRecordChunkCollection(kEventHandle, 0);
    }

    ~NsmEventLogRecordV2UpdateTest() override
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmEventLogRecordV2UpdateTest, Update_ErrorResponse_KeepsPollingForRetry)
{
    ASSERT_NE(nullptr, gpu);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(Invoke(mockSensorIO(errorResponse(NSM_ERR_NOT_READY))));

    (void)collector->update(gpu);

    EXPECT_EQ(1, collector->recordReadAttempts);
    EXPECT_EQ(NSM_EVENT_LOG_V2_MODE_GET_DATA, collector->mode);
    EXPECT_TRUE(collector->needsUpdate(0));
    EXPECT_EQ(0, nextRequest(*collector).transferHandle);
}

TEST_F(NsmEventLogRecordV2UpdateTest,
       Update_TransportFailure_StopsWithoutApplicationRetry)
{
    ASSERT_NE(nullptr, gpu);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(Invoke(mockSensorIO(NSM_ERROR)));

    (void)collector->update(gpu);

    // No response to retry against: transport retries belong to the
    // requester, the collector just stops polling for this event.
    EXPECT_EQ(0, collector->recordReadAttempts);
    EXPECT_FALSE(collector->needsUpdate(0));
    EXPECT_TRUE(collector->eventData.empty());
}
