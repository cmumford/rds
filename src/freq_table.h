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

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <rds_decoder.h>

/**
 * Is frequency a == b?
 */
bool freq_eq(const struct rds_freq* a, const struct rds_freq* b);

/**
 * Does the frequency code represent a count of frequencies to follow?
 */
bool is_freq_code_count(const uint8_t freq_code);

/**
 * Convert a frequency code to a count.
 *
 * Caller must have already determined code represents a count using
 * \p is_freq_code_count.
 */
uint8_t freq_code_to_count(const uint8_t freq_code);

/**
 * Convert the alternative frequency table index to the actual
 * frequency in kHz.
 *
 * See RBDS spec 3.2.1.6.1.
 *
 * @param freq_code The frequency code as specified by Table 10 in RBDS
 *                  specification 3.2.1.6.1. Values must be in the range
 *                  [0, 204].
 *
 * @param band      The current band used when interpreting \p freq_code.
 *
 * @return An integer representation of the frequency.
 */
uint16_t af_code_to_freq(uint8_t freq_code, enum rds_band band);

/**
 * Decode the very first block in the frequency table.
 */
void decode_freq_table_start_block(struct rds_af_decode_table* table,
                                   uint8_t num_freqs_in_table,
                                   uint8_t second_byte);

/**
 * Decode freqnency blocks 2..n of the frequency table.
 */
void decode_freq_table_nth_block(struct rds_af_decode_table* table,
                                 uint8_t first_byte,
                                 uint8_t second_byte);
