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

#ifndef PLATFORM_ENVIRONMENTAL_H
#define PLATFORM_ENVIRONMENTAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"
#include <stdbool.h>

const uint8_t MAX_VERSION_STRING_SIZE = 100;

/** @brief NSM Type3 platform environmental commands
 */
enum nsm_platform_environmental_commands {
	NSM_GET_TEMPERATURE_READING = 0x00,
	NSM_GET_POWER = 0x03,
	NSM_GET_ENERGY_COUNT = 0x06,
	NSM_GET_VOLTAGE = 0x0F,
	NSM_GET_ALTITUDE_PRESSURE = 0x6A,
	NSM_GET_INVENTORY_INFORMATION = 0x0C,
	NSM_GET_DRIVER_INFO = 0x14,
	NSM_GET_MIG_MODE = 0x4d,
	NSM_GET_CLOCK_LIMIT = 0x11,
	NSM_GET_CURRENT_CLOCK_FREQUENCY = 0x0B,
	NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME = 0x46,
	NSM_SET_MIG_MODE = 0x4e,
	NSM_GET_ECC_MODE = 0x4f,
	NSM_GET_ECC_ERROR_COUNTS = 0x7d,
	NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR = 0x09,
	NSM_SET_ECC_MODE = 0x7c,
	NSM_GET_POWER_SUPPLY_STATUS = 0x63,
	NSM_GET_GPU_PRESENCE_POWER_STATUS = 0x64,
	NSM_GET_MEMORY_CAPACITY_UTILIZATION = 0xAD,
	NSM_GET_ROW_REMAP_STATE_FLAGS = 0x7F,
	NSM_GET_ROW_REMAPPING_COUNTS = 0x7E
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
	DEFAULT_BASE_CLOCKS = 22,
	PCIERETIMER_0_EEPROM_VERSION = 144,
	PCIERETIMER_1_EEPROM_VERSION = 145,
	PCIERETIMER_2_EEPROM_VERSION = 146,
	PCIERETIMER_3_EEPROM_VERSION = 147,
	PCIERETIMER_4_EEPROM_VERSION = 148,
	PCIERETIMER_5_EEPROM_VERSION = 149,
	PCIERETIMER_6_EEPROM_VERSION = 150,
	PCIERETIMER_7_EEPROM_VERSION = 151
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

enum nsm_clock_type { GRAPHICS_CLOCK = 0, MEMORY_CLOCK = 1 };

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
	enum8 driver_state;
	uint8_t driver_version[1];
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

/** @struct nsm_get_numeric_sensor_reading_req
 *
 *  Structure representing NSM request to get reading of certain numeric
 * sensors.
 */
struct nsm_get_numeric_sensor_reading_req {
	struct nsm_common_req hdr;
	uint8_t sensor_id;
} __attribute__((packed));

/** @struct nsm_get_numeric_sensor_reading_uint32_resp
 *
 *  Structure representing NSM response to get reading of certain numeric
 * sensors.
 */
struct nsm_get_numeric_sensor_reading_uint32_resp {
	struct nsm_common_resp hdr;
	uint32_t reading;
} __attribute__((packed));

/** @struct nsm_get_temperature_reading_req
 *
 *  Structure representing NSM get temperature reading request.
 */
typedef struct nsm_get_numeric_sensor_reading_req
    nsm_get_temperature_reading_req;

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
typedef struct nsm_get_numeric_sensor_reading_uint32_resp
    nsm_get_current_power_draw_resp;

/** @struct nsm_get_current_energy_count_req
 *
 *  Structure representing NSM get current energy count request.
 */
typedef struct nsm_get_numeric_sensor_reading_req
    nsm_get_current_energy_count_req;

/** @struct nsm_get_current_energy_count_resp
 *
 *  Structure representing NSM get current energy count response.
 */
struct nsm_get_current_energy_count_resp {
	struct nsm_common_resp hdr;
	uint64_t reading;
} __attribute__((packed));

/** @struct nsm_get_voltage_req
 *
 *  Structure representing NSM get voltage request.
 */
typedef struct nsm_get_numeric_sensor_reading_req nsm_get_voltage_req;

/** @struct nsm_get_voltage_resp
 *
 *  Structure representing NSM get voltage response.
 */
typedef struct nsm_get_numeric_sensor_reading_uint32_resp nsm_get_voltage_resp;

/** @struct nsm_get_altitude_pressure_resp
 *
 *  Structure representing NSM get altitude pressure response.
 */
typedef struct nsm_get_numeric_sensor_reading_uint32_resp
    nsm_get_altitude_pressure_resp;

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

struct nsm_clock_limit {
	uint32_t requested_limit_min;
	uint32_t requested_limit_max;
	uint32_t present_limit_min;
	uint32_t present_limit_max;
} __attribute__((packed));

/** @struct nsm_get_clock_limit_req
 *
 *  Structure representing NSM get clock limit request.
 */
struct nsm_get_clock_limit_req {
	struct nsm_common_req hdr;
	uint8_t clock_id;
} __attribute__((packed));

/** @struct nsm_get_clock_limit_resp
 *
 *  Structure representing NSM get clock limit response.
 */
struct nsm_get_clock_limit_resp {
	struct nsm_common_resp hdr;
	struct nsm_clock_limit clock_limit;
} __attribute__((packed));

/** @struct nsm_get_curr_clock_freq_req
 *
 *  Structure representing NSM get curr clock freq request.
 */
struct nsm_get_curr_clock_freq_req {
	struct nsm_common_req hdr;
	uint8_t clock_id;
} __attribute__((packed));

/** @struct nsm_get_curr_clock_freq_resp
 *
 *  Structure representing NSM get current clock freq response.
 */
struct nsm_get_curr_clock_freq_resp {
	struct nsm_common_resp hdr;
	uint32_t clockFreq;
} __attribute__((packed));

/** @struct nsm_memory_capacity_utilization
 *
 *  Structure representing Memory Capacity Utilization.
 */
struct nsm_memory_capacity_utilization {
	uint32_t reserved_memory;
	uint32_t used_memory;
} __attribute__((packed));

/** @struct nsm_get_memory_capacity_util_resp
 *
 *  Structure representing Get Memory Capacity Utilization response.
 */
struct nsm_get_memory_capacity_util_resp {
	struct nsm_common_resp hdr;
	struct nsm_memory_capacity_utilization data;
} __attribute__((packed));

/** @struct nsm_get_accum_GPU_util_time_resp
 *
 *  Structure representing Get Accumulated GPU Utilization time response.
 */
struct nsm_get_accum_GPU_util_time_resp {
	struct nsm_common_resp hdr;
	uint32_t context_util_time;
	uint32_t SM_util_time;
} __attribute__((packed));

/** @struct nsm_get_power_supply_status_req
 *
 *  Structure representing NSM get power supply status information request.
 */
struct nsm_get_power_supply_status_req {
	struct nsm_common_req hdr;
} __attribute__((packed));

/** @struct nsm_get_power_supply_status_resp
 *
 *  Structure representing NSM get power supply status information response.
 */
struct nsm_get_power_supply_status_resp {
	struct nsm_common_resp hdr;
	uint8_t power_supply_status;
	uint8_t reserved1;
	uint8_t reserved2;
	uint8_t reserved3;
} __attribute__((packed));

/** @struct nsm_get_gpu_presence_and_power_status_req
 *
 *  Structure representing NSM get GPU presence and power status information
 * request.
 */
struct nsm_get_gpu_presence_and_power_status_req {
	struct nsm_common_req hdr;
} __attribute__((packed));

/** @struct nsm_get_gpu_presence_and_power_status_resp
 *
 *  Structure representing NSM get GPU presence and power status information
 * response.
 */
struct nsm_get_gpu_presence_and_power_status_resp {
	struct nsm_common_resp hdr;
	uint8_t presence;
	uint8_t power_status;
	uint8_t reserved1;
	uint8_t reserved2;
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
 *  @param[in] data_size - data size
 *  @param[in] driver_info_data - driver related info
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_driver_info_resp(uint8_t instance_id, uint8_t cc,
				uint16_t reason_code, const uint16_t data_size,
				const uint8_t *driver_info_data,
				struct nsm_msg *msg);

/** @brief Decode a Get Driver Information response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - Completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] driver_state - State of the driver
 *  @param[out] driver_version - Version of the driver
 *  @return nsm_completion_codes
 */
int decode_get_driver_info_resp(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *cc, uint16_t *reason_code,
				enum8 *driver_state, char *driver_version);

/** @struct nsm_get_MIG_reading_resp
 *
 *  Structure representing NSM get MIG Mode response.
 */
struct nsm_get_MIG_mode_resp {
	struct nsm_common_resp hdr;
	bitfield8_t flags;
} __attribute__((packed));

/** @struct nsm_set_mig_mode_req
 *
 *  Structure representing NSM set mig mode request.
 */
struct nsm_set_MIG_mode_req {
	struct nsm_common_req hdr;
	uint8_t requested_mode;
} __attribute__((packed));

/** @struct nsm_get_ECC_mode_resp
 *
 *  Structure representing NSM get ECC Mode response.
 */
struct nsm_get_ECC_mode_resp {
	struct nsm_common_resp hdr;
	bitfield8_t flags;
} __attribute__((packed));

/** @struct nsm_set_ECC_mode_req
 *
 *  Structure representing NSM set ECC mode request.
 */
struct nsm_set_ECC_mode_req {
	struct nsm_common_req hdr;
	uint8_t requested_mode;
} __attribute__((packed));

/** @struct nsm_ECC_error_counts
 *
 *  Structure representing NSM ECC error counts.
 */
struct nsm_ECC_error_counts {
	bitfield16_t flags;
	uint32_t sram_corrected;
	uint32_t sram_uncorrected_secded;
	uint32_t sram_uncorrected_parity;
	uint32_t dram_corrected;
	uint32_t dram_uncorrected;
} __attribute__((packed));

/** @struct nsm_get_ECC_error_counts_resp
 *
 *  Structure representing NSM get ECC error counts response.
 */
struct nsm_get_ECC_error_counts_resp {
	struct nsm_common_resp hdr;
	struct nsm_ECC_error_counts errorCounts;
} __attribute__((packed));

/** @struct nsm_EDPp_scaling_factors
 *
 *  Structure representing All the Programmable EDPp scaling factor.
 */
struct nsm_EDPp_scaling_factors {
	uint8_t default_scaling_factor;
	uint8_t maximum_scaling_factor;
	uint8_t minimum_scaling_factor;
} __attribute__((packed));

/** @struct nsm_get_programmable_EDPp_scaling_factor_resp
 *
 *  Structure representing Get Programmable EDPp Scaling Factor response.
 */
struct nsm_get_programmable_EDPp_scaling_factor_resp {
	struct nsm_common_resp hdr;
	struct nsm_EDPp_scaling_factors scaling_factors;
} __attribute__((packed));

/** @struct nsm_get_row_remap_state_resp
 *
 *  Structure representing NSM get row remap state response.
 */
struct nsm_get_row_remap_state_resp {
	struct nsm_common_resp hdr;
	bitfield8_t flags;
} __attribute__((packed));

/** @struct nsm_get_row_remapping_counts_resp
 *
 *  Structure representing Get Row Remapping counts response.
 */
struct nsm_get_row_remapping_counts_resp {
	struct nsm_common_resp hdr;
	uint32_t correctable_error;
	uint32_t uncorrectable_error;
} __attribute__((packed));

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

/**
 * @brief Decode a Get Inventory Information response message payload as uint32
 *
 * @param[in] inventory_information - Inventory Information
 * @param[in] data_size - data size in bytes
 * @return uint32_t Decoded uint32_t
 */
uint32_t
decode_inventory_information_as_uint32(const uint8_t *inventory_information,
				       const uint16_t data_size);

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

/** @brief Encode a Get Current Energy Counter Value request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] sensor_id - sensor id
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_current_energy_count_req(uint8_t instance, uint8_t sensor_id,
					struct nsm_msg *msg);

/** @brief Decode a Get Current Energy Counter Value request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] sensor_id - sensor id
 *  @return nsm_completion_codes
 */
int decode_get_current_energy_count_req(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *sensor_id);

/** @brief Encode a Get Current Energy Counter Value response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - reason code
 *  @param[in] energy_reading - temperature reading
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_current_energy_count_resp(uint8_t instance_id, uint8_t cc,
					 uint16_t reason_code,
					 uint64_t energy_reading,
					 struct nsm_msg *msg);

/** @brief Decode a Get Current Energy Counter Value response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - reason code
 *  @param[out] energy_reading - temperature_reading
 *  @return nsm_completion_codes
 */
int decode_get_current_energy_count_resp(const struct nsm_msg *msg,
					 size_t msg_len, uint8_t *cc,
					 uint16_t *reason_code,
					 uint64_t *energy_reading);

/** @brief Encode a Get Voltage request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] sensor_id - sensor id
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_voltage_req(uint8_t instance, uint8_t sensor_id,
			   struct nsm_msg *msg);

/** @brief Decode a Get Voltage request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] sensor_id - sensor id
 *  @return nsm_completion_codes
 */
int decode_get_voltage_req(const struct nsm_msg *msg, size_t msg_len,
			   uint8_t *sensor_id);

/** @brief Encode a Get Voltage response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - reason code
 *  @param[in] voltage - voltage reading
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_voltage_resp(uint8_t instance_id, uint8_t cc,
			    uint16_t reason_code, uint32_t voltage,
			    struct nsm_msg *msg);

/** @brief Decode a Get Voltage response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - reason code
 *  @param[out] voltage - voltage reading
 *  @return nsm_completion_codes
 */
int decode_get_voltage_resp(const struct nsm_msg *msg, size_t msg_len,
			    uint8_t *cc, uint16_t *reason_code,
			    uint32_t *voltage);

/** @brief Encode a Get Altitude Pressure request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_altitude_pressure_req(uint8_t instance_id, struct nsm_msg *msg);

/** @brief Encode a Get Altitude Pressure response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - reason code
 *  @param[in] reading - altitude pressure reading
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_altitude_pressure_resp(uint8_t instance_id, uint8_t cc,
				      uint16_t reason_code, uint32_t reading,
				      struct nsm_msg *msg);

/** @brief Decode a Get Altitude Pressure response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - reason code
 *  @param[out] reading - altitude pressure reading
 *  @return nsm_completion_codes
 */
int decode_get_altitude_pressure_resp(const struct nsm_msg *msg, size_t msg_len,
				      uint8_t *cc, uint16_t *reason_code,
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

/** @brief Encode a Get MIG mode request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_MIG_mode_req(uint8_t instance_id, struct nsm_msg *msg);

/** @brief Encode a Get MIG mode response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] flags - bits indicating MIG modes
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_MIG_mode_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, bitfield8_t *flags,
			     struct nsm_msg *msg);

/** @brief Decode a Get MIG mode response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] flags - flags indicating MIG modes
 *  @return nsm_completion_codes
 */
int decode_get_MIG_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
			     uint16_t *reason_code, bitfield8_t *flags);

/** @brief Encode a Set Mig Mode request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] requested_mode - Requested Mode
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_MIG_mode_req(uint8_t instance, uint8_t requested_mode,
			    struct nsm_msg *msg);

/** @brief Decode a Set Mig Mode request request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] requested_mode - Requested Mode
 *  @return nsm_completion_codes
 */
int decode_set_MIG_mode_req(const struct nsm_msg *msg, size_t msg_len,
			    uint8_t *requested_mode);

/** @brief Encode a Get MIG mode response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_MIG_mode_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, struct nsm_msg *msg);

/** @brief Decode a Get MIG mode response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @return nsm_completion_codes
 */
int decode_set_MIG_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
			     uint16_t *reason_code);

/** @brief Encode a Get ECC mode request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_ECC_mode_req(uint8_t instance_id, struct nsm_msg *msg);

/** @brief Encode a Get ECC mode response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] flags - bits indicating ECC modes
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_ECC_mode_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, bitfield8_t *flags,
			     struct nsm_msg *msg);

/** @brief Dncode a Get ECC mode response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] flags - flags indicating ECC modes
 *  @return nsm_completion_codes
 */
int decode_get_ECC_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
			     uint16_t *reason_code, bitfield8_t *flags);

