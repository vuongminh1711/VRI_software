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
    srsran_filesink_write(&ctx->sink, ctx->buffers[ctx->channel], ret);
  }

  srsran_rf_stop_rx_stream(ctx->rf);
  return NULL;
}

bool start_monitor(srsran_rf_t*   rf,
                   uint32_t       nof_prb,
                   uint32_t       channel,
                   double         freq_hz,
                   float          gain,
                   const char*    dir_path,
                   const char*    file_name,
                   monitor_ctx_t* ctx)
{
  memset(ctx, 0, sizeof(*ctx));

  if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "Error creating %s: %s\n", dir_path, strerror(errno));
    return false;
  }

  char path[512];
  snprintf(path, sizeof(path), "%s/%s", dir_path, file_name);

  // Overwrite (not append) every run so repeated captures don't pile up.
  if (srsran_filesink_init(&ctx->sink, path, SRSRAN_COMPLEX_FLOAT_BIN)) {
    fprintf(stderr, "Error opening IQ capture file %s\n", path);
    return false;
  }
  ctx->sink_open = true;

  ctx->rf                   = rf;
  ctx->channel               = channel;
  ctx->nof_samples_per_read = SRSRAN_SF_LEN_PRB(nof_prb); // 1 subframe (1 ms) per read
  for (uint32_t i = 0; i < SRSRAN_MAX_PORTS; i++) {
    ctx->buffers[i] = srsran_vec_cf_malloc(ctx->nof_samples_per_read);
  }

  srsran_rf_set_rx_freq(rf, channel, freq_hz);
  srsran_rf_set_rx_gain_ch(rf, channel, gain);

  printf("Monitor: capturing channel %u (%.2f MHz, %.1f dB) to %s\n", channel, freq_hz / 1e6, gain, path);

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
  if (ctx->sink_open) {
    srsran_filesink_free(&ctx->sink);
  }
  printf("Monitor: capture stopped.\n");
}
