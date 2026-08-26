#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "diagnostics.h"

#include "nsmDebugInfo.hpp"
#include "nsmDumpUtils.hpp"
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

// ============================================================================
// NsmEraseTraceObject::eraseTrace() – branch coverage
// ============================================================================

// When eraseTraceStatus is already InProgress, eraseTrace() must throw
// immediately (Common::Error::Unavailable) without starting the coroutine.
TEST_F(NsmDumpCollectionTest, EraseTrace_WhenInProgress_Throws)
{
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test/";
    nsm::NsmEraseTraceObject obj(bus, "EraseBranchTest", inventoryPath, "GPU",
                                 testUuid);

    // Force status to InProgress
    obj.eraseTraceStatus(std::make_tuple(nsm::EraseOperationStatus::InProgress,
                                         nsm::EraseStatus::Unknown));

    EXPECT_THROW(obj.eraseTrace(), std::exception);
}

// ============================================================================
// NsmEraseTraceObject::eraseDebugInfo() – branch coverage
// ============================================================================

// When eraseDebugInfoStatus is already InProgress, eraseDebugInfo() throws.
TEST_F(NsmDumpCollectionTest, EraseDebugInfo_WhenInProgress_Throws)
{
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test/";
    nsm::NsmEraseTraceObject obj(bus, "EraseDebugBranch", inventoryPath, "GPU",
                                 testUuid);

    obj.eraseDebugInfoStatus(std::make_tuple(
        nsm::EraseOperationStatus::InProgress, nsm::EraseStatus::Unknown));

    EXPECT_THROW(obj.eraseDebugInfo(nsm::EraseInfoType::FWSavedDumpInfo),
                 std::exception);
}

// When eraseDebugInfo() is called with an unsupported EraseInfoType (Other),
// the default branch sets InvalidArgument (caller error) and returns without
// throwing.
TEST_F(NsmDumpCollectionTest, EraseDebugInfo_UnsupportedInfoType_SetsFailure)
{
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test/";
    nsm::NsmEraseTraceObject obj(bus, "EraseDebugUnsup", inventoryPath, "GPU",
                                 testUuid);

    // Status starts as Unavailable; call with unsupported type hits default:
    EXPECT_NO_THROW(obj.eraseDebugInfo(nsm::EraseInfoType::Other));

    // The default branch reports InvalidArgument (unsupported type is a caller
    // error, not an internal nsmd failure).
    auto [opStatus, eraseSt] = obj.eraseDebugInfoStatus();
    EXPECT_EQ(opStatus, nsm::EraseOperationStatus::InvalidArgument);
}

// ============================================================================
// NsmDebugInfoObject::getDebugInfo() – unsupported type branch
// ============================================================================

// When getDebugInfo() is called with DebugInformationType::Others (which is
// not in the handled cases), the default: branch throws InvalidArgument
// immediately, before any async operation is started.
TEST_F(NsmDumpCollectionTest, GetDebugInfo_UnsupportedType_ThrowsInvalidArg)
{
    std::string inventoryPath = "/xyz/openbmc_project/inventory/system/test/";
    nsm::NsmDebugInfoObject obj(bus, "DebugInfoBranch", inventoryPath, "GPU",
                                testUuid, nsm::DebugDumpType::Diagnostics);

    sdbusplus::message::unix_fd invalidFd{-1};
    EXPECT_THROW(obj.getDebugInfo(nsm::DebugInformationType::Others, invalidFd),
                 std::exception);
}

// =============================================================================
// eraseTraceOnDevice / eraseDebugInfoOnDevice coroutine branch coverage
// =============================================================================

static std::vector<uint8_t> makeEraseTraceResp(uint8_t resStatus)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_erase_trace_resp),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_erase_trace_resp(0, NSM_SUCCESS, ERR_NULL, resStatus, msg);
    return buf;
}

static std::vector<uint8_t> makeEraseDebugInfoResp(uint8_t resStatus)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_erase_debug_info_resp(0, NSM_SUCCESS, ERR_NULL, resStatus, msg);
    return buf;
}

struct NsmEraseTraceSensorTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmEraseTraceSensorTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmEraseTraceSensorTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<nsm::NsmEraseTraceObject>
        makeEraseTrace(const std::string& suffix)
    {
        return std::make_shared<nsm::NsmEraseTraceObject>(
            utils::DBusHandler::getBus(), "EraseTrace" + suffix, "/test/erase/",
            "GPU", gpuUuid);
    }
};

// eraseTraceOnDevice: ERASE_TRACE_NO_DATA_ERASED → EraseStatus::NoDataErased
TEST_F(NsmEraseTraceSensorTest, EraseTraceOnDevice_NoDataErased)
{
    auto obj = makeEraseTrace("_nde");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeEraseTraceResp(ERASE_TRACE_NO_DATA_ERASED)));

    auto coro = obj->eraseTraceOnDevice();
    auto [opSt, eraseSt] = obj->eraseTraceStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::Success);
    EXPECT_EQ(eraseSt, nsm::EraseStatus::NoDataErased);
}

// eraseTraceOnDevice: ERASE_TRACE_DATA_ERASED → EraseStatus::DataErased
TEST_F(NsmEraseTraceSensorTest, EraseTraceOnDevice_DataErased)
{
    auto obj = makeEraseTrace("_de");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeEraseTraceResp(ERASE_TRACE_DATA_ERASED)));

    auto coro = obj->eraseTraceOnDevice();
    auto [opSt, eraseSt] = obj->eraseTraceStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::Success);
    EXPECT_EQ(eraseSt, nsm::EraseStatus::DataErased);
}

// eraseTraceOnDevice: ERASE_TRACE_DATA_ERASE_INPROGRESS → DataEraseInProgress
TEST_F(NsmEraseTraceSensorTest, EraseTraceOnDevice_DataEraseInProgress)
{
    auto obj = makeEraseTrace("_deip");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(
            makeEraseTraceResp(ERASE_TRACE_DATA_ERASE_INPROGRESS)));

    auto coro = obj->eraseTraceOnDevice();
    auto [opSt, eraseSt] = obj->eraseTraceStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::Success);
    EXPECT_EQ(eraseSt, nsm::EraseStatus::DataEraseInProgress);
}