/** @brief Encode a Set ECC Mode request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] requested_mode - Requested Mode
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_ECC_mode_req(uint8_t instance, uint8_t requested_mode,
			    struct nsm_msg *msg);

/** @brief Decode a Set ECC Mode request request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] requested_mode - Requested Mode
 *  @return nsm_completion_codes
 */
int decode_set_ECC_mode_req(const struct nsm_msg *msg, size_t msg_len,
			    uint8_t *requested_mode);

/** @brief Encode a Set ECC mode response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_ECC_mode_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, struct nsm_msg *msg);

/** @brief Decode a Set ECC mode response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @return nsm_completion_codes
 */
int decode_set_ECC_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
			     uint16_t *reason_code);

/** @brief Encode a Get ECC error counts request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_ECC_error_counts_req(uint8_t instance_id, struct nsm_msg *msg);

/** @brief Encode a Get ECC error counts response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] errorCounts - struct indicating ECC error counts
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_ECC_error_counts_resp(uint8_t instance_id, uint8_t cc,
				     uint16_t reason_code,
				     struct nsm_ECC_error_counts *errorCounts,
				     struct nsm_msg *msg);

/** @brief Dncode a Get ECC error counts response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] errorCounts - struct indicating ECC error counts
 *  @return nsm_completion_codes
 */
