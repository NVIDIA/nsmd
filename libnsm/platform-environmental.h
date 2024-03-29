#ifndef PLATFORM_ENVIRONMENTAL_H
#define PLATFORM_ENVIRONMENTAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"
#include <stdbool.h>

/** @brief NSM Type3 platform environmental commands
 */
enum nsm_platform_environmental_commands {
	NSM_GET_TEMPERATURE_READING = 0x00,
	NSM_GET_POWER = 0x01,
	NSM_GET_INVENTORY_INFORMATION = 0x11,
	NSM_GET_DRIVER_INFO = 0x14
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
	RATED_DEVICE_POWER_LIMIT = 15,
	MINIMUM_DEVICE_POWER_LIMIT = 16,
	MAXIMUM_DEVICE_POWER_LIMIT = 17,
	MINIMUM_MODULE_POWER_LIMIT = 18,
	MAXMUM_MODULE_POWER_LIMIT = 19,
	RATED_MODULE_POWER_LIMIT = 20,
	DEFAULT_BOOST_CLOCKS = 21,
	DEFAULT_BASE_CLOCKS = 22
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

typedef uint8_t enum8;

// Driver states as specified in your documentation.
typedef enum {
    DriverStateUnknown = 0,
    DriverNotLoaded = 1,
    DriverLoaded = 2
} DriverStateEnum;

// Driver info
struct nsm_driver_info {
	enum8 driver_state;
	char driver_version[1];
} __attribute__((packed));

struct nsm_inventory_property_record {
	uint8_t property_id;
	uint8_t data_type : 4;
	uint8_t reserved : 4;
	uint16_t data_length;
	uint8_t data[1];
} __attribute__((packed));

/** @struct nsm_get_driver_info_resp
 *
 *  Structure representing NSM get Driver Information response.
 */
struct nsm_get_driver_info_resp {
	struct nsm_common_resp hdr;
	struct nsm_driver_info driver_info;
} __attribute__((packed));

/** @struct nsm_get_inventory_information_req
 *
 *  Structure representing NSM get inventory information request.
 */
struct nsm_get_inventory_information_req {
	struct nsm_common_req hdr;
	uint8_t property_identifier;
} __attribute__((packed));

/** @struct nsm_get_inventory_information_resp
 *
 *  Structure representing NSM get inventory information response.
 */
struct nsm_get_inventory_information_resp {
	struct nsm_common_resp hdr;
	uint8_t inventory_information[1];
} __attribute__((packed));

/** @struct nsm_get_temperature_reading_req
 *
 *  Structure representing NSM get temperature reading request.
 */
struct nsm_get_temperature_reading_req {
	struct nsm_common_req hdr;
	uint8_t sensor_id;
} __attribute__((packed));

/** @struct nsm_get_temperature_reading_resp
 *
 *  Structure representing NSM get temperature reading response.
 */
struct nsm_get_temperature_reading_resp {
	struct nsm_common_resp hdr;
	int32_t reading;
} __attribute__((packed));

/** @struct nsm_get_current_power_draw_req
 *
 *  Structure representing NSM get current power draw request.
 */
struct nsm_get_current_power_draw_req {
	struct nsm_common_req hdr;
	uint8_t sensor_id;
	uint8_t averaging_interval;
} __attribute__((packed));

/** @struct nsm_get_current_power_draw_resp
 *
 *  Structure representing NSM get current power draw response.
 */
struct nsm_get_current_power_draw_resp {
	struct nsm_common_resp hdr;
	uint32_t reading;
} __attribute__((packed));

/** @struct nsm_aggregate_resp
 *
 *  Structure representing NSM aggregator variant response.
 */
struct nsm_aggregate_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t telemetry_count;
} __attribute__((packed));

/** @struct nsm_aggregate_resp_sample
 *
 *  Structure representing NSM telemetry sample of aggregator variant response.
 */
struct nsm_aggregate_resp_sample {
	uint8_t tag;
	uint8_t valid : 1;
	uint8_t length : 3;
	uint8_t reserved : 4;
	uint8_t data[1];
} __attribute__((packed));

/** @brief Encode a Get Driver Information request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_driver_info_req(uint8_t instance_id, struct nsm_msg *msg);

/** @brief Decode a Get Driver Information request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_driver_info_req(const struct nsm_msg *msg, size_t msg_len);

/** @brief Encode a Get Driver Information response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - Completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] driver_state - State of the driver
 *  @param[in] driver_version - Version of the driver
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_driver_info_resp(uint8_t instance_id, uint8_t cc,
				uint16_t reason_code, struct nsm_driver_info *data,
				struct nsm_msg *msg);

/** @brief Decode a Get Driver Information response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - Completion code
 *  @param[out] driver_state - State of the driver
 *  @param[out] driver_version - Version of the driver
 *  @return nsm_completion_codes
 */
int decode_get_driver_info_resp(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *cc, uint16_t *reason_code,
				struct nsm_driver_info *data);

/** @brief Encode a Get Inventory Information request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] property_identifier - unique identifier representing a device
 * property
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_inventory_information_req(uint8_t instance_id,
					 uint8_t property_identifier,
					 struct nsm_msg *msg);

/** @brief Decode a Get Inventory Information request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] property_identifier - unique identifier representing a device
 * property
 *  @return nsm_completion_codes
 */
int decode_get_inventory_information_req(const struct nsm_msg *msg,
					 size_t msg_len,
					 uint8_t *property_identifier);

