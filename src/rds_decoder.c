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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freq_table.h"
#include "freq_table_group.h"

#if !defined(SET_BITS)
#define SET_BITS(value, bits) (value |= (bits))
#endif

#if !defined(CLEAR_BITS)
#define CLEAR_BITS(value, bits) (value &= ~(bits))
#endif

// clang-format off

#define GT_CODE_MASK 0b1111000000000000  // Group type mask.
#define VERSION_CODE 0b0000100000000000  // Group type version code (A or B).
#define TP_CODE      0b0000010000000000  // Traffic program identification code.
#define PTY_MASK     0b0000001111100000  // Program type code mask.

// clang-format on

#define RT_VALIDATE_LIMIT 2

struct rds_decoder {
  struct rds_data* rds;  ///< Decode blocks into this (not owned by lib.).
  struct {
    /**
     * A pointer to a function to decode ODA block data.
     *
     * This can be null if the application does not intend to decode ODA data.
     */
    DecodeODAFunc decode_cb;

    /**
     * A pointer to a function to clear stored ODA data.
     *
     * This is generally called when tuning to new channels, or at other times
     * when the RDS data is to be cleared.
     */
    ClearODAFunc clear_cb;

    void* cb_data;            ///< User data passed to both callbacks.
  } oda;                      ///< ODA decode callbacks.
  bool advanced_ps_decoding;  ///< Algorithm when decoding PS text.
};

/**
 * Are two given group types equal?
 */
static bool GroupTypesEqual(const struct rds_group_type gta,
                            const struct rds_group_type gtb) {
  return gta.code == gtb.code && gta.version == gtb.version;
}

/**
 * Is the ODA data supposed to be in the given group type?
 */
static bool IsGroupTypeUsedByODA(const struct rds_data* rds,
                                 const struct rds_group_type gt) {
  for (size_t i = 0; i < ARRAY_SIZE(rds->oda); i++) {
    if (GroupTypesEqual(rds->oda[i].gt, gt))
      return true;
  }
  return false;
}

/**
 * Is the ODA application ID valid?
 */
static bool IsValidODAAppId(const uint16_t app_id) {
  return app_id != 0x0;
}

/**
 * Read the PTY (Program Type). Only call if BLER is acceptable.
 */
static void decode_pty(struct rds_data* rds, const struct rds_block* block) {
  rds->tp_code = block->val & TP_CODE;
  rds->pty = (block->val & PTY_MASK) >> 5;

  SET_BITS(rds->valid_values, RDS_TP_CODE);
#if defined(RDS_DEV)
  if (rds->tp_code)
    rds->stats.counts[PKTCNT_TP_CODE]++;
#endif

  SET_BITS(rds->valid_values, RDS_PTY);
#if defined(RDS_DEV)
  rds->stats.counts[PKTCNT_PTY]++;
#endif
}

/**
 * Decode traffic announcement bit.
 */
static void decode_ta(struct rds_data* rds, const struct rds_block* block) {
  // clang-format off
#define TA_MASK       0b0000000000010000
  // clang-format on

  rds->ta_code = block->val & TA_MASK ? true : false;
  SET_BITS(rds->valid_values, RDS_TA_CODE);
#if defined(RDS_DEV)
  rds->stats.counts[PKTCNT_TA_CODE]++;
#endif
}

/**
 * Read the MS data. Only call if BLER is acceptable.
 */
static void decode_ms(struct rds_data* rds, const struct rds_block* block) {
  // clang-format off
#define MS_MASK       0b0000000000001000
#define DIS_MAS       0b0000000000000100
#define DIS_ADDR_MASK 0b0000000000000011
  // clang-format on

  rds->music = block->val & MS_MASK ? true : false;
  SET_BITS(rds->valid_values, RDS_MS);
#if defined(RDS_DEV)
  rds->stats.counts[PKTCNT_MS]++;
#endif
}

/**
 * The basic implementation of the Radiotext update.
 *
 * Does no additional error detection.
 */
static void update_rt_simple(struct rds_rt* rt,
                             const struct rds_blocks* blocks,
                             uint8_t count,
                             uint8_t addr,
                             uint8_t* chars) {
  uint8_t i;
  for (i = 0; i < count; i++) {
    // Choose the appropriate block. Count > 2 check is necessary for 2B groups.
    uint8_t errCount;
    uint8_t blerMax;
    if ((i < 2) && (count > 2)) {
      errCount = blocks->c.errors;
      blerMax = BLERC_MAX;
    } else {
      errCount = blocks->d.errors;
      blerMax = BLERD_MAX;
    }

    if (errCount <= blerMax) {
      // Store the data in our temporary array.
      rt->display[addr + i] = chars[i];
      if (chars[i] == 0x0d) {
        // The end of message character has been received.
        // Wipe out the rest of the text.
        for (uint8_t j = addr + i + 1; j < ARRAY_SIZE(rt->display); j++) {
          rt->display[j] = 0;
        }
        break;
      }
    }
  }

  // Any null character before this should become a space.
  for (i = 0; i < addr; i++) {
    if (!rt->display[i])
      rt->display[i] = ' ';
  }
}

