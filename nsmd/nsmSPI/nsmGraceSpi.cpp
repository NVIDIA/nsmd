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

#ifdef ENABLE_GRACE_SPI_OPERATIONS

#include "nsmGraceSpi.hpp"

#include <phosphor-logging/lg2.hpp>

#include <chrono> // To set start and end time in progress interface

#define NSM_SPI_PROGRESS_INTERFACE "/xyz/openbmc_project/status/SPI_Operation"

// Constants for reading spi part:
#define FPGA_CACHE_SIZE 30   // Size of the FPGA cache
#define BYTES_FIRST_BLOCK 29 // Number of bytes to read from the first block
#define BYTES_LAST_BLOCK 9   // Number of bytes to read from the last block
#define BLOCKS_TO_READ                                                         \
    5 // Number of blocks to read (128 bytes/30 bytes rounded up)

namespace nsm
{

// Initialize static counter
std::atomic<uint32_t> NsmGraceSpiObject::interfaceCounter{0};

NsmGraceSpiObject::NsmGraceSpiObject(sdbusplus::bus::bus& bus,
                                     const std::string& name,
                                     const std::string& inventoryPath,
                                     const std::string& type,
                                     const uuid_t& uuid) :
    NsmObject(name, type), SpiIntf(bus, (inventoryPath + name).c_str()),
    uuid(uuid)
{
    lg2::debug("NsmGraceSpiObject: {NAME}", "NAME", name.c_str());
    objPath = inventoryPath + name;
    fdName = name + "_read_contents";
}

sdbusplus::message::object_path NsmGraceSpiObject::eraseSpi()
{
    lg2::debug("NsmGraceSpiObject: Erase SPI Requested");

    if (startSpiOperation() != NSM_SW_SUCCESS)
    {
        lg2::error("NsmGraceSpiObject: Erase Unavailable");
        throw Common::Error::Unavailable();
    }

    eraseSpiAsyncHandler().detach();

    return sdbusplus::message::object_path(
        std::string(NSM_SPI_PROGRESS_INTERFACE) + "_" +
        std::to_string(interfaceCounter - 1));
}

sdbusplus::message::object_path NsmGraceSpiObject::readSpi()
{
    lg2::debug("NSMSPI: Starting NSM Read");

    if (startSpiOperation() != NSM_SW_SUCCESS)
    {
        lg2::error("NSMSPI: Read Unavailable");
        throw Common::Error::Unavailable();
    }

    // Close any existing file descriptor before starting new operation
    if (auto currentProgress = getCurrentProgress())
    {
        try
        {
            auto currentFd = currentProgress->spiReadFd();
            if (currentFd.fd >= 0)
            {
                close(currentFd.fd);
            }
            // Reset the file descriptor
            currentProgress->spiReadFd(sdbusplus::message::unix_fd(-1));
        }
        catch (const std::exception& e)
        {
            lg2::error("Failed to clean up previous file descriptor: {ERROR}",
                       "ERROR", e.what());
        }
    }

    readSpiAsyncHandler().detach();

    return sdbusplus::message::object_path(
        std::string(NSM_SPI_PROGRESS_INTERFACE) + "_" +
        std::to_string(interfaceCounter - 1));
}

uint8_t NsmGraceSpiObject::startSpiOperation()
{
    lg2::debug("NsmGraceSpiObject: Starting SPI Operation");
    if (cmdInProgress)
    {
        lg2::error("NsmGraceSpiObject: A command is already in progress");
        return NSM_SW_ERROR;
    }
    cmdInProgress = true;

    // Create unique interface path with incrementing counter.
    // Counter lifecycle:
    // 1. Counter value N is used to create interface
    // "/xyz/openbmc_project/status/SPI_Operation_<N>"
    // 2. Counter is incremented to N+1 via post-increment (counter++)
    // 3. When referencing this interface in eraseSpi()/readSpi(), we use
    // (counter-1)
    //    to get back to value N that was used here
    std::string progressInterface = std::string(NSM_SPI_PROGRESS_INTERFACE) +
                                    "_" + std::to_string(interfaceCounter++);

    // Create new progress object and add to history
    auto newProgress = std::make_unique<SpiProgress>(get_bus(),
                                                     progressInterface.c_str());

    // Remove oldest entry if we've reached max size
    if (progressHistory.size() >= MAX_PROGRESS_HISTORY)
    {
        progressHistory.pop_front();
    }

    // Initialize the new progress object
    newProgress->startTime(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    newProgress->status(SpiProgress::OperationStatus::InProgress);
    newProgress->progress(0);
    newProgress->spiReadFd(sdbusplus::message::unix_fd(-1));

    // Add to history
    progressHistory.push_back(std::move(newProgress));

    return NSM_SW_SUCCESS;
}

void NsmGraceSpiObject::finishSpiOperation(
    SpiProgress::OperationStatus opStatus)
{
    lg2::debug("NsmGraceSpiObject: Finishing SPI Operation");

    if (auto currentProgress = getCurrentProgress())
    {
        currentProgress->status(opStatus);
        currentProgress->progress(100);
        currentProgress->completedTime(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }
    cmdInProgress = false;
}

requester::Coroutine
    NsmGraceSpiObject::checkSpiStatus(std::shared_ptr<NsmDevice> device,
                                      enum nsm_spi_status* status)
{
    lg2::debug("NsmGraceSpiObject: Check spi bus status");

    if (status == NULL)
    {
        co_return NSM_SW_ERROR_NULL;
    }

    Request spiStatus(sizeof(nsm_msg_hdr) + sizeof(nsm_read_spi_status_req));

    auto spiStatusMsg = reinterpret_cast<struct nsm_msg*>(spiStatus.data());

    auto rc = encode_read_spi_status_req(0, spiStatusMsg);

#ifdef ENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Tx, std::vector<unsigned char>(
                       spiStatus.data(),
                       spiStatus.data() + (sizeof(nsm_msg_hdr) +
                                           sizeof(nsm_read_spi_status_req))));
#endif

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmGraceSpi encode_read_spi_status_req failed. eid={EID} rc={RC}",
            "EID", device->getEid(), "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> spiStatusResponseMsg;
    size_t spiStatusResponseLen = 0;

    rc = co_await device->postPatchIO(device->getEid(), spiStatus,
                                      spiStatusResponseMsg,
                                      spiStatusResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmGraceSpi postPatchNsmCommand for Spi transaction failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", device->getEid());
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_read_spi_status_resp(spiStatusResponseMsg.get(),
                                     spiStatusResponseLen, &cc, &reasonCode,
                                     status);

#ifdef ENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Rx,
        std::vector<unsigned char>(
            reinterpret_cast<const uint8_t*>(spiStatusResponseMsg.get()),
            reinterpret_cast<const uint8_t*>(spiStatusResponseMsg.get()) +
                (sizeof(nsm_msg_hdr) + sizeof(nsm_read_spi_status_resp))));
#endif

    LG2_ERROR_FLT(
        "decode_read_spi_status_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);

    // coverity[missing_return]
    co_return cc ? cc : rc;
}

requester::Coroutine
    NsmGraceSpiObject::checkIfWriteComplete(std::shared_ptr<NsmDevice> device,
                                            bool* writeComplete)
{
    lg2::debug("NsmGraceSpiObject: Check spi write operation status");

    if (writeComplete == NULL)
    {
        co_return NSM_SW_ERROR_NULL;
    }

    auto rc = co_await requestSpiStatusRegister(device);

    Request spiStatus(sizeof(nsm_msg_hdr) + sizeof(nsm_read_spi_block_req));

    auto spiStatusMsg = reinterpret_cast<struct nsm_msg*>(spiStatus.data());

    rc = encode_read_spi_block_req(0, spiStatusMsg, 0);

#ifdef ENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Tx, std::vector<unsigned char>(
                       spiStatus.data(),
                       spiStatus.data() + (sizeof(nsm_msg_hdr) +
                                           sizeof(nsm_read_spi_block_req))));
#endif

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmGraceSpiObject encode_read_spi_block_req failed. eid={EID} rc={RC}",
            "EID", device->getEid(), "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> spiStatusResponseMsg;
    size_t spiStatusResponseLen = 0;

