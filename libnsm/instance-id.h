/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
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

#ifndef INSTANCE_ID_H
#define INSTANCE_ID_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libnsm/base.h"
#include "libnsm/requester/mctp.h"
#include <stdint.h>

typedef uint8_t instance_id_t;
struct instance_db;

#ifdef __STDC_HOSTED__
/**
 * @brief Instantiates an instance ID database object for a given database path
 *
 * @param[out] ctx - *ctx must be NULL, and will point to a instance ID
 * 		     database object on success.
 * @param[in] dbpath - the path to the instance ID database file to use
 *
 * @return int - Returns 0 on success. Returns -EINVAL if ctx is NULL or *ctx
 * 		 is not NULL. Returns -ENOMEM if memory couldn't be allocated.
 *		 Returns the errno if the database couldn't be opened.
 * */
int instance_db_init(struct instance_db **ctx, const char *dbpath);

/**
 * @brief Instantiates an instance ID database object for the default database
 * 	  path
 *
 * @param[out] ctx - *ctx will point to a instance ID database object on
 * 	       success.
 *
 * @return int - Returns 0 on success. Returns -EINVAL if ctx is NULL or *ctx
 * 		 is not NULL. Returns -ENOMEM if memory couldn't be allocated.
 * 		 Returns the errno if the database couldn't be opened.
 * */
int instance_db_init_default(struct instance_db **ctx);

/**
 * @brief Destroys an instance ID database object
 *
 * @param[in] ctx - instance ID database object
 *
 * @return int - Returns 0 on success or if *ctx is NULL. No specific errors are
 *		 specified.
 * */
int instance_db_destroy(struct instance_db *ctx);

/**
 * @brief Allocates an instance ID for a destination TID from the instance ID
 * 	  database
 *
 * @param[in] ctx - instance ID database object
 * @param[in] iid - caller owned pointer to a instance ID object. On
 * 	      success, this points to an instance ID to use for a request
 * 	      message.
 *
 * @return int - Returns 0 on success if we were able to allocate an instance
 * 		 ID. Returns -EINVAL if the iid pointer is NULL. Returns -EAGAIN
 *		 if a successive call may succeed. Returns -EPROTO if the
 *		 operation has entered an undefined state.
 */
int instance_id_alloc(struct instance_db *ctx, mctp_eid_t eid,
		      instance_id_t *iid);

/**
 * @brief Frees an instance ID previously allocated by instance_id_alloc
 *
 * @param[in] ctx - instance ID database object
 * @param[in] iid - If this instance ID was not previously allocated by
 * 	      instance_id_alloc then EINVAL is returned.
 *
 * @return int - Returns 0 on success. Returns -EINVAL if the iid supplied was
 * 		 not previously allocated by instance_id_alloc or it has
 * 		 previously been freed. Returns -EAGAIN if a successive call may
 * 		 succeed. Returns -EPROTO if the operation has entered an
 *		 undefined state.
 */
int instance_id_free(struct instance_db *ctx, mctp_eid_t eid,
		     instance_id_t iid);

#endif /* __STDC_HOSTED__*/

#ifdef __cplusplus
}
#endif

#endif /* INSTANCE_ID_H */