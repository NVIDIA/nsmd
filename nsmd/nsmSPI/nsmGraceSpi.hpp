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

#pragma once

#ifdef ENABLE_GRACE_SPI_OPERATIONS

#define SPI_BLOCK_SIZE 64 * 1024
#define SPI_SECTORS 1024

#define SPI_READ_BLOCK_SIZE 256
#define SPI_SIZE_BYTES SPI_SECTORS* SPI_BLOCK_SIZE

#define MAX_NUMBER_OF_WRITE_POLL_CYCLES 100

#include "nsmObjectFactory.hpp"

#include <com/nvidia/SPI/SPI/server.hpp>
#include <xyz/openbmc_project/Common/Progress/server.hpp>

namespace nsm
{
using namespace sdbusplus::com::nvidia;
using namespace sdbusplus::server;

using SpiIntf = object_t<sdbusplus::com::nvidia::SPI::server::SPI>;

using SpiProgress = sdbusplus::server::xyz::openbmc_project::common::Progress;

class NsmGraceSpiObject : public NsmObject, public SpiIntf
{
  public:
    NsmGraceSpiObject(sdbusplus::bus::bus& bus, const std::string& name,
                      const std::string& inventoryPath, const std::string& type,
                      const uuid_t& uuid);

    sdbusplus::message::object_path eraseSpi();
    sdbusplus::message::object_path readSpi();

  private:
    uint8_t startSpiOperation();
    void finishSpiOperation(SpiProgress::OperationStatus opProgress);

    requester::Coroutine checkSpiStatus(SensorManager& manager, eid_t eid,
                                        enum nsm_spi_status* status);
    requester::Coroutine checkIfWriteComplete(SensorManager& manager, eid_t,
                                              bool* writeComplete);

    requester::Coroutine executeSpiTransaction(SensorManager& manager,
                                               eid_t eid, uint16_t writeBytes,
                                               uint16_t readBytes = 0);

    requester::Coroutine sendSpiDataRequest(SensorManager& manager, eid_t eid,
                                            enum nsm_spi_command command);

    requester::Coroutine setSpiWriteEnable(SensorManager& manager, eid_t eid);
    requester::Coroutine setSpi4ByteAddressMode(SensorManager& manager,
                                                eid_t eid);
    requester::Coroutine eraseBlock(SensorManager& manager, eid_t eid,
                                    uint32_t blockAddress);

    requester::Coroutine readToCache(SensorManager& manager, eid_t eid,
                                     uint32_t blockAddress);
    requester::Coroutine transferCacheToFile(SensorManager& manager, eid_t eid,
                                             int fileDesc);

    requester::Coroutine requestSpiStatusRegister(SensorManager& manager,
                                                  eid_t eid);

    requester::Coroutine initSpi(SensorManager& manager, eid_t eid);

    requester::Coroutine eraseSpiAsyncHandler();
    requester::Coroutine readSpiAsyncHandler();

    std::string objPath;
    uuid_t uuid;

    bool cmdInProgress{false};
    SpiProgress* opProgress;

    std::string fdName;
};
} // namespace nsm

#endif
