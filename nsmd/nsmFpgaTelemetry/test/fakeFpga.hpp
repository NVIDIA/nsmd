// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#pragma once

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "device-configuration.h"
#include "fpga/fpga-controller.h"
#include "fpga/fpga-tables.h"

#include "nsmFpgaController.hpp"
#include "nsmFpgaSensors.hpp"
#include "nsmFpgaStatus.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <vector>

namespace
{
using Record = std::array<uint8_t, NSM_FPGA_RECORD_SIZE>;

// The detached idle watcher retains its controller and status until its timer
// runs. Drain it before another test registers the same D-Bus object path.
inline void drainFpgaWatchers(
    const std::vector<std::weak_ptr<nsm::NsmFpgaStatus>>& statuses)
{
    const auto expired = [&statuses] {
        return std::all_of(statuses.begin(), statuses.end(),
                           [](const auto& status) { return status.expired(); });
    };
    auto event = sdeventplus::Event::get_default();
    const auto timeout = std::chrono::microseconds(
        2 * nsm::NsmFpgaController::idleReleaseUsec + 1000000);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!expired() && std::chrono::steady_clock::now() < deadline)
    {
        if (sd_event_run(event.get(), 10000) < 0)
        {
            break;
        }
    }
    EXPECT_TRUE(expired()) << "FPGA status retained after test cleanup";
}

static uint64_t getLe(const uint8_t* p, size_t bytes)
{
    uint64_t value = 0;
    for (size_t i = 0; i < bytes; ++i)
    {
        value |= uint64_t{p[i]} << (8 * i);
    }
    return value;
}

// Scripted FPGA behind the Type 5 MMIO window of the P0 map.
struct FakeFpga
{
    const nsm_fpga_window* window = &nsm_fpga_map_p0()->window;
    // Word returned per CSR address, calibration word 1 (passed) by default
    std::map<uint64_t, uint32_t> words{{0x05000400ULL, 1}};
    uint8_t acquireResult = NSM_FPGA_CTRL_OK;
    uint8_t recoverResult = NSM_FPGA_CTRL_OK;
    uint8_t authorityOwner = NSM_FPGA_OWNER_NONE;
    size_t pendingCompletions = 0;
    // Exchange index (1-based) whose answer is lost in transport, 0 = none
    size_t loseExchange = 0;
    // Exchange index answered with the no-session completion, 0 = none
    size_t noSessionExchange = 0;
    // Authority status: activated epoch, last grant, bank flags
    uint64_t authorityEpoch = 0x68e7a1c0;
    uint32_t authorityGeneration = 3;
    uint8_t authorityFlags = NSM_FPGA_AUTH_INITIALIZED |
                             NSM_FPGA_AUTH_AVAILABLE | NSM_FPGA_AUTH_RELEASED;
    // Called after a WRITE32 lands in words, with its address and value
    std::function<void(uint64_t, uint32_t)> onWrite;
    // Called after a READ32 completion is answered, with the CSR address
    std::function<void(uint64_t)> onRead;
    // Completions answered with a FAULT before success resumes
    size_t faultCompletions = 0;
    // Completions answered with a record of garbage bytes before success
    size_t corruptCompletions = 0;
    // Exchange index (1-based) whose GET carries too few data bytes, 0 = none
    size_t shortReplyExchange = 0;
    // Byte ops answered with a target NACK
    bool nackBytes = false;
    // Data byte every successful byte read returns
    uint8_t byteData = 0;
    // Scripted I2C bus: answers a byte op's argument with data and NACK
    std::function<void(uint32_t, uint8_t&, bool&)> onByte;

    // One byte of a bounded transaction, through the same scripted bus
    bool busByte(uint32_t argument, uint8_t& data)
    {
        data = byteData;
        bool nack = nackBytes;
        if (onByte)
        {
            onByte(argument, data, nack);
        }
        return !nack;
    }

