#include "test/mockDBusHandler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmDebugInfo.hpp"
#include "nsmEraseTrace.hpp"
#include "nsmLogInfo.hpp"

class NsmDumpCollectionTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                       0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
};

// NsmDebugInfoObject Constructor Tests
TEST_F(NsmDumpCollectionTest,
       DebugInfoObject_ConstructorWithDiagnosticsDumpType)
{
    std::string name = "DebugDump1";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test";
    std::string type = "GPU";
    nsm::DebugDumpType dumpType = nsm::DebugDumpType::Diagnostics;

    nsm::NsmDebugInfoObject debugInfo(bus, name, inventoryPath, type, testUuid,
                                      dumpType);
    EXPECT_EQ(debugInfo.getName(), name);
    EXPECT_EQ(debugInfo.getType(), type);
}

TEST_F(NsmDumpCollectionTest, DebugInfoObject_ConstructorWithNetworkDumpType)
{
    std::string name = "DebugDump2";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test";
    std::string type = "GPU";
    nsm::DebugDumpType dumpType = nsm::DebugDumpType::Network;

    nsm::NsmDebugInfoObject debugInfo(bus, name, inventoryPath, type, testUuid,
                                      dumpType);
    EXPECT_EQ(debugInfo.getName(), name);
    EXPECT_EQ(debugInfo.getType(), type);
}

// NsmEraseTraceObject Constructor Tests
TEST_F(NsmDumpCollectionTest, EraseTraceObject_ConstructorBasic)
{
    std::string name = "EraseTrace1";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test";
    std::string type = "GPU";

    nsm::NsmEraseTraceObject eraseTrace(bus, name, inventoryPath, type,
                                        testUuid);
    EXPECT_EQ(eraseTrace.getName(), name);
    EXPECT_EQ(eraseTrace.getType(), type);
}

// NsmLogInfoObject Constructor Tests
TEST_F(NsmDumpCollectionTest, LogInfoObject_ConstructorBasic)
{
    std::string name = "LogInfo1";
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test";
    std::string type = "GPU";

    nsm::NsmLogInfoObject logInfo(bus, name, inventoryPath, type, testUuid);
    EXPECT_EQ(logInfo.getName(), name);
    EXPECT_EQ(logInfo.getType(), type);
}