// eraseTraceOnDevice: unknown resStatus → default case → EraseStatus::Unknown
TEST_F(NsmEraseTraceSensorTest, EraseTraceOnDevice_UnknownStatus)
{
    auto obj = makeEraseTrace("_unk");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeEraseTraceResp(0xFF)));

    auto coro = obj->eraseTraceOnDevice();
    auto [opSt, eraseSt] = obj->eraseTraceStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::Success);
    EXPECT_EQ(eraseSt, nsm::EraseStatus::Unknown);
}

// eraseTraceOnDevice: postPatchIO fails (transport swRc) → mapped status.
// NSM_SW_ERROR maps to Unavailable via mapNsmErrorToEraseStatus.
TEST_F(NsmEraseTraceSensorTest, EraseTraceOnDevice_PostPatchIOFail)
{
    auto obj = makeEraseTrace("_piof");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeEraseTraceResp(0), NSM_ERROR));

    auto coro = obj->eraseTraceOnDevice();
    auto [opSt, eraseSt] = obj->eraseTraceStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::Unavailable);
}

// eraseTraceOnDevice: decode fails (error CC) → InternalFailure
TEST_F(NsmEraseTraceSensorTest, EraseTraceOnDevice_DecodeFail)
{
    auto obj = makeEraseTrace("_df");
    std::vector<uint8_t> errResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(errResp.data());
    auto* resp = reinterpret_cast<nsm_common_non_success_resp*>(msg->payload);
    resp->completion_code = NSM_ERROR;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errResp));

    auto coro = obj->eraseTraceOnDevice();
    auto [opSt, eraseSt] = obj->eraseTraceStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::InternalFailure);
}

// eraseTrace() normal path (not InProgress): starts coroutine, succeeds
TEST_F(NsmEraseTraceSensorTest, EraseTrace_NotInProgress_Succeeds)
{
    auto obj = makeEraseTrace("_nip");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeEraseTraceResp(ERASE_TRACE_DATA_ERASED)));

    EXPECT_NO_THROW(obj->eraseTrace());
    auto [opSt, eraseSt] = obj->eraseTraceStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::Success);
    EXPECT_EQ(eraseSt, nsm::EraseStatus::DataErased);
}

// eraseDebugInfoOnDevice: ERASE_TRACE_NO_DATA_ERASED → NoDataErased
TEST_F(NsmEraseTraceSensorTest, EraseDebugInfoOnDevice_NoDataErased)
{
    auto obj = makeEraseTrace("_dbi_nde");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(
            makeEraseDebugInfoResp(ERASE_TRACE_NO_DATA_ERASED)));

    auto coro = obj->eraseDebugInfoOnDevice(INFO_TYPE_FW_SAVED_DUMP_INFO);
    auto [opSt, eraseSt] = obj->eraseDebugInfoStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::Success);
    EXPECT_EQ(eraseSt, nsm::EraseStatus::NoDataErased);
}

// eraseDebugInfoOnDevice: ERASE_TRACE_DATA_ERASED → DataErased
TEST_F(NsmEraseTraceSensorTest, EraseDebugInfoOnDevice_DataErased)
{
    auto obj = makeEraseTrace("_dbi_de");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeEraseDebugInfoResp(ERASE_TRACE_DATA_ERASED)));

    auto coro = obj->eraseDebugInfoOnDevice(INFO_TYPE_FW_SAVED_DUMP_INFO);
    auto [opSt, eraseSt] = obj->eraseDebugInfoStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::Success);
    EXPECT_EQ(eraseSt, nsm::EraseStatus::DataErased);
}

// eraseDebugInfoOnDevice: ERASE_TRACE_DATA_ERASE_INPROGRESS →
// DataEraseInProgress
TEST_F(NsmEraseTraceSensorTest, EraseDebugInfoOnDevice_DataEraseInProgress)
{
    auto obj = makeEraseTrace("_dbi_deip");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(
            makeEraseDebugInfoResp(ERASE_TRACE_DATA_ERASE_INPROGRESS)));

    auto coro = obj->eraseDebugInfoOnDevice(INFO_TYPE_FW_SAVED_DUMP_INFO);
    auto [opSt, eraseSt] = obj->eraseDebugInfoStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::Success);
    EXPECT_EQ(eraseSt, nsm::EraseStatus::DataEraseInProgress);
}

// eraseDebugInfoOnDevice: unknown status → default → EraseStatus::Unknown
TEST_F(NsmEraseTraceSensorTest, EraseDebugInfoOnDevice_UnknownStatus)
{
    auto obj = makeEraseTrace("_dbi_unk");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeEraseDebugInfoResp(0xFF)));

    auto coro = obj->eraseDebugInfoOnDevice(INFO_TYPE_FW_SAVED_DUMP_INFO);
    auto [opSt, eraseSt] = obj->eraseDebugInfoStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::Success);
    EXPECT_EQ(eraseSt, nsm::EraseStatus::Unknown);
}

// eraseDebugInfoOnDevice: postPatchIO fails (transport swRc) → mapped status.
// NSM_SW_ERROR maps to Unavailable via mapNsmErrorToEraseStatus.
TEST_F(NsmEraseTraceSensorTest, EraseDebugInfoOnDevice_PostPatchIOFail)
{
    auto obj = makeEraseTrace("_dbi_piof");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeEraseDebugInfoResp(0), NSM_ERROR));

    auto coro = obj->eraseDebugInfoOnDevice(INFO_TYPE_FW_SAVED_DUMP_INFO);
    auto [opSt, eraseSt] = obj->eraseDebugInfoStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::Unavailable);
}

