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

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if !defined(ARRAY_SIZE)
#define ARRAY_SIZE(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))
#endif

/**
 * Define this constant to expose data used during library development.
 */
#define RDS_DEV

// clang-format off

/** \addtogroup BLER Block errors.
 * @{
 */

#define BLER_NONE   0  ///< No block errors.
#define BLER_1_2    1  ///< 1-2 block errors.
#define BLER_3_5    2  ///< 3-5 block errors.
#define BLER_6_PLUS 3  ///< 6+ block errors.

/** @}*/

// clang-format on

/** \addtogroup BLER_MAX
 * @{
 * Maximum allowed block error rate.
 *
 * Set the maximum allowable errors in a block considered acceptable.
 * It's critical that block B be correct since it determines what's
 * contained in the latter blocks. For this reason, a stricter tolerance
 * is placed on block B.
 */
#define BLERA_MAX BLER_3_5  ///< Maximum allowed errors for block A.
#define BLERB_MAX BLER_1_2  ///< Maximum allowed errors for block B.
#define BLERC_MAX BLER_3_5  ///< Maximum allowed errors for block C.
#define BLERD_MAX BLER_3_5  ///< Maximum allowed errors for block D.

/** @}*/

/**
 * Representa an RDS data block (A..D).
 */
struct rds_block {
  uint16_t val;    ///< The block value.
  uint8_t errors;  ///< # of block errors (See BLER_*).
};

/**
 * All four RDS data blocks.
 */
struct rds_blocks {
  struct rds_block a;  ///< RDS block A.
  struct rds_block b;  ///< RDS block B.
  struct rds_block c;  ///< RDS block C.
  struct rds_block d;  ///< RDS block D.
};

/**
 * RDS group type code and version.
 */
struct rds_group_type {
  uint8_t code;  ///< A The group type code 0..15.
  char version;  ///< The group type version ('A' or 'B').
};

#define NUM_TDC 32  ///< The number of transparent data codes.
#define TDC_LEN 32  ///< The # of transparent data bytes we keep (per code).

// clang-format off

#if defined(RDS_DEV)

/**
 * Enumerations used when counting the number of receipts of various
 * RDS data packets.
 */
enum rds_packet_counts {
  PKTCNT_AF      = 0,
  PKTCNT_CLOCK   = 1,
  PKTCNT_EON     = 2,
  PKTCNT_EWS     = 3,
  PKTCNT_FBT     = 4,
  PKTCNT_IH      = 5,
  PKTCNT_PAGING  = 6,
  PKTCNT_PIC     = 7,
  PKTCNT_PI_CODE = 8,
  PKTCNT_PS      = 9,
  PKTCNT_PTY     = 10,
  PKTCNT_PTYN    = 11,
  PKTCNT_RT      = 12,
  PKTCNT_SLC     = 13,
  PKTCNT_TDC     = 14,
  PKTCNT_TMC     = 15,
  PKTCNT_TA_CODE = 16,
  PKTCNT_TP_CODE = 17,
  PKTCNT_MS      = 18,

  PKTCNT_NUM     = 20  // Number of values in this enum.
};

#endif // defined(RDS_DEV)

/**
 * Constants to represent trhe values in rds_data that are valid.
 */
enum rds_values {
  RDS_AF      = 0x00001,
  RDS_CLOCK   = 0x00002,
  RDS_EWS     = 0x00004,
  RDS_FBT     = 0x00008,
  RDS_MC      = 0x00010,
  RDS_PIC     = 0x00020,
  RDS_PI_CODE = 0x00040,
  RDS_PS      = 0x00080,
  RDS_PTY     = 0x00100,
  RDS_PTYN    = 0x00200,
  RDS_RT      = 0x00400,
  RDS_SLC     = 0x00800,
  RDS_TDC     = 0x01000,
  RDS_TA_CODE = 0x02000,
  RDS_TP_CODE = 0x04000,
  RDS_MS      = 0x08000,
  RDS_EON     = 0x10000,
};

/**
 * Slow labeling code variant code.
 */
enum rds_variant_code {
  SLC_VARIANT_PAGING        = 0, ///< Paging + extended country code.
  SLC_VARIANT_TMC_ID        = 1, ///< TMC Identification.
  SLC_VARIANT_PAGING_ID     = 2, ///< Paging identification.
  SLC_VARIANT_LANG          = 3, ///< Language codes.
  SLC_VARIANT_NOT_ASSIGNED1 = 4, ///< Not assigned.
  SLC_VARIANT_NOT_ASSIGNED5 = 5, ///< Not assigned.
  SLC_VARIANT_BROADCAST     = 6, ///< For use by broadcasters.
  SLC_VARIANT_EWS           = 7  ///< Identification of EWS channel.
};

// clang-format on

/**
 * The frequency band for alternative frequency decoding.
 */
enum rds_band {
  AF_BAND_UHF = 0,    ///< The UHF band.
  AF_BAND_LF_MF = 1,  ///< The LF/LF bands.
};

