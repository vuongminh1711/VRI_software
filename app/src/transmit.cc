#include "transmit.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "rf_helpers.h"
#include "srsran/phy/io/filesink.h"
#include "srsran/srsran.h"

// Fixed test identity/allocation for the synthetic channel: a made-up C-RNTI
// address and a conservative mid-range MCS, since there is no real UE to
// schedule for -- this is a bench-test clone of the cell's *control*
// characteristics, not a mirror of live user traffic.
#define TEST_CRNTI 0x1234
#define TEST_MCS_IDX 10
#define MAX_SRATE_DELTA_HZ 2

bool transmit_channel(srsran_rf_t*            rf,
                       const channel_config_t* cfg,
                       float                   tx_gain,
                       uint32_t                nof_frames,
                       const char*             iq_out_path)
{
  bool dry_run = (iq_out_path != nullptr);

  srsran_cell_t cell = cfg->cell;

  if (cell.nof_ports > 1) {
    printf("Note: discovered cell has %d antenna ports; transmitting all %d streams "
           "(requires an RF device opened with that many TX channels).\n",
           cell.nof_ports,
           cell.nof_ports);
  }
  srsran_tm_t transmission_mode = (cell.nof_ports == 1) ? SRSRAN_TM1 : SRSRAN_TM2;

  uint32_t sf_n_re      = SRSRAN_SF_LEN_RE(cell.nof_prb, cell.cp);
  uint32_t sf_n_samples = 2 * SRSRAN_SLOT_LEN(srsran_symbol_sz(cell.nof_prb));

  uint8_t* data[SRSRAN_MAX_CODEWORDS] = {};
  for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
    data[i] = srsran_vec_u8_malloc(SOFTBUFFER_SIZE);
    if (!data[i]) {
      fprintf(stderr, "Error allocating data buffer\n");
      return false;
    }
    bzero(data[i], sizeof(uint8_t) * SOFTBUFFER_SIZE);
  }

  cf_t* sf_buffer[SRSRAN_MAX_PORTS]     = {};
  cf_t* output_buffer[SRSRAN_MAX_PORTS] = {};
  for (uint32_t i = 0; i < SRSRAN_MAX_PORTS; i++) {
    sf_buffer[i] = srsran_vec_cf_malloc(sf_n_re);
    if (!sf_buffer[i]) {
      fprintf(stderr, "Error allocating sf_buffer\n");
      return false;
    }
  }
  for (uint32_t i = 0; i < SRSRAN_MAX_PORTS; i++) {
    output_buffer[i] = srsran_vec_cf_malloc(sf_n_samples);
    if (!output_buffer[i]) {
      fprintf(stderr, "Error allocating output_buffer\n");
      return false;
    }
    srsran_vec_cf_zero(output_buffer[i], sf_n_samples);
  }
  cf_t* sf_symbols[SRSRAN_MAX_PORTS];
  for (uint32_t i = 0; i < SRSRAN_MAX_PORTS; i++) {
    sf_symbols[i] = sf_buffer[i % cell.nof_ports];
  }

  srsran_ofdm_t ifft[SRSRAN_MAX_PORTS];
  for (uint32_t i = 0; i < cell.nof_ports; i++) {
    if (srsran_ofdm_tx_init(&ifft[i], cell.cp, sf_buffer[i], output_buffer[i], cell.nof_prb)) {
      fprintf(stderr, "Error creating iFFT object for port %u\n", i);
      return false;
    }
    srsran_ofdm_set_normalize(&ifft[i], true);
  }

  srsran_pbch_t pbch;
  if (srsran_pbch_init(&pbch)) {
    fprintf(stderr, "Error creating PBCH object\n");
    return false;
  }
  if (srsran_pbch_set_cell(&pbch, cell)) {
    fprintf(stderr, "Error setting cell in PBCH object\n");
    return false;
  }

  srsran_regs_t regs;
  if (srsran_regs_init(&regs, cell)) {
    fprintf(stderr, "Error initiating regs\n");
    return false;
  }

  srsran_pcfich_t pcfich;
  if (srsran_pcfich_init(&pcfich, 1)) {
    fprintf(stderr, "Error creating PCFICH object\n");
    return false;
  }
  if (srsran_pcfich_set_cell(&pcfich, &regs, cell)) {
    fprintf(stderr, "Error setting cell in PCFICH object\n");
    return false;
  }

  srsran_pdcch_t pdcch;
  if (srsran_pdcch_init_enb(&pdcch, cell.nof_prb)) {
    fprintf(stderr, "Error creating PDCCH object\n");
    return false;
  }
  if (srsran_pdcch_set_cell(&pdcch, &regs, cell)) {
    fprintf(stderr, "Error setting cell in PDCCH object\n");
    return false;
  }

  srsran_pdsch_t pdsch;
  if (srsran_pdsch_init_enb(&pdsch, cell.nof_prb)) {
    fprintf(stderr, "Error creating PDSCH object\n");
    return false;
  }
  if (srsran_pdsch_set_cell(&pdsch, cell)) {
    fprintf(stderr, "Error setting cell in PDSCH object\n");
    return false;
  }

  srsran_softbuffer_tx_t* softbuffers[SRSRAN_MAX_CODEWORDS];
  for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
    softbuffers[i] = (srsran_softbuffer_tx_t*)calloc(sizeof(srsran_softbuffer_tx_t), 1);
    if (!softbuffers[i]) {
      fprintf(stderr, "Error allocating soft buffer\n");
      return false;
    }
    if (srsran_softbuffer_tx_init(softbuffers[i], cell.nof_prb)) {
      fprintf(stderr, "Error initiating soft buffer\n");
      return false;
    }
  }

  cf_t  pss_signal[SRSRAN_PSS_LEN];
  float sss_signal0[SRSRAN_SSS_LEN];
  float sss_signal5[SRSRAN_SSS_LEN];
  srsran_pss_generate(pss_signal, cell.id % 3);
  srsran_sss_generate(sss_signal0, sss_signal5, cell.id);

  srsran_refsignal_t csr_refs;
  if (srsran_refsignal_cs_init(&csr_refs, cell.nof_prb)) {
    fprintf(stderr, "Error initializing CRS\n");
    return false;
  }
  if (srsran_refsignal_cs_set_cell(&csr_refs, cell)) {
    fprintf(stderr, "Error setting cell in CRS\n");
    return false;
  }

  // RF: sample rate, gain, frequency (skipped entirely in dry-run: rf is left untouched)
  int srate = srsran_sampling_freq_hz(cell.nof_prb);
  if (srate <= 0) {
    fprintf(stderr, "Invalid number of PRB %d\n", cell.nof_prb);
    return false;
  }
  if (!dry_run) {
    printf("Setting TX sampling rate %.2f MHz\n", (float)srate / 1e6);
    double srate_rf = srsran_rf_set_tx_srate(rf, (double)srate);
    if (std::abs(srate - (int)srate_rf) > MAX_SRATE_DELTA_HZ) {
      fprintf(stderr, "Could not set TX sampling rate: wanted %d got %f\n", srate, srate_rf);
      return false;
    }
    srsran_rf_set_tx_gain(rf, tx_gain);
    printf("Set TX gain: %.1f dB\n", srsran_rf_get_tx_gain(rf));
    printf("Set TX freq: %.2f MHz\n", srsran_rf_set_tx_freq(rf, cell.nof_ports, cfg->freq_hz) / 1e6);
    printf("Transmitting clone of cell %d (%d PRB, %d ports, CFI=%u) — route TX into RX via a cable/attenuator, "
           "not an antenna.\n",
           cell.id,
           cell.nof_prb,
           cell.nof_ports,
           cfg->cfi);
  } else {
    printf("Dry run: generating clone of cell %d (%d PRB, %d ports, CFI=%u, %.2f MHz) to %s — no RF TX involved.\n",
           cell.id,
           cell.nof_prb,
           cell.nof_ports,
           cfg->cfi,
           cfg->freq_hz / 1e6,
           iq_out_path);
  }

  srsran_filesink_t iq_sink;
  if (dry_run) {
    if (srsran_filesink_init(&iq_sink, iq_out_path, SRSRAN_COMPLEX_FLOAT_BIN)) {
      fprintf(stderr, "Error opening IQ output file %s\n", iq_out_path);
      return false;
    }
  }

  // Fixed downlink resource allocation: whole bandwidth, one codeword, test RNTI.
  srsran_dci_dl_t dci_dl;
  memset(&dci_dl, 0, sizeof(dci_dl));
  dci_dl.rnti         = TEST_CRNTI;
  dci_dl.format       = SRSRAN_DCI_FORMAT1;
  dci_dl.alloc_type   = SRSRAN_RA_ALLOC_TYPE0;
  uint32_t nof_rbg     = (uint32_t)ceilf((float)cell.nof_prb / srsran_ra_type0_P(cell.nof_prb));
  dci_dl.type0_alloc.rbg_bitmask = (nof_rbg >= 32) ? 0xFFFFFFFFu : ((1u << nof_rbg) - 1u);
  dci_dl.tb[0].mcs_idx = TEST_MCS_IDX;
  dci_dl.tb[0].rv      = 0;
  dci_dl.tb[0].cw_idx  = 0;
  SRSRAN_DCI_TB_DISABLE(dci_dl.tb[1]);

  srsran_dci_location_t locations[SRSRAN_NOF_SF_X_FRAME][30];
  srsran_dl_sf_cfg_t          dl_sf;
  memset(&dl_sf, 0, sizeof(dl_sf));
  dl_sf.cfi = cfg->cfi;
  for (uint32_t i = 0; i < SRSRAN_NOF_SF_X_FRAME; i++) {
    dl_sf.tti = i;
    srsran_pdcch_ue_locations(&pdcch, &dl_sf, locations[i], 30, TEST_CRNTI);
  }

  srsran_pdsch_cfg_t pdsch_cfg;
  memset(&pdsch_cfg, 0, sizeof(pdsch_cfg));
  for (uint32_t j = 0; j < SRSRAN_MAX_CODEWORDS; j++) {
    pdsch_cfg.softbuffers.tx[j] = softbuffers[j];
  }
  pdsch_cfg.rnti = TEST_CRNTI;

  uint32_t sfn = 0;
  uint32_t nf  = 0;
  bool     start_of_burst = true;
  while ((nof_frames == 0 || nf < nof_frames) && !go_exit) {
    for (uint32_t sf_idx = 0; sf_idx < SRSRAN_NOF_SF_X_FRAME && (nof_frames == 0 || nf < nof_frames) && !go_exit;
         sf_idx++) {
      srsran_vec_cf_zero(sf_symbols[0], sf_n_re);

      if (sf_idx == 0 || sf_idx == 5) {
        srsran_pss_put_slot(pss_signal, sf_symbols[0], cell.nof_prb, cell.cp);
        srsran_sss_put_slot(sf_idx ? sss_signal5 : sss_signal0, sf_symbols[0], cell.nof_prb, cell.cp);
      }
      for (uint32_t i = 1; i < cell.nof_ports; i++) {
        memcpy(sf_symbols[i], sf_symbols[0], sizeof(cf_t) * sf_n_re);
      }

      dl_sf.tti = nf * 10 + sf_idx;
      for (uint32_t i = 0; i < cell.nof_ports; i++) {
        srsran_refsignal_cs_put_sf(&csr_refs, &dl_sf, i, sf_symbols[i]);
      }

      uint8_t bch_payload[SRSRAN_BCH_PAYLOAD_LEN];
      srsran_pbch_mib_pack(&cell, sfn, bch_payload);
      if (sf_idx == 0) {
        srsran_pbch_encode(&pbch, bch_payload, sf_symbols, nf % 4);
      }

      dl_sf.cfi = cfg->cfi;
      srsran_pcfich_encode(&pcfich, &dl_sf, sf_symbols);

      // PSS/SSS/PBCH occupy subframes 0 and 5; carry PDCCH+PDSCH on the rest,
      // filled with random bytes since there is no real payload to send.
      bool send_data = (sf_idx != 0 && sf_idx != 5);
      if (send_data) {
        dl_sf.sf_type = SRSRAN_SF_NORM;

        srsran_dci_msg_t dci_msg;
        srsran_dci_msg_pack_pdsch(&cell, &dl_sf, NULL, &dci_dl, &dci_msg);
        dci_msg.location = locations[sf_idx][0];
        srsran_pdcch_encode(&pdcch, &dl_sf, &dci_msg, sf_symbols);

        srsran_ra_dl_dci_to_grant(&cell, &dl_sf, transmission_mode, false, &dci_dl, &pdsch_cfg.grant);

        if (pdsch_cfg.grant.tb[0].enabled) {
          for (int i = 0; i < pdsch_cfg.grant.tb[0].tbs / 8; i++) {
            data[0][i] = (uint8_t)rand();
          }
        }
        srsran_pdsch_encode(&pdsch, &dl_sf, &pdsch_cfg, data, sf_symbols);
      }

      for (uint32_t i = 0; i < cell.nof_ports; i++) {
        srsran_ofdm_tx_sf(&ifft[i]);
      }

      float norm_factor =
          send_data ? (float)cell.nof_prb / 15 / sqrtf((float)pdsch_cfg.grant.nof_prb) : (float)cell.nof_prb / 15;
      for (uint32_t i = 0; i < cell.nof_ports; i++) {
        srsran_vec_sc_prod_cfc(output_buffer[i], 0.8f * norm_factor, output_buffer[i], SRSRAN_SF_LEN_PRB(cell.nof_prb));
      }
      if (dry_run) {
        srsran_filesink_write_multi(&iq_sink, (void**)output_buffer, sf_n_samples, cell.nof_ports);
      } else {
        srsran_rf_send_multi(rf, (void**)output_buffer, sf_n_samples, true, start_of_burst, false);
      }
      start_of_burst = false;
    }
    nf++;
    sfn = (sfn + 1) % 1024;
  }

  for (uint32_t i = 0; i < SRSRAN_MAX_CODEWORDS; i++) {
    srsran_softbuffer_tx_free(softbuffers[i]);
    free(softbuffers[i]);
    free(data[i]);
  }
  srsran_pdsch_free(&pdsch);
  srsran_pdcch_free(&pdcch);
  srsran_regs_free(&regs);
  srsran_pcfich_free(&pcfich);
  srsran_pbch_free(&pbch);
  srsran_refsignal_free(&csr_refs);
  for (uint32_t i = 0; i < cell.nof_ports; i++) {
    srsran_ofdm_tx_free(&ifft[i]);
  }
  for (uint32_t i = 0; i < SRSRAN_MAX_PORTS; i++) {
    free(sf_buffer[i]);
    free(output_buffer[i]);
  }

  if (dry_run) {
    srsran_filesink_free(&iq_sink);
    printf("Dry run: wrote IQ samples to %s\n", iq_out_path);
  }

  return true;
}