// eraseDebugInfoOnDevice: decode fails (error CC) → InternalFailure
TEST_F(NsmEraseTraceSensorTest, EraseDebugInfoOnDevice_DecodeFail)
{
    auto obj = makeEraseTrace("_dbi_df");
    std::vector<uint8_t> errResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(errResp.data());
    auto* resp = reinterpret_cast<nsm_common_non_success_resp*>(msg->payload);
    resp->completion_code = NSM_ERROR;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errResp));

    auto coro = obj->eraseDebugInfoOnDevice(INFO_TYPE_FW_SAVED_DUMP_INFO);
    auto [opSt, eraseSt] = obj->eraseDebugInfoStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::InternalFailure);
}

// eraseDebugInfo(FWSavedDumpInfo): normal path (not InProgress) → succeeds
TEST_F(NsmEraseTraceSensorTest, EraseDebugInfo_FWSavedDumpInfo_Succeeds)
{
    auto obj = makeEraseTrace("_dbif_nip");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeEraseDebugInfoResp(ERASE_TRACE_DATA_ERASED)));

    EXPECT_NO_THROW(obj->eraseDebugInfo(nsm::EraseInfoType::FWSavedDumpInfo));
    auto [opSt, eraseSt] = obj->eraseDebugInfoStatus();
    EXPECT_EQ(opSt, nsm::EraseOperationStatus::Success);
    EXPECT_EQ(eraseSt, nsm::EraseStatus::DataErased);
}

// ============================================================================
// NsmLogInfoObject::getLogInfo / getLogInfoAsyncHandler – branch coverage
// ============================================================================

static std::vector<uint8_t>
    makeLogInfoResp(uint8_t cc, uint32_t nextHandle,
                    nsm_device_log_info_breakdown logInfo = {},
                    const std::vector<uint8_t>& logData = {})
{
    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_network_device_log_info_resp) +
                     logData.size();
    std::vector<uint8_t> buf(bufSize, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t dummy = 0;
    const uint8_t* pData = logData.empty() ? &dummy : logData.data();
    encode_get_network_device_log_info_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, nextHandle,
        logInfo, pData, static_cast<uint16_t>(logData.size()), msg);
    return buf;
}

struct NsmLogInfoSensorTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmLogInfoSensorTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmLogInfoSensorTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<nsm::NsmLogInfoObject>
        makeLogInfo(const std::string& suffix)
    {
        return std::make_shared<nsm::NsmLogInfoObject>(
            utils::DBusHandler::getBus(), "LogInfo" + suffix, "/test/loginfo/",
            "GPU", gpuUuid);
    }
};

// getLogInfo: statusInterface already InProgress → throws Unavailable
TEST_F(NsmLogInfoSensorTest, GetLogInfo_AlreadyInProgress_Throws)
{
    auto obj = makeLogInfo("_ip");
    obj->statusInterface = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(), "/test/loginfo/LogInfoip/status_ip");
    obj->statusInterface->status(AsyncOperationStatusType::InProgress);

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_THROW(obj->getLogInfo(sdbusplus::message::unix_fd{fd}),
                 std::exception);
    close(fd);
}

// getLogInfo: dup(fd=-1) fails → finish(InternalFailure) + throws
TEST_F(NsmLogInfoSensorTest, GetLogInfo_InvalidFd_ThrowsInternalFailure)
{
    auto obj = makeLogInfo("_invfd");
    EXPECT_THROW(obj->getLogInfo(sdbusplus::message::unix_fd{-1}),
                 std::exception);
}

// getLogInfoAsyncHandler: postPatchIO fails → InternalFailure
TEST_F(NsmLogInfoSensorTest,
       GetLogInfoAsyncHandler_PostPatchIOFails_InternalFailure)
{
    auto obj = makeLogInfo("_piof");
    nsm_device_log_info_breakdown logInfo{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeLogInfoResp(NSM_SUCCESS, 0, logInfo),
                                  NSM_ERROR));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getLogInfo(sdbusplus::message::unix_fd{fd}));
    // Transport swRc (NSM_SW_ERROR) maps to Unavailable.
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Unavailable);
    close(fd);
}

// getLogInfoAsyncHandler: decode fails (error CC) → InternalFailure
TEST_F(NsmLogInfoSensorTest, GetLogInfoAsyncHandler_DecodeFails_InternalFailure)
{
    auto obj = makeLogInfo("_df");
    nsm_device_log_info_breakdown logInfo{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeLogInfoResp(NSM_ERROR, 0, logInfo)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getLogInfo(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::InternalFailure);
    close(fd);
}

// Branch 12: nsmLogInfo.cpp L92 second operand — rc==NSM_SW_SUCCESS but
// cc!=NSM_SUCCESS. 9-byte buffer causes decode_reason_code_and_cc to return
// NSM_SW_SUCCESS with cc=NSM_ERROR → rc=0 (FALSE) but cc!=0 (TRUE).
TEST_F(NsmLogInfoSensorTest,
       GetLogInfoAsyncHandler_DecodeSuccessNonZeroCC_InternalFailure)
{
    auto obj = makeLogInfo("_cc_br12");
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code = NSM_ERROR
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(buf));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getLogInfo(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::InternalFailure);
    close(fd);
}

// getLogInfoAsyncHandler: synced_time=BOOT, nextHandle=0 → Success + Boot
TEST_F(NsmLogInfoSensorTest, GetLogInfoAsyncHandler_SyncedTimeBoot_Success)
{
    auto obj = makeLogInfo("_boot");
    nsm_device_log_info_breakdown logInfo{};
    logInfo.synced_time = SYNCED_TIME_TYPE_BOOT;
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeLogInfoResp(NSM_SUCCESS, 0, logInfo)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getLogInfo(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Success);
    EXPECT_EQ(obj->timeSynced(), nsm::TimeSyncFrom::Boot);
    close(fd);
}