static void bump_rt_validation_count(struct rds_rt* rt) {
  uint8_t i;
  for (i = 0; i < ARRAY_SIZE(rt->pvt.hi_prob_cnt); i++) {
    if (!rt->pvt.hi_prob[i]) {
      rt->pvt.hi_prob[i] = ' ';
      rt->pvt.hi_prob_cnt[i]++;
    }
  }
  for (i = 0; i < ARRAY_SIZE(rt->pvt.hi_prob_cnt); i++)
    rt->pvt.hi_prob_cnt[i]++;

  // Wipe out the cached text.
  memset(rt->pvt.hi_prob_cnt, 0, sizeof(rt->pvt.hi_prob_cnt));
  memset(rt->pvt.hi_prob, 0, sizeof(rt->pvt.hi_prob));
  memset(rt->pvt.lo_prob, 0, sizeof(rt->pvt.lo_prob));
}

/**
 * The advanced implementation of the Radiotext update.
 *
 * This implementation of the Radiotext update attempts to further error
 * correct the data by making sure that the data has been identical for
 * multiple receptions of each byte.
 */
static void update_rt_advance(struct rds_rt* rt,
                              const struct rds_blocks* blocks,
                              uint8_t count,
                              uint8_t addr,
                              uint8_t* byte) {
  uint8_t i;
  bool text_changing = false;  // Indicates if the Radiotext is changing.

  for (i = 0; i < count; i++) {
    uint8_t errCount;
    uint8_t blerMax;
    // Choose the appropriate block. Count > 2 check is necessary for 2B groups.
    if ((i < 2) && (count > 2)) {
      errCount = blocks->c.errors;
      blerMax = BLERC_MAX;
    } else {
      errCount = blocks->d.errors;
      blerMax = BLERD_MAX;
    }
    if (errCount <= blerMax) {
      if (!byte[i])
        byte[i] = ' ';  // translate nulls to spaces.

      // The new byte matches the high probability byte.
      if (rt->pvt.hi_prob[addr + i] == byte[i]) {
        if (rt->pvt.hi_prob_cnt[addr + i] < RT_VALIDATE_LIMIT) {
          rt->pvt.hi_prob_cnt[addr + i]++;
        } else {
          // we have received this byte enough to max out our counter and push
          // it into the low probability array as well.
          rt->pvt.hi_prob_cnt[addr + i] = RT_VALIDATE_LIMIT;
          rt->pvt.lo_prob[addr + i] = byte[i];
        }
      } else if (rt->pvt.lo_prob[addr + i] == byte[i]) {
        // The new byte is a match with the low probability byte. Swap them,
        // reset the counter and flag the text as in transition. Note that the
        // counter for this character goes higher than the validation limit
        // because it will get knocked down later.
        if (rt->pvt.hi_prob_cnt[addr + i] >= RT_VALIDATE_LIMIT) {
          text_changing = true;
          rt->pvt.hi_prob_cnt[addr + i] = RT_VALIDATE_LIMIT + 1;
        } else {
          rt->pvt.hi_prob_cnt[addr + i] = RT_VALIDATE_LIMIT;
        }
        rt->pvt.lo_prob[addr + i] = rt->pvt.hi_prob[addr + i];
        rt->pvt.hi_prob[addr + i] = byte[i];
      } else if (!rt->pvt.hi_prob_cnt[addr + i]) {
        // The new byte is replacing an empty byte in the high proability array.
        rt->pvt.hi_prob[addr + i] = byte[i];
        rt->pvt.hi_prob_cnt[addr + i] = 1;
      } else {
        // The new byte doesn't match anything, put it in the low probability
        // array.
        rt->pvt.lo_prob[addr + i] = byte[i];
      }
    }
  }

  if (!text_changing)
    return;

  // When the text is changing, decrement the count for all characters to
  // prevent displaying part of a message that is in transition.
  for (i = 0; i < ARRAY_SIZE(rt->pvt.hi_prob_cnt); i++) {
    if (rt->pvt.hi_prob_cnt[i] > 1)
      rt->pvt.hi_prob_cnt[i]--;
  }
}

/**
 * Update the Program Service text in our buffers from the shadow registers.
 *
 * This implementation of the Program Service update attempts to display only
 * complete messages for stations who rotate text through the PS field in
 * violation of the RBDS standard as well as providing enhanced error detection.
 *
 * This function is from the Silicon Labs sample application.
 */
