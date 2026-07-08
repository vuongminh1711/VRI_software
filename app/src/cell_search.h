#ifndef VRI_APP_CELL_SEARCH_H
#define VRI_APP_CELL_SEARCH_H

#include "channel_config.h"
#include "srsran/phy/rf/rf.h"

// Tunes rf to freq_hz, searches for a cell (PSS/SSS) and decodes its MIB
// (bandwidth, ports, PHICH config, frame type). Fills out->cell and returns
// true only if a cell was found, its MIB decoded, and it reports >0 ports.
bool find_cell(srsran_rf_t* rf, double freq_hz, bool force_tdd, float rx_gain, channel_config_t* out);

#endif // VRI_APP_CELL_SEARCH_H
