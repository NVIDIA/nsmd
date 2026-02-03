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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "nsmGraceSpi.hpp"

namespace nsm
{
requester::Coroutine createNsmGraceSpi(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

using namespace nsm;

struct NsmGraceSpiTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_SPI";
    const std::string name = "GraceSPI";
    const std::string objPath = "/xyz/openbmc_project/inventory/system/grace";
    const std::string inventoryPath =
        "/xyz/openbmc_project/inventory/system/chassis/";

    const uuid_t graceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> grace;

    NsmGraceSpiTest() : SensorManagerTest(devices)
    {
        grace = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(graceUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(grace, nullptr);
    }

    ~NsmGraceSpiTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"Type", std::string("NSM_SPI")},
        {"UUID", graceUuid},
        {"InventoryObjPath", inventoryPath},
    };
};

TEST_F(NsmGraceSpiTest, goodTestCreateGraceSpi)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = basicProperties;

    createNsmGraceSpi(mockManager, basicIntfName, objPath);

    EXPECT_EQ(1, grace->deviceSensors.size());

    auto spiObject =
        std::dynamic_pointer_cast<NsmGraceSpiObject>(grace->deviceSensors[0]);
    EXPECT_NE(spiObject, nullptr);
    EXPECT_EQ(spiObject->getName(), name);
}

TEST_F(NsmGraceSpiTest, badTestMissingUUID)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = basicProperties;
    propertyMap.erase("UUID");

    EXPECT_THROW_COROUTINE(
        createNsmGraceSpi(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

TEST_F(NsmGraceSpiTest, badTestInvalidUUID)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = basicProperties;
    propertyMap["UUID"] = "INVALID:UUID";

    EXPECT_THROW_COROUTINE(
        createNsmGraceSpi(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

TEST_F(NsmGraceSpiTest, testNsmGraceSpiObjectConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    EXPECT_EQ(spiObject.getName(), name);
    EXPECT_EQ(spiObject.getType(), "NSM_SPI");
    EXPECT_EQ(spiObject.cmdInProgress, false);
}

TEST_F(NsmGraceSpiTest, testStartSpiOperation)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    EXPECT_EQ(spiObject.cmdInProgress, false);

    auto rc = spiObject.startSpiOperation();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(spiObject.cmdInProgress, true);
    EXPECT_EQ(spiObject.progressHistory.size(), 1);

    // Cannot start another operation while one is in progress
    rc = spiObject.startSpiOperation();
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

TEST_F(NsmGraceSpiTest, testFinishSpiOperation)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    spiObject.startSpiOperation();
    EXPECT_EQ(spiObject.cmdInProgress, true);

    spiObject.finishSpiOperation(SpiProgress::OperationStatus::Completed);
    EXPECT_EQ(spiObject.cmdInProgress, false);

    auto currentProgress = spiObject.getCurrentProgress();
    EXPECT_NE(currentProgress, nullptr);
    EXPECT_EQ(currentProgress->status(),
              SpiProgress::OperationStatus::Completed);
    EXPECT_EQ(currentProgress->progress(), 100);
}

TEST_F(NsmGraceSpiTest, testCheckSpiStatus)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    const Response statusResp{0x10,
                              0xDE,
                              0x00,
                              0x89,
                              NSM_TYPE_FIRMWARE_UTILITIES,
                              NSM_READ_SPI_STATUS,
                              NSM_SUCCESS,
                              0,
                              0,
                              1,
                              0,
                              NSM_SPI_READY};

    EXPECT_CALL(*grace, postPatchIO)
        .WillOnce(mockSensorIO(statusResp, Response{}));

    enum nsm_spi_status status = NSM_SPI_ERROR;
    spiObject.checkSpiStatus(grace, &status);
    EXPECT_EQ(status, NSM_SPI_READY);
}

TEST_F(NsmGraceSpiTest, testExecuteSpiTransaction)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    const Response transactionResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_FIRMWARE_UTILITIES,
                                   NSM_SEND_SPI_TRANSACTION,
                                   NSM_SUCCESS,
                                   0,
                                   0};

    EXPECT_CALL(*grace, postPatchIO)
        .WillOnce(mockSensorIO(transactionResp, Response{}));

    spiObject.executeSpiTransaction(grace, 1, 1);
}