    rc = co_await device->postPatchIO(device->getEid(), spiStatus,
                                      spiStatusResponseMsg,
                                      spiStatusResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmGraceSpiObject postPatchNsmCommand for Spi transaction failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", device->getEid());
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    uint8_t data[30];

    rc = decode_read_spi_block_resp(spiStatusResponseMsg.get(),
                                    spiStatusResponseLen, &cc, &reasonCode,
                                    data, 30);

#ifdef ENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Rx,
        std::vector<unsigned char>(
            reinterpret_cast<const uint8_t*>(spiStatusResponseMsg.get()),
            reinterpret_cast<const uint8_t*>(spiStatusResponseMsg.get()) +
                (sizeof(nsm_msg_hdr) + sizeof(nsm_read_spi_block_resp))));
#endif

    *writeComplete = !(data[1] & 0x01);

    LG2_ERROR_FLT(
        "decode_read_spi_block_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);

    // coverity[missing_return]
    co_return cc ? cc : rc;
}

requester::Coroutine NsmGraceSpiObject::executeSpiTransaction(
    std::shared_ptr<NsmDevice> device, uint16_t writeBytes, uint16_t readBytes)
{
    lg2::debug("NsmGraceSpiObject: Executing spi transaction");

    Request spiTransaction(sizeof(nsm_msg_hdr) +
                           sizeof(nsm_send_spi_transaction_req));

    auto spiTransactionMsg =
        reinterpret_cast<struct nsm_msg*>(spiTransaction.data());

    auto rc = encode_send_spi_transaction_req(0, spiTransactionMsg, writeBytes,
                                              readBytes);

#ifdef ENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Tx,
        std::vector<unsigned char>(
            spiTransaction.data(),
            spiTransaction.data() +
                (sizeof(nsm_msg_hdr) + sizeof(nsm_send_spi_transaction_req))));
#endif

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmGraceSpiObject encode_send_spi_transaction_req failed. eid={EID} rc={RC}",
            "EID", device->getEid(), "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> spiTransactionResponseMsg;
    size_t spiTransactionResponseLen = 0;

