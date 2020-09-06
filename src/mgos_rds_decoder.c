/*
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

#include <rds_decoder.h>

#include <mgos.h>
#include <stdlib.h>

struct rds_decoder* mgos_rds_decoder_create(struct rds_decoder_config* config) {
  return rds_decoder_create(config);
}

void mgos_rds_decoder_delete(struct rds_decoder* decoder) {
  rds_decoder_delete(decoder);
}

// A required function for all MGOS libraries.
void mgos_rds_init() {
  LOG(LL_INFO, ("Initialized RDS decoder library"));
}

void mgos_rds_decoder_set_oda_callbacks(struct rds_decoder* decoder,
                                        DecodeODAFunc decode_cb,
                                        ClearODAFunc clear_cb,
                                        void* cb_data) {
  rds_decoder_set_oda_callbacks(decoder, decode_cb, clear_cb, cb_data);
}

void mgos_rds_decoder_decode(struct rds_decoder* decoder,
                             const struct rds_blocks* blocks) {
  rds_decoder_decode(decoder, blocks);
}

void mgos_rds_decoder_reset(struct rds_decoder* decoder) {
  rds_decoder_reset(decoder);
}