    // A bounded transaction: writes, repeated start, reads, NACK and STOP
    Record transaction(Record r, uint8_t opcode, uint32_t argument)
    {
        const uint8_t writes = (opcode >> 3) & 0x3;
        const uint8_t reads = opcode & 0x7;
        const uint32_t device = argument & 0x7f;
        uint32_t payload = 0;
        uint8_t done = 0;
        for (uint8_t i = 0; i < writes; ++i)
        {
            const uint32_t byte = (argument >> (8 * (i + 1))) & 0xff;
            uint32_t flags = (i == 0 ? NSM_FPGA_I2C_START : 0) |
                             (reads == 0 && i + 1 == writes ? NSM_FPGA_I2C_STOP
                                                            : 0);
            uint8_t data = 0;
            if (!busByte(device | (byte << 8) | (flags << 16), data))
            {
                r[4] = NSM_FPGA_RESULT_TARGET_NACK;
                r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
                       NSM_FPGA_FLAG_BUS_RELEASED;
                r[28] = 1;
                r[29] = done;
                return r;
            }
            done++;
        }
        for (uint8_t i = 0; i < reads; ++i)
        {
            const bool last = i + 1 == reads;
            uint32_t flags =
                NSM_FPGA_I2C_READ | (i == 0 ? NSM_FPGA_I2C_START : 0) |
                (last ? NSM_FPGA_I2C_STOP | NSM_FPGA_I2C_READ_NACK : 0);
            uint8_t data = 0;
            if (!busByte(device | (flags << 16), data))
            {
                r[4] = NSM_FPGA_RESULT_TARGET_NACK;
                r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
                       NSM_FPGA_FLAG_BUS_RELEASED;
                r[28] = 1;
                r[29] = static_cast<uint8_t>(writes | (i << 4));
                return r;
            }
            payload |= static_cast<uint32_t>(data) << (8 * i);
        }
        r[4] = NSM_FPGA_RESULT_SUCCESS;
        r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
               NSM_FPGA_FLAG_BUS_RELEASED |
               (reads ? NSM_FPGA_FLAG_READ_VALID : 0);
        for (size_t i = 0; i < 4; ++i)
        {
            r[28 + i] = static_cast<uint8_t>(payload >> (8 * i));
        }
        return r;
    }
    size_t exchanges = 0;
    size_t acquires = 0;
    size_t renews = 0;
    size_t releases = 0;
    size_t recovers = 0;
    Record control{};
    // The last header record, as sent for a byte or transaction op and as
    // unfolded from the word record for a word op
    Record header{};
    // Station address and data of the last word record
    uint64_t wordAddress = 0;
    uint32_t wordData = 0;

    Record authorityReply()
    {
        Record r{};
        for (size_t i = 0; i < 4; ++i)
        {
            r[i] = static_cast<uint8_t>(NSM_FPGA_PROFILE_IBC2 >> (8 * i));
        }
        r[4] = 1;
        r[5] = authorityFlags;
        r[6] = authorityOwner;
        for (size_t i = 0; i < 8; ++i)
        {
            r[8 + i] = static_cast<uint8_t>(authorityEpoch >> (8 * i));
        }
        for (size_t i = 0; i < 4; ++i)
        {
            r[20 + i] = static_cast<uint8_t>(authorityGeneration >> (8 * i));
        }
        return r;
    }

    Record controlReply()
    {
        const uint8_t opcode = control[4];
        Record r{};
        std::memcpy(r.data(), control.data(), 4);
        r[4] = opcode;
        r[5] = opcode == NSM_FPGA_CTRL_ACQUIRE   ? acquireResult
               : opcode == NSM_FPGA_CTRL_RECOVER ? recoverResult
                                                 : uint8_t{NSM_FPGA_CTRL_OK};
        // The bank rejects an ACQUIRE outside its epoch or generation order
        if (opcode == NSM_FPGA_CTRL_ACQUIRE &&
            (getLe(control.data() + 8, 8) != authorityEpoch ||
             getLe(control.data() + 20, 4) != authorityGeneration + 1))
        {
            r[5] = NSM_FPGA_CTRL_STALE;
        }
        r[6] = opcode == NSM_FPGA_CTRL_RECOVER ? NSM_FPGA_OWNER_NONE
                                               : NSM_FPGA_OWNER_NSM;
        r[7] = 1;
        std::memcpy(r.data() + 8, control.data() + 8, 24);
        return r;
    }