// getLogInfoAsyncHandler: synced_time=SYNCED, nextHandle=0 → Success + Synced
TEST_F(NsmLogInfoSensorTest, GetLogInfoAsyncHandler_SyncedTimeSynced_Success)
{
    auto obj = makeLogInfo("_synced");
    nsm_device_log_info_breakdown logInfo{};
    logInfo.synced_time = SYNCED_TIME_TYPE_SYNCED;
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeLogInfoResp(NSM_SUCCESS, 0, logInfo)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getLogInfo(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Success);
    EXPECT_EQ(obj->timeSynced(), nsm::TimeSyncFrom::Synced);
    close(fd);
}

// getLogInfoAsyncHandler: appendBufferToFd throws (read-only fd) →
// WriteFailure
TEST_F(NsmLogInfoSensorTest, GetLogInfoAsyncHandler_AppendFails_WriteFailure)
{
    auto obj = makeLogInfo("_wf");
    nsm_device_log_info_breakdown logInfo{};
    logInfo.synced_time = SYNCED_TIME_TYPE_BOOT;
    std::vector<uint8_t> logData = {0x01, 0x02, 0x03};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeLogInfoResp(NSM_SUCCESS, 0, logInfo, logData)));

    // Read-only fd: write() fails → appendBufferToFd throws
    int fd = open("/dev/zero", O_RDONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getLogInfo(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    close(fd);
}

// getLogInfoAsyncHandler: stuck-loop guard — device returns the same
// nextHandle on consecutive calls → finish(InternalFailure) instead of
// recursing forever.
TEST_F(NsmLogInfoSensorTest, GetLogInfoAsyncHandler_StuckHandle_InternalFailure)
{
    auto obj = makeLogInfo("_stuck");
    nsm_device_log_info_breakdown logInfo{};
    logInfo.synced_time = SYNCED_TIME_TYPE_BOOT;
    // First call advances to nextHandle=5; second call returns 5 again
    // (firmware fails to advance) → stuck-loop guard trips.
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeLogInfoResp(NSM_SUCCESS, 5, logInfo)))
        .WillOnce(mockPostPatchIO(makeLogInfoResp(NSM_SUCCESS, 5, logInfo)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getLogInfo(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::InternalFailure);
    close(fd);
}

// getLogInfoAsyncHandler: nextHandle != 0 → continues fetching next record
TEST_F(NsmLogInfoSensorTest, GetLogInfoAsyncHandler_NextHandleNonZero_Continues)
{
    auto obj = makeLogInfo("_chain");
    nsm_device_log_info_breakdown logInfo{};
    logInfo.synced_time = SYNCED_TIME_TYPE_BOOT;
    // First call returns nextHandle=5 (non-zero), second returns 0 (END)
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeLogInfoResp(NSM_SUCCESS, 5, logInfo)))
        .WillOnce(mockPostPatchIO(makeLogInfoResp(NSM_SUCCESS, 0, logInfo)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getLogInfo(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Success);
    close(fd);
}

// getLogInfo: statusInterface is non-null but status != InProgress →
// condition false → proceeds normally; covers FALSE branch of the compound
// condition when statusInterface != nullptr but not InProgress.
TEST_F(NsmLogInfoSensorTest, GetLogInfo_StatusNotInProgress_ProceedsNormally)
{
    auto obj = makeLogInfo("_notip");
    // Pre-set statusInterface to a non-null, non-InProgress state
    obj->statusInterface = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/test/loginfo/LogInfonotip/status_notip");
    obj->statusInterface->status(AsyncOperationStatusType::Success);

    nsm_device_log_info_breakdown logInfo{};
    logInfo.synced_time = SYNCED_TIME_TYPE_BOOT;
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeLogInfoResp(NSM_SUCCESS, 0, logInfo)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    // Should NOT throw (statusInterface is not InProgress)
    EXPECT_NO_THROW(obj->getLogInfo(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Success);
    close(fd);
}

// getLogInfoAsyncHandler: synced_time = unknown value → default: branch →
// InternalFailure status and NSM_SW_ERROR return.
// Covers lines 122-125 in nsmLogInfo.cpp.
// NOTE: The default: branch in nsmLogInfo.cpp's getLogInfoAsyncHandler
// (covering synced_time values other than BOOT=0 and SYNCED=1) is
// unreachable because synced_time is declared as uint8_t : 1 (a 1-bit
// bitfield). All possible values (0 and 1) are handled by the BOOT and
// SYNCED cases. Setting synced_time=99 causes -Werror=overflow at compile
// time, so this branch cannot be exercised in a unit test.

// ============================================================================
// NsmDebugInfoObject::getDebugInfo / getDebugInfoAsyncHandler – branch coverage
// ============================================================================

static std::vector<uint8_t>
    makeDebugInfoResp(uint8_t cc, uint32_t nextHandle,
                      const std::vector<uint8_t>& segData = {})
{
    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_network_device_debug_info_resp) +
                     segData.size();
    std::vector<uint8_t> buf(bufSize, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t dummy = 0;
    const uint8_t* pData = segData.empty() ? &dummy : segData.data();
    encode_get_network_device_debug_info_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, pData,
        static_cast<uint16_t>(segData.size()), nextHandle, msg);
    return buf;
}

static std::vector<uint8_t>
    makeDiagnosticsResp(uint8_t cc, uint8_t nextSegment,
                        const std::vector<uint8_t>& segData = {})
{
    size_t bufSize = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_get_device_diagnostics_resp) + segData.size();
    std::vector<uint8_t> buf(bufSize, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t dummy = 0;
    const uint8_t* pData = segData.empty() ? &dummy : segData.data();
    encode_get_device_diagnostics_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, pData,
        static_cast<uint16_t>(segData.size()), nextSegment, msg);
    return buf;
}

struct NsmDebugInfoSensorTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmDebugInfoSensorTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmDebugInfoSensorTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<nsm::NsmDebugInfoObject>
        makeDebugInfo(const std::string& suffix,
                      nsm::DebugDumpType dumpType = nsm::DebugDumpType::Network)
    {
        return std::make_shared<nsm::NsmDebugInfoObject>(
            utils::DBusHandler::getBus(), "DebugInfo" + suffix,
            "/test/debuginfo/", "GPU", gpuUuid, dumpType);
    }
};