TEST_F(NsmGraceSpiTest, testSendSpiDataRequest)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    const Response statusResp{0x10,
                              0xDE,
                              0x00,
                              0x89,
                              NSM_TYPE_FIRMWARE_UTILITIES,
                              NSM_READ_SPI_STATUS,
                              NSM_SUCCESS,
                              0,
                              0,
                              1,
                              0,
                              NSM_SPI_READY};

    const Response commandResp{0x10,
                               0xDE,
                               0x00,
                               0x89,
                               NSM_TYPE_FIRMWARE_UTILITIES,
                               NSM_SEND_SPI_COMMAND,
                               NSM_SUCCESS,
                               0,
                               0};

    const Response transactionResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_FIRMWARE_UTILITIES,
                                   NSM_SEND_SPI_TRANSACTION,
                                   NSM_SUCCESS,
                                   0,
                                   0};

    EXPECT_CALL(*grace, postPatchIO)
        .Times(3)
        .WillOnce(mockSensorIO(statusResp, Response{}))
        .WillOnce(mockSensorIO(commandResp, Response{}))
        .WillOnce(mockSensorIO(transactionResp, Response{}));

    spiObject.sendSpiDataRequest(grace, NSM_SPI_WRITE_ENABLE);
}

TEST_F(NsmGraceSpiTest, testEraseBlock)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    const Response statusResp{0x10,
                              0xDE,
                              0x00,
                              0x89,
                              NSM_TYPE_FIRMWARE_UTILITIES,
                              NSM_READ_SPI_STATUS,
                              NSM_SUCCESS,
                              0,
                              0,
                              1,
                              0,
                              NSM_SPI_READY};

    const Response eraseResp{0x10,
                             0xDE,
                             0x00,
                             0x89,
                             NSM_TYPE_FIRMWARE_UTILITIES,
                             NSM_SEND_SPI_OPERATION,
                             NSM_SUCCESS,
                             0,
                             0};

    const Response transactionResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_FIRMWARE_UTILITIES,
                                   NSM_SEND_SPI_TRANSACTION,
                                   NSM_SUCCESS,
                                   0,
                                   0};

    // Mock response for status register check
    uint8_t statusRegData[30] = {0};
    statusRegData[1] = 0x00; // Write complete
    Response statusRegResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_spi_block_resp) + 30, 0);
    auto statusRegMsg = reinterpret_cast<nsm_msg*>(statusRegResp.data());
    encode_read_spi_block_resp(0, NSM_SUCCESS, 0, statusRegData, 30,
                               statusRegMsg);

    const Response statusReqResp{0x10,
                                 0xDE,
                                 0x00,
                                 0x89,
                                 NSM_TYPE_FIRMWARE_UTILITIES,
                                 NSM_SEND_SPI_COMMAND,
                                 NSM_SUCCESS,
                                 0,
                                 0};

    const Response transaction2Resp{0x10,
                                    0xDE,
                                    0x00,
                                    0x89,
                                    NSM_TYPE_FIRMWARE_UTILITIES,
                                    NSM_SEND_SPI_TRANSACTION,
                                    NSM_SUCCESS,
                                    0,
                                    0};

    EXPECT_CALL(*grace, postPatchIO)
        .Times(7)
        .WillOnce(mockSensorIO(statusResp, Response{}))
        .WillOnce(mockSensorIO(eraseResp, Response{}))
        .WillOnce(mockSensorIO(transactionResp, Response{}))
        .WillOnce(mockSensorIO(statusReqResp, Response{}))
        .WillOnce(mockSensorIO(transaction2Resp, Response{}))
        .WillOnce(mockSensorIO(statusReqResp, Response{}))
        .WillOnce(mockSensorIO(statusRegResp, Response{}));

    spiObject.eraseBlock(grace, 0);
}

TEST_F(NsmGraceSpiTest, testReadToCache)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    const Response readResp{0x10,
                            0xDE,
                            0x00,
                            0x89,
                            NSM_TYPE_FIRMWARE_UTILITIES,
                            NSM_SEND_SPI_OPERATION,
                            NSM_SUCCESS,
                            0,
                            0};

    const Response transactionResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_FIRMWARE_UTILITIES,
                                   NSM_SEND_SPI_TRANSACTION,
                                   NSM_SUCCESS,
                                   0,
                                   0};

    EXPECT_CALL(*grace, postPatchIO)
        .Times(2)
        .WillOnce(mockSensorIO(readResp, Response{}))
        .WillOnce(mockSensorIO(transactionResp, Response{}));

    spiObject.readToCache(grace, 0);
}

