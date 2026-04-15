/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "nsmEventLogRecordV2.hpp"

#include "device-capability-discovery.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmEventLogRecordV2::NsmEventLogRecordV2(
    const std::string& name, const std::string& type, uint8_t mode,
    uint16_t eventHandle, std::shared_ptr<NsmCPEREvent> cperEvent) :
    NsmSensor(name, type), mode(mode), eventHandle(eventHandle),
    cperEvent(cperEvent)
{
    eventData.reserve(SINGLE_EVENT_RECORD_SIZE_KB * BYTES_PER_KB);
    lg2::info("NsmEventLogRecordV2: create sensor: {NAME}", "NAME",
              name.c_str());
}

requester::Coroutine
    NsmEventLogRecordV2::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    auto rc = co_await NsmSensor::update(nsmDevice);
    if (rc != NSM_SW_SUCCESS)
    {
        if (mode == NSM_EVENT_LOG_V2_MODE_GET_DATA)
        { // If update fails, clear the event data and stop polling
            eventData.clear();
        }
        stopPollingAndLogRecord(rc);
    }
    co_return NSM_SW_SUCCESS;
}

bool NsmEventLogRecordV2::needsUpdate(
    [[maybe_unused]] const uint64_t& currentTimestampInUsec) const
{
    return shouldPollForData;
}

std::optional<std::vector<uint8_t>>
    NsmEventLogRecordV2::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_event_log_record_v2_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_nsm_get_event_log_record_v2_req(
        instanceId, mode, eventHandle, transferHandle, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_nsm_get_event_log_record_v2_req failed for eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmEventLogRecordV2::decodeFirstHandleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    struct nsm_event_log_record_v2_first_fields fields{};

    auto rc = decode_nsm_get_event_log_record_v2_resp_first_handle(
        responseMsg, responseLen, &cc, &fields);

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmEventLogRecordV2 decodeFirstHandle failed: rc={RC}, cc={CC}",
            "RC", rc, "CC", cc);
        return cc ? cc : rc;
    }

    lg2::debug(
        "NsmEventLogRecordV2 first_handle: nextTransferHandle={NTH}, eventHandle={EH}, msgType={MT}, eventId={EID}, eventDataLen={LEN}",
        "NTH", fields.next_transfer_handle, "EH", fields.event_handle, "MT",
        fields.nvidia_message_type, "EID", fields.event_id, "LEN",
        fields.event_data_len);

    if (fields.event_data != nullptr && fields.event_data_len > 0)
    {
        eventData.insert(eventData.end(), fields.event_data,
                         fields.event_data + fields.event_data_len);
    }
    transferHandle = fields.next_transfer_handle;
    nextEventHandle = fields.event_handle;

    return NSM_SW_SUCCESS;
}

uint8_t NsmEventLogRecordV2::decodeNextHandleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    struct nsm_event_log_record_v2_next_fields fields{};

    auto rc = decode_nsm_get_event_log_record_v2_resp_next_handle(
        responseMsg, responseLen, &cc, &fields);

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmEventLogRecordV2 decodeNextHandle failed: rc={RC}, cc={CC}",
            "RC", rc, "CC", cc);
        return cc ? cc : rc;
    }

    lg2::debug(
        "NsmEventLogRecordV2 next_handle: nextTransferHandle={NTH}, eventHandle={EH}, eventDataLen={LEN}",
        "NTH", fields.next_transfer_handle, "EH", fields.event_handle, "LEN",
        fields.event_data_len);

    if (fields.event_data != nullptr && fields.event_data_len > 0)
    {
        eventData.insert(eventData.end(), fields.event_data,
                         fields.event_data + fields.event_data_len);
    }
    transferHandle = fields.next_transfer_handle;
    nextEventHandle = fields.event_handle;

    return NSM_SW_SUCCESS;
}