int decode_get_ECC_error_counts_resp(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *cc, uint16_t *data_size,
				     uint16_t *reason_code,
				     struct nsm_ECC_error_counts *errorCounts);

/** @brief Encode data of a Get Current Energy Count readings response message
 *
 *  @param[in] energy_count - energy_count
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encode_aggregate_energy_count_data(uint64_t energy_count, uint8_t *data,
				       size_t *data_len);

/** @brief Decode data of a Get Current Energy Count readings response message
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] energy_count - energy_count
 *  @return nsm_completion_codes
 */
int decode_aggregate_energy_count_data(const uint8_t *data, size_t data_len,
				       uint64_t *energy_count);

/** @brief Encode data of a Get Voltage readings response message
 *
 *  @param[in] voltage - voltage
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encode_aggregate_voltage_data(uint32_t voltage, uint8_t *data,
				  size_t *data_len);

/** @brief Decode data of a Get Voltage readings response message
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] voltage - voltage
 *  @return nsm_completion_codes
 */
int decode_aggregate_voltage_data(const uint8_t *data, size_t data_len,
				  uint32_t *voltage);

/** @brief Encode a Get Programmable EDPp Scaling Factor request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_programmable_EDPp_scaling_factor_req(uint8_t instance_id,
						    struct nsm_msg *msg);

/** @brief Encode a Get Programmable EDPp Scaling Factor response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] scaling_factors - struct representing programmable EDPp scaling
 * factors
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_programmable_EDPp_scaling_factor_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_EDPp_scaling_factors *scaling_factors, struct nsm_msg *msg);

/** @brief Dncode a Get Programmable EDPp Scaling Factor response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] scaling_factors - struct representing programmable EDPp scaling
 * factors
 *  @return nsm_completion_codes
 */
