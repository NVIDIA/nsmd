/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "nsmCPEREvent.hpp"
#include "nsmSensor.hpp"

#include <memory>

namespace nsm
{

inline constexpr uint16_t SINGLE_EVENT_RECORD_SIZE_KB = 4;
inline constexpr uint8_t MAX_RECORD_READ_ATTEMPTS = 2;
inline constexpr uint8_t MAX_RECORD_ACK_ATTEMPTS = 2;

class NsmCPEREvent;

class NsmEventLogRecordV2 : public NsmSensor
{
  public:
    NsmEventLogRecordV2(const std::string& name, const std::string& type,
                        uint8_t mode, uint16_t eventHandle,
                        std::shared_ptr<NsmCPEREvent> cperEvent);
    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    uint8_t handleGetModeResponseMsg(const struct nsm_msg* responseMsg,
                                     size_t responseLen);

    uint8_t handleAckModeResponseMsg(const struct nsm_msg* responseMsg,
                                     size_t responseLen);

    uint8_t decodeFirstHandleResponseMsg(const struct nsm_msg* responseMsg,
                                         size_t responseLen);
    uint8_t decodeNextHandleResponseMsg(const struct nsm_msg* responseMsg,
                                        size_t responseLen);

    bool isRecordCollectionInProgress() const;
    void triggerRecordChunkCollection(uint16_t eventHandle,
                                      uint16_t transferHandle);
    bool needsUpdate(const uint64_t& currentTimestampInUsec) const override;
    void markRecordCollectionComplete();
    void stopPollingAndLogRecord(uint8_t rc);

  private:
    uint8_t mode;
    uint16_t eventHandle;
    uint16_t transferHandle = 0;
    bool recordCollectionInProgress = false;
    bool shouldPollForData = false;
    std::weak_ptr<NsmCPEREvent> cperEvent;
    std::vector<uint8_t> eventData;
    uint8_t recordReadAttempts = 0;
    uint8_t recordAckAttempts = 0;
    uint16_t nextEventHandle = NO_MORE_HANDLES;
};

} // namespace nsm
