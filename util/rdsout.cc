#include <string.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <vector>

#include "rds_spy_log_reader.h"

using std::cerr;
using std::cout;
using std::endl;

namespace {

#define UNUSED(expr) \
  do {               \
    (void)(expr);    \
  } while (0)

// clang-format off
//
// http://www.rds.org.uk/2010/pdf/R17_032_1.pdf
#define AID_RT_PLUS 0x4BD7 // Radiotext Plus (RT+).
#define AID_TMC     0xCD46
#define AID_ITUNES  0xC3B0 // iTunes tagging.

// clang-format on

struct ODAStats {
  int rtplus_cnt = 0;
  int tmc_cnt = 0;
  int itunes_cnt = 0;
};

void DecodeODA(uint16_t app_id,
               const struct rds_data* rds,
               const struct rds_blocks* blocks,
               struct rds_group_type gt,
               void* user_data) {
  UNUSED(rds);
  UNUSED(blocks);
  UNUSED(gt);

  ODAStats* oda_stats = (ODAStats*)user_data;

  switch (app_id) {
    case AID_RT_PLUS:
      oda_stats->rtplus_cnt++;
      break;
    case AID_TMC:
      oda_stats->tmc_cnt++;
      break;
    case AID_ITUNES:
      oda_stats->itunes_cnt++;
      break;
    case 0x0:
      break;
  }
}

void ClearODA(void* user_data) {
  ODAStats* oda_stats = (ODAStats*)user_data;
  *oda_stats = ODAStats();
}

bool is_whitespace(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\n';
}

void trim(char* str) {
  int idx = strlen(str) - 1;
  while (idx >= 0) {
    if (!is_whitespace(str[idx]))
      return;
    str[idx] = '\0';
    idx--;
  }
}

}  // namespace

int main(int argc, const char** argv) {
  std::vector<struct rds_blocks> file_blocks;
  if (argc != 2) {
    cerr << "usage rdsstats <path/to/rdsspy.log>" << endl;
    return 1;
  }

  if (!LoadRdsSpyFile(argv[1], &file_blocks)) {
    cerr << "Can't read \"" << argv[1] << '\"' << endl;
    return 2;
  }
  if (file_blocks.empty()) {
    cerr << '\"' << argv[1] << "\" is empty" << endl;
    return 3;
  }

  struct rds_data rds_data;
  memset(&rds_data, 0, sizeof(rds_data));
  ODAStats oda_stats;

  const rds_decoder_config config = {
      .advanced_ps_decoding = true,
      .rds_data = &rds_data,
  };
  rds_decoder* decoder = rds_decoder_create(&config);
  rds_decoder_set_oda_callbacks(decoder, DecodeODA, ClearODA, &oda_stats);

  std::string last;
  char rt[65];
  for (const auto& blocks : file_blocks) {
    rds_decoder_decode(decoder, &blocks);
    if (rds_data.valid_values & RDS_RT) {
      if (rds_data.rt.decode_rt == RT_A)
        memcpy(rt, rds_data.rt.a.display, 64);
      else
        memcpy(rt, rds_data.rt.b.display, 64);
      rt[64] = '\0';
      trim(rt);
      if (last != rt) {
        cout << rt << std::endl;
        last = rt;
      }
    }
  }

  rds_decoder_delete(decoder);

  return 0;
}