enum rds_af_attrib {
  AF_ATTRIB_SAME_PROG = 0,   ///< Alt freq is the same program as tuned freq.
  AF_ATTRIB_REG_VARIANT = 1  ///< Alt freq is regional variant as tuned freq.
};

/**
 * Alternative frequency encoding method.
 */
typedef enum rds_af_encoding {
  AF_EM_UNKNOWN = 0,  ///< Encoding method is not yet unknown.
  AF_EM_A = 1,        ///< Encoding method A.
  AF_EM_B = 2,        ///< Encoding method B.
} rds_af_encoding;

/**
 * Represents a frequency in a frequency band.
 */
struct rds_freq {
  enum rds_band band;  ///< The frequency band.

  /**
   * How does this frequency relate to the tuned frequency.
   *
   * Only valid when this instance is used to specify an alternative frequency.
   */
  enum rds_af_attrib attrib;

  /// If band is UHF then frequency is in multiples of 10 MHz.
  ///   e.g. 885 = 88.5 MHz or 1079 = 107.9 MHz.
  /// Otherwise frequency is in KHz.
  ///   e.g. 531 = 531 KHz.
  uint16_t freq;
};

/**
 * A table of frequencies.
 */
struct rds_af_table {
  struct rds_freq tuned_freq;  ///< The tuned frequency (method B only).
  uint8_t count;               ///< Number of entries in table below.
  struct rds_freq entry[25];   ///< Array of alternative frequencies.
};

/**
 * Used to decode alternative frequencies into an rds_af_table.
 */
struct rds_af_decode_table {
  struct rds_af_table table;   ///< The table where new freqs will be inserted.
  rds_af_encoding enc_method;  ///< Encoding method.
  struct {
    enum rds_band band;               ///< Band(s) for following freqs.
    rds_af_encoding prev_enc_method;  ///< Previous table encoding method.
    uint8_t expected_cnt;             ///< The number of freqs to follow.
  } pvt;                              ///< Private data used during decoding.
};

/**
 * A collection of AF decode tables.
 */
struct rds_af_table_group {
  struct {
    int8_t current_table_idx;            ///< Index of current decode table.
  } pvt;                                 ///< Private data for decodinging.
  uint8_t count;                         ///< Number of tables in use.
  struct rds_af_decode_table table[20];  ///< Decoded alternative frequencies.
};                                       ///< Alternate frequencies.

/**
 * Program item number code.
 */
struct rds_pic {
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
};

/**
 * RDS (Radio Data System) data.
 *
 * This contains all data extracted from the RDS data stream. Some data, like
 * PTY, PIC, only represents the last values received. Other values, PS, RT,
 * etc. represent accumulated values.
 */
struct rds_data {
  uint16_t pi_code;    ///< Program Identification Code.
  struct rds_pic pic;  ///< Program item number code.
  uint8_t pty;         ///< The Program Type (PTY) code.
  bool tp_code;        ///< Traffic Program Code (RDS standard 3.2.1.3).
  bool ta_code;        ///< Traffic announcement code. See 3.2.1.3.
  bool music;          ///< true if music, false if speech. See 3.2.1.4.

  // Note: NONE of the strings in this structure are null terminated!
  struct {
    uint8_t display[8];  ///< PS text to display.
    struct {
      uint8_t hi_prob[8];      ///< Temporary PS text (high probability).
      uint8_t lo_prob[8];      ///< Temporary PS text (low probability).
      uint8_t hi_prob_cnt[8];  ///< Hit count of high probability PS text.
    } pvt;                     ///< PS decoder private data.
  } ps;                        ///< The Program Service data.

  struct {
    uint8_t display[64];  ///< Radiotext text to display.
    struct {
      uint8_t hi_prob[64];      ///< Temporary Radiotext (high probability).
      uint8_t lo_prob[64];      ///< Temporary Radiotext (low probability).
      uint8_t hi_prob_cnt[64];  ///< Hit count of high probability Radiotext.
      bool flag;                ///< Radiotext A/B flag.
      bool flag_valid;          ///< Radiotext A/B flag is valid.
      bool saved_flag;          ///< Saved Radiotext A/B flag.
      bool saved_flag_valid;    ///< Saved Radiotext A/B flag is valid.
    } pvt;                      ///< RT decoder private data.
  } rt;                         ///< The Radiotext data.

  struct {
    bool day_high;      ///< Modified Julian Day high bit.
    uint16_t day_low;   ///< Modified Julian Day low 16 bits.
    uint8_t hour;       ///< Hour (UTC).
    uint8_t minute;     ///< Minute (UTC).
    int8_t utc_offset;  ///< Local Time Offset from UTC in multiples of 1/2 hrs.
  } clock;              ///< The clock time (current broadcast time).

