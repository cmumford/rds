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

#include "freq_table_group.h"

#include <stddef.h>

#include "freq_table.h"
#include "rds_misc.h"

/**
 * Find a table if the table's tuned frequency matches `tuned_freq`.
 */
static int8_t find_af_table_idx(struct rds_af_table_group* group,
                                const struct rds_freq* tuned_freq) {
  for (uint8_t i = 0; i < group->count; i++) {
    if (freq_eq(&group->table[i].table.tuned_freq, tuned_freq))
      return i;
  }
  return -1;
}

/**
 * Decode the very first block in an alt freq table.
 */
static void decode_af_start_block(struct rds_af_table_group* group,
                                  uint8_t num_freqs_in_table,
                                  uint8_t second_byte) {
  enum rds_af_encoding encoding_method = AF_EM_UNKNOWN;

  if (group->count == 1 && group->table[0].enc_method == AF_EM_A) {
    // There is only every one "A" table, so reuse this one.
    group->pvt.current_table_idx = 0;
    encoding_method = AF_EM_A;
  } else {
    group->pvt.current_table_idx = -1;
  }

  if (num_freqs_in_table == 1) {
    // Only Method A encoding has a single-entry table, and there is only
    // one table with this method, so we know it.
    group->pvt.current_table_idx = 0;
    encoding_method = AF_EM_A;
  }

  struct rds_af_decode_table* table = NULL;

  if (group->pvt.current_table_idx == -1) {
    // TODO: Make AF Method A more robust. Technically the second byte could
    // be a special code. Handle this correctly.
    const struct rds_freq freq = {
        .band = AF_BAND_UHF,
        .attrib = AF_ATTRIB_SAME_PROG,
        .freq = af_code_to_freq(second_byte, AF_BAND_UHF)};
    group->pvt.current_table_idx = find_af_table_idx(group, &freq);
    if (group->pvt.current_table_idx == -1) {
      if (group->count == ARRAY_SIZE(group->table)) {
        // All tables are in use - can't allocate a new one.
        return;
      }
      // Allocate a new table.
      group->pvt.current_table_idx = group->count++;
      table = &group->table[group->pvt.current_table_idx];
      table->enc_method = encoding_method;

      if (table->enc_method == AF_EM_UNKNOWN) {
        // Don't know if method A or B yet, so save in tuned_freq. Will
        // move to entries if encoding method turns out to be method A.
        table->table.tuned_freq = freq;
      }
    }
  } else {
    table = &group->table[group->pvt.current_table_idx];
  }

  decode_freq_table_start_block(&group->table[group->pvt.current_table_idx],
                                num_freqs_in_table, second_byte);
}

/**
 * Decode freqnency blocks 2..n of the AF table.
 */
static void decode_af_nth_block(struct rds_af_table_group* group,
                                uint8_t first_byte,
                                uint8_t second_byte) {
  if (group->pvt.current_table_idx < 0) {
    return;
  }

  decode_freq_table_nth_block(&group->table[group->pvt.current_table_idx],
                              first_byte, second_byte);
}

/******************************************/
/*vvvvvvvvvv EXPORTED FUNCTIONS *vvvvvvvvv*/
/******************************************/

void decode_freq_group_block(struct rds_af_table_group* group,
                             const uint16_t block) {
  const uint8_t first_byte = (uint8_t)(block >> 8);
  const uint8_t second_byte = block & 0xFF;

  if (is_freq_code_count(first_byte))
    decode_af_start_block(group, freq_code_to_count(first_byte), second_byte);
  else
    decode_af_nth_block(group, first_byte, second_byte);
}
