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

#include "freq_table.h"

#include <stddef.h>
#include <string.h>

#include <rds_decoder.h>

// See table 12 in RBDS spec section 3.2.1.6.1.
static const uint8_t AF_MIN_FREQ_CODE = 1;
static const uint8_t AF_MAX_FREQ_CODE = 204;
static const uint8_t AF_FILLER_CODE = 205;
static const uint8_t AF_MIN_COUNT_CODE = 225;
static const uint8_t AF_MAX_COUNT_CODE = 249;
static const uint8_t AF_LF_MF_FOLLOWS = 250;

/**
 * Does the frequency code represent a frequency?
 */
static bool freq_code_is_freq(const uint8_t freq_code) {
  return (AF_MIN_FREQ_CODE <= freq_code && freq_code <= AF_MAX_FREQ_CODE);
}

/**
 * Is frequency a < b?
 */
static bool freq_lt(const struct rds_freq* a, const struct rds_freq* b) {
  if (a->band == b->band)
    return a->freq < b->freq;
  if (a->band == AF_BAND_LF_MF && b->band == AF_BAND_UHF)
    return true;
  return false;  // Otherwise b is LF/MF and A is UHF.
}

static int8_t find_af_freq_idx(const struct rds_af_table* table,
                               const struct rds_freq* freq) {
  for (uint8_t i = 0; i < table->count; i++) {
    if (freq_eq(&table->entry[i], freq))
      return i;
  }
  return -1;
}

static bool freq_in_af_table(const struct rds_af_table* table,
                             const struct rds_freq* freq) {
  return find_af_freq_idx(table, freq) != -1;
}

static void dec_af_expected_count(struct rds_af_decode_table* table) {
  if (table->pvt.expected_cnt == 0)
    return;
  table->pvt.expected_cnt--;
}

/**
 * Insert an alternative frequency to end of the AF table.
 *
 * @param table  The table to which to add the frequency.
 * @param freq   The frequency.
 */
static bool insert_alt_freq(struct rds_af_table* table,
                            const struct rds_freq* freq) {
  if (table->count >= ARRAY_SIZE(table->entry)) {
    // Array is full, do nothing (for now).
    return false;
  }

  if (freq_in_af_table(table, freq))
    return false;

  table->entry[table->count++] = *freq;
  return true;
}

/**
 * Add an alternative frequency to end of the AF table.
 *
 * @param table  The table to which to add the frequency.
 * @param freq   The frequency.
 */
static bool add_alt_freq(struct rds_af_decode_table* table,
                         const struct rds_freq* freq) {
  dec_af_expected_count(table);
  return insert_alt_freq(&table->table, freq);
}

/**
 * Handle the frequency code.
 *
 * @return true if code handled (i.e. not an actual frequency), or false
 *         if not handled (freq_code represents a frequency and should be
 *         handled.
 */
static bool handle_freq_code(struct rds_af_decode_table* table,
                             uint8_t freq_code) {
  if (freq_code == AF_FILLER_CODE) {
    dec_af_expected_count(table);
    return true;
  }
  if (freq_code == AF_LF_MF_FOLLOWS) {
    table->pvt.band = AF_BAND_LF_MF;
    dec_af_expected_count(table);
    return true;
  }
  // All others outside of codes which map to frequencies are ignored.
  const bool handled = !freq_code_is_freq(freq_code);
  if (handled)
    dec_af_expected_count(table);
  return handled;
}

/******************************************/
/*vvvvvvvvvv EXPORTED FUNCTIONS *vvvvvvvvv*/
/******************************************/

void decode_freq_table_start_block(struct rds_af_decode_table* table,
                                   uint8_t num_freqs_in_table,
                                   uint8_t second_byte) {
  table->pvt.expected_cnt = num_freqs_in_table;
  table->pvt.band = AF_BAND_UHF;  // Always start with UHF, then LF/MF.

  if (table->pvt.prev_enc_method != AF_EM_UNKNOWN)
    table->enc_method = table->pvt.prev_enc_method;

  if (handle_freq_code(table, second_byte))
    return;

  const struct rds_freq freq = {
      .band = table->pvt.band,
      .attrib = AF_ATTRIB_SAME_PROG,
      .freq = af_code_to_freq(second_byte, table->pvt.band)};

  add_alt_freq(table, &freq);
}

