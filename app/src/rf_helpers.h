#ifndef VRI_APP_RF_HELPERS_H
#define VRI_APP_RF_HELPERS_H

#include "srsran/phy/rf/rf.h"
#include "srsran/srsran.h"

// Set by the SIGINT handler; polled by the search/receive/transmit loops so
// Ctrl+C stops cleanly instead of killing the process mid-RF-transaction.
extern volatile bool go_exit;

// Installs a SIGINT handler that sets go_exit, unblocking the signal first in
// case it was inherited as blocked (same pattern as lib/examples/pdsch_ue.c).
void install_sigint_handler();

// Opens rf_dev/rf_args with the given number of channels. Returns true on success.
bool open_rf(srsran_rf_t* rf, const char* dev, const char* args, uint32_t nof_channels);

void close_rf(srsran_rf_t* rf);

// Multi-antenna recv callback used by srsran_ue_sync_*, same shape as the
// per-example wrappers named srsran_rf_recv_wrapper in the lib/examples files
// (renamed here to avoid clashing with those symbols if ever linked together).
int app_rf_recv_wrapper(void* h, cf_t* data[SRSRAN_MAX_PORTS], uint32_t nsamples, srsran_timestamp_t* t);

#endif // VRI_APP_RF_HELPERS_H
