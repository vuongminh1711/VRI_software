#ifndef VRI_APP_RECEIVE_H
#define VRI_APP_RECEIVE_H

#include "channel_config.h"
#include "srsran/phy/rf/rf.h"

// Locks onto cfg->cell (already found by find_cell) and observes up to
// nof_subframes aligned subframes to determine the control-region size
// (CFI) via PCFICH. Fills cfg->cfi. Returns false if sync never locked.
bool capture_cfi(srsran_rf_t* rf, channel_config_t* cfg, uint32_t nof_subframes);

#endif // VRI_APP_RECEIVE_H
