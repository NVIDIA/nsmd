#ifndef PLATFORM_ENVIRONMENTAL_H
#define PLATFORM_ENVIRONMENTAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"

/** @brief NSM Type3 platform environmental commands
 */
enum nsm_platform_environmental_commands {
	NSM_GET_TEMPERATURE_READING = 0x01,
	NSM_GET_POWER = 0x02,
	NSM_GET_POWER_LIMITS = 0x03,
	NSM_SET_POWER_LIMITS = 0x04,
	NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR = 0x05,
	NSM_SET_PROGRAMMABLE_EDPP_SCALING_FACTOR = 0x06,
	NSM_GET_INVENTORY_INFORMATION = 0x07,
	NSM_GET_OEM_INFORMATION = 0x08,
	NSM_GET_CURRENT_CLOCK_FREQUENCY = 0x09,
	NSM_SET_CLOCK_LIMIT = 0x0a,
	NSM_GET_CLOCK_LIMIT = 0x0b,
	NSM_GET_VIOLATION_DURATION = 0x0c,
	NSM_GET_FAN_COUNT = 0x0d,
	NSM_READ_FAN_PROPERTIES = 0x0e,
	NSM_READ_FAN_SPEED = 0xf,
	NSM_GET_CURRENT_FAN_CURVE_POINTS = 0x10,
	NSM_GET_DEFAULT_FAN_CURVE_POINTS = 0x11,
	NSM_SET_FAN_CURVE_POINTS = 0x12,
	NSM_GET_ECC_MODE = 0x13,
	NSM_SET_ECC_MODE = 0x14,
	NSM_GET_VOLTAGE = 0x15,
	NSM_SET_THERMAL_PARAMETERS = 0x16,
	NSM_READ_THERMAL_PARAMETERS = 0x17
};

enum nsm_inventory_property_identifiers {
	BOARD_PART_NUMBER = 0,
	SERIAL_NUMBER = 1,
	MARKETING_NAME = 2,
	DEVICE_PART_NUMBER = 3,
	FRU_PART_NUMBER = 4,
	MEMORY_VENDOR = 5,
	MEMORY_PART_NUMBER = 6,
	MAXIMUM_MEMORY_CAPACITY = 7,
	BUILD_DATE = 8,
	FIRMWARE_VERSION = 9,
	DEVICE_GUID = 10,
	INFO_ROM_VERSION = 11,
	PRODUCT_LENGTH = 12,
	PRODUCT_WIDTH = 13,
	PRODUCT_HEIGHT = 14,
	MAXIMUM_PCIE_LINK_SPEED = 15,
	MAXIMUM_PCIE_LINK_WIDTH = 16,
	PCIE_VENDOR_ID = 17,
	PCIE_DEVICE_ID = 18,
	PCIE_SUBSYSTEM_VENDOR_ID = 19,
	PCIE_SUBSYSTEM_DEVICE_ID = 20,
	RATED_DEVICE_POWER_LIMIT = 21,
	MINIMUM_DEVICE_POWER_LIMIT = 22,
	MAXIMUM_DEVICE_POWER_LIMIT = 23,
	MAXIMUM_MODULE_POWER_LIMIT = 24,
	MINIMUM_MODULE_POWER_LIMIT = 25,
	RATED_MODULE_POWER_LIMIT = 26
};

enum nsm_data_type {
	NvBool8 = 0,
	NvU8 = 1,
	NvS8 = 2,
	NvU16 = 3,
	NvS16 = 4,
	NvU32 = 5,
	NvS32 = 6,
	NvU64 = 7,
	NvS64 = 8,
	NvS24_8 = 9,
	NvCString = 10,
	NvCharArray = 11,
};

struct nsm_inventory_property_record {
	uint8_t property_id;
	uint8_t data_type : 4;
	uint8_t reserved : 4;
	uint16_t data_length;
	uint8_t data[1];
} __attribute__((packed));

/** @struct nsm_get_inventory_information_req
 *
 *  Structure representing NSM get inventory information request.
 */
struct nsm_get_inventory_information_req {
	uint8_t command;
	uint8_t data_size;
	uint8_t arg1;
	uint8_t arg2;
	uint32_t transfer_handle;
} __attribute__((packed));

/** @struct nsm_get_inventory_information_resp
 *
 *  Structure representing NSM get inventory information response.
 */
struct nsm_get_inventory_information_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t data_size;
	uint32_t next_transfer_handle;
	uint8_t inventory_information[1];
} __attribute__((packed));

/** @struct nsm_get_temperature_reading_req
 *
 *  Structure representing NSM get temperature reading request.
 */
struct nsm_get_temperature_reading_req {
	uint8_t command;
	uint8_t data_size;
	uint8_t arg1;
	uint8_t arg2;
} __attribute__((packed));

/** @struct nsm_get_temperature_reading_resp
 *
 *  Structure representing NSM get temperature reading response.
 */
struct nsm_get_temperature_reading_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t data_size;
	uint32_t reading;
} __attribute__((packed));

/** @brief Encode a Get Inventory Information request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] transfer_handle - transfer Handle
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_inventory_information_req(uint8_t instance_id,
					 uint32_t transfer_handle,
					 struct nsm_msg *msg);

/** @brief Decode a Get Inventory Information request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] transfer handle - transfer handle
 *  @return nsm_completion_codes
 */
int decode_get_inventory_information_req(const struct nsm_msg *msg,
					 size_t msg_len,
					 uint32_t *transfer_handle);

/** @brief Encode a Get Inventory Information response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] next_transfer_handle - next transfer handle
 *  @param[in] inventory_information - Inventory Information
 *  @param[in] inventory_information_len - Inventory Information length
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_inventory_information_resp(uint8_t instance_id, uint8_t cc,
					  uint32_t next_transfer_handle,
					  const uint8_t *inventory_information,
					  uint32_t inventory_information_len,
					  struct nsm_msg *msg);

/** @brief Decode a Get Inventory Information response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] data_size - data size in bytes
 *  @param[out] next_transfer_handle - next transfer handle
 *  @param[out] inventory_information - Inventory Information
 *  @return nsm_completion_codes
 */
int decode_get_inventory_information_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *data_size,
					  uint32_t *next_transfer_handle,
					  uint8_t *inventory_information);

/** @brief Encode a Get temperature readings request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] sensor_id - sensor id
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_temperature_reading_req(uint8_t instance, uint8_t sensor_id,
				       struct nsm_msg *msg);

/** @brief Decode a Get temperature readings request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] sensor_id - sensor id
 *  @return nsm_completion_codes
 */
int decode_get_temperature_reading_req(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t *sensor_id);

/** @brief Encode a Get temperature readings response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] temperature_reading - temperature reading
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_temperature_reading_resp(uint8_t instance_id, uint8_t cc,
					real32_t temperature_reading,
					struct nsm_msg *msg);

/** @brief Decode a Get temperature readings response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] temperature_reading - temperature_reading
 *  @return nsm_completion_codes
 */
int decode_get_temperature_reading_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					real32_t *temperature_reading);

#ifdef __cplusplus
}
#endif
#endif