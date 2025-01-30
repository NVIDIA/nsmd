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

namespace nsm
{

NsmGraceSpiObject::NsmGraceSpiObject(sdbusplus::bus::bus& bus,
                                     const std::string& name,
                                     const std::string& inventoryPath,
                                     const std::string& type,
                                     const uuid_t& uuid) :
    NsmObject(name, type),
    SpiIntf(bus, (inventoryPath + name).c_str()), uuid(uuid), opProgress(NULL)
{
    lg2::debug("NsmGraceSpiObject: {NAME}", "NAME", name.c_str());

    objPath = inventoryPath + name;

    fdName = name + "_read_contents";
    sdbusplus::message::unix_fd unixFd(0);
    spiReadFd(unixFd, true);
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

    return sdbusplus::message::object_path(NSM_SPI_PROGRESS_INTERFACE);
}

sdbusplus::message::object_path NsmGraceSpiObject::readSpi()
{
    lg2::debug("NSMSPI: Starting NSM Read");

    if (startSpiOperation() != NSM_SW_SUCCESS)
    {
        lg2::error("NSMSPI: Read Unavailable");
        throw Common::Error::Unavailable();
    }

    if (spiReadFd() != 0)
    {
        close(spiReadFd());
        sdbusplus::message::unix_fd unixFd(0);
        spiReadFd(unixFd, true);
    }

    readSpiAsyncHandler().detach();

    return sdbusplus::message::object_path(NSM_SPI_PROGRESS_INTERFACE);
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

    if (opProgress != NULL)
    {
        lg2::debug("NsmGraceSpiObject: Clearing prior operation status");
        delete opProgress;
        opProgress = NULL;
    }
    opProgress = new SpiProgress(get_bus(), NSM_SPI_PROGRESS_INTERFACE);

    opProgress->startTime(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    opProgress->status(SpiProgress::OperationStatus::InProgress);
    opProgress->progress(0);

    return NSM_SW_SUCCESS;
}

void NsmGraceSpiObject::finishSpiOperation(
    SpiProgress::OperationStatus opStatus)
{
    lg2::debug("NsmGraceSpiObject: Finishing SPI Operation");

    opProgress->status(opStatus);
    opProgress->progress(100);
    opProgress->completedTime(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    cmdInProgress = false;
}

requester::Coroutine
    NsmGraceSpiObject::checkSpiStatus(SensorManager& manager, eid_t eid,
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
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> spiStatusResponseMsg;
    size_t spiStatusResponseLen = 0;

    rc = co_await manager.SendRecvNsmMsg(eid, spiStatus, spiStatusResponseMsg,
                                         spiStatusResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmGraceSpi SendRecvNsmMsg for Spi transaction failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;

    rc = decode_read_spi_status_resp(spiStatusResponseMsg.get(),
                                     spiStatusResponseLen, &cc, &reason_code,
                                     status);

#ifdef ENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Rx,
        std::vector<unsigned char>(
            reinterpret_cast<const uint8_t*>(spiStatusResponseMsg.get()),
            reinterpret_cast<const uint8_t*>(spiStatusResponseMsg.get()) +
                (sizeof(nsm_msg_hdr) + sizeof(nsm_read_spi_status_resp))));
#endif

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        co_return NSM_SW_SUCCESS;
    }
    else
    {
        logHandleResponseMsg("NsmGraceSpi decode_read_spi_status_resp",
                             reason_code, cc, rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
}

requester::Coroutine
    NsmGraceSpiObject::checkIfWriteComplete(SensorManager& manager, eid_t eid,
                                            bool* writeComplete)
{
    lg2::debug("NsmGraceSpiObject: Check spi write operation status");

    if (writeComplete == NULL)
    {
        co_return NSM_SW_ERROR_NULL;
    }

    auto rc = co_await requestSpiStatusRegister(manager, eid);

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
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> spiStatusResponseMsg;
    size_t spiStatusResponseLen = 0;

    rc = co_await manager.SendRecvNsmMsg(eid, spiStatus, spiStatusResponseMsg,
                                         spiStatusResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmGraceSpiObject SendRecvNsmMsg for Spi transaction failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint8_t data[30];

    rc = decode_read_spi_block_resp(spiStatusResponseMsg.get(),
                                    spiStatusResponseLen, &cc, &reason_code,
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

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        co_return NSM_SW_SUCCESS;
    }
    else
    {
        logHandleResponseMsg("NsmGraceSpi decode_read_spi_block_resp",
                             reason_code, cc, rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
}

requester::Coroutine NsmGraceSpiObject::executeSpiTransaction(
    SensorManager& manager, eid_t eid, uint16_t writeBytes, uint16_t readBytes)
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
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> spiTransactionResponseMsg;
    size_t spiTransactionResponseLen = 0;

    rc = co_await manager.SendRecvNsmMsg(eid, spiTransaction,
                                         spiTransactionResponseMsg,
                                         spiTransactionResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmGraceSpiObject SendRecvNsmMsg for Spi transaction failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;

    rc = decode_send_spi_transaction_resp(spiTransactionResponseMsg.get(),
                                          spiTransactionResponseLen, &cc,
                                          &reason_code);

#ifdef ENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP
    utils::printBuffer(
        utils::Rx,
        std::vector<unsigned char>(
            reinterpret_cast<const uint8_t*>(spiTransactionResponseMsg.get()),
            reinterpret_cast<const uint8_t*>(spiTransactionResponseMsg.get()) +
                (sizeof(nsm_msg_hdr) + sizeof(nsm_send_spi_transaction_resp))));
#endif

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        co_return NSM_SW_SUCCESS;
    }
    else
    {
        logHandleResponseMsg("NsmGraceSpi decode_send_spi_transaction_resp",
                             reason_code, cc, rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
}

requester::Coroutine
    NsmGraceSpiObject::sendSpiDataRequest(SensorManager& manager, eid_t eid,
                                          enum nsm_spi_command command)
{
    lg2::debug("NsmGraceSpiObject: Sending spi data request {COMM}", "COMM",
               command);

    enum nsm_spi_status status = NSM_SPI_ERROR;

    auto rc = co_await checkSpiStatus(manager, eid, &status);

    if (rc != NSM_SW_SUCCESS)
    {
        // And errors will be logged in the called method
        co_return rc;
    }

    if (status != NSM_SPI_READY)
    {
        lg2::error("NsmGraceSpiObject SPI not ready eid={EID} ", "EID", eid);
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
            "EID", eid, "RC", rc);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> spiCommandResponseMsg;
    size_t spiCommandResponseLen = 0;

    rc = co_await manager.SendRecvNsmMsg(
        eid, spiCommandRequest, spiCommandResponseMsg, spiCommandResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmGraceSpiObject SendRecvNsmMsg for Spi Write Enable failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;

    rc = decode_send_spi_command_resp(spiCommandResponseMsg.get(),
                                      spiCommandResponseLen, &cc, &reason_code);

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
            rc = co_await executeSpiTransaction(manager, eid, 0x01, 0x01);
        }
        else
        {
            rc = co_await executeSpiTransaction(manager, eid, 0x01);
        }

        co_return rc;
    }
    else
    {
        logHandleResponseMsg("NsmGraceSpi decode_send_spi_command_resp",
                             reason_code, cc, rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
}

requester::Coroutine
    NsmGraceSpiObject::setSpiWriteEnable(SensorManager& manager, eid_t eid)
{
    lg2::debug("NsmGraceSpiObject: Enabling spi write");
    auto rc = co_await sendSpiDataRequest(manager, eid, NSM_SPI_WRITE_ENABLE);

    co_return rc;
}

requester::Coroutine
    NsmGraceSpiObject::setSpi4ByteAddressMode(SensorManager& manager, eid_t eid)
{
    lg2::debug("NsmGraceSpiObject: Setting 4 byte address mode");
    auto rc = co_await sendSpiDataRequest(manager, eid,
                                          NSM_SPI_4_BYTE_ADDRESS_MODE);

    co_return rc;
}

requester::Coroutine
    NsmGraceSpiObject::requestSpiStatusRegister(SensorManager& manager,
                                                eid_t eid)
{
    lg2::debug("NsmGraceSpiObject: Requesting status register");
    auto rc = co_await sendSpiDataRequest(manager, eid, NSM_SPI_STATUS_REG);

    co_return rc;
}

requester::Coroutine NsmGraceSpiObject::eraseBlock(SensorManager& manager,
                                                   eid_t eid,
                                                   uint32_t blockAddress)
{
    lg2::debug("NsmGraceSpiObject: Erasing block {BLOCK}", "BLOCK",
               blockAddress);
    enum nsm_spi_status status = NSM_SPI_ERROR;

    auto rc = co_await checkSpiStatus(manager, eid, &status);

    if (rc != NSM_SW_SUCCESS)
    {
        // And errors will be logged in the called method
        co_return rc;
    }

    if (status != NSM_SPI_READY)
    {
        lg2::error("NsmGraceSpiObject SPI not ready eid={EID} ", "EID", eid);
        co_return NSM_SW_ERROR;
    }

    Request eraseBlock(sizeof(nsm_msg_hdr) +
                       sizeof(nsm_send_spi_operation_req));

    auto eraseBlockMsg = reinterpret_cast<struct nsm_msg*>(eraseBlock.data());

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

    rc = co_await manager.SendRecvNsmMsg(eid, eraseBlock, eraseBlockResponseMsg,
                                         eraseBlockResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmGraceSpiObject SendRecvNsmMsg for set Spi operation failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;

    rc = decode_send_spi_operation_resp(
        eraseBlockResponseMsg.get(), eraseBlockResponseLen, &cc, &reason_code);

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
        rc = co_await executeSpiTransaction(manager, eid, 0x05);

        if (rc == NSM_SW_SUCCESS)
        {
            for (auto i = 0; i <= MAX_NUMBER_OF_WRITE_POLL_CYCLES; i++)
            {
                lg2::debug(
                    "NsmGraceSpiObject: Checking if erase completed {COUNT}",
                    "COUNT", i);
                bool writeComplete = false;
                rc = co_await checkIfWriteComplete(manager, eid,
                                                   &writeComplete);
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
    else
    {
        logHandleResponseMsg("NsmGraceSpi decode_send_spi_operation_resp",
                             reason_code, cc, rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
}

requester::Coroutine NsmGraceSpiObject::readToCache(SensorManager& manager,
                                                    eid_t eid,
                                                    uint32_t blockAddress)
{
    Request transferBlock(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_send_spi_operation_req));

    auto transferBlockMsg =
        reinterpret_cast<struct nsm_msg*>(transferBlock.data());

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

    rc = co_await manager.SendRecvNsmMsg(
        eid, transferBlock, transferBlockResponseMsg, transferBlockResponseLen);

    if (rc)
    {
        lg2::error(
            "NsmSpiRead SendRecvNsmMsg for set Spi operation failed with RC={RC}, eid={EID}",
            "RC", rc, "EID", eid);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;

    rc = decode_send_spi_operation_resp(transferBlockResponseMsg.get(),
                                        transferBlockResponseLen, &cc,
                                        &reason_code);

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
        rc = co_await executeSpiTransaction(manager, eid, 0x05, 256);

        co_return rc;
    }
    else
    {
        logHandleResponseMsg("NsmSpiRead decode_send_spi_operation_resp",
                             reason_code, cc, rc);
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
}

requester::Coroutine
    NsmGraceSpiObject::transferCacheToFile(SensorManager& manager, eid_t eid,
                                           int fileDesc)
{
    for (auto i = 0; i <= 8; i++)
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
                "EID", eid, "RC", rc);
            co_return rc;
        }

        std::shared_ptr<const nsm_msg> readBlockResponseMsg;
        size_t readBlockResponseLen = 0;

        rc = co_await manager.SendRecvNsmMsg(
            eid, readBlock, readBlockResponseMsg, readBlockResponseLen);

        if (rc)
        {
            lg2::error(
                "NsmSpiRead SendRecvNsmMsg for read Spi block failed with RC={RC}, eid={EID}",
                "RC", rc, "EID", eid);
            co_return rc;
        }

        uint8_t cc = NSM_ERROR;
        uint16_t reason_code = ERR_NULL;
        uint8_t buffer[30];
        size_t dataRead = 0;

        if (i < 8)
        {
            rc = decode_read_spi_block_resp(readBlockResponseMsg.get(),
                                            readBlockResponseLen, &cc,
                                            &reason_code, buffer, 30);
            dataRead = 30;
        }
        else
        {
            rc = decode_read_spi_last_block_resp(readBlockResponseMsg.get(),
                                                 readBlockResponseLen, &cc,
                                                 &reason_code, buffer, 30);
            dataRead = 16;
        }

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
            size_t written = write(fileDesc, buffer, dataRead);

            if (written != dataRead)
            {
                lg2::error("NsmSpiRead Failed to write data to file");
                co_return NSM_SW_ERROR_COMMAND_FAIL;
            }
            co_return rc;
        }
        else
        {
            logHandleResponseMsg("NsmSpiRead decode_send_spi_operation_resp",
                                 reason_code, cc, rc);
            co_return NSM_SW_ERROR_COMMAND_FAIL;
        }
    }
}

requester::Coroutine NsmGraceSpiObject::initSpi(SensorManager& manager,
                                                eid_t eid)
{
    auto rc = co_await setSpiWriteEnable(manager, eid);

    if (rc != NSM_SW_SUCCESS)
    {
        co_return rc;
    }

    rc = co_await setSpi4ByteAddressMode(manager, eid);

    if (rc != NSM_SW_SUCCESS)
    {
        co_return rc;
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmGraceSpiObject::eraseSpiAsyncHandler()
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);

    auto rc = co_await initSpi(manager, eid);

    if (rc != NSM_SW_SUCCESS)
    {
        finishSpiOperation(SpiProgress::OperationStatus::Failed);
        co_return rc;
    }

    for (auto i = 0; i < SPI_SECTORS; i++)
    {
        rc = co_await setSpiWriteEnable(manager, eid);

        if (rc != NSM_SW_SUCCESS)
        {
            finishSpiOperation(SpiProgress::OperationStatus::Failed);
            co_return rc;
        }

        rc = co_await eraseBlock(manager, eid, i * SPI_BLOCK_SIZE);

        if (rc != NSM_SW_SUCCESS)
        {
            finishSpiOperation(SpiProgress::OperationStatus::Failed);
            co_return rc;
        }

        // Update progress
        uint8_t percentComplete =
            (uint8_t)(((float)i / (float)SPI_SECTORS) * 100);

        lg2::info("NsmGraceSpiObject Erase percent complete: {COMP}", "COMP",
                  percentComplete);

        if (opProgress != NULL)
        {
            opProgress->progress(percentComplete);
        }
    }

    finishSpiOperation(SpiProgress::OperationStatus::Completed);
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmGraceSpiObject::readSpiAsyncHandler()
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);

    auto rc = co_await initSpi(manager, eid);

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
        rc = co_await readToCache(manager, eid, i);

        if (rc != NSM_SW_SUCCESS)
        {
            finishSpiOperation(SpiProgress::OperationStatus::Failed);
            co_return rc;
        }

        rc = co_await transferCacheToFile(manager, eid, fileDesc);

        if (rc != NSM_SW_SUCCESS)
        {
            finishSpiOperation(SpiProgress::OperationStatus::Failed);
            co_return rc;
        }

        // Update progress
        uint8_t percentComplete =
            (uint8_t)(((float)i / (float)SPI_SIZE_BYTES) * 100);

        lg2::info("NsmGraceSpiObject Read percent complete: {COMP}", "COMP",
                  percentComplete);

        if (opProgress != NULL)
        {
            opProgress->progress(percentComplete);
        }
    }

    sdbusplus::message::unix_fd unixFd(fileDesc);
    spiReadFd(unixFd, true);

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
        auto name = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Name", interface.c_str());

        auto type = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Type", interface.c_str());

        auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", interface.c_str());

        auto inventoryObjPath = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "InventoryObjPath", interface.c_str());

        auto nsmDevice = manager.getNsmDevice(uuid);
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

            nsmDevice->deviceSensors.emplace_back(spiSensor);
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
