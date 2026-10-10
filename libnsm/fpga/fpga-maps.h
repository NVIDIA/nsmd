// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#ifndef LIBNSM_FPGA_MAPS_H
#define LIBNSM_FPGA_MAPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* The FPGA controller map each platform runs: one table row per platform */

#define NSM_FPGA_MAP_DIGEST_SIZE 32

/* Platforms with an FPGA controller map, in table order */
enum nsm_fpga_platform {
	NSM_FPGA_PLATFORM_LP30 = 0,
	NSM_FPGA_PLATFORM_COUNT,
};

/* The P0 controller window of the LP30 FPGA: the configuration type the
   build advertises and the register that reports its map digest */
#define NSM_FPGA_P0_CONFIGURATION_TYPE 2
#define NSM_FPGA_P0_IDENTITY_ADDRESS 0x1080
#define NSM_FPGA_P0_IDENTITY_DWORDS 8

/* Identity of one platform's map: the Platform name an entity-manager entry
   carries, the build name, the register that reports the digest and the
   digest the build must report */
struct nsm_fpga_map_identity {
	const char *platform;
	const char *name;
	uint32_t configuration_type;
	uint64_t identity_address;
	size_t identity_dwords;
	uint8_t digest[NSM_FPGA_MAP_DIGEST_SIZE];
};

extern const struct nsm_fpga_map_identity
    nsm_fpga_map_identity_table[NSM_FPGA_PLATFORM_COUNT];

/** @brief The map identity of a platform by its name, NULL when unknown. */
const struct nsm_fpga_map_identity *
nsm_fpga_map_identity_by_platform(const char *platform);

#ifdef __cplusplus
}
#endif
#endif