static void update_ps_advanced(struct rds_data* rds,
                               uint8_t char_idx,
                               uint8_t byte) {
  const uint8_t PS_VALIDATE_LIMIT = 2;

  if (char_idx >= ARRAY_SIZE(rds->ps.display))
    return;

  uint8_t i;
  bool in_transition = false;  ///< Indicates if the PS text is in transition.
  bool complete = true;  ///< Indicates the PS text is ready to be displayed.

  if (rds->ps.pvt.hi_prob[char_idx] == byte) {
    // The new byte matches the high probability byte.
    if (rds->ps.pvt.hi_prob_cnt[char_idx] < PS_VALIDATE_LIMIT) {
      rds->ps.pvt.hi_prob_cnt[char_idx]++;
    } else {
      // we have received this byte enough to max out our counter and push it
      // into the low probability array as well.
      rds->ps.pvt.hi_prob_cnt[char_idx] = PS_VALIDATE_LIMIT;
      rds->ps.pvt.lo_prob[char_idx] = byte;
    }
  } else if (rds->ps.pvt.lo_prob[char_idx] == byte) {
    // The new byte is a match with the low probability byte. Swap them, reset
    // the counter and flag the text as in transition. Note that the counter for
    // this character goes higher than the validation limit because it will get
    // knocked down later.
    if (rds->ps.pvt.hi_prob_cnt[char_idx] >= PS_VALIDATE_LIMIT) {
      in_transition = true;
      rds->ps.pvt.hi_prob_cnt[char_idx] = PS_VALIDATE_LIMIT + 1;
    } else {
      rds->ps.pvt.hi_prob_cnt[char_idx] = PS_VALIDATE_LIMIT;
    }
    rds->ps.pvt.lo_prob[char_idx] = rds->ps.pvt.hi_prob[char_idx];
    rds->ps.pvt.hi_prob[char_idx] = byte;
  } else if (!rds->ps.pvt.hi_prob_cnt[char_idx]) {
    // The new byte is replacing an empty byte in the high probability array.
    rds->ps.pvt.hi_prob[char_idx] = byte;
    rds->ps.pvt.hi_prob_cnt[char_idx] = 1;
  } else {
    // The new byte doesn't match anything, put it in the low probability array.
    rds->ps.pvt.lo_prob[char_idx] = byte;
  }

  if (in_transition) {
    // When the text is changing, decrement the count for all characters to
    // prevent displaying part of a message that is in transition.
    for (i = 0; i < ARRAY_SIZE(rds->ps.pvt.hi_prob_cnt); i++) {
      if (rds->ps.pvt.hi_prob_cnt[i] > 1) {
        rds->ps.pvt.hi_prob_cnt[i]--;
      }
    }
  }

  // The PS text is incomplete if any character in the high probability array
  // has been seen fewer times than the validation limit.
  for (i = 0; i < ARRAY_SIZE(rds->ps.pvt.hi_prob_cnt); i++) {
    if (rds->ps.pvt.hi_prob_cnt[i] < PS_VALIDATE_LIMIT) {
      complete = false;
      break;
    }
  }

  // If the PS text in the high probability array is complete copy it to the
  // display array.
  if (complete) {
    SET_BITS(rds->valid_values, RDS_PS);
    memcpy(rds->ps.display, rds->ps.pvt.hi_prob, sizeof(rds->ps.pvt.hi_prob));
  }
}

/**
 * The basic implementation of the Program Service update.
 *
 * This should be as-per the RBDS specification.
 */
static void update_ps_simple(struct rds_data* rds,
                             uint8_t char_idx,
                             uint8_t current_ps_byte) {
  if (char_idx >= ARRAY_SIZE(rds->ps.display))
    return;
  rds->ps.display[char_idx] = current_ps_byte;
  SET_BITS(rds->valid_values, RDS_PS);
}

/**
 * Decode alternative frequencies from group 0A IAW RDBS specification
 * section 3.2.1.6.2.
 *
 * This *should* be correctly handling method A and B encoding. However our
 * frequency table is hard-coded to 25 frequency entries.
 */
static void decode_alt_freq(struct rds_data* rds,
                            const struct rds_blocks* blocks) {
  // Current implementation is intolerant of errors.
  if (blocks->c.errors != BLER_NONE)
    return;

  SET_BITS(rds->valid_values, RDS_AF);
#if defined(RDS_DEV)
  rds->stats.counts[PKTCNT_AF]++;
#endif

  decode_freq_group_block(&rds->af, blocks->c.val);
}

/**
 * Decode group type 0 data.
 *
 *  0A: Basic tuning and switching information (pt 1).
 *  0B: Basic tuning and switching information (pt 2).
 */
static void decode_group_type_0(const struct rds_decoder* decoder,
                                const struct rds_group_type gt,
                                const struct rds_blocks* blocks) {
  if (gt.version == 'A')
    decode_alt_freq(decoder->rds, blocks);

  if (blocks->d.errors > BLERD_MAX)
    return;

  decode_ta(decoder->rds, &blocks->b);
  decode_ms(decoder->rds, &blocks->b);

  uint16_t pair_idx = (blocks->b.val & 0x03) * 2;
  if (decoder->advanced_ps_decoding) {
    update_ps_advanced(decoder->rds, pair_idx + 0, blocks->d.val >> 8);
    update_ps_advanced(decoder->rds, pair_idx + 1, blocks->d.val & 0xFF);
  } else {
    update_ps_simple(decoder->rds, pair_idx + 0, blocks->d.val >> 8);
    update_ps_simple(decoder->rds, pair_idx + 1, blocks->d.val & 0xFF);
  }
#if defined(RDS_DEV)
  decoder->rds->stats.counts[PKTCNT_PS]++;
#endif
}

/**
 * Decode slow labeling codes and program item number.
 *
 * See RBDS spec. 3.1.5.2.
 */
