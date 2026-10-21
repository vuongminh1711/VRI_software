#ifndef VRI_APP_MONITOR_H
#define VRI_APP_MONITOR_H

#include <pthread.h>

#include "srsran/phy/rf/rf.h"
#include "srsran/srsran.h"

// Background capture of BOTH RX channels (channel 0 -- whatever cell search
// already tuned it to -- and channel 1, the TX->RX loopback) to two separate
// IQ files on disk, so each can be loaded offline in GNU Radio (File Source,
// Complex Float32) while vri_app itself holds exclusive access to the USRP.
// This is a diagnostic split: normally only channel 1 matters, but dumping
// channel 0 too lets you compare, in the same run, whether real samples are
// actually landing on both channel indices or only channel 0.
// Files are truncated fresh on every start_monitor() call, never appended,
// so repeated runs don't pile up disk usage.
struct monitor_ctx_t {
  pthread_t         thread;
  srsran_rf_t*      rf;
  volatile bool      running;
  cf_t*              buffers[SRSRAN_MAX_PORTS]; // [0] and [1] both captured now
  uint32_t           nof_samples_per_read;
  srsran_filesink_t  sink0;
  srsran_filesink_t  sink1;
  bool               sink0_open;
  bool               sink1_open;
};

// rf must already be open with at least 2 RX channels. Tunes channel 1 to
// freq_hz/gain (channel 0 keeps whatever cell search already configured it
// to), creates dir_path if needed, and starts a thread that writes raw
// complex-float32 IQ from channel 0 into "<dir_path>/<file_name_ch0>" and
// from channel 1 into "<dir_path>/<file_name_ch1>" until stop_monitor() is
// called.
bool start_monitor(srsran_rf_t*     rf,
                   uint32_t         nof_prb,
                   double           freq_hz,
                   float            gain,
                   const char*      dir_path,
                   const char*      file_name_ch0,
                   const char*      file_name_ch1,
                   monitor_ctx_t*   ctx);

void stop_monitor(monitor_ctx_t* ctx);

#endif // VRI_APP_MONITOR_H