int decode_get_programmable_EDPp_scaling_factor_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code, struct nsm_EDPp_scaling_factors *scaling_factors);

/** @brief Encode a Get clock limit request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] clock_id - clock id, Graphics(0)/Memory(1) clock
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_clock_limit_req(uint8_t instance, uint8_t clock_id,
			       struct nsm_msg *msg);

/** @brief Decode a Get clock limit request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] clock_id - clock id, Graphics(0)/Memory(1) clock
 *  @return nsm_completion_codes
 */
int decode_get_clock_limit_req(const struct nsm_msg *msg, size_t msg_len,
			       uint8_t *clock_id);

/** @brief Encode a Get ECC error counts response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] clock_limit - struct indicating clock limit
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_clock_limit_resp(uint8_t instance_id, uint8_t cc,
				uint16_t reason_code,
				struct nsm_clock_limit *clock_limit,
				struct nsm_msg *msg);

/** @brief Dncode a Get ECC error counts response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] errorCounts - struct indicating clock limit
 *  @return nsm_completion_codes
 */
int decode_get_clock_limit_resp(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *cc, uint16_t *data_size,
				uint16_t *reason_code,
				struct nsm_clock_limit *clock_limit);

/** @brief Encode a Get Current Clock Frequency req message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] clock_id - clock id, Graphics(0)/Memory(1) clock
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_curr_clock_freq_req(uint8_t instance, uint8_t clock_id,
				   struct nsm_msg *msg);

/** @brief Decode a Get Current Clock Frequency request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] clock_id - clock id, Graphics(0)/Memory(1) clock
 *  @return nsm_completion_codes
 */