TEST_F(NsmGraceSpiTest, testSetSpiWriteEnable)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    const Response statusResp{0x10,
                              0xDE,
                              0x00,
                              0x89,
                              NSM_TYPE_FIRMWARE_UTILITIES,
                              NSM_READ_SPI_STATUS,
                              NSM_SUCCESS,
                              0,
                              0,
                              1,
                              0,
                              NSM_SPI_READY};

    const Response commandResp{0x10,
                               0xDE,
                               0x00,
                               0x89,
                               NSM_TYPE_FIRMWARE_UTILITIES,
                               NSM_SEND_SPI_COMMAND,
                               NSM_SUCCESS,
                               0,
                               0};

    const Response transactionResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_FIRMWARE_UTILITIES,
                                   NSM_SEND_SPI_TRANSACTION,
                                   NSM_SUCCESS,
                                   0,
                                   0};

    EXPECT_CALL(*grace, postPatchIO)
        .Times(3)
        .WillOnce(mockSensorIO(statusResp, Response{}))
        .WillOnce(mockSensorIO(commandResp, Response{}))
        .WillOnce(mockSensorIO(transactionResp, Response{}));

    spiObject.setSpiWriteEnable(grace);
}

TEST_F(NsmGraceSpiTest, testSetSpi4ByteAddressMode)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    const Response statusResp{0x10,
                              0xDE,
                              0x00,
                              0x89,
                              NSM_TYPE_FIRMWARE_UTILITIES,
                              NSM_READ_SPI_STATUS,
                              NSM_SUCCESS,
                              0,
                              0,
                              1,
                              0,
                              NSM_SPI_READY};

    const Response commandResp{0x10,
                               0xDE,
                               0x00,
                               0x89,
                               NSM_TYPE_FIRMWARE_UTILITIES,
                               NSM_SEND_SPI_COMMAND,
                               NSM_SUCCESS,
                               0,
                               0};

    const Response transactionResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_FIRMWARE_UTILITIES,
                                   NSM_SEND_SPI_TRANSACTION,
                                   NSM_SUCCESS,
                                   0,
                                   0};

    EXPECT_CALL(*grace, postPatchIO)
        .Times(3)
        .WillOnce(mockSensorIO(statusResp, Response{}))
        .WillOnce(mockSensorIO(commandResp, Response{}))
        .WillOnce(mockSensorIO(transactionResp, Response{}));

    spiObject.setSpi4ByteAddressMode(grace);
}

TEST_F(NsmGraceSpiTest, badTestCheckSpiStatusNotReady)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    const Response statusResp{0x10,
                              0xDE,
                              0x00,
                              0x89,
                              NSM_TYPE_FIRMWARE_UTILITIES,
                              NSM_READ_SPI_STATUS,
                              NSM_SUCCESS,
                              0,
                              0,
                              1,
                              0,
                              NSM_SPI_BUSY};

    EXPECT_CALL(*grace, postPatchIO)
        .WillOnce(mockSensorIO(statusResp, Response{}));

    enum nsm_spi_status status = NSM_SPI_ERROR;
    spiObject.checkSpiStatus(grace, &status);
    EXPECT_EQ(status, NSM_SPI_BUSY);
}

TEST_F(NsmGraceSpiTest, badTestCheckSpiStatusError)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    const Response statusResp{0x10,
                              0xDE,
                              0x00,
                              0x89,
                              NSM_TYPE_FIRMWARE_UTILITIES,
                              NSM_READ_SPI_STATUS,
                              NSM_ERROR,
                              0,
                              0,
                              1,
                              0,
                              NSM_SPI_ERROR};

    EXPECT_CALL(*grace, postPatchIO)
        .WillOnce(mockSensorIO(statusResp, Response{}));

    enum nsm_spi_status status;
    auto rc = spiObject.checkSpiStatus(grace, &status);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmGraceSpiTest, testCheckIfWriteComplete)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    // Mock status register request
    const Response statusReqResp{0x10,
                                 0xDE,
                                 0x00,
                                 0x89,
                                 NSM_TYPE_FIRMWARE_UTILITIES,
                                 NSM_SEND_SPI_COMMAND,
                                 NSM_SUCCESS,
                                 0,
                                 0};

    const Response transactionResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_FIRMWARE_UTILITIES,
                                   NSM_SEND_SPI_TRANSACTION,
                                   NSM_SUCCESS,
                                   0,
                                   0};

    // Mock response for status register check
    uint8_t statusRegData[30] = {0};
    statusRegData[1] = 0x00; // Write complete (bit 0 = 0)
    Response statusRegResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_spi_block_resp) + 30, 0);
    auto statusRegMsg = reinterpret_cast<nsm_msg*>(statusRegResp.data());
    encode_read_spi_block_resp(0, NSM_SUCCESS, 0, statusRegData, 30,
                               statusRegMsg);

    EXPECT_CALL(*grace, postPatchIO)
        .Times(3)
        .WillOnce(mockSensorIO(statusReqResp, Response{}))
        .WillOnce(mockSensorIO(transactionResp, Response{}))
        .WillOnce(mockSensorIO(statusRegResp, Response{}));

    bool writeComplete = false;
    spiObject.checkIfWriteComplete(grace, &writeComplete);
    EXPECT_TRUE(writeComplete);
}

