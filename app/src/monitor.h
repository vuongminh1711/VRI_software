#ifndef VRI_APP_MONITOR_H
#define VRI_APP_MONITOR_H

#include <pthread.h>

#include "srsran/phy/rf/rf.h"
#include "srsran/srsran.h"

// Background capture of one RX channel (the TX->RX loopback) to a single IQ
// file on disk, so it can be loaded offline in GNU Radio (File Source,
// Complex Float32) while vri_app itself holds exclusive access to the USRP.
// The file is truncated fresh on every start_monitor() call, never appended,
// so repeated runs don't pile up disk usage.
struct monitor_ctx_t {
  pthread_t         thread;
  srsran_rf_t*      rf;
  volatile bool      running;
  cf_t*              buffers[SRSRAN_MAX_PORTS]; // [0]=scratch (unused channel), [1]=captured
  uint32_t           channel;
  uint32_t           nof_samples_per_read;
  srsran_filesink_t  sink;
  bool               sink_open;
};

// rf must already be open with at least channel+1 RX channels. Tunes that
// channel to freq_hz/gain, creates dir_path if needed, and starts a thread
// that writes raw complex-float32 IQ from `channel` into
// "<dir_path>/<file_name>" until stop_monitor() is called.
bool start_monitor(srsran_rf_t*     rf,
                   uint32_t         nof_prb,
                   uint32_t         channel,
                   double           freq_hz,
                   float            gain,
                   const char*      dir_path,
                   const char*      file_name,
                   monitor_ctx_t*   ctx);

void stop_monitor(monitor_ctx_t* ctx);

#endif // VRI_APP_MONITOR_H