static void decode_slow_labelling_codes(struct rds_data* rds,
                                        const struct rds_blocks* blocks) {
  // clang-format off

const uint16_t C_SLC_LA   = 0b1000000000000000;  // Linkage actuator mask.
const uint16_t C_SLC_VC   = 0b0111000000000000;  // Variant code mask.
const uint16_t C_SLC_DATA = 0b0000111111111111;  // Value mask.

const uint16_t C_SLC_PAGING_MASK  = 0b0000111100000000;
const uint16_t C_SLC_COUNTRY_MASK = 0b0000000011111111;
  // clang-format on

  // 3.2.1.8.3: With LA=1, a service carrying codes TP=1 or TP=0/TA=1 must
  // not be linked to another service carrying the codes TP=0/TA=0.

  if (blocks->c.errors > BLERC_MAX)
    return;

  SET_BITS(rds->valid_values, RDS_SLC);
#if defined(RDS_DEV)
  rds->stats.counts[PKTCNT_SLC]++;
#endif

  rds->slc.la = (blocks->c.val & C_SLC_LA) ? true : false;
  rds->slc.variant_code =
      (enum rds_variant_code)((blocks->c.val & C_SLC_VC) >> 12);
  switch (rds->slc.variant_code) {
    case SLC_VARIANT_PAGING:
      rds->slc.data.paging.paging = (blocks->c.val & C_SLC_PAGING_MASK) >> 8;
      rds->slc.data.paging.country_code = blocks->c.val & C_SLC_COUNTRY_MASK;
      break;
    case SLC_VARIANT_TMC_ID:
      rds->slc.data.tmc_id = blocks->c.val & C_SLC_DATA;
      break;
    case SLC_VARIANT_PAGING_ID:
      rds->slc.data.paging_id = blocks->c.val & C_SLC_DATA;
      break;
    case SLC_VARIANT_LANG:
      rds->slc.data.language_codes = blocks->c.val & C_SLC_DATA;
      break;
    case SLC_VARIANT_NOT_ASSIGNED1:
    case SLC_VARIANT_NOT_ASSIGNED5:
      rds->slc.data.tmc_id = 0;
      break;
    case SLC_VARIANT_BROADCAST:
      rds->slc.data.broadcasters = blocks->c.val & C_SLC_DATA;
      break;
    case SLC_VARIANT_EWS:
      rds->slc.data.ews_channel_id = blocks->c.val & C_SLC_DATA;
      break;
  }
}

/**
 * See RBDS spec. 3.1.5.2.
 */
static void decode_program_item_number_code(uint16_t raw_value,
                                            struct rds_pic* pic) {
  // clang-format off

  // Program item number code masks.
#define PI_DAY    0b1111100000000000
#define PI_HOUR   0b0000011111000000
#define PI_MINUTE 0b0000000000111111

  // clang-format on

  pic->day = 0;
  pic->hour = 0;
  pic->minute = 0;

  pic->day = raw_value >> 11;
  if (pic->day) {
    // Spec says that if top five bits are zero others are undefined.
    pic->hour = (raw_value & PI_HOUR) >> 6;
    pic->minute = raw_value & PI_MINUTE;
  }
}

/**
 * Decode group type 1 data.
 *
 *  1A: Program Item Number and slow labeling codes.
 *  1B: Program Item Number.
 */
static void decode_group_type_1(const struct rds_decoder* decoder,
                                const struct rds_group_type gt,
                                const struct rds_blocks* blocks) {
  if (gt.version == 'A')
    decode_slow_labelling_codes(decoder->rds, blocks);

  if (blocks->d.errors <= BLERD_MAX) {
    decode_program_item_number_code(blocks->d.val, &decoder->rds->pic);
    SET_BITS(decoder->rds->valid_values, RDS_PIC);
#if defined(RDS_DEV)
    decoder->rds->stats.counts[PKTCNT_PIC]++;
#endif
  }
}

/**
 * Decode group type 2 data.
 *
 * Group type 2 is Radiotext.
 */
static void decode_group_type_2(const struct rds_decoder* decoder,
                                const struct rds_group_type gt,
                                const struct rds_blocks* blocks) {
  uint8_t rtchars[4];

  enum rds_rt_text decode_rt = (blocks->b.val & 0x0010) >> 4 ? RT_A : RT_B;
  struct rds_rt* rt =
      decode_rt == RT_A ? &decoder->rds->rt.a : &decoder->rds->rt.b;

  if (gt.version == 'A') {
    if (blocks->c.errors > BLERC_MAX || blocks->d.errors > BLERD_MAX)
      return;
    rtchars[0] = (uint8_t)(blocks->c.val >> 8);
    rtchars[1] = (uint8_t)(blocks->c.val & 0xFF);
    rtchars[2] = (uint8_t)(blocks->d.val >> 8);
    rtchars[3] = (uint8_t)(blocks->d.val & 0xFF);

    const uint8_t addr = (blocks->b.val & 0xf) * 4;

    update_rt_simple(rt, blocks, 4, addr, rtchars);
    if (decoder->rds->rt.decode_rt != decode_rt)
      bump_rt_validation_count(rt);
    update_rt_advance(rt, blocks, 4, addr, rtchars);
  } else {
    if (blocks->d.errors > BLERD_MAX)
      return;
    rtchars[0] = (uint8_t)(blocks->d.val >> 8);
    rtchars[1] = (uint8_t)(blocks->d.val & 0xFF);
    rtchars[2] = 0;
    rtchars[3] = 0;

    const uint8_t addr = (blocks->b.val & 0xf) * 2;

    // The last 32 bytes are unused in this format.
    rt->display[32] = 0x0d;
    rt->pvt.hi_prob[32] = 0x0d;
    rt->pvt.lo_prob[32] = 0x0d;
    rt->pvt.hi_prob_cnt[32] = RT_VALIDATE_LIMIT;

    update_rt_simple(rt, blocks, 2, addr, rtchars);
    if (decoder->rds->rt.decode_rt != decode_rt)
      bump_rt_validation_count(rt);
    update_rt_advance(rt, blocks, 2, addr, rtchars);
  }
  decoder->rds->rt.decode_rt = decode_rt;
  SET_BITS(decoder->rds->valid_values, RDS_RT);
#if defined(RDS_DEV)
  decoder->rds->stats.counts[PKTCNT_RT]++;
#endif
}

/**
 * Decode open data.
 */