  struct {
    bool la;  ///< Linkage Actuator. RDSM spec. (3.2.1.8.3).
    enum rds_variant_code variant_code;  ///< How to interpret `data` below.
    union {
      struct {
        uint8_t paging;        ///< Paging.
        uint8_t country_code;  ///< Extended country codes.
      } paging;
      uint16_t tmc_id;          ///< TMC identification.
      uint16_t paging_id;       ///< Paging identification.
      uint16_t language_codes;  ///< Language codes.
      uint16_t broadcasters;    ///< For use by broadcasters.
      uint16_t ews_channel_id;  ///< Identification of EWS channel ID.
    } data;                     ///< payload of the SLC packet.
  } slc;                        ///< Slow labeling codes.

  struct {
    uint8_t display[8];  ///< The PTYN to display.
    bool last_ab;        ///< Last displayed AB flag value.
  } ptyn;                ///< Program Type Name.

  struct rds_af_table_group af;  ///< Alternative frequencies.

  struct {
    struct {
      uint8_t ps[8];                  ///< Program Service data.
      uint8_t pty;                    ///< The Program Type (PTY) code.
      bool tp_code;                   ///< Traffic Program Code.
      bool ta_code;                   ///< Traffic announcement code.
      struct rds_af_decode_table af;  ///< Alternative frequencies.
      uint16_t pi_code;               ///< Program identification code.
      struct rds_pic pic;             ///< Program item number code.
    } on;                             ///< Other network data.
    struct {
      struct rds_freq tn_tuned_freq;  ///< This network tuned frequency.
      struct rds_freq on_freq;        ///< Other network frequency.
    } maps[5];                        ///< Mapping table of this=>other freqs.
  } eon;                              ///< Enhanced Other Network data.

  uint8_t oda_cnt;  ///< the number of currently active ODA's.
  struct {
    uint16_t id;               ///< Application Identificion (AID).
    struct rds_group_type gt;  ///< Group type where data is received.
    uint16_t pkt_count;        ///< Number of packets of this AID received.
  } oda[10];                   ///< The ODA group types active.

  struct {
    uint8_t data[NUM_TDC][TDC_LEN];  ///< TDC data.
    uint8_t curr_channel;            ///< Current TDC channel from 5A.
  } tdc;

  struct {
    struct rds_block b;  ///< EWS block B data. (non EWS bits set to zero.)
    struct rds_block c;  ///< EWS block C data.
    struct rds_block d;  ///< EWS block D data.
  } ews;                 ///< Emergency Warning System data.

#if defined(RDS_DEV)
  struct {
    int counts[PKTCNT_NUM];  ///< The number of received packets for some types.
    struct {
      uint16_t a;           ///< Number of A versions for the group.
      uint16_t b;           ///< Number of B versions for the group.
    } groups[16];           ///< Group counts.
    uint16_t data_cnt;      ///< # of times RDS data was received.
    uint16_t blckb_errors;  ///< # of times block B exceeded BLERB_MAX.
  } stats;
#endif  // defined(RDS_DEV)

  /// Bitmask (See rds_values) of valid values in this field.
  uint32_t valid_values;
};

/**
 * The RDS decoder configuration parameters.
 */
struct rds_decoder_config {
  bool advanced_ps_decoding;  ///< Algorithm when decoding PS text.
  /**
   * The structure into which all decoded RDS data will be written.
   *
   * The library host must ensure that this pointer remains valid as long
   * as the decoder is in use. The host is responsible for ensuring this
   * structure is deleted once the decoder has been destroyed.
   */
  struct rds_data* rds_data;
};

/**
 * A function to decode received ODA block data.
 */
typedef void (*DecodeODAFunc)(uint16_t app_id,
                              const struct rds_data*,
                              const struct rds_blocks*,
                              struct rds_group_type gt,
                              void* cb_data);

/**
 * A function to clear any saved ODA data.
 */
typedef void (*ClearODAFunc)(void* cb_data);

/**
 * Creates a new RDS decoder.
 *
 * @param config The decoder configuration parameters.
 *
 * @return opaque handle (NULL if an error occurred).
 */
struct rds_decoder* rds_decoder_create(const struct rds_decoder_config* config);

/**
 * Delete the RDS decoder.
 *
 * @param decoder The decoder to delete.
 */
void rds_decoder_delete(struct rds_decoder* decoder);

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
 *                  rds_clear_data is called.
 * @param cb_data   Data to be passed to the callbacks when invoked.
 */
void rds_decoder_set_oda_callbacks(struct rds_decoder* decoder,
                                   DecodeODAFunc decode_cb,
                                   ClearODAFunc clear_cb,
                                   void* cb_data);

/**
 * Decode the RDS data from the supplied \p blocks.
 *
 * The supplied blocks will be decoded into the rds_data supplied when
 * creating the decoder (using rds_decoder_create).
 *
 * @param decoder The RDS decoder to use for decoding.
 * @param blocks  The RDS block data to decode.
 */
void rds_decoder_decode(struct rds_decoder* decoder,
                        const struct rds_blocks* blocks);

/**
 * Reset the decoder (and any decoded data) to the default state.
 */
void rds_decoder_reset(struct rds_decoder* decoder);

#ifdef __cplusplus
}
#endif /* __cplusplus */
