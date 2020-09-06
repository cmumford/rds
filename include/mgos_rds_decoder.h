/**
 * @file
 *
 * @author Chris Mumford
 *
 * @license
 *
 * Copyright 2020 Christopher Mumford
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "rds_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Creates a new RDS decoder.
 *
 * @param config The decoder configuration parameters.
 *
 * @return opaque handle (NULL if an error occurred).
 */
struct rds_decoder* mgos_rds_decoder_create(
    const struct rds_decoder_config* config);

/**
 * Delete the RDS decoder.
 *
 * @param decoder The decoder to delete.
 */
void mgos_rds_decoder_delete(struct rds_decoder* decoder);

/**
 * Set the RDS ODA decoding callback functions.
 *
 * @param decoder   The RDS decoder.
 *
 * @param decode_cb Address of function responsible for decoding (and storing)
 *                  the RDS ODA block data.
 *
 * @param clear_cb  Address of function responsible for clearing any stored
 *                  decoded RDS ODA data. This will be called when
 *                  mgos_rds_clear_data is called.
 * @param cb_data   Data to be passed to the callbacks when invoked.
 */
void mgos_rds_decoder_set_oda_callbacks(struct rds_decoder* decoder,
                                        DecodeODAFunc decode_cb,
                                        ClearODAFunc clear_cb,
                                        void* cb_data);

/**
 * Decode the RDS data from the supplied \p blocks.
 *
 * The supplied blocks will be decoded into the rds_data supplied when
 * creating the decoder (using mgos_rds_decoder_create).
 *
 * @param decoder The RDS decoder to use for decoding.
 * @param blocks  The RDS block data to decode.
 */
void mgos_rds_decoder_decode(struct rds_decoder* decoder,
                             const struct rds_blocks* blocks);

/**
 * Reset the decoder (and any decoded data) to the default state.
 */
void mgos_rds_decoder_reset(struct rds_decoder* decoder);

#ifdef __cplusplus
}
#endif /* __cplusplus */