    rc = co_await device->postPatchIO(device->getEid(), spiTransaction,
                                      spiTransactionResponseMsg,
                                      spiTransactionResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmGraceSpiObject postPatchNsmCommand for Spi transaction failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", device->getEid());
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_send_spi_transaction_resp(spiTransactionResponseMsg.get(),
                                          spiTransactionResponseLen, &cc,
                                          &reasonCode);

#ifdef ENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Rx,
        std::vector<unsigned char>(
            reinterpret_cast<const uint8_t*>(spiTransactionResponseMsg.get()),
            reinterpret_cast<const uint8_t*>(spiTransactionResponseMsg.get()) +
                (sizeof(nsm_msg_hdr) + sizeof(nsm_send_spi_transaction_resp))));
#endif

    LG2_ERROR_FLT(
        "decode_send_spi_transaction_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);

    // coverity[missing_return]
    co_return cc ? cc : rc;
}

requester::Coroutine
    NsmGraceSpiObject::sendSpiDataRequest(std::shared_ptr<NsmDevice> device,
                                          enum nsm_spi_command command)
{
    lg2::debug("NsmGraceSpiObject: Sending spi data request {COMM}", "COMM",
               command);

    enum nsm_spi_status status = NSM_SPI_ERROR;

    auto rc = co_await checkSpiStatus(device, &status);

    if (rc != NSM_SW_SUCCESS)
    {
        // And errors will be logged in the called method
        co_return rc;
    }

    if (status != NSM_SPI_READY)
    {
        lg2::error("NsmGraceSpiObject SPI not ready eid={EID} ", "EID",
                   device->getEid());
        co_return NSM_SW_ERROR;
    }

    Request spiCommandRequest(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_send_spi_command_req));

    auto spiCommandRequestMsg =
        reinterpret_cast<struct nsm_msg*>(spiCommandRequest.data());

    rc = encode_send_spi_command_req(0, spiCommandRequestMsg, command);

#ifdef ENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(utils::Tx, std::vector<unsigned char>(
                                      spiCommandRequest.data(),
                                      spiCommandRequest.data() +
                                          (sizeof(nsm_msg_hdr) +
                                           sizeof(nsm_send_spi_command_req))));