TEST_F(NsmGraceSpiTest, testCheckIfWriteNotComplete)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    const Response statusReqResp{0x10,
                                 0xDE,
                                 0x00,
                                 0x89,
                                 NSM_TYPE_FIRMWARE_UTILITIES,
                                 NSM_SEND_SPI_COMMAND,
                                 NSM_SUCCESS,
                                 0,
                                 0};

    const Response transactionResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_FIRMWARE_UTILITIES,
                                   NSM_SEND_SPI_TRANSACTION,
                                   NSM_SUCCESS,
                                   0,
                                   0};

    // Write in progress (bit 0 = 1)
    uint8_t statusRegData[30] = {0};
    statusRegData[1] = 0x01; // Write in progress
    Response statusRegResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_spi_block_resp) + 30, 0);
    auto statusRegMsg = reinterpret_cast<nsm_msg*>(statusRegResp.data());
    encode_read_spi_block_resp(0, NSM_SUCCESS, 0, statusRegData, 30,
                               statusRegMsg);

    EXPECT_CALL(*grace, postPatchIO)
        .Times(3)
        .WillOnce(mockSensorIO(statusReqResp, Response{}))
        .WillOnce(mockSensorIO(transactionResp, Response{}))
        .WillOnce(mockSensorIO(statusRegResp, Response{}));

    bool writeComplete = true;   // Start with true
    spiObject.checkIfWriteComplete(grace, &writeComplete);
    EXPECT_FALSE(writeComplete); // Should be false after check
}

TEST_F(NsmGraceSpiTest, testInitSpi)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    // Mock responses for setSpiWriteEnable and setSpi4ByteAddressMode
    const Response statusResp{0x10,
                              0xDE,
                              0x00,
                              0x89,
                              NSM_TYPE_FIRMWARE_UTILITIES,
                              NSM_READ_SPI_STATUS,
                              NSM_SUCCESS,
                              0,
                              0,
                              1,
                              0,
                              NSM_SPI_READY};

    const Response commandResp{0x10,
                               0xDE,
                               0x00,
                               0x89,
                               NSM_TYPE_FIRMWARE_UTILITIES,
                               NSM_SEND_SPI_COMMAND,
                               NSM_SUCCESS,
                               0,
                               0};

    const Response transactionResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_FIRMWARE_UTILITIES,
                                   NSM_SEND_SPI_TRANSACTION,
                                   NSM_SUCCESS,
                                   0,
                                   0};

    EXPECT_CALL(*grace, postPatchIO)
        .Times(6) // 3 for write enable + 3 for 4-byte mode
        .WillOnce(mockSensorIO(statusResp, Response{}))
        .WillOnce(mockSensorIO(commandResp, Response{}))
        .WillOnce(mockSensorIO(transactionResp, Response{}))
        .WillOnce(mockSensorIO(statusResp, Response{}))
        .WillOnce(mockSensorIO(commandResp, Response{}))
        .WillOnce(mockSensorIO(transactionResp, Response{}));

    auto rc = spiObject.initSpi(grace);
}

