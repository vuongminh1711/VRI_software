#ifndef VRI_APP_CHANNEL_CONFIG_H
#define VRI_APP_CHANNEL_CONFIG_H

#include "srsran/srsran.h"

// Control characteristics of an LTE downlink cell, captured during search/receive
// and reused to configure the transmit side of the clone.
struct channel_config_t {
  srsran_cell_t cell;      // id, cp, nof_prb, nof_ports, phich_length/resources, frame_type
  double        freq_hz;   // center frequency used for the search
  int           earfcn;    // -1 if a raw frequency was given instead of an EARFCN
  uint32_t      cfi;       // captured PCFICH control-region size (1..3)
  bool          force_tdd; // CLI hint passed into cell search
};

#endif // VRI_APP_CHANNEL_CONFIG_H
