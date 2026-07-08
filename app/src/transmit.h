#ifndef VRI_APP_TRANSMIT_H
#define VRI_APP_TRANSMIT_H

#include "channel_config.h"
#include "srsran/phy/rf/rf.h"

// Transmits a synthetic downlink channel matching cfg's cell identity/config
// (PCI, bandwidth, CP, ports, PHICH, CFI): PSS/SSS/PBCH/CRS every subframe as
// required, PDCCH+PDSCH addressed to a fixed test RNTI filled with random
// bytes (no real user payload). Runs for nof_frames frames, or until go_exit
// (SIGINT) if nof_frames == 0. Intended for a TX->RX loopback cable, not an
// antenna.
void transmit_channel(srsran_rf_t* rf, const channel_config_t* cfg, float tx_gain, uint32_t nof_frames);

#endif // VRI_APP_TRANSMIT_H