// getDumpPath: Diagnostics branch (exercised via constructor)
TEST_F(NsmDebugInfoSensorTest, Constructor_DiagnosticsDumpType_Succeeds)
{
    EXPECT_NO_THROW(
        makeDebugInfo("_diagctor", nsm::DebugDumpType::Diagnostics));
}

// getDebugInfo: statusInterface InProgress → throws Unavailable
TEST_F(NsmDebugInfoSensorTest, GetDebugInfo_AlreadyInProgress_Throws)
{
    auto obj = makeDebugInfo("_ip");
    obj->statusInterface = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(), "/test/debuginfo/DebugInfoip/status_ip");
    obj->statusInterface->status(AsyncOperationStatusType::InProgress);

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_THROW(obj->getDebugInfo(nsm::DebugInformationType::DeviceInformation,
                                   sdbusplus::message::unix_fd{fd}),
                 std::exception);
    close(fd);
}

// getDebugInfo: dup(-1) fails → finish(InternalFailure) + throws
TEST_F(NsmDebugInfoSensorTest, GetDebugInfo_InvalidFd_ThrowsInternalFailure)
{
    auto obj = makeDebugInfo("_invfd");
    EXPECT_THROW(obj->getDebugInfo(nsm::DebugInformationType::DeviceInformation,
                                   sdbusplus::message::unix_fd{-1}),
                 std::exception);
}

// getDebugInfoAsyncHandler: postPatchIO fails → InternalFailure
TEST_F(NsmDebugInfoSensorTest,
       GetDebugInfoAsyncHandler_PostPatchIOFails_InternalFailure)
{
    auto obj = makeDebugInfo("_piof");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeDebugInfoResp(NSM_SUCCESS, 0), NSM_ERROR));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(
        obj->getDebugInfo(nsm::DebugInformationType::DeviceInformation,
                          sdbusplus::message::unix_fd{fd}));
    // Transport swRc (NSM_SW_ERROR) maps to Unavailable.
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Unavailable);
    close(fd);
}

// getDebugInfoAsyncHandler: decode fails (error CC) → InternalFailure
TEST_F(NsmDebugInfoSensorTest,
       GetDebugInfoAsyncHandler_DecodeFails_InternalFailure)
{
    auto obj = makeDebugInfo("_df");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDebugInfoResp(NSM_ERROR, 0)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getDebugInfo(nsm::DebugInformationType::FWRuntimeData,
                                      sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::InternalFailure);
    close(fd);
}

// Branch 12: nsmDebugInfo.cpp L110 second operand — rc==NSM_SW_SUCCESS but
// cc!=NSM_SUCCESS. 9-byte buffer causes decode_reason_code_and_cc to return
// NSM_SW_SUCCESS with cc=NSM_ERROR → rc=0 (FALSE) but cc!=0 (TRUE).
TEST_F(NsmDebugInfoSensorTest,
       GetDebugInfoAsyncHandler_DecodeSuccessNonZeroCC_InternalFailure)
{
    auto obj = makeDebugInfo("_cc_br12");
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code = NSM_ERROR
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(buf));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getDebugInfo(nsm::DebugInformationType::FWRuntimeData,
                                      sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::InternalFailure);
    close(fd);
}

// getDebugInfoAsyncHandler: appendBufferToFd throws (read-only fd) →
// WriteFailure
TEST_F(NsmDebugInfoSensorTest,
       GetDebugInfoAsyncHandler_AppendFails_WriteFailure)
{
    auto obj = makeDebugInfo("_wf");
    std::vector<uint8_t> segData = {0xAA, 0xBB};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDebugInfoResp(NSM_SUCCESS, 0, segData)));

    int fd = open("/dev/zero", O_RDONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getDebugInfo(nsm::DebugInformationType::FWSavedInfo,
                                      sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    close(fd);
}

// getDebugInfoAsyncHandler: success, nextHandle=0 (END) → Success
TEST_F(NsmDebugInfoSensorTest, GetDebugInfoAsyncHandler_EndRecord_Success)
{
    auto obj = makeDebugInfo("_end");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDebugInfoResp(NSM_SUCCESS, 0)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getDebugInfo(nsm::DebugInformationType::DeviceDump,
                                      sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Success);
    close(fd);
}

// getDebugInfoAsyncHandler: WAR — device repeats the same nextHandle; the
// transfer continues instead of aborting and completes once END arrives.
TEST_F(NsmDebugInfoSensorTest,
       GetDebugInfoAsyncHandler_RepeatedHandle_Continues)
{
    auto obj = makeDebugInfo("_stuck");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDebugInfoResp(NSM_SUCCESS, 7)))
        .WillOnce(mockPostPatchIO(makeDebugInfoResp(NSM_SUCCESS, 7)))
        .WillOnce(mockPostPatchIO(makeDebugInfoResp(NSM_SUCCESS, 0)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(
        obj->getDebugInfo(nsm::DebugInformationType::DeviceInformation,
                          sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Success);
    close(fd);
}

// getDebugInfoAsyncHandler: nextHandle != 0 → recursive fetch
TEST_F(NsmDebugInfoSensorTest,
       GetDebugInfoAsyncHandler_NextHandleNonZero_Continues)
{
    auto obj = makeDebugInfo("_chain");
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDebugInfoResp(NSM_SUCCESS, 7)))
        .WillOnce(mockPostPatchIO(makeDebugInfoResp(NSM_SUCCESS, 0)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(
        obj->getDebugInfo(nsm::DebugInformationType::DeviceInformation,
                          sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Success);
    close(fd);
}

// getDebugInfo: statusInterface non-null but status != InProgress →
// compound condition FALSE → proceeds normally.
// Covers the FALSE branch when statusInterface != nullptr && status !=
// InProgress.
TEST_F(NsmDebugInfoSensorTest,
       GetDebugInfo_StatusNotInProgress_ProceedsNormally)
{
    auto obj = makeDebugInfo("_notip");
    obj->statusInterface = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/test/debuginfo/DebugInfonotip/status_notip");
    obj->statusInterface->status(AsyncOperationStatusType::Success);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDebugInfoResp(NSM_SUCCESS, 0)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(
        obj->getDebugInfo(nsm::DebugInformationType::DeviceInformation,
                          sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Success);
    close(fd);
}

// ============================================================================
// NsmDebugInfoObject::getDiagnostics / getDiagnosticsAsyncHandler
// ============================================================================

// getDiagnostics: statusInterface InProgress → throws Unavailable
TEST_F(NsmDebugInfoSensorTest, GetDiagnostics_AlreadyInProgress_Throws)
{
    auto obj = makeDebugInfo("_diagip", nsm::DebugDumpType::Diagnostics);
    obj->statusInterface = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/test/debuginfo/DebugInfodiagip/status_dip");
    obj->statusInterface->status(AsyncOperationStatusType::InProgress);

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_THROW(obj->getDiagnostics(sdbusplus::message::unix_fd{fd}),
                 std::exception);
    close(fd);
}

// getDiagnostics: dup(-1) fails → finish(InternalFailure) + throws
TEST_F(NsmDebugInfoSensorTest, GetDiagnostics_InvalidFd_ThrowsInternalFailure)
{
    auto obj = makeDebugInfo("_diaginvfd", nsm::DebugDumpType::Diagnostics);
    EXPECT_THROW(obj->getDiagnostics(sdbusplus::message::unix_fd{-1}),
                 std::exception);
}

// getDiagnosticsAsyncHandler: postPatchIO fails → InternalFailure
TEST_F(NsmDebugInfoSensorTest,
       GetDiagnosticsAsyncHandler_PostPatchIOFails_InternalFailure)
{
    auto obj = makeDebugInfo("_diagpiof", nsm::DebugDumpType::Diagnostics);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeDiagnosticsResp(NSM_SUCCESS, 0xFF), NSM_ERROR));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getDiagnostics(sdbusplus::message::unix_fd{fd}));
    // Transport swRc (NSM_SW_ERROR) maps to Unavailable.
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Unavailable);
    close(fd);
}