    Record completionReply()
    {
        if (corruptCompletions > 0)
        {
            // Not a record at all: no profile word, every byte 0xa5
            corruptCompletions--;
            Record garbage{};
            garbage.fill(0xa5);
            return garbage;
        }
        Record r{};
        std::memcpy(r.data(), header.data(), 4);
        r[6] = NSM_FPGA_OWNER_NSM;
        r[7] = header[4];
        std::memcpy(r.data() + 8, header.data() + 8, 20);
        if (pendingCompletions > 0)
        {
            pendingCompletions--;
            r[4] = NSM_FPGA_RESULT_PENDING;
            r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_PENDING;
            return r;
        }
        if (faultCompletions > 0)
        {
            faultCompletions--;
            r[4] = NSM_FPGA_RESULT_FAULT;
            r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
                   NSM_FPGA_FLAG_FAULTED;
            r[29] = 0x2;
            return r;
        }
        if (header[4] == NSM_FPGA_OP_WRITE32)
        {
            // A write completes quiescent with no read data
            r[4] = NSM_FPGA_RESULT_SUCCESS;
            r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT;
            return r;
        }
        if (nsm_fpga_xfer_opcode_valid(header[4]))
        {
            return transaction(
                r, header[4],
                static_cast<uint32_t>(getLe(header.data() + 28, 4)));
        }
        if (header[4] == NSM_FPGA_OP_BYTE)
        {
            // A byte op: the argument word sits at bytes 28 to 31 of the header
            const auto argument =
                static_cast<uint32_t>(getLe(header.data() + 28, 4));
            const bool read = ((argument >> 16) & 1) != 0;
            const bool stop = ((argument >> 18) & 1) != 0;
            uint8_t data = byteData;
            bool nack = nackBytes;
            if (onByte)
            {
                onByte(argument, data, nack);
            }
            if (nack)
            {
                r[4] = NSM_FPGA_RESULT_TARGET_NACK;
                r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
                       NSM_FPGA_FLAG_BUS_RELEASED;
                return r;
            }
            r[4] = NSM_FPGA_RESULT_SUCCESS;
            r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
                   (read ? NSM_FPGA_FLAG_READ_VALID : 0) |
                   (stop ? NSM_FPGA_FLAG_BUS_RELEASED : NSM_FPGA_FLAG_BUS_HELD);
            r[28] = read ? data : 0;
            return r;
        }
        r[4] = NSM_FPGA_RESULT_SUCCESS;
        r[5] = NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_QUIESCENT |
               NSM_FPGA_FLAG_READ_VALID;
        const uint64_t address = wordAddress;
        const auto it = words.find(address);
        const uint32_t value = it == words.end() ? 0 : it->second;
        for (size_t i = 0; i < 4; ++i)
        {
            r[28 + i] = static_cast<uint8_t>(value >> (8 * i));
        }
        if (onRead && header[4] == NSM_FPGA_OP_READ32)
        {
            onRead(address);
        }
        return r;
    }