/** @brief Encode a Get Inventory Information response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] inventory_information_len - inventory information size in bytes
 *  @param[in] inventory_information - Inventory Information
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_inventory_information_resp(uint8_t instance_id, uint8_t cc,
					  uint16_t reason_code,
					  const uint16_t data_size,
					  const uint8_t *inventory_information,
					  struct nsm_msg *msg);

/** @brief Decode a Get Inventory Information response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] data_size - data size in bytes
 *  @param[out] inventory_information - Inventory Information
 *  @return nsm_completion_codes
 */
int decode_get_inventory_information_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *reason_code,
					  uint16_t *data_size,
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
 *  @param[in] reason_code - NSM reason code
 *  @param[in] temperature_reading - temperature reading
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_temperature_reading_resp(uint8_t instance_id, uint8_t cc,
					uint16_t reason_code,
					double temperature_reading,
					struct nsm_msg *msg);

/** @brief Decode a Get temperature readings response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] temperature_reading - temperature_reading
 *  @return nsm_completion_codes
 */
int decode_get_temperature_reading_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					uint16_t *reason_code,
					double *temperature_reading);

/** @brief Encode a Get current power draw request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] sensor_id - sensor id
 *  @param[in] averaging_interval - averaging interval
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_current_power_draw_req(uint8_t instance_id, uint8_t sensor_id,
				      uint8_t averaging_interval,
				      struct nsm_msg *msg);

/** @brief Decode a Get current power draw request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] sensor_id - sensor id
 *  @param[out] averaging_interval - averaging interval
 *  @return nsm_completion_codes
 */
int decode_get_current_power_draw_req(const struct nsm_msg *msg, size_t msg_len,
				      uint8_t *sensor_id,
				      uint8_t *averaging_interval);

/** @brief Encode a Get current power draw response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - reason code
 *  @param[in] reading - current power draw reading
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_current_power_draw_resp(uint8_t instance_id, uint8_t cc,
				       uint16_t reason_code, uint32_t reading,
				       struct nsm_msg *msg);

/** @brief Decode a Get current power draw response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - reason code
 *  @param[out] reading - current power draw reading
 *  @return nsm_completion_codes
 */
int decode_get_current_power_draw_resp(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t *cc,
				       uint16_t *reason_code,
				       uint32_t *reading);

/** @brief Encode reading of a Get current power draw response message
 *
 *  @param[in] reading - current power draw reading
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encode_aggregate_get_current_power_draw_reading(uint32_t reading,
						    uint8_t *data,
						    size_t *data_len);

/** @brief Decode reading of a Get current power draw response message
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] reading - current power draw reading
 *  @return nsm_completion_codes
 */
int decode_aggregate_get_current_power_draw_reading(const uint8_t *data,
						    size_t data_len,
						    uint32_t *reading);

/** @brief Encode timestamp of a Get current power draw response message
 *
 *  @param[in] timestamp - timestamp of current power draw reading
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encode_aggregate_timestamp_data(uint64_t timestamp, uint8_t *data,
				    size_t *data_len);

/** @brief Decode timestamp of a Get current power draw response message
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] timestamp - timestamp of current power draw reading
 *  @return nsm_completion_codes
 */
int decode_aggregate_timestamp_data(const uint8_t *data, size_t data_len,
				    uint64_t *timestamp);

/** @brief Encode an aggregate response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] command - NSM command
 *  @param[in] cc - response message completion code
 *  @param[in] telemetry_count - telemetry count
 *  @param[out] msg - response message
 *  @return nsm_completion_codes
 */
int encode_aggregate_resp(uint8_t instance_id, uint8_t command, uint8_t cc,
			  uint16_t telemetry_count, struct nsm_msg *msg);

/** @brief Decode an aggregate response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] consumed_len - consumed number of bytes of message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] telemetry_count - telemetry count
 *  @return nsm_completion_codes
 */
int decode_aggregate_resp(const struct nsm_msg *msg, size_t msg_len,
			  size_t *consumed_len, uint8_t *cc,
			  uint16_t *telemetry_count);

/** @brief Encode a telemetry sample of an aggregate response message
 *
 *  @param[in] tag - tag of telemetry sample
 *  @param[in] data - telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] msg    - telemetry sample of an aggregate response message
 *  @param[out] sample_len - consumed number of bytes of message
 *  @return nsm_completion_codes
 */
int encode_aggregate_resp_sample(uint8_t tag, bool valid, const uint8_t *data,
				 size_t data_len,
				 struct nsm_aggregate_resp_sample *sample,
				 size_t *sample_len);

/** @brief Decode a telemetry sample of an aggregate response message
 *
 *  @param[in] sample    - telemetry sample of an aggregate response message
 *  @param[in] msg_len - Length of a message
 *  @param[out] consumed_len - consumed number of bytes of message
 *  @param[out] tag - pointer to tag of telemetry sample
 *  @param[out] valid - pointer to valid bit of telemetry sample
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int decode_aggregate_resp_sample(const struct nsm_aggregate_resp_sample *sample,
				 size_t msg_len, size_t *consumed_len,
				 uint8_t *tag, bool *valid,
				 const uint8_t **data, size_t *data_len);

/** @brief Encode data of a Get temperature readings response message
 *
 *  @param[in] temperature_reading - temperature_reading
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encode_aggregate_temperature_reading_data(double temperature_reading,
					      uint8_t *data, size_t *data_len);

/** @brief Decode data of a Get temperature readings response message
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] temperature_reading - temperature_reading
 *  @return nsm_completion_codes
 */
int decode_aggregate_temperature_reading_data(const uint8_t *data,
					      size_t data_len,
					      double *temperature_reading);
#ifdef __cplusplus
}
#endif
#endif