static void decode_oda(const struct rds_decoder* decoder,
                       const struct rds_group_type gt,
                       const struct rds_blocks* blocks) {
  uint8_t idx = 0;
  for (; idx < decoder->rds->oda_cnt; idx++) {
    if (GroupTypesEqual(decoder->rds->oda[idx].gt, gt))
      break;
  }
  if (idx == decoder->rds->oda_cnt)
    return;

  decoder->rds->oda[idx].pkt_count++;
  if (decoder->oda.decode_cb) {
    decoder->oda.decode_cb(decoder->rds->oda[idx].id, decoder->rds, blocks, gt,
                           decoder->oda.cb_data);
  }
}

/**
 * Decode group type 3 data.
 *
 * See section 3.1.5.4 in RBDS specification.
 *
 *  3A: Application Identification for Open Data.
 *  3B: Open data application.
 */
static void decode_group_type_3(const struct rds_decoder* decoder,
                                const struct rds_group_type gt,
                                const struct rds_blocks* blocks) {
  if (gt.version == 'A') {
    // Entire block is app id (AID) so we want no errors.
    if (blocks->d.errors == BLER_NONE) {
      const uint16_t app_id = blocks->d.val;
      if (!IsValidODAAppId(app_id))
        return;
      // See if this ODA is already in our iist.
      uint8_t idx = 0;
      while (idx < decoder->rds->oda_cnt) {
        if (decoder->rds->oda[idx].id == app_id) {
          // Reset it - just in case it changes.
          decoder->rds->oda[idx].gt.code = (blocks->b.val & 0b11110) >> 1;
          decoder->rds->oda[idx].gt.version = blocks->b.val & 0x1 ? 'B' : 'A';
          break;
        }
        idx++;
      }
      if (idx == decoder->rds->oda_cnt && idx < ARRAY_SIZE(decoder->rds->oda)) {
        decoder->rds->oda[idx].id = app_id;
        decoder->rds->oda[idx].gt.code = (blocks->b.val & 0b11110) >> 1;
        decoder->rds->oda[idx].gt.version = blocks->b.val & 0x1 ? 'B' : 'A';
        decoder->rds->oda_cnt++;

        // TODO - Finish Group 3A ODA bits.
#if 0
        if (app_id == AID_RT_PLUS) {
          // Don't currently suport templates. These are in the C block.
  const uint16_t rfu_mask  = 0x1110000000000000; // future use.
  const uint16_t cb_flag   = 0x0001000000000000;
  const uint16_t scb_mask  = 0x0000111100000000; // Server control bits.
  const uint16_t tmpt_mask = 0x0000000011111111;
        }
#endif
      }
    }
  } else {
    decode_oda(decoder, gt, blocks);
  }
}

/**
 * Decode the Clock IAW RBDS standard, sect. 3.1.5.6.
 */
static void update_clock(const struct rds_decoder* decoder,
                         const struct rds_blocks* blocks) {
  if (blocks->b.errors > BLERB_MAX)
    return;
  if (blocks->c.errors > BLERC_MAX)
    return;
  if (blocks->d.errors > BLERD_MAX)
    return;
  if ((blocks->b.errors + blocks->c.errors + blocks->d.errors) > BLERB_MAX)
    return;

  const uint16_t b = blocks->b.val;
  const uint16_t c = blocks->c.val;
  const uint16_t d = blocks->d.val;

  // clang-format off

#define B_JDATE           0b0000000000000011 // bottom two bits of B.
#define C_JDATE           0b1111111111111110 // Top 15 bits of C.
#define D_HOUR            0b1111000000000000 // Top nibble of D.
#define D_MINUTE          0b0000111111000000 // Middle 6 bits of D.
#define D_UTC_OFFSET      0b0000000000011111 // Bottom 5 bits of D.
#define D_UTC_OFFSET_SIGN 0b0000000000100000 // Sign of offset.

  // clang-format on

  SET_BITS(decoder->rds->valid_values, RDS_CLOCK);
#if defined(RDS_DEV)
  decoder->rds->stats.counts[PKTCNT_CLOCK]++;
#endif

  // Julian date is a 17-bit value.
  decoder->rds->clock.day_high = (b & B_JDATE) >> 1;
  decoder->rds->clock.day_low = ((b & 0x1) << 15) | ((c & C_JDATE) >> 1);
  decoder->rds->clock.hour = ((c & 0x1) << 4) | ((d & D_HOUR) >> 12);
  decoder->rds->clock.minute = ((d & D_MINUTE) >> 6);
  decoder->rds->clock.utc_offset = d & D_UTC_OFFSET;
  if (d & D_UTC_OFFSET_SIGN)
    decoder->rds->clock.utc_offset = -decoder->rds->clock.utc_offset;
}

/**
 * Decode group type 4 data.
 *
 *  4A: Clock-time and date.
 *  4B: Open data application.
 */
static void decode_group_type_4(const struct rds_decoder* decoder,
                                const struct rds_group_type gt,
                                const struct rds_blocks* blocks) {
  if (gt.version == 'A')
    update_clock(decoder, blocks);
  else
    decode_oda(decoder, gt, blocks);
}

static void decode_tdc_block(struct rds_data* rds,
                             const struct rds_block* block) {
  const uint8_t channel = rds->tdc.curr_channel;
  if (channel >= NUM_TDC)
    return;

  SET_BITS(rds->valid_values, RDS_TDC);
#if defined(RDS_DEV)
  rds->stats.counts[PKTCNT_TDC]++;
#endif
  memmove(&rds->tdc.data[channel][0], &rds->tdc.data[channel][2], TDC_LEN - 2);

  rds->tdc.data[channel][TDC_LEN - 2] = block->val >> 8;
  rds->tdc.data[channel][TDC_LEN - 1] = block->val & 0xFF;
}