#endif

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmGraceSpiObject encode_send_spi_command_req failed. eid={EID} rc={RC}",
            "EID", device->getEid(), "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> spiCommandResponseMsg;
    size_t spiCommandResponseLen = 0;

    rc = co_await device->postPatchIO(device->getEid(), spiCommandRequest,
                                      spiCommandResponseMsg,
                                      spiCommandResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmGraceSpiObject postPatchNsmCommand for Spi Write Enable failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", device->getEid());
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_send_spi_command_resp(spiCommandResponseMsg.get(),
                                      spiCommandResponseLen, &cc, &reasonCode);

#ifdef ENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Rx,
        std::vector<unsigned char>(
            reinterpret_cast<const uint8_t*>(spiCommandResponseMsg.get()),
            reinterpret_cast<const uint8_t*>(spiCommandResponseMsg.get()) +
                (sizeof(nsm_msg_hdr) + sizeof(nsm_send_spi_command_resp))));
#endif

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // If we are reading the status reg we need to read back the
        // result
        if (command == NSM_SPI_STATUS_REG)
        {
            rc = co_await executeSpiTransaction(device, 0x01, 0x01);
        }
        else
        {
            rc = co_await executeSpiTransaction(device, 0x01);
        }
    }

    LG2_ERROR_FLT(
        "decode_send_spi_command_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);

    // coverity[missing_return]
    co_return cc ? cc : rc;
}

requester::Coroutine
    NsmGraceSpiObject::setSpiWriteEnable(std::shared_ptr<NsmDevice> device)
{
    lg2::debug("NsmGraceSpiObject: Enabling spi write");
    auto rc = co_await sendSpiDataRequest(device, NSM_SPI_WRITE_ENABLE);

    co_return rc;
}

requester::Coroutine
    NsmGraceSpiObject::setSpi4ByteAddressMode(std::shared_ptr<NsmDevice> device)
{
    lg2::debug("NsmGraceSpiObject: Setting 4 byte address mode");
    auto rc = co_await sendSpiDataRequest(device, NSM_SPI_4_BYTE_ADDRESS_MODE);

    co_return rc;
}

requester::Coroutine NsmGraceSpiObject::requestSpiStatusRegister(
    std::shared_ptr<NsmDevice> device)
{
    lg2::debug("NsmGraceSpiObject: Requesting status register");
    auto rc = co_await sendSpiDataRequest(device, NSM_SPI_STATUS_REG);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmGraceSpiObject requestSpiStatusRegister failed with rc={RC}",
            "RC", rc);
    }
    co_return rc;
}

requester::Coroutine
    NsmGraceSpiObject::eraseBlock(std::shared_ptr<NsmDevice> device,
                                  uint32_t blockAddress)
{
    lg2::debug("NsmGraceSpiObject: Erasing block {BLOCK}", "BLOCK",
               blockAddress);
    enum nsm_spi_status status = NSM_SPI_ERROR;

    auto rc = co_await checkSpiStatus(device, &status);

    if (rc != NSM_SW_SUCCESS)
    {
        // And errors will be logged in the called method
        co_return rc;
    }

    if (status != NSM_SPI_READY)
    {
        lg2::error("NsmGraceSpiObject SPI not ready eid={EID} ", "EID",
                   device->getEid());
        co_return NSM_SW_ERROR;
    }

    Request eraseBlock(sizeof(nsm_msg_hdr) +
                       sizeof(nsm_send_spi_operation_req));

    auto eraseBlockMsg = reinterpret_cast<struct nsm_msg*>(eraseBlock.data());

    auto eid = device->getEid();
    rc = encode_send_spi_operation_req(0, eraseBlockMsg, blockAddress,
                                       NSM_SPI_ERASE);

#ifdef ENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Tx,
        std::vector<unsigned char>(
            eraseBlock.data(),
            eraseBlock.data() +
                (sizeof(nsm_msg_hdr) + sizeof(nsm_send_spi_operation_req))));