int decode_get_curr_clock_freq_req(const struct nsm_msg *msg, size_t msg_len,
				   uint8_t *clock_id);

/** @brief Encode a Get Current Clock Frequency response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] clockFreq - current clock Freq in MhZ
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_curr_clock_freq_resp(uint8_t instance_id, uint8_t cc,
				    uint16_t reason_code, uint32_t *clockFreq,
				    struct nsm_msg *msg);

/** @brief Decode a Get Current Clock Frequency response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] clockFreq - current clock Freq in MhZ
 *  @return nsm_completion_codes
 */
int decode_get_curr_clock_freq_resp(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *cc, uint16_t *data_size,
				    uint16_t *reason_code, uint32_t *clockFreq);

/** @brief Encode a Get Accumulated GPU Utilization request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_accum_GPU_util_time_req(uint8_t instance, struct nsm_msg *msg);

/** @brief Encode a Get Accumulated GPU Utilization response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] context_util_time - Accumulated context utilization time in
 * milliseconds
 *  @param[in] SM_util_time - Accumulated SM utilization time in milliseconds
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_accum_GPU_util_time_resp(uint8_t instance_id, uint8_t cc,
					uint16_t reason_code,
					uint32_t *context_util_time,
					uint32_t *SM_util_time,
					struct nsm_msg *msg);

/** @brief Dncode a Get Accumulated GPU Utilization response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] context_util_time - Accumulated context utilization time in
 * milliseconds
 *  @param[out] SM_util_time - Accumulated SM utilization time in milliseconds
 *  @return nsm_completion_codes
 */