uint8_t NsmEventLogRecordV2::handleGetModeResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t rc = (transferHandle == 0)
                     ? decodeFirstHandleResponseMsg(responseMsg, responseLen)
                     : decodeNextHandleResponseMsg(responseMsg, responseLen);

    if (rc != NSM_SW_SUCCESS)
    {
        eventData.clear(); // Avoid Logging Corrupted Data
        recordReadAttempts++;
        lg2::error(
            "NsmEventLogRecordV2 handleGetModeResponseMsg failed: rc={RC}, attempts={ATTEMPTS}",
            "RC", rc, "ATTEMPTS", recordReadAttempts);
        if (recordReadAttempts >= MAX_RECORD_READ_ATTEMPTS)
        {
            // Try to ACK Record
            mode = NSM_EVENT_LOG_V2_MODE_ACKNOWLEDGEMENT;
            transferHandle = 0;
        }
        else
        {
            // Retry from beginning
            transferHandle = 0;
        }
        return NSM_SW_SUCCESS;
    }

    if (transferHandle == 0)
    {
        mode = NSM_EVENT_LOG_V2_MODE_ACKNOWLEDGEMENT;
    }

    return NSM_SW_SUCCESS;
}

uint8_t NsmEventLogRecordV2::handleAckModeResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t rc = decodeNextHandleResponseMsg(responseMsg, responseLen);
    if (rc != NSM_SW_SUCCESS)
    {
        // No need to clear record data, as Ack doesn't fetch CPER report
        recordAckAttempts++;
        if (recordAckAttempts >= MAX_RECORD_ACK_ATTEMPTS)
        {
            // No retry, log error
            lg2::error(
                "NsmEventLogRecordV2 handleAckModeResponseMsg failed: rc={RC}, attempts={ATTEMPTS}",
                "RC", rc, "ATTEMPTS", recordAckAttempts);
        }
        else
        {
            // Retry if all the retries are not exhausted
            transferHandle = 0;
            mode = NSM_EVENT_LOG_V2_MODE_ACKNOWLEDGEMENT;
            return NSM_SW_SUCCESS;
        }
    }
    stopPollingAndLogRecord(rc);
    return NSM_SW_SUCCESS;
}

uint8_t
    NsmEventLogRecordV2::handleResponseMsg(const struct nsm_msg* responseMsg,
                                           size_t responseLen)
{
    uint8_t rc = NSM_SW_SUCCESS;
    if (mode == NSM_EVENT_LOG_V2_MODE_GET_DATA)
    {
        rc = handleGetModeResponseMsg(responseMsg, responseLen);
    }
    else if (mode == NSM_EVENT_LOG_V2_MODE_ACKNOWLEDGEMENT)
    {
        rc = handleAckModeResponseMsg(responseMsg, responseLen);
    }
    return rc;
}

bool NsmEventLogRecordV2::isRecordCollectionInProgress() const
{
    return recordCollectionInProgress;
}

void NsmEventLogRecordV2::markRecordCollectionComplete()
{
    recordCollectionInProgress = false;
}

void NsmEventLogRecordV2::stopPollingAndLogRecord(uint8_t rc)
{
    shouldPollForData = false;
    auto cperEventPtr = cperEvent.lock();
    if (cperEventPtr)
    {
        cperEventPtr->logRecordOnRf(eventData, rc, nextEventHandle);
    }
}

/**
 * @brief Trigger the record chunk collection
 *
 * Resets the record collection state and starts the collection process.
 *
 * @param eventHandle    The event handle to collect the record chunk for
 * @param transferHandle The transfer handle to collect the record chunk for
 */
void NsmEventLogRecordV2::triggerRecordChunkCollection(uint16_t eventHandle,
                                                       uint16_t transferHandle)
{
    eventData.clear();
    mode = NSM_EVENT_LOG_V2_MODE_GET_DATA;
    recordCollectionInProgress = true;
    this->eventHandle = eventHandle;
    this->transferHandle = transferHandle;
    shouldPollForData = true;
    recordReadAttempts = 0;
    recordAckAttempts = 0;
}

} // namespace nsm