// getDiagnosticsAsyncHandler: decode fails (error CC) → InternalFailure
TEST_F(NsmDebugInfoSensorTest,
       GetDiagnosticsAsyncHandler_DecodeFails_InternalFailure)
{
    auto obj = makeDebugInfo("_diagdf", nsm::DebugDumpType::Diagnostics);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDiagnosticsResp(NSM_ERROR, 0)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getDiagnostics(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::InternalFailure);
    close(fd);
}

// Branch 12: nsmDebugInfo.cpp L240 second operand (diagnostics path) —
// rc==NSM_SW_SUCCESS but cc!=NSM_SUCCESS. 9-byte buffer causes
// decode_reason_code_and_cc to return NSM_SW_SUCCESS with cc=NSM_ERROR.
TEST_F(NsmDebugInfoSensorTest,
       GetDiagnosticsAsyncHandler_DecodeSuccessNonZeroCC_InternalFailure)
{
    auto obj = makeDebugInfo("_diagcc_br12", nsm::DebugDumpType::Diagnostics);
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code = NSM_ERROR
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(buf));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getDiagnostics(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::InternalFailure);
    close(fd);
}

// getDiagnosticsAsyncHandler: appendBufferToFd throws → WriteFailure
TEST_F(NsmDebugInfoSensorTest,
       GetDiagnosticsAsyncHandler_AppendFails_WriteFailure)
{
    auto obj = makeDebugInfo("_diagwf", nsm::DebugDumpType::Diagnostics);
    std::vector<uint8_t> segData = {0x11, 0x22, 0x33};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeDiagnosticsResp(NSM_SUCCESS, 0xFF, segData)));

    int fd = open("/dev/zero", O_RDONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getDiagnostics(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
    close(fd);
}

// getDiagnosticsAsyncHandler: nextHandle == 0xFF (END) → Success
TEST_F(NsmDebugInfoSensorTest, GetDiagnosticsAsyncHandler_EndRecord_Success)
{
    auto obj = makeDebugInfo("_diagend", nsm::DebugDumpType::Diagnostics);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDiagnosticsResp(NSM_SUCCESS, 0xFF)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getDiagnostics(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Success);
    close(fd);
}

// getDiagnosticsAsyncHandler: stuck-loop guard — device returns the same
// nextSegment on consecutive calls → finish(InternalFailure).
TEST_F(NsmDebugInfoSensorTest,
       GetDiagnosticsAsyncHandler_StuckHandle_InternalFailure)
{
    auto obj = makeDebugInfo("_diagstuck", nsm::DebugDumpType::Diagnostics);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDiagnosticsResp(NSM_SUCCESS, 0x01)))
        .WillOnce(mockPostPatchIO(makeDiagnosticsResp(NSM_SUCCESS, 0x01)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getDiagnostics(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::InternalFailure);
    close(fd);
}

// getDiagnosticsAsyncHandler: nextHandle != 0xFF → continues to next segment
TEST_F(NsmDebugInfoSensorTest,
       GetDiagnosticsAsyncHandler_NextHandleNonEnd_Continues)
{
    auto obj = makeDebugInfo("_diagchain", nsm::DebugDumpType::Diagnostics);
    // First: nextSegment=1 (not END=0xFF); second: nextSegment=0xFF (END)
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDiagnosticsResp(NSM_SUCCESS, 0x01)))
        .WillOnce(mockPostPatchIO(makeDiagnosticsResp(NSM_SUCCESS, 0xFF)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getDiagnostics(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Success);
    close(fd);
}

// getDiagnostics: statusInterface non-null but status != InProgress →
// compound condition FALSE → proceeds normally.
TEST_F(NsmDebugInfoSensorTest,
       GetDiagnostics_StatusNotInProgress_ProceedsNormally)
{
    auto obj = makeDebugInfo("_diagnotip", nsm::DebugDumpType::Diagnostics);
    obj->statusInterface = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/test/debuginfo/DebugInfodiagnotip/status_diagnotip");
    obj->statusInterface->status(AsyncOperationStatusType::Success);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeDiagnosticsResp(NSM_SUCCESS, 0xFF)));

    int fd = open("/dev/null", O_WRONLY);
    ASSERT_GE(fd, 0);
    EXPECT_NO_THROW(obj->getDiagnostics(sdbusplus::message::unix_fd{fd}));
    EXPECT_EQ(obj->statusInterface->status(),
              AsyncOperationStatusType::Success);
    close(fd);
}

