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

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmAERError.hpp"

using namespace nsm;

struct NsmPCIeAERErrorStatusTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "AERErrorStatus";
    const std::string type = "NSM_AERError";
    const std::string path = "/xyz/openbmc_project/sensors/aer_error/GPU0";
    const uint8_t deviceIndex = 0;

    NsmDeviceTable devices;
    std::shared_ptr<NsmPCIeAERErrorStatus> aerError;
    std::shared_ptr<NsmAERErrorStatusIntf> aerErrorStatusIntf;
    std::shared_ptr<NsmDevice> nsmDevice;

    NsmPCIeAERErrorStatusTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();

        nsmDevice = std::make_shared<MockNsmDevice>(0x10, 0, "MCTP_EID", "16",
                                                    0);
        aerErrorStatusIntf = std::make_shared<NsmAERErrorStatusIntf>(
            bus, path.c_str(), deviceIndex, nsmDevice);

        aerError = std::make_shared<NsmPCIeAERErrorStatus>(
            name, type, aerErrorStatusIntf, deviceIndex);

        EXPECT_NE(aerError, nullptr);
        EXPECT_EQ(aerError->getName(), name);
        EXPECT_EQ(aerError->getType(), type);
    }
};

TEST_F(NsmPCIeAERErrorStatusTest, goodTestConstructor)
{
    EXPECT_NE(aerError->aerErrorStatusIntf, nullptr);
    EXPECT_EQ(aerError->deviceIndex, deviceIndex);
}

TEST_F(NsmPCIeAERErrorStatusTest, goodTestDeviceIndex)
{
    EXPECT_EQ(aerError->deviceIndex, 0);

    auto aerError2 = std::make_shared<NsmPCIeAERErrorStatus>(
        "AERError2", type, aerErrorStatusIntf, 5);
    EXPECT_EQ(aerError2->deviceIndex, 5);
}

TEST_F(NsmPCIeAERErrorStatusTest, goodTestAERErrorStatusIntf)
{
    EXPECT_NE(aerError->aerErrorStatusIntf, nullptr);
    EXPECT_EQ(aerError->aerErrorStatusIntf, aerErrorStatusIntf);
}

TEST_F(NsmPCIeAERErrorStatusTest, goodTestMultipleInstances)
{
    auto aerError1 = std::make_shared<NsmPCIeAERErrorStatus>(
        "AERError1", type, aerErrorStatusIntf, 0);
    auto aerError2 = std::make_shared<NsmPCIeAERErrorStatus>(
        "AERError2", type, aerErrorStatusIntf, 1);
    auto aerError3 = std::make_shared<NsmPCIeAERErrorStatus>(
        "AERError3", type, aerErrorStatusIntf, 2);

    EXPECT_EQ(aerError1->getName(), "AERError1");
    EXPECT_EQ(aerError2->getName(), "AERError2");
    EXPECT_EQ(aerError3->getName(), "AERError3");

    EXPECT_EQ(aerError1->deviceIndex, 0);
    EXPECT_EQ(aerError2->deviceIndex, 1);
    EXPECT_EQ(aerError3->deviceIndex, 2);
}

struct NsmAERErrorStatusIntfTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string path = "/xyz/openbmc_project/sensors/aer_error/GPU0";
    const uint8_t deviceIndex = 0;

    NsmDeviceTable devices;
    std::shared_ptr<NsmAERErrorStatusIntf> aerIntf;
    std::shared_ptr<NsmDevice> nsmDevice;

    NsmAERErrorStatusIntfTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        nsmDevice = std::make_shared<MockNsmDevice>(0x10, 0, "MCTP_EID", "16",
                                                    0);

        aerIntf = std::make_shared<NsmAERErrorStatusIntf>(
            bus, path.c_str(), deviceIndex, nsmDevice);

        EXPECT_NE(aerIntf, nullptr);
    }
};

TEST_F(NsmAERErrorStatusIntfTest, goodTestConstructor)
{
    EXPECT_NE(aerIntf->device, nullptr);
    EXPECT_EQ(aerIntf->deviceIndex, deviceIndex);
}

TEST_F(NsmAERErrorStatusIntfTest, goodTestDevice)
{
    EXPECT_EQ(aerIntf->device, nsmDevice);
}

TEST_F(NsmAERErrorStatusIntfTest, goodTestCorrectableErrorStatus)
{
    std::string status = "0x00001234";
    aerIntf->aerCorrectableErrorStatus(status);
    EXPECT_EQ(aerIntf->aerCorrectableErrorStatus(), status);
}

TEST_F(NsmAERErrorStatusIntfTest, goodTestUncorrectableErrorStatus)
{
    std::string status = "0xABCD5678";
    aerIntf->aerUncorrectableErrorStatus(status);
    EXPECT_EQ(aerIntf->aerUncorrectableErrorStatus(), status);
}

TEST_F(NsmAERErrorStatusIntfTest, goodTestBothErrorStatuses)
{
    std::string correctableStatus = "0x00000001";
    std::string uncorrectableStatus = "0x00000002";

    aerIntf->aerCorrectableErrorStatus(correctableStatus);
    aerIntf->aerUncorrectableErrorStatus(uncorrectableStatus);

    EXPECT_EQ(aerIntf->aerCorrectableErrorStatus(), correctableStatus);
    EXPECT_EQ(aerIntf->aerUncorrectableErrorStatus(), uncorrectableStatus);
}