/**
 * Decode group type 5 data.
 *
 *  5A: Transparent data channels or ODA.
 *  5B: Transparent data channels or ODA.
 */
static void decode_group_type_5(const struct rds_decoder* decoder,
                                const struct rds_group_type gt,
                                const struct rds_blocks* blocks) {
  if (IsGroupTypeUsedByODA(decoder->rds, gt)) {
    decode_oda(decoder, gt, blocks);
    return;
  }
  // Used for TDC.
  if (gt.version == 'A') {
    decoder->rds->tdc.curr_channel = blocks->b.val & 0x11111;
    decode_tdc_block(decoder->rds, &blocks->c);
    decode_tdc_block(decoder->rds, &blocks->d);
  } else {
    decode_tdc_block(decoder->rds, &blocks->d);
  }
}

static void decode_in_house_data(const struct rds_decoder* decoder) {
#if defined(RDS_DEV)
  decoder->rds->stats.counts[PKTCNT_IH]++;
#else
  // According to RBDS spec.: "Consumer receivers should ignore the in-house
  // information coded in these groups".
  UNUSED(decoder);
#endif
}

/**
 * Decode group type 6 data.
 *
 *  6A: In-house applications or ODA (pt 1).
 *  6B: In-house applications or ODA (pt 2).
 */
static void decode_group_type_6(const struct rds_decoder* decoder,
                                const struct rds_group_type gt,
                                const struct rds_blocks* blocks) {
  if (IsGroupTypeUsedByODA(decoder->rds, gt)) {
    decode_oda(decoder, gt, blocks);
    return;
  }
  decode_in_house_data(decoder);
}

static void decode_radio_paging(const struct rds_decoder* decoder) {
  // No stations seem to broadcast this data. Will implement if/when needed.
#if defined(RDS_DEV)
  decoder->rds->stats.counts[PKTCNT_PAGING]++;
#else
  UNUSED(decoder);
#endif
}

/**
 * Decode group type 7 data.
 *
 *  7A: Radio Paging.
 *  7B: Open data application.
 */
static void decode_group_type_7(const struct rds_decoder* decoder,
                                const struct rds_group_type gt,
                                const struct rds_blocks* blocks) {
  if (gt.version == 'A') {
    if (IsGroupTypeUsedByODA(decoder->rds, gt))
      decode_oda(decoder, gt, blocks);
    else
      decode_radio_paging(decoder);
  } else {
    decode_oda(decoder, gt, blocks);
  }
}

/**
 * Decode TMC data from 8A IAW 3.1.5.12.
 */
static void decode_tmc(const struct rds_decoder* decoder) {
#if defined(RDS_DEV)
  decoder->rds->stats.counts[PKTCNT_TMC]++;
#else
  // TODO: Implement TMC. This requires obtaining a copy of EN ISO 14819-1:2013.
  UNUSED(decoder);
#endif
}

/**
 * Decode group type 8 data.
 *
 *  8A: Traffic Message Channel.
 *  8B: Open data.
 */
static void decode_group_type_8(const struct rds_decoder* decoder,
                                const struct rds_group_type gt,
                                const struct rds_blocks* blocks) {
  if (IsGroupTypeUsedByODA(decoder->rds, gt)) {
    decode_oda(decoder, gt, blocks);
    return;
  }

  if (gt.version == 'A')
    decode_tmc(decoder);
}

static void decode_ews(const struct rds_decoder* decoder,
                       const struct rds_blocks* blocks) {
#if defined(RDS_DEV)
  decoder->rds->stats.counts[PKTCNT_EWS]++;
#endif

  // Format and application of the bits allocated for EWS messages may be
  // assigned unilaterally by each country.
  SET_BITS(decoder->rds->valid_values, RDS_EWS);
  decoder->rds->ews.b = blocks->b;
  decoder->rds->ews.b.val = decoder->rds->ews.b.val & 0b11111;
  decoder->rds->ews.c = blocks->c;
  decoder->rds->ews.d = blocks->d;
}

/**
 * Decode group type 9 data.
 *
 *  9A: Allocation of EWS message bits.
 *  9B: Open data.
 */
static void decode_group_type_9(const struct rds_decoder* decoder,
                                const struct rds_group_type gt,
                                const struct rds_blocks* blocks) {
  if (IsGroupTypeUsedByODA(decoder->rds, gt)) {
    decode_oda(decoder, gt, blocks);
    return;
  }

  if (gt.version == 'A')
    decode_ews(decoder, blocks);
}

static void update_ptyn(struct rds_data* rds, uint8_t char_idx, uint8_t ch) {
  if (char_idx >= ARRAY_SIZE(rds->ptyn.display))
    return;
  rds->ptyn.display[char_idx] = ch;
}

static void decode_ptyn(const struct rds_decoder* decoder,
                        const struct rds_blocks* blocks) {
  // clang-format off

  #define B_PTYN_AB_FLAG      0b10000
  #define B_PTYN_RESERVED     0b01110
  #define B_PTYN_SEGMENT_ADDR 0b00001

  // clang-format on

  SET_BITS(decoder->rds->valid_values, RDS_PTYN);
#if defined(RDS_DEV)
  decoder->rds->stats.counts[PKTCNT_PTYN]++;
#endif
  const bool ab_val = blocks->b.val & B_PTYN_AB_FLAG;
  if (decoder->rds->ptyn.last_ab != ab_val) {
    memset(decoder->rds->ptyn.display, 0, sizeof(decoder->rds->ptyn.display));
    decoder->rds->ptyn.last_ab = ab_val;
  }

  const uint8_t base = (blocks->b.val & B_PTYN_SEGMENT_ADDR) ? 4 : 0;
  if (blocks->c.errors <= BLERC_MAX) {
    update_ptyn(decoder->rds, base + 0, blocks->c.val >> 8);
    update_ptyn(decoder->rds, base + 1, blocks->c.val & 0xFF);
  }
  if (blocks->d.errors <= BLERD_MAX) {
    update_ptyn(decoder->rds, base + 2, blocks->d.val >> 8);
    update_ptyn(decoder->rds, base + 3, blocks->d.val & 0xFF);
  }
}

