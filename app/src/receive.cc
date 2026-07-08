#include "receive.h"

#include <cstdio>
#include <cstring>

#include "rf_helpers.h"
#include "srsran/srsran.h"

bool capture_cfi(srsran_rf_t* rf, channel_config_t* cfg, uint32_t nof_subframes)
{
  int srate = srsran_sampling_freq_hz(cfg->cell.nof_prb);
  if (srate <= 0) {
    fprintf(stderr, "Invalid number of PRB %d\n", cfg->cell.nof_prb);
    return false;
  }
  srsran_rf_set_rx_srate(rf, (double)srate);
  srsran_rf_start_rx_stream(rf, false);

  srsran_ue_sync_t ue_sync;
  memset(&ue_sync, 0, sizeof(ue_sync));
  if (srsran_ue_sync_init_multi_decim(
          &ue_sync, cfg->cell.nof_prb, cfg->cell.id == 1000, app_rf_recv_wrapper, 1, (void*)rf, 0)) {
    fprintf(stderr, "Error initiating ue_sync\n");
    return false;
  }
  if (srsran_ue_sync_set_cell(&ue_sync, cfg->cell)) {
    fprintf(stderr, "Error setting cell in ue_sync\n");
    srsran_ue_sync_free(&ue_sync);
    return false;
  }

  cf_t* sf_buffer[SRSRAN_MAX_PORTS] = {};
  uint32_t max_num_samples          = 3 * SRSRAN_SF_LEN_PRB(cfg->cell.nof_prb);
  sf_buffer[0]                      = srsran_vec_cf_malloc(max_num_samples);

  srsran_ue_dl_t ue_dl;
  memset(&ue_dl, 0, sizeof(ue_dl));
  bool ok = true;
  if (srsran_ue_dl_init(&ue_dl, sf_buffer, cfg->cell.nof_prb, 1)) {
    fprintf(stderr, "Error initiating UE downlink processing module\n");
    ok = false;
  } else if (srsran_ue_dl_set_cell(&ue_dl, cfg->cell)) {
    fprintf(stderr, "Error setting cell in UE downlink processing module\n");
    ok = false;
  }

  uint32_t cfi_counts[4] = {0, 0, 0, 0}; // index 1..3 valid
  uint32_t nof_captured  = 0;

  if (ok) {
    srsran_ue_dl_cfg_t ue_dl_cfg;
    memset(&ue_dl_cfg, 0, sizeof(ue_dl_cfg));
    ue_dl_cfg.chest_cfg.sync_error_enable = true;

    printf("Capturing control region size (CFI) over %u subframes...\n", nof_subframes);
    while (nof_captured < nof_subframes && !go_exit) {
      cf_t* buffers[SRSRAN_MAX_CHANNELS] = {};
      buffers[0]                         = sf_buffer[0];
      int ret                            = srsran_ue_sync_zerocopy(&ue_sync, buffers, max_num_samples);
      if (ret < 0) {
        fprintf(stderr, "Error calling srsran_ue_sync_work()\n");
        break;
      }
      if (ret != 1) {
        continue; // not yet a full aligned subframe
      }

      srsran_dl_sf_cfg_t dl_sf;
      memset(&dl_sf, 0, sizeof(dl_sf));
      dl_sf.tti = srsran_ue_sync_get_sfidx(&ue_sync);

      if (srsran_ue_dl_decode_fft_estimate(&ue_dl, &dl_sf, &ue_dl_cfg) >= 0) {
        if (dl_sf.cfi >= 1 && dl_sf.cfi <= 3) {
          cfi_counts[dl_sf.cfi]++;
        }
      }
      nof_captured++;
    }

    uint32_t best_cfi = 2; // fallback default, same convention srsRAN uses for a forced MBSFN CFI
    uint32_t best_count = 0;
    for (uint32_t c = 1; c <= 3; c++) {
      if (cfi_counts[c] > best_count) {
        best_count = cfi_counts[c];
        best_cfi   = c;
      }
    }
    cfg->cfi = best_cfi;
    printf("Captured CFI=%u (seen in %u/%u subframes)\n", cfg->cfi, best_count, nof_captured);
  }

  srsran_rf_stop_rx_stream(rf);
  srsran_ue_dl_free(&ue_dl);
  srsran_ue_sync_free(&ue_sync);
  free(sf_buffer[0]);

  return ok;
}