    requester::Coroutine io(eid_t, Request& request,
                            std::shared_ptr<const nsm_msg>& responseMsg,
                            size_t& responseLen, bool)
    {
        exchanges++;
        if (exchanges == loseExchange)
        {
            // coverity[missing_return]
            co_return NSM_SW_ERROR_TIMEOUT;
        }
        auto msg = reinterpret_cast<const nsm_msg*>(request.data());
        const bool write = msg->payload[0] == NSM_SET_DEVICE_CONFIG_V2;
        uint32_t type = 0;
        std::vector<uint8_t> body(request.size());
        uint16_t bodyLen = 0;
        int rc = write ? decode_set_device_config_v2_req(
                             msg, request.size(), &type, body.data(), &bodyLen)
                       : decode_get_device_config_v2_req(
                             msg, request.size(), &type, body.data(), &bodyLen);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        EXPECT_EQ(type, 2u);
        const uint64_t address = getLe(body.data(), 8);
        std::vector<uint8_t> response;
        uint8_t cc = NSM_SUCCESS;
        uint16_t reason = ERR_NULL;
        if (exchanges == noSessionExchange)
        {
            cc = NSM_ERROR;
            reason = NSM_FPGA_REASON_NO_SESSION;
        }
        if (write)
        {
            EXPECT_EQ(bodyLen, 12 + NSM_FPGA_RECORD_SIZE);
            Record record{};
            std::memcpy(record.data(), body.data() + 12, NSM_FPGA_RECORD_SIZE);
            if (address == window->control_write)
            {
                control = record;
                acquires += record[4] == NSM_FPGA_CTRL_ACQUIRE;
                renews += record[4] == NSM_FPGA_CTRL_RENEW;
                releases += record[4] == NSM_FPGA_CTRL_RELEASE;
                if (record[4] == NSM_FPGA_CTRL_RECOVER)
                {
                    recovers++;
                    // An accepted RECOVER activates the fresh epoch
                    if (recoverResult == NSM_FPGA_CTRL_OK)
                    {
                        authorityEpoch = getLe(record.data() + 8, 8);
                        authorityFlags = NSM_FPGA_AUTH_INITIALIZED |
                                         NSM_FPGA_AUTH_AVAILABLE |
                                         NSM_FPGA_AUTH_RELEASED;
                        authorityOwner = NSM_FPGA_OWNER_NONE;
                    }
                }
            }
            else if (address == window->compact)
            {
                // Unfold the word record into the header view the
                // completion is built from: identity, station, opcode
                header = Record{};
                for (size_t i = 0; i < 4; ++i)
                {
                    header[i] =
                        static_cast<uint8_t>(NSM_FPGA_PROFILE_IBC2 >> (8 * i));
                }
                header[4] = record[21];
                header[5] = record[20];
                header[6] = record[22];
                std::memcpy(header.data() + 8, record.data(), 20);
                wordAddress = getLe(record.data() + 24, 4);
                wordData = static_cast<uint32_t>(getLe(record.data() + 28, 4));
                if (record[21] == NSM_FPGA_OP_WRITE32)
                {
                    words[wordAddress] = wordData;
                    if (onWrite)
                    {
                        onWrite(wordAddress, wordData);
                    }
                }
            }
            else if (address == window->data_write)
            {
                header = record;
            }
            else
            {
                ADD_FAILURE() << "write to unexpected address " << address;
            }
            response.resize(sizeof(nsm_msg_hdr) +
                            (cc == NSM_SUCCESS
                                 ? sizeof(nsm_common_resp)
                                 : sizeof(nsm_common_non_success_resp)));
            encode_set_device_config_v2_resp(
                0, cc, reason, reinterpret_cast<nsm_msg*>(response.data()));
        }
        else
        {
            Record reply{};
            if (address == window->status)
            {
                reply = authorityReply();
            }
            else if (address == window->control_read)
            {
                reply = controlReply();
            }
            else if (address == window->completion_read)
            {
                reply = completionReply();
            }
            else
            {
                ADD_FAILURE() << "read from unexpected address " << address;
            }
            if (cc == NSM_SUCCESS)
            {
                // A short reply carries 8 of the 32 record bytes
                const size_t dataLen =
                    exchanges == shortReplyExchange ? 8 : reply.size();
                response.resize(sizeof(nsm_msg_hdr) +
                                sizeof(nsm_get_device_config_v2_resp) - 1 +
                                dataLen);
                encode_get_device_config_v2_resp(
                    0, cc, reason, reply.data(), static_cast<uint16_t>(dataLen),
                    nullptr, 0, reinterpret_cast<nsm_msg*>(response.data()));
            }
            else
            {
                response.resize(sizeof(nsm_msg_hdr) +
                                sizeof(nsm_common_non_success_resp));
                encode_get_device_config_v2_resp(
                    0, cc, reason, nullptr, 0, nullptr, 0,
                    reinterpret_cast<nsm_msg*>(response.data()));
            }
        }
        responseLen = response.size();
        auto copy = std::shared_ptr<nsm_msg>(
            reinterpret_cast<nsm_msg*>(malloc(responseLen)),
            [](nsm_msg* p) { free(p); });
        std::memcpy(copy.get(), response.data(), responseLen);
        responseMsg = copy;
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
};

// Test-only rules on the word stations of the P0 map, one per rule kind
const nsm_fpga_state_entry testStates[] = {
    {0x7, 0, NSM_FPGA_STATE_ABSENT, NSM_FPGA_HEALTH_OK},
    {0x7, 1, NSM_FPGA_STATE_ENABLED, NSM_FPGA_HEALTH_OK},
    {0x7, 2, NSM_FPGA_STATE_FAULT, NSM_FPGA_HEALTH_CRITICAL},
    {0x7, 4, NSM_FPGA_STATE_STARTING, NSM_FPGA_HEALTH_OK},
    {0, 0, NSM_FPGA_STATE_UNAVAILABLE_OFFLINE, NSM_FPGA_HEALTH_WARNING}};
const nsm_fpga_command telemetryCmds[] = {{
    .op = NSM_FPGA_OP_READ32,
    .station = 29,
    .u = {.csr = {0x05000400ULL, 0}},
}};
const nsm_fpga_command stateCmds[] = {{
    .op = NSM_FPGA_OP_READ32,
    .station = 31,
    .u = {.csr = {0x05000400ULL, 0}},
}};
const nsm_fpga_command secondStateCmds[] = {{
    .op = NSM_FPGA_OP_READ32,
    .station = 34,
    .u = {.csr = {0x05000400ULL, 0}},
}};
const nsm_fpga_command numericCmds[] = {{
    .op = NSM_FPGA_OP_READ32,
    .station = 29,
    .u = {.csr = {0x05000420ULL, 0}},
}};
// Three captured words: a serial number, then eight bytes of part text
const nsm_fpga_command identityCmds[] = {{.op = NSM_FPGA_OP_READ32,
                                          .station = 29,
                                          .capture = NSM_FPGA_BUF_ITEM,
                                          .u = {.csr = {0x05000600ULL, 0}}},
                                         {.op = NSM_FPGA_OP_READ32,
                                          .station = 29,
                                          .capture = NSM_FPGA_BUF_ITEM,
                                          .u = {.csr = {0x05000604ULL, 0}}},
                                         {.op = NSM_FPGA_OP_READ32,
                                          .station = 29,
                                          .capture = NSM_FPGA_BUF_ITEM,
                                          .u = {.csr = {0x05000608ULL, 0}}}};
const nsm_fpga_identity_field identityFields[] = {
    {.asset = NSM_FPGA_ASSET_SERIAL,
     .format = NSM_FPGA_IDENTITY_HEX_LE,
     .buffer = NSM_FPGA_BUF_ITEM,
     .offset = 0,
     .length = 4},
    {.asset = NSM_FPGA_ASSET_PART,
     .format = NSM_FPGA_IDENTITY_ASCII,
     .buffer = NSM_FPGA_BUF_ITEM,
     .offset = 4,
     .length = 8}};
// Lane vectors: one word of per-lane flags, two words of 16 bit powers
const nsm_fpga_command txFaultCmds[] = {{.op = NSM_FPGA_OP_READ32,
                                         .station = 29,
                                         .capture = NSM_FPGA_BUF_ITEM,
                                         .u = {.csr = {0x05000610ULL, 0}}}};
const nsm_fpga_command rxLosCmds[] = {{.op = NSM_FPGA_OP_READ32,
                                       .station = 29,
                                       .capture = NSM_FPGA_BUF_ITEM,
                                       .u = {.csr = {0x05000614ULL, 0}}}};
const nsm_fpga_command txPowerCmds[] = {{.op = NSM_FPGA_OP_READ32,
                                         .station = 29,
                                         .capture = NSM_FPGA_BUF_ITEM,
                                         .u = {.csr = {0x05000620ULL, 0}}},
                                        {.op = NSM_FPGA_OP_READ32,
                                         .station = 29,
                                         .capture = NSM_FPGA_BUF_ITEM,
                                         .u = {.csr = {0x05000624ULL, 0}}}};
const nsm_fpga_command rxPowerCmds[] = {{.op = NSM_FPGA_OP_READ32,
                                         .station = 29,
                                         .capture = NSM_FPGA_BUF_ITEM,
                                         .u = {.csr = {0x05000628ULL, 0}}},
                                        {.op = NSM_FPGA_OP_READ32,
                                         .station = 29,
                                         .capture = NSM_FPGA_BUF_ITEM,
                                         .u = {.csr = {0x0500062cULL, 0}}}};
const nsm_fpga_rule telemetryRule{
    .name = "Telemetry_State",
    .commands = telemetryCmds,
    .command_count = 1,
    .kind = NSM_FPGA_RULE_STATE,
    .group = "Test_Group",
    .states = testStates,
    .state_count = 5,
};
const nsm_fpga_rule stateRule{
    .name = "Test_State",
    .commands = stateCmds,
    .command_count = 1,
    .kind = NSM_FPGA_RULE_STATE,
    .group = "Test_Group",
    .states = testStates,
    .state_count = 5,
};
const nsm_fpga_rule secondStateRule{
    .name = "Test_State_B",
    .commands = secondStateCmds,
    .command_count = 1,
    .kind = NSM_FPGA_RULE_STATE,
    .group = "Test_Group",
    .states = testStates,
    .state_count = 5,
};
const nsm_fpga_rule numericRule{
    .name = "Test_Temp_0",
    .commands = numericCmds,
    .command_count = 1,
    .kind = NSM_FPGA_RULE_NUMERIC,
    .numeric = {.sensor_type = "temperature",
                .format = NSM_FPGA_FORMAT_FIXED,
                .buffer = NSM_FPGA_BUF_ITEM,
                .bit_width = 32,
                .scale = 0.5,
                .bias = 0.0,
                .min_value = std::numeric_limits<double>::quiet_NaN(),
                .max_value = 100.0},
    .group = "Test_Group",
};
const nsm_fpga_rule identityRule{
    .name = "Test_Inventory",
    .commands = identityCmds,
    .command_count = 3,
    .kind = NSM_FPGA_RULE_IDENTITY,
    .group = "Test_Group",
    .identity = identityFields,
    .identity_count = 2,
};
constexpr nsm_fpga_numeric_spec laneFlags(uint8_t vector)
{
    return {.format = NSM_FPGA_FORMAT_FIXED,
            .buffer = NSM_FPGA_BUF_ITEM,
            .bit_width = 1,
            .scale = 1.0,
            .min_value = std::numeric_limits<double>::quiet_NaN(),
            .max_value = std::numeric_limits<double>::quiet_NaN(),
            .element_count = 8,
            .stride_bits = 1,
            .vector = vector};
}
constexpr nsm_fpga_numeric_spec lanePower(uint8_t vector)
{
    return {.format = NSM_FPGA_FORMAT_U16BE,
            .buffer = NSM_FPGA_BUF_ITEM,
            .scale = 0.5,
            .min_value = std::numeric_limits<double>::quiet_NaN(),
            .max_value = std::numeric_limits<double>::quiet_NaN(),
            .element_count = 4,
            .stride_bits = 16,
            .vector = vector};
}
const nsm_fpga_rule txFaultRule{.name = "Test_Tx_Fault",
                                .commands = txFaultCmds,
                                .command_count = 1,
                                .kind = NSM_FPGA_RULE_ARRAY,
                                .numeric = laneFlags(NSM_FPGA_VECTOR_TX_FAULT),
                                .group = "Test_Group"};
const nsm_fpga_rule rxLosRule{.name = "Test_Rx_Los",
                              .commands = rxLosCmds,
                              .command_count = 1,
                              .kind = NSM_FPGA_RULE_ARRAY,
                              .numeric = laneFlags(NSM_FPGA_VECTOR_RX_LOS),
                              .group = "Test_Group"};
const nsm_fpga_rule txPowerRule{.name = "Test_Tx_Power",
                                .commands = txPowerCmds,
                                .command_count = 2,
                                .kind = NSM_FPGA_RULE_ARRAY,
                                .numeric = lanePower(NSM_FPGA_VECTOR_TX_POWER),
                                .group = "Test_Group"};
const nsm_fpga_rule rxPowerRule{.name = "Test_Rx_Power",
                                .commands = rxPowerCmds,
                                .command_count = 2,
                                .kind = NSM_FPGA_RULE_ARRAY,
                                .numeric = lanePower(NSM_FPGA_VECTOR_RX_POWER),
                                .group = "Test_Group"};
} // namespace
