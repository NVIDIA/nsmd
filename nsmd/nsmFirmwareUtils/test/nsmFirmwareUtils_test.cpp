#include "base.h"
#include "firmware-utils.h"

#include "test/mockDBusHandler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmKeyMgmt.hpp"

class NsmFirmwareUtilsTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                       0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
};

// NsmKeyMgmt Constructor Test
TEST_F(NsmFirmwareUtilsTest, KeyMgmt_ConstructorBasic)
{
    std::string chassisName = "GPU0";
    std::string type = "GPU";
    uint16_t componentClassification = 1;
    uint16_t componentIdentifier = 2;
    uint8_t componentClassificationIndex = 0;

    // Create progress interface
    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               chassisName + "/Progress";
    auto progressIntf =
        std::make_shared<nsm::ProgressIntf>(bus, progressPath.c_str());

    nsm::NsmKeyMgmt keyMgmt(bus, chassisName, type, testUuid, progressIntf,
                            componentClassification, componentIdentifier,
                            componentClassificationIndex);

    EXPECT_EQ(keyMgmt.getName(), chassisName);
    EXPECT_EQ(keyMgmt.getType(), type);
    EXPECT_EQ(keyMgmt.componentClassification, componentClassification);
    EXPECT_EQ(keyMgmt.componentIdentifier, componentIdentifier);
    EXPECT_EQ(keyMgmt.componentClassificationIndex,
              componentClassificationIndex);
}

// NsmKeyMgmt addSlotObject Test
TEST_F(NsmFirmwareUtilsTest, KeyMgmt_AddSlotObject)
{
    std::string chassisName = "GPU0";
    std::string type = "GPU";
    uint16_t componentClassification = 1;
    uint16_t componentIdentifier = 2;
    uint8_t componentClassificationIndex = 0;

    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               chassisName + "/Progress";
    auto progressIntf =
        std::make_shared<nsm::ProgressIntf>(bus, progressPath.c_str());

    nsm::NsmKeyMgmt keyMgmt(bus, chassisName, type, testUuid, progressIntf,
                            componentClassification, componentIdentifier,
                            componentClassificationIndex);

    EXPECT_EQ(keyMgmt.fwSlotObjects.size(), 0);

    // Create and add slot objects
    std::string chassisPath = std::string(chassisInventoryBasePath);
    std::vector<utils::Association> associations;
    auto slot1 = std::make_shared<nsm::NsmFirmwareSlot>(
        bus, chassisPath, associations, 0, nsm::SlotIntf::FirmwareType::AP,
        chassisName);

    keyMgmt.addSlotObject(slot1);
    EXPECT_EQ(keyMgmt.fwSlotObjects.size(), 1);

    auto slot2 = std::make_shared<nsm::NsmFirmwareSlot>(
        bus, chassisPath, associations, 1, nsm::SlotIntf::FirmwareType::EC,
        chassisName);

    keyMgmt.addSlotObject(slot2);
    EXPECT_EQ(keyMgmt.fwSlotObjects.size(), 2);
}

// NsmKeyMgmt getPath Test
TEST_F(NsmFirmwareUtilsTest, KeyMgmt_GetPathMethod)
{
    std::string chassisName = "GPU0";
    std::string type = "GPU";

    std::string progressPath = std::string(chassisInventoryBasePath) + "/" +
                               chassisName + "/Progress";
    auto progressIntf =
        std::make_shared<nsm::ProgressIntf>(bus, progressPath.c_str());

    nsm::NsmKeyMgmt keyMgmt(bus, chassisName, type, testUuid, progressIntf, 1,
                            2, 0);

    std::string expectedPath = std::string(chassisInventoryBasePath) + "/" +
                               chassisName;
    EXPECT_EQ(keyMgmt.getPath(chassisName), expectedPath);
}

// ============================================================================
// NsmKeyMgmt::genRequestMsg – success path
// ============================================================================

TEST_F(NsmFirmwareUtilsTest, KeyMgmt_GenRequestMsg_ReturnsValidRequest)
{
    std::string progressPath = std::string(chassisInventoryBasePath) +
                               "/GPU_genreq/Progress";
    auto progressIntf =
        std::make_shared<nsm::ProgressIntf>(bus, progressPath.c_str());
    nsm::NsmKeyMgmt keyMgmt(bus, "GPU_genreq", "GPU", testUuid, progressIntf, 1,
                            2, 0);

    auto request = keyMgmt.genRequestMsg(5, 0);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_query_req));
}

