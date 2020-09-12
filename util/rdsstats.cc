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

void PrintStats(const rds_data& rds_data, const ODAStats& oda_stats) {
#if defined(RDS_DEV)
  cout << "RDS: " << rds_data.stats.data_cnt << endl;
  cout << "BERR: " << rds_data.stats.blckb_errors << endl;
  for (int i = 0; i < 16; i++) {
    cout << i << "A: " << rds_data.stats.groups[i].a << endl;
    cout << i << "B: " << rds_data.stats.groups[i].b << endl;
  }

  cout << "AF: " << rds_data.stats.counts[PKTCNT_AF] << endl;
  cout << "CLOCK: " << rds_data.stats.counts[PKTCNT_CLOCK] << endl;
  cout << "EON: " << rds_data.stats.counts[PKTCNT_EON] << endl;
  cout << "EWS: " << rds_data.stats.counts[PKTCNT_EWS] << endl;
  cout << "FBT: " << rds_data.stats.counts[PKTCNT_FBT] << endl;
  cout << "IH: " << rds_data.stats.counts[PKTCNT_IH] << endl;
  cout << "MS: " << rds_data.stats.counts[PKTCNT_MS] << endl;
  cout << "PAGING: " << rds_data.stats.counts[PKTCNT_PAGING] << endl;
  cout << "PI_CODE: " << rds_data.stats.counts[PKTCNT_PI_CODE] << endl;
  cout << "PS: " << rds_data.stats.counts[PKTCNT_PS] << endl;
  cout << "PTY: " << rds_data.stats.counts[PKTCNT_PTY] << endl;
  cout << "PTYN: " << rds_data.stats.counts[PKTCNT_PTYN] << endl;
  cout << "RT: " << rds_data.stats.counts[PKTCNT_RT] << endl;
  cout << "SLC: " << rds_data.stats.counts[PKTCNT_SLC] << endl;
  cout << "TA_CODE: " << rds_data.stats.counts[PKTCNT_TA_CODE] << endl;
  cout << "TDC: " << rds_data.stats.counts[PKTCNT_TDC] << endl;
  cout << "TMC: " << rds_data.stats.counts[PKTCNT_TMC] << endl;
  cout << "TP_CODE: " << rds_data.stats.counts[PKTCNT_TP_CODE] << endl;

  cout << "RT+: " << oda_stats.rtplus_cnt << endl;
  cout << "RDS-TMC: " << oda_stats.tmc_cnt << endl;
  cout << "iTunes: " << oda_stats.itunes_cnt << endl;
#else
  UNUSED(f);
  UNUSED(oda_stats);
#endif
}

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

  for (const auto& blocks : file_blocks)
    rds_decoder_decode(decoder, &blocks);

  rds_decoder_delete(decoder);

  PrintStats(rds_data, oda_stats);

  return 0;
}