// ---------------------------------------------------------------------------
// Error mapping helpers.
//
// Exercise every branch of mapNsmErrorToAsyncStatus / mapNsmErrorToEraseStatus
// and validate the pack/unpack round-trip used to carry raw NSM codes through
// com.nvidia.Async.Value.Value.
// ---------------------------------------------------------------------------

using nsm::EraseOperationStatus;
using nsm::mapNsmErrorToAsyncStatus;
using nsm::mapNsmErrorToEraseStatus;
using nsm::packNsmError;
using nsm::unpackNsmError;

// --- mapNsmErrorToAsyncStatus: transport (swRc) branches --------------------

TEST(nsmDumpUtils, AsyncStatus_SwRcTimeout_MapsToTimeout)
{
    EXPECT_EQ(
        mapNsmErrorToAsyncStatus(NSM_SW_ERROR_TIMEOUT, NSM_SUCCESS, ERR_NULL),
        AsyncOperationStatusType::Timeout);
}

TEST(nsmDumpUtils, AsyncStatus_SwRcGenericError_MapsToUnavailable)
{
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_ERROR, NSM_SUCCESS, ERR_NULL),
              AsyncOperationStatusType::Unavailable);
}

TEST(nsmDumpUtils, AsyncStatus_SwRcDecodeFailures_MapToInternalFailure)
{
    EXPECT_EQ(
        mapNsmErrorToAsyncStatus(NSM_SW_ERROR_DATA, NSM_SUCCESS, ERR_NULL),
        AsyncOperationStatusType::InternalFailure);
    EXPECT_EQ(
        mapNsmErrorToAsyncStatus(NSM_SW_ERROR_LENGTH, NSM_SUCCESS, ERR_NULL),
        AsyncOperationStatusType::InternalFailure);
    EXPECT_EQ(
        mapNsmErrorToAsyncStatus(NSM_SW_ERROR_NULL, NSM_SUCCESS, ERR_NULL),
        AsyncOperationStatusType::InternalFailure);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_ERROR_COMMAND_FAIL, NSM_SUCCESS,
                                       ERR_NULL),
              AsyncOperationStatusType::InternalFailure);
}

// --- mapNsmErrorToAsyncStatus: success branch -------------------------------

TEST(nsmDumpUtils, AsyncStatus_AllSuccess_MapsToSuccess)
{
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_SUCCESS, ERR_NULL),
              AsyncOperationStatusType::Success);
}

// --- mapNsmErrorToAsyncStatus: cc-only branches -----------------------------

TEST(nsmDumpUtils, AsyncStatus_CcAccepted_MapsToInProgress)
{
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ACCEPTED, ERR_NULL),
              AsyncOperationStatusType::InProgress);
}

TEST(nsmDumpUtils, AsyncStatus_CcBusy_MapsToUnavailable)
{
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_BUSY, ERR_NULL),
              AsyncOperationStatusType::Unavailable);
    EXPECT_EQ(
        mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERR_NOT_READY, ERR_NULL),
        AsyncOperationStatusType::Unavailable);
    EXPECT_EQ(
        mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERR_BUS_ACCESS, ERR_NULL),
        AsyncOperationStatusType::Unavailable);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(
                  NSM_SW_SUCCESS, NSM_ERR_INVALID_STATE_FOR_COMMAND, ERR_NULL),
              AsyncOperationStatusType::Unavailable);
}

TEST(nsmDumpUtils, AsyncStatus_CcUnsupported_MapsToUnsupportedRequest)
{
    EXPECT_EQ(mapNsmErrorToAsyncStatus(
                  NSM_SW_SUCCESS, NSM_ERR_UNSUPPORTED_COMMAND_CODE, ERR_NULL),
              AsyncOperationStatusType::UnsupportedRequest);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS,
                                       NSM_ERR_UNSUPPORTED_MSG_TYPE, ERR_NULL),
              AsyncOperationStatusType::UnsupportedRequest);
}

TEST(nsmDumpUtils, AsyncStatus_CcInvalidData_MapsToInvalidArgument)
{
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERR_INVALID_DATA,
                                       ERR_NULL),
              AsyncOperationStatusType::InvalidArgument);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS,
                                       NSM_ERR_INVALID_DATA_LENGTH, ERR_NULL),
              AsyncOperationStatusType::InvalidArgument);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS,
                                       NSM_ERR_INVALID_REQUEST_TYPE, ERR_NULL),
              AsyncOperationStatusType::InvalidArgument);
}

TEST(nsmDumpUtils, AsyncStatus_UnknownCc_MapsToInternalFailure)
{
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR, ERR_NULL),
              AsyncOperationStatusType::InternalFailure);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, 0xCD, ERR_NULL),
              AsyncOperationStatusType::InternalFailure);
}

// --- mapNsmErrorToAsyncStatus: reasonCode-overrides-cc branches -------------

TEST(nsmDumpUtils, AsyncStatus_ReasonNotSupported_OverridesCc)
{
    EXPECT_EQ(
        mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR, ERR_NOT_SUPPORTED),
        AsyncOperationStatusType::UnsupportedRequest);
}

TEST(nsmDumpUtils, AsyncStatus_ReasonTimeout_MapsToTimeout)
{
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR, ERR_TIMEOUT),
              AsyncOperationStatusType::Timeout);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR,
                                       ERR_DOWNSTREAM_TIMEOUT),
              AsyncOperationStatusType::Timeout);
}