// ============================================================================
// NsmKeyMgmt::handleResponseMsg branch coverage
//
// Three branches to cover in handleResponseMsg:
//
//   [1] DecodeFail: too-short buffer → first
//   decode_nsm_code_auth_key_perm_query_resp
//       returns rc != NSM_SW_SUCCESS → condition TRUE → early return with rc.
//
//   [2] ErrorCC: valid-sized buffer but cc = NSM_ERROR in response →
//       first decode: rc = NSM_SW_SUCCESS, cc = NSM_ERROR → condition TRUE via
//       cc != NSM_SUCCESS → early return with rc (= 0).
//
//   [3] Success: valid response with cc = NSM_SUCCESS and bitmap_length = 1 →
//       both decode calls succeed → updates member fields → returns cc (= 0).
//       Also covers the for-loop zero-iterations branch (no slots added).
// ============================================================================

TEST_F(NsmFirmwareUtilsTest, KeyMgmt_HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::string progressPath = std::string(chassisInventoryBasePath) +
                               "/GPU_decodefail/Progress";
    auto progressIntf =
        std::make_shared<nsm::ProgressIntf>(bus, progressPath.c_str());
    nsm::NsmKeyMgmt keyMgmt(bus, "GPU_decodefail", "GPU", testUuid,
                            progressIntf, 1, 2, 0);

    // Too-short buffer: decode fails with NSM_SW_ERROR_LENGTH → TRUE branch
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN + 1,
                             0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERR_INVALID_DATA;
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = keyMgmt.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmFirmwareUtilsTest, KeyMgmt_HandleResponseMsg_ErrorCC_CoversTrueBranch)
{
    std::string progressPath = std::string(chassisInventoryBasePath) +
                               "/GPU_errorcc/Progress";
    auto progressIntf =
        std::make_shared<nsm::ProgressIntf>(bus, progressPath.c_str());
    nsm::NsmKeyMgmt keyMgmt(bus, "GPU_errorcc", "GPU", testUuid, progressIntf,
                            1, 2, 0);

    // encode_reason_code (called when cc != NSM_SUCCESS) fills a
    // nsm_common_non_success_resp payload.  decode_reason_code_and_cc
    // checks msg_len == sizeof(nsm_msg_hdr) +
    // sizeof(nsm_common_non_success_resp) exactly when cc != NSM_SUCCESS, so
    // pass that precise length.
    std::vector<uint8_t> buf(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    ASSERT_EQ(encode_nsm_code_auth_key_perm_query_resp(
                  0, NSM_ERROR, ERR_NULL, 0, 0, 0, nullptr, nullptr, nullptr,
                  nullptr, msg),
              NSM_SW_SUCCESS);

    // Exact non-success message length: decode_reason_code_and_cc reads cc =
    // NSM_ERROR → condition TRUE → early return with rc = NSM_SW_SUCCESS.
    size_t msg_len = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
    auto rc = keyMgmt.handleResponseMsg(msg, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmFirmwareUtilsTest, KeyMgmt_HandleResponseMsg_Success_UpdatesFields)
{
    std::string progressPath = std::string(chassisInventoryBasePath) +
                               "/GPU_success/Progress";
    auto progressIntf =
        std::make_shared<nsm::ProgressIntf>(bus, progressPath.c_str());
    nsm::NsmKeyMgmt keyMgmt(bus, "GPU_success", "GPU", testUuid, progressIntf,
                            1, 2, 0);

    // Encode a valid success response with permission_bitmap_length = 1.
    // decode_nsm_code_auth_key_perm_query_resp checks:
    //   msg_len - sizeof(nsm_msg_hdr) -
    //   sizeof(nsm_code_auth_key_perm_query_resp)
    //       == permission_bitmap_length * 4
    // So pass exactly sizeof(nsm_msg_hdr) +
    // sizeof(nsm_code_auth_key_perm_query_resp)
    //   + 4 * bitmap_length bytes.
    constexpr uint8_t bitmapLen = 1;
    uint8_t bitmap[bitmapLen] = {0x0F};
    size_t msg_len = sizeof(nsm_msg_hdr) +
                     sizeof(nsm_code_auth_key_perm_query_resp) + 4 * bitmapLen;
    std::vector<uint8_t> buf(msg_len, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    ASSERT_EQ(encode_nsm_code_auth_key_perm_query_resp(
                  0, NSM_SUCCESS, ERR_NULL, 1, 2, bitmapLen, bitmap, bitmap,
                  bitmap, bitmap, msg),
              NSM_SW_SUCCESS);

    // Both decode calls succeed, no slots → for-loop zero-iterations branch
    auto rc = keyMgmt.handleResponseMsg(msg, msg_len);
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(keyMgmt.bitmapLength, bitmapLen);
}