TEST_F(NsmGraceSpiTest, badTestSendSpiDataRequestNotReady)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    // SPI not ready
    const Response statusResp{0x10,
                              0xDE,
                              0x00,
                              0x89,
                              NSM_TYPE_FIRMWARE_UTILITIES,
                              NSM_READ_SPI_STATUS,
                              NSM_SUCCESS,
                              0,
                              0,
                              1,
                              0,
                              NSM_SPI_BUSY};

    EXPECT_CALL(*grace, postPatchIO)
        .WillOnce(mockSensorIO(statusResp, Response{}));

    auto rc = spiObject.sendSpiDataRequest(grace, NSM_SPI_WRITE_ENABLE);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmGraceSpiTest, badTestEraseBlockNotReady)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    // SPI not ready
    const Response statusResp{0x10,
                              0xDE,
                              0x00,
                              0x89,
                              NSM_TYPE_FIRMWARE_UTILITIES,
                              NSM_READ_SPI_STATUS,
                              NSM_SUCCESS,
                              0,
                              0,
                              1,
                              0,
                              NSM_SPI_ERROR};

    EXPECT_CALL(*grace, postPatchIO)
        .WillOnce(mockSensorIO(statusResp, Response{}));

    auto rc = spiObject.eraseBlock(grace, 0);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmGraceSpiTest, badTestCheckSpiStatusNullPointer)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    auto rc = spiObject.checkSpiStatus(grace, nullptr);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST_F(NsmGraceSpiTest, badTestCheckIfWriteCompleteNullPointer)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    auto rc = spiObject.checkIfWriteComplete(grace, nullptr);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST_F(NsmGraceSpiTest, testProgressHistoryLimit)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    // Fill up the progress history beyond MAX_PROGRESS_HISTORY
    for (size_t i = 0; i < MAX_PROGRESS_HISTORY + 5; i++)
    {
        spiObject.startSpiOperation();
        spiObject.finishSpiOperation(SpiProgress::OperationStatus::Completed);
    }

    // Should not exceed MAX_PROGRESS_HISTORY
    EXPECT_EQ(spiObject.progressHistory.size(), MAX_PROGRESS_HISTORY);
}

TEST_F(NsmGraceSpiTest, testGetCurrentProgress)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    // No progress yet
    EXPECT_EQ(spiObject.getCurrentProgress(), nullptr);

    // Start operation
    spiObject.startSpiOperation();
    EXPECT_NE(spiObject.getCurrentProgress(), nullptr);

    // Finish operation
    spiObject.finishSpiOperation(SpiProgress::OperationStatus::Completed);
    EXPECT_NE(spiObject.getCurrentProgress(), nullptr);
}

TEST_F(NsmGraceSpiTest, badTestEraseBlockCompletionError)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    const Response statusResp{0x10,
                              0xDE,
                              0x00,
                              0x89,
                              NSM_TYPE_FIRMWARE_UTILITIES,
                              NSM_READ_SPI_STATUS,
                              NSM_SUCCESS,
                              0,
                              0,
                              1,
                              0,
                              NSM_SPI_READY};

    const Response eraseRespError{0x10,
                                  0xDE,
                                  0x00,
                                  0x89,
                                  NSM_TYPE_FIRMWARE_UTILITIES,
                                  NSM_SEND_SPI_OPERATION,
                                  NSM_ERROR,
                                  0x12,
                                  0x34}; // Error response

    EXPECT_CALL(*grace, postPatchIO)
        .Times(2)
        .WillOnce(mockSensorIO(statusResp, Response{}))
        .WillOnce(mockSensorIO(eraseRespError, Response{}));

    auto rc = spiObject.eraseBlock(grace, 0);
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmGraceSpiTest, testRequestSpiStatusRegister)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmGraceSpiObject spiObject(bus, name, inventoryPath, "NSM_SPI", graceUuid);

    const Response statusResp{0x10,
                              0xDE,
                              0x00,
                              0x89,
                              NSM_TYPE_FIRMWARE_UTILITIES,
                              NSM_READ_SPI_STATUS,
                              NSM_SUCCESS,
                              0,
                              0,
                              1,
                              0,
                              NSM_SPI_READY};

    const Response commandResp{0x10,
                               0xDE,
                               0x00,
                               0x89,
                               NSM_TYPE_FIRMWARE_UTILITIES,
                               NSM_SEND_SPI_COMMAND,
                               NSM_SUCCESS,
                               0,
                               0};

    const Response transactionResp{0x10,
                                   0xDE,
                                   0x00,
                                   0x89,
                                   NSM_TYPE_FIRMWARE_UTILITIES,
                                   NSM_SEND_SPI_TRANSACTION,
                                   NSM_SUCCESS,
                                   0,
                                   0};

    EXPECT_CALL(*grace, postPatchIO)
        .Times(3)
        .WillOnce(mockSensorIO(statusResp, Response{}))
        .WillOnce(mockSensorIO(commandResp, Response{}))
        .WillOnce(mockSensorIO(transactionResp, Response{}));

    spiObject.requestSpiStatusRegister(grace);
}

#endif // ENABLE_GRACE_SPI_OPERATIONS