void decode_freq_table_nth_block(struct rds_af_decode_table* table,
                                 uint8_t first_byte,
                                 uint8_t second_byte) {
  if (table->pvt.expected_cnt == 0) {
    // Got more frequency codes than we were expecting. Probably missed
    // a block to start a new table, so do nothing.
    return;
  }

  const bool handled_first = handle_freq_code(table, first_byte);
  struct rds_freq first_freq = {
      .band = table->pvt.band,
      .attrib = AF_ATTRIB_SAME_PROG,  // May be overridden.
      .freq = af_code_to_freq(first_byte, table->pvt.band)};
  const bool handled_second = handle_freq_code(table, second_byte);
  struct rds_freq second_freq = {
      .band = table->pvt.band,
      .attrib = AF_ATTRIB_SAME_PROG,  // May be overridden.
      .freq = af_code_to_freq(second_byte, table->pvt.band)};

  if (table->enc_method == AF_EM_UNKNOWN) {
    if (handled_first && handled_second) {
      // Still don't know, figure out next entry.
      return;
    }
    if (handled_first || handled_second) {
      // If only one handled, but not the second then this must be method A.
      table->enc_method = AF_EM_A;
    } else if (freq_eq(&first_freq, &table->table.tuned_freq) ||
               freq_eq(&second_freq, &table->table.tuned_freq)) {
      // If either frequencies match first freq then must be method B.
      table->enc_method = AF_EM_B;
    } else {
      // Neither match tuned freq, so must be method A.
      table->enc_method = AF_EM_A;

      if (table->table.tuned_freq.freq != 0) {
        // Move the frequency, which we saved because we didn't know if this
        // was method A or B, into the table.
        add_alt_freq(table, &table->table.tuned_freq);
        memset(&table->table.tuned_freq, 0, sizeof(table->table.tuned_freq));
      }
    }
  }

  table->pvt.prev_enc_method = table->enc_method;

  if (table->enc_method == AF_EM_A) {
    if (!handled_first)
      add_alt_freq(table, &first_freq);
    if (!handled_second)
      add_alt_freq(table, &second_freq);
    return;
  }

  // Method B:
  if (handled_first || handled_second) {
    // Should be a programming error. Method B's should always have
    // two real frequencies.
    return;
  }
  if (freq_eq(&table->table.tuned_freq, &first_freq)) {
    if (freq_lt(&first_freq, &second_freq))
      second_freq.attrib = AF_ATTRIB_REG_VARIANT;
    add_alt_freq(table, &second_freq);
  } else if (freq_eq(&table->table.tuned_freq, &second_freq)) {
    if (freq_lt(&first_freq, &second_freq))
      first_freq.attrib = AF_ATTRIB_REG_VARIANT;
    add_alt_freq(table, &first_freq);
  } else {
#if defined(RDS_DEV)
    // strcpy(rds->debug, "AF B, but no freq matches tuned.");
#endif
  }
}

bool freq_eq(const struct rds_freq* a, const struct rds_freq* b) {
  return a->band == b->band && a->freq == b->freq;
}

bool is_freq_code_count(const uint8_t freq_code) {
  return (AF_MIN_COUNT_CODE <= freq_code && freq_code <= AF_MAX_COUNT_CODE);
}

uint8_t freq_code_to_count(const uint8_t freq_code) {
  return 1 + freq_code - AF_MIN_COUNT_CODE;
}

uint16_t af_code_to_freq(uint8_t freq_code, enum rds_band band) {
  if (band == AF_BAND_UHF)  // If a UHF band.
    return 876 + freq_code - 1;

  if (freq_code < 16)  // If LF
    return 153 + 9 * (freq_code - 1);

  return 531 + 9 * (freq_code - 16);  // MF
}
