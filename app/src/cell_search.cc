#include "cell_search.h"

#include <cstdio>
#include <cstring>

// rf_utils.h has no extern "C" guard of its own (unlike srsran/srsran.h and
// srsran/phy/rf/rf.h), so wrap it explicitly for C++ linkage.
extern "C" {
#include "srsran/phy/rf/rf_utils.h"
}

bool find_cell(srsran_rf_t* rf, double freq_hz, bool force_tdd, float rx_gain, channel_config_t* out)
{
  srsran_rf_set_rx_gain(rf, rx_gain);
  srsran_rf_set_rx_freq(rf, 0, freq_hz);
  printf("Set RX freq: %.2f MHz\n", freq_hz / 1e6);

  cell_search_cfg_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.max_frames_pbch      = SRSRAN_DEFAULT_MAX_FRAMES_PBCH;
  cfg.max_frames_pss       = SRSRAN_DEFAULT_MAX_FRAMES_PSS;
  cfg.nof_valid_pss_frames = SRSRAN_DEFAULT_NOF_VALID_PSS_FRAMES;
  cfg.init_agc             = 0;
  cfg.force_tdd            = force_tdd;

  memset(&out->cell, 0, sizeof(out->cell));

  printf("Searching for cell...\n");
  int ret = rf_search_and_decode_mib(rf, 1, &cfg, -1 /* search all N_id_2 */, &out->cell, NULL);
  if (ret < 0) {
    fprintf(stderr, "Error searching for cell\n");
    return false;
  }
  if (ret == 0 || out->cell.nof_ports == 0) {
    fprintf(stderr, "No cell found at %.2f MHz\n", freq_hz / 1e6);
    return false;
  }

  out->freq_hz = freq_hz;
  return true;
}