/**
 * Decode group type 10 data.
 *
 *  10A: Program Type Name (PTYN).
 *  10B: Open data.
 */
static void decode_group_type_10(const struct rds_decoder* decoder,
                                 const struct rds_group_type gt,
                                 const struct rds_blocks* blocks) {
  if (gt.version == 'A')
    decode_ptyn(decoder, blocks);
  else
    decode_oda(decoder, gt, blocks);
}

/**
 * Decode group type 11 data.
 *
 *  11A: Open data.
 *  11B: Open data.
 */
static void decode_group_type_11(const struct rds_decoder* decoder,
                                 const struct rds_group_type gt,
                                 const struct rds_blocks* blocks) {
  decode_oda(decoder, gt, blocks);
}

/**
 * Decode group type 12 data.
 *
 *  12A: Open data.
 *  12B: Open data.
 */
static void decode_group_type_12(const struct rds_decoder* decoder,
                                 const struct rds_group_type gt,
                                 const struct rds_blocks* blocks) {
  decode_oda(decoder, gt, blocks);
}

/**
 * Decode group type 13 data.
 *
 *  13A: Open data.
 *  13B: Open data.
 */
static void decode_group_type_13(const struct rds_decoder* decoder,
                                 const struct rds_group_type gt,
                                 const struct rds_blocks* blocks) {
  decode_oda(decoder, gt, blocks);
}

/**
 * Decode block EON data from block 14A.
 */
static void decode_eon_block_a(struct rds_data* rds,
                               const struct rds_blocks* blocks) {
  // clang-format off

#define EON_VC_PS1        0
#define EON_VC_PS2        1
#define EON_VC_PS3        2
#define EON_VC_PS4        3
#define EON_VC_AF         4
#define EON_VC_FREQ1      5
#define EON_VC_FREQ2      6
#define EON_VC_FREQ3      7
#define EON_VC_FREQ4      8
#define EON_VC_FREQ5      9
#define EON_VC_UNALLOC1  10
#define EON_VC_UNALLOC2  11
#define EON_VC_LINKAGE   12
#define EON_VC_PTY_TA    13
#define EON_VC_PIN       14
#define EON_VC_RESERVED  15

  // clang-format on

  switch (blocks->b.val & 0xf) {  // Low four bits is variant code.
    case EON_VC_PS1:
      rds->eon.on.ps[0] = blocks->c.val >> 8;
      rds->eon.on.ps[1] = blocks->c.val & 0xFF;
      break;
    case EON_VC_PS2:
      rds->eon.on.ps[2] = blocks->c.val >> 8;
      rds->eon.on.ps[3] = blocks->c.val & 0xFF;
      break;
    case EON_VC_PS3:
      rds->eon.on.ps[4] = blocks->c.val >> 8;
      rds->eon.on.ps[5] = blocks->c.val & 0xFF;
      break;
    case EON_VC_PS4:
      rds->eon.on.ps[6] = blocks->c.val >> 8;
      rds->eon.on.ps[7] = blocks->c.val & 0xFF;
      break;
    case EON_VC_AF: {  // See RBDS 3.2.1.6.6.
      const uint8_t first_byte = blocks->c.val >> 8;
      if (is_freq_code_count(first_byte)) {
        rds->eon.on.af.pvt.band = AF_BAND_UHF;
        decode_freq_table_start_block(&rds->eon.on.af,
                                      freq_code_to_count(first_byte),
                                      blocks->c.val & 0xFF);
      } else {
        decode_freq_table_nth_block(&rds->eon.on.af, first_byte,
                                    blocks->c.val & 0xFF);
      }
    } break;
    case EON_VC_FREQ1:
      break;
    case EON_VC_FREQ2:
      break;
    case EON_VC_FREQ3:
      break;
    case EON_VC_FREQ4:
      break;
    case EON_VC_FREQ5:
      break;
    case EON_VC_UNALLOC1:
      break;
    case EON_VC_UNALLOC2:
      break;
    case EON_VC_LINKAGE:
      break;
    case EON_VC_PTY_TA:
      rds->eon.on.pty = blocks->c.val > 11;       // top five bits.
      rds->eon.on.ta_code = blocks->c.val & 0x1;  // bottom bit.
      break;
    case EON_VC_PIN:
      break;
    case EON_VC_RESERVED:
      break;
  }
}

/**
 * Decode group type 14 data IAW RBDS spec. 3.1.5.19.
 *
 *  14A: Enhanced Other Networks (OEN) information.
 *  14B: Enhanced Other Networks (OEN) information.
 */