#endif

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmGraceSpiObject encode_send_spi_operation_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> eraseBlockResponseMsg;
    size_t eraseBlockResponseLen = 0;

    rc = co_await device->postPatchIO(eid, eraseBlock, eraseBlockResponseMsg,
                                      eraseBlockResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmGraceSpiObject postPatchIO for set Spi operation failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_send_spi_operation_resp(
        eraseBlockResponseMsg.get(), eraseBlockResponseLen, &cc, &reasonCode);

#ifdef ENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Rx,
        std::vector<unsigned char>(
            reinterpret_cast<const uint8_t*>(eraseBlockResponseMsg.get()),
            reinterpret_cast<const uint8_t*>(eraseBlockResponseMsg.get()) +
                (sizeof(nsm_msg_hdr) + sizeof(nsm_send_spi_operation_resp))));
#endif

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        rc = co_await executeSpiTransaction(device, 0x05);

        if (rc == NSM_SW_SUCCESS)
        {
            for (auto i = 0; i <= MAX_NUMBER_OF_WRITE_POLL_CYCLES; i++)
            {
                lg2::debug(
                    "NsmGraceSpiObject: Checking if erase completed {COUNT}",
                    "COUNT", i);
                bool writeComplete = false;
                rc = co_await checkIfWriteComplete(device, &writeComplete);
                if (rc != NSM_SW_SUCCESS)
                {
                    co_return rc;
                }

                if (writeComplete)
                {
                    lg2::debug("NsmGraceSpiObject: Erase block completed");
                    break;
                }

                if (i == MAX_NUMBER_OF_WRITE_POLL_CYCLES)
                {
                    lg2::error("NsmGraceSpiObject: Erase timed out");
                    co_return NSM_SW_ERROR_COMMAND_FAIL;
                }

                // TBD: May want to add a delay here to avoid
                //      spamming the bus. Test to make sure
                //      telemetry is OK first.
            }

            co_return rc;
        }
    }

    LG2_ERROR_FLT(
        "decode_send_spi_operation_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);

    // coverity[missing_return]
    co_return cc ? cc : rc;
}

requester::Coroutine
    NsmGraceSpiObject::readToCache(std::shared_ptr<NsmDevice> device,
                                   uint32_t blockAddress)
{
    Request transferBlock(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_send_spi_operation_req));

    auto transferBlockMsg =
        reinterpret_cast<struct nsm_msg*>(transferBlock.data());

    auto eid = device->getEid();
    auto rc = encode_send_spi_operation_req(0, transferBlockMsg, blockAddress,
                                            NSM_SPI_READ);

#ifdef ENABLE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Tx,
        std::vector<unsigned char>(
            transferBlock.data(),
            transferBlock.data() +
                (sizeof(nsm_msg_hdr) + sizeof(nsm_send_spi_operation_req))));
#endif

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmSpiRead encode_send_spi_operation_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> transferBlockResponseMsg;
    size_t transferBlockResponseLen = 0;

    rc = co_await device->postPatchIO(
        eid, transferBlock, transferBlockResponseMsg, transferBlockResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmSpiRead postPatchIO for set Spi operation failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;

    rc = decode_send_spi_operation_resp(transferBlockResponseMsg.get(),
                                        transferBlockResponseLen, &cc,
                                        &reasonCode);

#ifdef ENABLE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Rx,
        std::vector<unsigned char>(
            reinterpret_cast<const uint8_t*>(transferBlockResponseMsg.get()),
            reinterpret_cast<const uint8_t*>(transferBlockResponseMsg.get()) +
                (sizeof(nsm_msg_hdr) + sizeof(nsm_send_spi_operation_resp))));
#endif

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // We read one extra byte as the command byte is returned
        rc = co_await executeSpiTransaction(device, 0x05, SPI_READ_BLOCK_SIZE);

        co_return rc;
    }

    LG2_ERROR_FLT(
        "decode_send_spi_operation_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);

    // coverity[missing_return]
    co_return cc ? cc : rc;
}