int decode_get_accum_GPU_util_time_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code, uint32_t *context_util_time, uint32_t *SM_util_time);

/** @brief Encode a Get power supply status request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_power_supply_status_req(uint8_t instance_id,
				       struct nsm_msg *msg);

/** @brief Decode a Get power supply status request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_power_supply_status_req(const struct nsm_msg *msg,
				       size_t msg_len);

/** @brief Encode a Get power supply status response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] power_supply_status - GPUs power supply status
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_power_supply_status_resp(uint8_t instance_id, uint8_t cc,
					uint16_t reason_code,
					uint8_t power_supply_status,
					struct nsm_msg *msg);

/** @brief Decode a Get power supply status response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] power_supply_status - pointer to GPUs power supply status
 *  @return nsm_completion_codes
 */
int decode_get_power_supply_status_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					uint16_t *reason_code,
					uint8_t *power_supply_status);

/** @brief Encode a Get gpu presence and power status request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_gpu_presence_and_power_status_req(uint8_t instance_id,
						 struct nsm_msg *msg);

/** @brief Decode a Get gpu presence and power status request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_gpu_presence_and_power_status_req(const struct nsm_msg *msg,
						 size_t msg_len);

/** @brief Encode a Get gpu presence and power status response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] presence - GPUs presence
 *  @param[in] power_status - GPUs power status
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_gpu_presence_and_power_status_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code, uint8_t presence,
    uint8_t power_status, struct nsm_msg *msg);

/** @brief Decode a Get gpu presence and power status response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] presence - pointer to GPUs presence
 *  @param[out] power_status - pointer to GPUs power status
 *  @return nsm_completion_codes
 */