static void decode_group_type_14(const struct rds_decoder* decoder,
                                 const struct rds_group_type gt,
                                 const struct rds_blocks* blocks) {
#if defined(RDS_DEV)
  decoder->rds->stats.counts[PKTCNT_EON]++;
#endif

  SET_BITS(decoder->rds->valid_values, RDS_EON);

  // See sect. 3.2.1.8.
  if (gt.version == 'A') {
    decode_eon_block_a(decoder->rds, blocks);
  } else {
    if (blocks->d.errors <= BLERD_MAX)
      decoder->rds->eon.on.pi_code = blocks->d.val;
    decoder->rds->eon.on.tp_code = (blocks->b.val & 0b1000) ? true : false;
    decoder->rds->eon.on.ta_code = (blocks->b.val & 0b0100) ? true : false;
  }
}

static void decode_fast_basic_tuning(const struct rds_decoder* decoder,
                                     const struct rds_blocks* blocks) {
#if defined(RDS_DEV)
  decoder->rds->stats.counts[PKTCNT_FBT]++;
#endif
  if (blocks->d.errors > BLERD_MAX)
    return;
}

/**
 * Decode group type 15 data.
 *
 *  15A: Fast basic tuning and switching information.
 *  15B: Fast basic tuning and switching information.
 */
static void decode_group_type_15(const struct rds_decoder* decoder,
                                 const struct rds_group_type gt,
                                 const struct rds_blocks* blocks) {
  if (gt.version == 'A') {
    // According to 1998 RBDS specifiction fast basic tuning in 15A is being
    // phased out, and as of 2008 this should be available for reuse.
  } else {
    decode_fast_basic_tuning(decoder, blocks);
  }
  decode_ta(decoder->rds, &blocks->b);
}

/******************************************/
/*vvvvvvvvvv EXPORTED FUNCTIONS *vvvvvvvvv*/
/******************************************/

void rds_decoder_decode(struct rds_decoder* decoder,
                        const struct rds_blocks* blocks) {
#if defined(RDS_DEV)
  decoder->rds->stats.data_cnt++;
#endif

  if (blocks->a.errors <= BLERA_MAX) {
    decoder->rds->pi_code = blocks->a.val;
    SET_BITS(decoder->rds->valid_values, RDS_PI_CODE);
#if defined(RDS_DEV)
    decoder->rds->stats.counts[PKTCNT_PI_CODE]++;
#endif
  }

  if (blocks->b.errors > BLERB_MAX) {
#if defined(RDS_DEV)
    decoder->rds->stats.blckb_errors++;
#endif
    return;
  }

  const struct rds_group_type gt = {
      .code = (blocks->b.val & GT_CODE_MASK) >> 12,
      .version = blocks->b.val & VERSION_CODE ? 'B' : 'A',
  };

  if (gt.version == 'B' && blocks->c.errors <= BLERC_MAX &&
      blocks->c.errors < blocks->b.errors) {
    decoder->rds->pi_code = blocks->c.val;
    SET_BITS(decoder->rds->valid_values, RDS_PI_CODE);
#if defined(RDS_DEV)
    decoder->rds->stats.counts[PKTCNT_PI_CODE]++;
#endif
  }

#if defined(RDS_DEV)
  if (gt.version == 'A')
    decoder->rds->stats.groups[gt.code].a++;
  else
    decoder->rds->stats.groups[gt.code].b++;
#endif

  decode_pty(decoder->rds, &blocks->b);

  switch (gt.code) {
    case 0:
      decode_group_type_0(decoder, gt, blocks);
      break;
    case 1:
      decode_group_type_1(decoder, gt, blocks);
      break;
    case 2:
      decode_group_type_2(decoder, gt, blocks);
      break;
    case 3:
      decode_group_type_3(decoder, gt, blocks);
      break;
    case 4:
      decode_group_type_4(decoder, gt, blocks);
      break;
    case 5:
      decode_group_type_5(decoder, gt, blocks);
      break;
    case 6:
      decode_group_type_6(decoder, gt, blocks);
      break;
    case 7:
      decode_group_type_7(decoder, gt, blocks);
      break;
    case 8:
      decode_group_type_8(decoder, gt, blocks);
      break;
    case 9:
      decode_group_type_9(decoder, gt, blocks);
      break;
    case 10:
      decode_group_type_10(decoder, gt, blocks);
      break;
    case 11:
      decode_group_type_11(decoder, gt, blocks);
      break;
    case 12:
      decode_group_type_12(decoder, gt, blocks);
      break;
    case 13:
      decode_group_type_13(decoder, gt, blocks);
      break;
    case 14:
      decode_group_type_14(decoder, gt, blocks);
      break;
    case 15:
      decode_group_type_15(decoder, gt, blocks);
      break;
  }
}

void rds_decoder_reset(struct rds_decoder* decoder) {
  memset(decoder->rds, 0, sizeof(struct rds_data));
  decoder->rds->af.pvt.current_table_idx = -1;
  if (decoder->oda.clear_cb)
    decoder->oda.clear_cb(decoder->oda.cb_data);
}

struct rds_decoder* rds_decoder_create(
    const struct rds_decoder_config* config) {
  struct rds_decoder* decoder =
      (struct rds_decoder*)calloc(1, sizeof(struct rds_decoder));
  decoder->rds = config->rds_data;
  decoder->advanced_ps_decoding = config->advanced_ps_decoding;
  return decoder;
}

void rds_decoder_delete(struct rds_decoder* decoder) {
  if (!decoder)
    return;
  free(decoder);
}

void rds_decoder_set_oda_callbacks(struct rds_decoder* decoder,
                                   DecodeODAFunc decode_cb,
                                   ClearODAFunc clear_cb,
                                   void* cb_data) {
  decoder->oda.decode_cb = decode_cb;
  decoder->oda.clear_cb = clear_cb;
  decoder->oda.cb_data = cb_data;
}
