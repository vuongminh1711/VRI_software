#include "rf_helpers.h"

#include <csignal>
#include <cstdio>

volatile bool go_exit = false;

static void sig_int_handler(int signo)
{
  if (signo == SIGINT) {
    printf("SIGINT received. Exiting...\n");
    go_exit = true;
  }
}

void install_sigint_handler()
{
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);
  signal(SIGINT, sig_int_handler);
}

bool open_rf(srsran_rf_t* rf, const char* dev, const char* args, uint32_t nof_channels)
{
  printf("Opening RF device '%s' (args: '%s', channels: %u)...\n", dev, args, nof_channels);
  if (srsran_rf_open_devname(rf, dev, const_cast<char*>(args), nof_channels)) {
    fprintf(stderr, "Error opening RF device\n");
    return false;
  }
  srsran_rf_suppress_stdout(rf);
  return true;
}

void close_rf(srsran_rf_t* rf)
{
  srsran_rf_close(rf);
}

int app_rf_recv_wrapper(void* h, cf_t* data[SRSRAN_MAX_PORTS], uint32_t nsamples, srsran_timestamp_t* t)
{
  void* ptr[SRSRAN_MAX_PORTS];
  for (int i = 0; i < SRSRAN_MAX_PORTS; i++) {
    ptr[i] = data[i];
  }
  return srsran_rf_recv_with_time_multi((srsran_rf_t*)h, ptr, nsamples, true, NULL, NULL);
}