requester::Coroutine
    NsmGraceSpiObject::transferCacheToFile(std::shared_ptr<NsmDevice> device,
                                           int fileDesc)
{
    for (auto i = 0; i <= BLOCKS_TO_READ - 1; i++)
    {
        Request readBlock(sizeof(nsm_msg_hdr) + sizeof(nsm_read_spi_block_req));

        auto readBlockMsg = reinterpret_cast<struct nsm_msg*>(readBlock.data());

        auto rc = encode_read_spi_block_req(0, readBlockMsg, i);

#ifdef ENABLE_SPI_OPERATION_RAW_DEBUG_DUMP
        utils::printBuffer(
            utils::Tx,
            std::vector<unsigned char>(
                readBlock.data(),
                readBlock.data() +
                    (sizeof(nsm_msg_hdr) + sizeof(nsm_read_spi_block_req))));
#endif

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "NsmSpiRead encode_read_spi_block_req failed. eid={EID} rc={RC}",
                "EID", device->getEid(), "RC", rc);
            co_return rc;
        }

        std::shared_ptr<const nsm_msg> readBlockResponseMsg;
        size_t readBlockResponseLen = 0;

        rc = co_await device->postPatchIO(device->getEid(), readBlock,
                                          readBlockResponseMsg,
                                          readBlockResponseLen);

        if (rc)
        {
            lg2::error(
                "NsmSpiRead postPatchNsmCommand for read Spi block failed with RC={RC}, eid={EID}",
                "RC", rc, "EID", device->getEid());
            co_return rc;
        }

        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        uint8_t buffer[FPGA_CACHE_SIZE];

        rc = decode_read_spi_block_resp(readBlockResponseMsg.get(),
                                        readBlockResponseLen, &cc, &reasonCode,
                                        buffer, FPGA_CACHE_SIZE);

#ifdef ENABLE_SPI_OPERATION_RAW_DEBUG_DUMP
        utils::printBuffer(
            utils::Rx,
            std::vector<unsigned char>(
                reinterpret_cast<const uint8_t*>(readBlockResponseMsg.get()),
                reinterpret_cast<const uint8_t*>(readBlockResponseMsg.get()) +
                    (sizeof(nsm_msg_hdr) + sizeof(nsm_read_spi_block_resp))));