int decode_get_gpu_presence_and_power_status_resp(const struct nsm_msg *msg,
						  size_t msg_len, uint8_t *cc,
						  uint16_t *reason_code,
						  uint8_t *presence,
						  uint8_t *power_status);

/** @brief Encode a Get Row Remap State request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_row_remap_state_req(uint8_t instance_id, struct nsm_msg *msg);

/** @brief Decode a Get row remap state request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_row_remap_state_req(const struct nsm_msg *msg, size_t msg_len);

/** @brief Encode a Get row remap state response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] flags - bits indicating row remap state flags
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_row_remap_state_resp(uint8_t instance_id, uint8_t cc,
				    uint16_t reason_code, bitfield8_t *flags,
				    struct nsm_msg *msg);

/** @brief Decode a Get row remap state response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] flags - flags indicating row remap state
 *  @return nsm_completion_codes
 */
int decode_get_row_remap_state_resp(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *cc, uint16_t *data_size,
				    uint16_t *reason_code, bitfield8_t *flags);

/** @brief Encode a Get Row Remapping Counts request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_row_remapping_counts_req(uint8_t instance_id,
					struct nsm_msg *msg);

/** @brief Decode a Get row remapping counts request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_row_remapping_counts_req(const struct nsm_msg *msg,
					size_t msg_len);

/** @brief Encode a Get Row Remapping Counts response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] correctable_error - Correctable error count
 *  @param[in] uncorrectabale_error - Uncorrectable error count
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_row_remapping_counts_resp(uint8_t instance_id, uint8_t cc,
					 uint16_t reason_code,
					 uint32_t correctable_error,
					 uint32_t uncorrectable_error,
					 struct nsm_msg *msg);

/** @brief Dncode a Get Row Remapping Counts response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] correctable_error - Correctable error count
 *  @param[out] uncorrectabale_error - Uncorrectable error count
 *  @return nsm_completion_codes
 */
int decode_get_row_remapping_counts_resp(const struct nsm_msg *msg,
					 size_t msg_len, uint8_t *cc,
					 uint16_t *data_size,
					 uint16_t *reason_code,
					 uint32_t *correctable_error,
					 uint32_t *uncorrectable_error);
/** @brief Encode a Get Memory Capacity Utilization request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_memory_capacity_util_req(uint8_t instance_id,
					struct nsm_msg *msg);

/** @brief Decode a Get Memory Capacity Utilization request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_memory_capacity_util_req(const struct nsm_msg *msg,
					size_t msg_len);

/** @brief Encode a Get Memory Capacity Utilization response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct representing memory capacity Utilization
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_memory_capacity_util_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_memory_capacity_utilization *data, struct nsm_msg *msg);

/** @brief Dncode a Get Memory Capacity Utilization response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - reason code
 *  @param[out] data - struct representing memory capacity Utilization
 *  @return nsm_completion_codes
 */
int decode_get_memory_capacity_util_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code, struct nsm_memory_capacity_utilization *data);

#ifdef __cplusplus
}
#endif
#endif