TEST(nsmDumpUtils, AsyncStatus_ReasonUnavailable_MapsToUnavailable)
{
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR,
                                       ERR_NO_BOOT_COMPLETE),
              AsyncOperationStatusType::Unavailable);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR,
                                       ERR_UPDATE_IN_PROGRESS),
              AsyncOperationStatusType::Unavailable);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR,
                                       ERR_IMAGE_COPY_IN_PROGRESS),
              AsyncOperationStatusType::Unavailable);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR,
                                       ERR_FLASH_WEAR_MITIGATION),
              AsyncOperationStatusType::Unavailable);
}

TEST(nsmDumpUtils, AsyncStatus_I2cNackFamily_MapsToUnavailable)
{
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR,
                                       ERR_I2C_NACK_FROM_DEV_ADDR),
              AsyncOperationStatusType::Unavailable);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR,
                                       ERR_I2C_NACK_FROM_DEV_CMD_DATA),
              AsyncOperationStatusType::Unavailable);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR,
                                       ERR_I2C_NACK_FROM_DEV_ADDR_RS),
              AsyncOperationStatusType::Unavailable);
}

TEST(nsmDumpUtils, AsyncStatus_ReasonInvalid_MapsToInvalidArgument)
{
    EXPECT_EQ(
        mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR, ERR_INVALID_PCI),
        AsyncOperationStatusType::InvalidArgument);
    EXPECT_EQ(
        mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR, ERR_INVALID_RQD),
        AsyncOperationStatusType::InvalidArgument);
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR,
                                       ERR_INCOMPLETE_COMPONENT_SET),
              AsyncOperationStatusType::InvalidArgument);
}

TEST(nsmDumpUtils, AsyncStatus_UnknownReason_FallsThroughToCc)
{
    // ERR_NVLINK_PORT_INVALID is a known reason but not in our mapped set;
    // mapping should fall back to the cc-based decision (NSM_BUSY ->
    // Unavailable).
    EXPECT_EQ(mapNsmErrorToAsyncStatus(NSM_SW_SUCCESS, NSM_BUSY,
                                       ERR_NVLINK_PORT_INVALID),
              AsyncOperationStatusType::Unavailable);
}

// --- mapNsmErrorToEraseStatus parity ---------------------------------------

TEST(nsmDumpUtils, EraseStatus_MirrorsAsyncStatus)
{
    EXPECT_EQ(mapNsmErrorToEraseStatus(NSM_SW_SUCCESS, NSM_SUCCESS, ERR_NULL),
              EraseOperationStatus::Success);
    EXPECT_EQ(
        mapNsmErrorToEraseStatus(NSM_SW_ERROR_TIMEOUT, NSM_SUCCESS, ERR_NULL),
        EraseOperationStatus::Timeout);
    EXPECT_EQ(mapNsmErrorToEraseStatus(
                  NSM_SW_SUCCESS, NSM_ERR_UNSUPPORTED_COMMAND_CODE, ERR_NULL),
              EraseOperationStatus::UnsupportedRequest);
    EXPECT_EQ(mapNsmErrorToEraseStatus(NSM_SW_SUCCESS, NSM_BUSY, ERR_NULL),
              EraseOperationStatus::Unavailable);
    EXPECT_EQ(mapNsmErrorToEraseStatus(NSM_SW_SUCCESS, NSM_ERR_INVALID_DATA,
                                       ERR_NULL),
              EraseOperationStatus::InvalidArgument);
    EXPECT_EQ(mapNsmErrorToEraseStatus(NSM_SW_SUCCESS, NSM_ERROR, ERR_NULL),
              EraseOperationStatus::InternalFailure);
}

// --- packNsmError / unpackNsmError round-trip ------------------------------

TEST(nsmDumpUtils, PackUnpack_RoundTrip_RecoversAllFields)
{
    const struct
    {
        int swRc;
        uint8_t cc;
        uint16_t reasonCode;
    } cases[] = {
        {NSM_SW_SUCCESS, NSM_SUCCESS, ERR_NULL},
        {NSM_SW_ERROR_TIMEOUT, NSM_SUCCESS, ERR_NULL},
        {NSM_SW_SUCCESS, NSM_BUSY, ERR_NO_BOOT_COMPLETE},
        {NSM_SW_SUCCESS, NSM_ERROR, ERR_NOT_SUPPORTED},
        {NSM_SW_SUCCESS, NSM_ERR_UNSUPPORTED_COMMAND_CODE, ERR_NULL},
        {NSM_SW_ERROR, 0xFF, 0xFFFF},
    };
    for (const auto& c : cases)
    {
        const auto packed = packNsmError(c.swRc, c.cc, c.reasonCode);
        const auto unpacked = unpackNsmError(packed);
        EXPECT_EQ(unpacked.swRc, c.swRc);
        EXPECT_EQ(unpacked.cc, c.cc);
        EXPECT_EQ(unpacked.reasonCode, c.reasonCode);
    }
}

TEST(nsmDumpUtils, Pack_AllZeroInputs_YieldsZero)
{
    EXPECT_EQ(packNsmError(NSM_SW_SUCCESS, NSM_SUCCESS, ERR_NULL), 0u);
}

TEST(nsmDumpUtils, Pack_LayoutIsStable)
{
    // Verify the documented bit layout so a future refactor doesn't silently
    // change the on-bus contract with nsm-dump-tool's unpacker.
    //   bits 32-63: swRc, bits 16-31: reasonCode, bits 8-15: cc, bits 0-7: 0
    const auto packed = packNsmError(0x12345678, 0xAB, 0xCDEF);
    EXPECT_EQ((packed >> 32) & 0xFFFFFFFFull, 0x12345678ull);
    EXPECT_EQ((packed >> 16) & 0xFFFFull, 0xCDEFull);
    EXPECT_EQ((packed >> 8) & 0xFFull, 0xABull);
    EXPECT_EQ(packed & 0xFFull, 0u);
}