#endif

        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            uint8_t dataRead = 0;
            size_t written = 0;

            // The first byte read is the command byte, and
            // we don't want to write it to the file
            if (i == 0)
            {
                dataRead = BYTES_FIRST_BLOCK;
                written = write(fileDesc, &buffer[1], dataRead);
            }
            // The last block is only 9 bytes to complete the
            // 128 byte read (minus the command byte)
            else if (i == (BLOCKS_TO_READ - 1))
            {
                dataRead = BYTES_LAST_BLOCK;
                written = write(fileDesc, buffer, dataRead);
            }
            else
            {
                dataRead = FPGA_CACHE_SIZE;
                written = write(fileDesc, buffer, dataRead);
            }

            if (written != dataRead)
            {
                lg2::error("NsmSpiRead Failed to write data to file");
                co_return NSM_SW_ERROR_COMMAND_FAIL;
            }
        }
        else
        {
            LG2_ERROR_FLT(
                "NsmSpiRead decode_send_spi_operation_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
                "REASONCODE", reasonCode, "CC", cc, "RC", rc);
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmGraceSpiObject::initSpi(std::shared_ptr<NsmDevice> device)
{
    auto rc = co_await setSpiWriteEnable(device);

    if (rc != NSM_SW_SUCCESS)
    {
        co_return rc;
    }

    rc = co_await setSpi4ByteAddressMode(device);

    if (rc != NSM_SW_SUCCESS)
    {
        co_return rc;
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmGraceSpiObject::eraseSpiAsyncHandler()
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);

    auto rc = co_await initSpi(device);

    if (rc != NSM_SW_SUCCESS)
    {
        finishSpiOperation(SpiProgress::OperationStatus::Failed);
        co_return rc;
    }

    for (auto i = 0; i < SPI_SECTORS; i++)
    {
        rc = co_await setSpiWriteEnable(device);

        if (rc != NSM_SW_SUCCESS)
        {
            finishSpiOperation(SpiProgress::OperationStatus::Failed);
            co_return rc;
        }

        rc = co_await eraseBlock(device, i * SPI_BLOCK_SIZE);

        if (rc != NSM_SW_SUCCESS)
        {
            finishSpiOperation(SpiProgress::OperationStatus::Failed);
            co_return rc;
        }

        // Update progress
        uint8_t percentComplete =
            (uint8_t)(((float)i / ((float)SPI_SECTORS)) * 100);

        lg2::info("NsmGraceSpiObject Erase percent complete: {COMP}", "COMP",
                  percentComplete);

        if (auto currentProgress = getCurrentProgress())
        {
            currentProgress->progress(percentComplete);
        }
    }

    finishSpiOperation(SpiProgress::OperationStatus::Completed);
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmGraceSpiObject::readSpiAsyncHandler()
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    auto eid = device->getEid();

    auto rc = co_await initSpi(device);

    if (rc != NSM_SW_SUCCESS)
    {
        finishSpiOperation(SpiProgress::OperationStatus::Failed);
        co_return rc;
    }

    int fileDesc = memfd_create(fdName.c_str(), 0);

    if (fileDesc == -1)
    {
        lg2::error("NsmSpiObject Read: memfd_create eid={EID} error={ERROR}",
                   "EID", eid, "ERROR", strerror(errno));
        finishSpiOperation(SpiProgress::OperationStatus::Failed);
        co_return NSM_SW_ERROR;
    }

    for (auto i = 0; i < SPI_SIZE_BYTES; i += SPI_READ_BLOCK_SIZE)
    {
        rc = co_await readToCache(device, i);

        if (rc != NSM_SW_SUCCESS)
        {
            finishSpiOperation(SpiProgress::OperationStatus::Failed);
            co_return rc;
        }

        rc = co_await transferCacheToFile(device, fileDesc);

        if (rc != NSM_SW_SUCCESS)
        {
            finishSpiOperation(SpiProgress::OperationStatus::Failed);
            co_return rc;
        }

        // Update progress
        uint8_t percentComplete =
            (uint8_t)(((float)i / ((float)SPI_SIZE_BYTES)) * 100);

        lg2::info("NsmGraceSpiObject Read percent complete: {COMP}", "COMP",
                  percentComplete);

        if (auto currentProgress = getCurrentProgress())
        {
            currentProgress->progress(percentComplete);
        }
    }

    int r = lseek(fileDesc, 0, SEEK_SET);
    if (r < 0)
    {
        lg2::error("Seeking file failed {RETURNCODE}", "RETURNCODE", r);
        finishSpiOperation(SpiProgress::OperationStatus::Failed);
        co_return rc;
    }

    // Update the file descriptor in current progress
    if (auto currentProgress = getCurrentProgress())
    {
        sdbusplus::message::unix_fd unixFd(fileDesc);
        currentProgress->spiReadFd(unixFd);
    }
    finishSpiOperation(SpiProgress::OperationStatus::Completed);
    co_return NSM_SW_SUCCESS;
}

static requester::Coroutine createNsmGraceSpi(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath)
{
    try
    {
        auto& bus = utils::DBusHandler::getBus();
        auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
            utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

        std::string name{};
        if (allCurrentIfaceProperties.count("Name"))
        {
            name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
        }
        std::string type{};
        if (allCurrentIfaceProperties.count("Type"))
        {
            type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
        }
        uuid_t uuid{};
        if (allCurrentIfaceProperties.count("UUID"))
        {
            uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
        }
        std::string inventoryObjPath{};
        if (allCurrentIfaceProperties.count("InventoryObjPath"))
        {
            inventoryObjPath = std::get<std::string>(
                allCurrentIfaceProperties.at("InventoryObjPath"));
        }

        auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
        if (!nsmDevice)
        {
            // cannot found a nsmDevice for the sensor
            lg2::error(
                "The UUID of NSM_Processor (for SPI Interface) PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", name, "TYPE", type);

            // coverity[missing_return]
            co_return NSM_ERROR;
        }

        if (type == "NSM_SPI")
        {
            auto spiSensor = std::make_shared<NsmGraceSpiObject>(
                bus, name, inventoryObjPath, type, uuid);

            nsmDevice->addDeviceSensors(spiSensor);
        }
    }

    catch (const std::exception& e)
    {
        lg2::error(
            "Error in addStaticSensor for path {PATH} and interface {INTF}, {ERROR}",
            "PATH", objPath, "INTF", interface, "ERROR", e);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(createNsmGraceSpi,
                               "xyz.openbmc_project.Configuration.NSM_SPI")

} // namespace nsm

#endif
