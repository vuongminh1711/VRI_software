#include "monitor.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include "rf_helpers.h"

static void* monitor_thread_fnc(void* arg)
{
  monitor_ctx_t* ctx = (monitor_ctx_t*)arg;

  srsran_rf_start_rx_stream(ctx->rf, false);

  void* ptr[SRSRAN_MAX_PORTS] = {};
  for (uint32_t i = 0; i < SRSRAN_MAX_PORTS; i++) {
    ptr[i] = ctx->buffers[i];
  }

  while (ctx->running && !go_exit) {
    int ret = srsran_rf_recv_with_time_multi(ctx->rf, ptr, ctx->nof_samples_per_read, true, NULL, NULL);
    if (ret < 0) {
      break;
    }
    srsran_filesink_write(&ctx->sink0, ctx->buffers[0], ret);
    srsran_filesink_write(&ctx->sink1, ctx->buffers[1], ret);
  }

  srsran_rf_stop_rx_stream(ctx->rf);
  return NULL;
}

bool start_monitor(srsran_rf_t*   rf,
                   uint32_t       nof_prb,
                   double         freq_hz,
                   float          gain,
                   const char*    dir_path,
                   const char*    file_name_ch0,
                   const char*    file_name_ch1,
                   monitor_ctx_t* ctx)
{
  memset(ctx, 0, sizeof(*ctx));

  if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "Error creating %s: %s\n", dir_path, strerror(errno));
    return false;
  }

  char path0[512];
  char path1[512];
  snprintf(path0, sizeof(path0), "%s/%s", dir_path, file_name_ch0);
  snprintf(path1, sizeof(path1), "%s/%s", dir_path, file_name_ch1);

  // Overwrite (not append) every run so repeated captures don't pile up.
  if (srsran_filesink_init(&ctx->sink0, path0, SRSRAN_COMPLEX_FLOAT_BIN)) {
    fprintf(stderr, "Error opening IQ capture file %s\n", path0);
    return false;
  }
  ctx->sink0_open = true;

  if (srsran_filesink_init(&ctx->sink1, path1, SRSRAN_COMPLEX_FLOAT_BIN)) {
    fprintf(stderr, "Error opening IQ capture file %s\n", path1);
    srsran_filesink_free(&ctx->sink0);
    ctx->sink0_open = false;
    return false;
  }
  ctx->sink1_open = true;

  ctx->rf                   = rf;
  ctx->nof_samples_per_read = SRSRAN_SF_LEN_PRB(nof_prb); // 1 subframe (1 ms) per read
  for (uint32_t i = 0; i < SRSRAN_MAX_PORTS; i++) {
    ctx->buffers[i] = srsran_vec_cf_malloc(ctx->nof_samples_per_read);
  }

  // Channel 0 keeps whatever frequency/gain cell search already configured
  // it to; only channel 1 (the loopback/monitor channel) needs tuning here.
  srsran_rf_set_rx_freq(rf, 1, freq_hz);
  srsran_rf_set_rx_gain_ch(rf, 1, gain);

  printf("Monitor: capturing channel 0 to %s, channel 1 (%.2f MHz, %.1f dB) to %s\n",
         path0,
         freq_hz / 1e6,
         gain,
         path1);

  ctx->running = true;
  if (pthread_create(&ctx->thread, NULL, monitor_thread_fnc, ctx) != 0) {
    fprintf(stderr, "Error starting monitor thread\n");
    ctx->running = false;
    return false;
  }
  return true;
}

void stop_monitor(monitor_ctx_t* ctx)
{
  if (!ctx->running) {
    return;
  }
  ctx->running = false;
  pthread_join(ctx->thread, NULL);

  for (uint32_t i = 0; i < SRSRAN_MAX_PORTS; i++) {
    free(ctx->buffers[i]);
  }
  if (ctx->sink0_open) {
    srsran_filesink_free(&ctx->sink0);
  }
  if (ctx->sink1_open) {
    srsran_filesink_free(&ctx->sink1);
  }
  printf("Monitor: capture stopped.\n");
}
