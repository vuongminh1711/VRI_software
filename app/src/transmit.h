#ifndef VRI_APP_TRANSMIT_H
#define VRI_APP_TRANSMIT_H

#include "channel_config.h"
#include "srsran/phy/rf/rf.h"

// Generates a synthetic downlink channel matching cfg's cell identity/config
// (PCI, bandwidth, CP, ports, PHICH, CFI): PSS/SSS/PBCH/CRS every subframe as
// required, PDCCH+PDSCH addressed to a fixed test RNTI filled with random
// bytes (no real user payload). Runs for nof_frames frames, or until go_exit
// (SIGINT) if nof_frames == 0.
//
// If iq_out_path is NULL, the generated samples are sent over rf's TX chain
// as before (intended for a TX->RX loopback cable, not an antenna). If
// iq_out_path is non-NULL, no RF TX setup or sending happens at all -- rf is
// untouched -- and the generated samples are instead written to that path as
// raw interleaved complex-float32 (one interleaved sample per port per
// subframe), so the channel can be produced and inspected without any TX
// hardware.
// Returns false (with an error already printed) if any encoder/RF/file setup
// step fails before generation starts; true otherwise, including a clean
// SIGINT.
bool transmit_channel(srsran_rf_t*            rf,
                       const channel_config_t* cfg,
                       float                   tx_gain,
                       uint32_t                nof_frames,
                       const char*             iq_out_path = nullptr);

#endif // VRI_APP_TRANSMIT_H
