#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <unistd.h>

#include "cell_search.h"
#include "channel_config.h"
#include "monitor.h"
#include "receive.h"
#include "rf_helpers.h"
#include "transmit.h"

#include "srsran/common/crash_handler.h"
#include "srsran/srsran.h"

struct args_t {
  double   freq_hz        = 0.0;
  int      earfcn         = -1;
  char*    rf_dev         = (char*)"";
  char*    rf_args        = (char*)"";
  float    rx_gain        = 40.0f;
  float    tx_gain        = 60.0f;
  uint32_t nof_capture_sf = 100;
  uint32_t nof_tx_frames  = 0; // 0 = run until Ctrl+C
  uint32_t max_tx_ports   = 1; // physical TX chains available for the clone
  bool     force_tdd      = false;
  bool     verbose        = false;
  bool     monitor        = false; // capture the TX->RX loopback to a file for offline viewing
  float    monitor_gain   = 20.0f;
};

static void usage(const char* prog, const args_t& def)
{
  printf("Usage: %s -f <hz> | -c <earfcn> [options]\n", prog);
  printf("\t-f <hz>      center frequency in Hz\n");
  printf("\t-c <earfcn>  EARFCN (alternative to -f)\n");
  printf("\t-d <dev>     RF device name [Default '%s']\n", def.rf_dev);
  printf("\t-a <args>    RF device args [Default '%s']\n", def.rf_args);
  printf("\t-g <db>      RX gain during search/capture [Default %.1f]\n", def.rx_gain);
  printf("\t-G <db>      TX gain during transmit [Default %.1f]\n", def.tx_gain);
  printf("\t-n <N>       subframes to sample for CFI capture [Default %u]\n", def.nof_capture_sf);
  printf("\t-N <N>       frames to transmit, 0 = until Ctrl+C [Default %u]\n", def.nof_tx_frames);
  printf("\t-P <N>       physical TX chains available, clamps the clone's antenna port count [Default %u]\n",
         def.max_tx_ports);
  printf("\t-m           capture the TX->RX loopback to iq_capture/loopback.fc32 while transmitting\n");
  printf("\t             (needs -a with 2 channels per side, e.g. tx_subdev_spec=\"A:0 B:0\",\n");
  printf("\t             rx_subdev_spec=\"A:0 B:0\"; channel 1 is the monitor)\n");
  printf("\t-M <db>      monitor channel RX gain [Default %.1f]\n", def.monitor_gain);
  printf("\t-t           force-TDD hint for cell search\n");
  printf("\t-v           verbose\n");
}

// srsran_rf_close() reliably segfaults inside the vendored UHD teardown path
// for a dual-channel X300 session (confirmed via crash-handler backtraces),
// and exit()'s normal static-destructor sequence then aborts on the
// already-corrupted state. We don't need graceful RF cleanup for correctness
// -- the OS reclaims sockets/threads/memory on process exit regardless -- so
// skip both srsran_rf_close() and exit()'s teardown by using _exit() directly.
[[noreturn]] static void exit_now(int code)
{
  fflush(stdout);
  fflush(stderr);
  _exit(code);
}

static args_t parse_args(int argc, char** argv)
{
  args_t def;
  args_t args      = def;
  bool   have_freq = false;

  int opt;
  while ((opt = getopt(argc, argv, "f:c:d:a:g:G:n:N:P:M:mtvh")) != -1) {
    switch (opt) {
      case 'f':
        args.freq_hz = strtod(optarg, NULL);
        have_freq    = true;
        break;
      case 'c':
        args.earfcn = (int)strtol(optarg, NULL, 10);
        break;
      case 'd':
        args.rf_dev = optarg;
        break;
      case 'a':
        args.rf_args = optarg;
        break;
      case 'g':
        args.rx_gain = strtof(optarg, NULL);
        break;
      case 'G':
        args.tx_gain = strtof(optarg, NULL);
        break;
      case 'n':
        args.nof_capture_sf = (uint32_t)strtol(optarg, NULL, 10);
        break;
      case 'N':
        args.nof_tx_frames = (uint32_t)strtol(optarg, NULL, 10);
        break;
      case 'P':
        args.max_tx_ports = (uint32_t)strtol(optarg, NULL, 10);
        break;
      case 'm':
        args.monitor = true;
        break;
      case 'M':
        args.monitor_gain = strtof(optarg, NULL);
        break;
      case 't':
        args.force_tdd = true;
        break;
      case 'v':
        args.verbose = true;
        break;
      default:
        usage(argv[0], def);
        exit(1);
    }
  }

  if (have_freq == (args.earfcn >= 0)) {
    fprintf(stderr, "Specify exactly one of -f <hz> or -c <earfcn>\n\n");
    usage(argv[0], def);
    exit(1);
  }
  if (args.earfcn >= 0) {
    args.freq_hz = srsran_band_fd((uint32_t)args.earfcn) * 1e6;
  }

  return args;
}

int main(int argc, char** argv)
{
  srsran_debug_handle_crash(argc, argv);

  args_t args = parse_args(argc, argv);
  if (args.verbose) {
    increase_srsran_verbose_level();
  }

  install_sigint_handler();

  // Open once and keep the same handle for the whole run (search, capture,
  // transmit, monitor): closing and immediately reopening a UHD session on
  // the same X300 crashes inside srsran_rf_close()'s UHD teardown path, so
  // we avoid that instead of working around it. The channel count needed for
  // the eventual clone doesn't depend on what cell search finds (we always
  // clamp to -P / the monitor's 2nd channel), so it can be decided up front.
  uint32_t nof_channels = args.max_tx_ports;
  if (args.monitor && nof_channels < 2) {
    nof_channels = 2;
  }
  srsran_rf_t rf;
  if (!open_rf(&rf, args.rf_dev, args.rf_args, nof_channels)) {
    return 1;
  }

  channel_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.earfcn    = args.earfcn;
  cfg.force_tdd = args.force_tdd;

  bool found = !go_exit && find_cell(&rf, args.freq_hz, args.force_tdd, args.rx_gain, &cfg);
  if (!found) {
    fprintf(stderr, "Could not find a cell at %.2f MHz\n", args.freq_hz / 1e6);
    exit_now(1);
  }

  srsran_cell_fprint(stdout, &cfg.cell, 0);
  printf("Found CELL ID %d: %d PRB, %d ports, %s\n",
         cfg.cell.id,
         cfg.cell.nof_prb,
         cfg.cell.nof_ports,
         cfg.cell.frame_type == SRSRAN_TDD ? "TDD" : "FDD");

  if (!go_exit && !capture_cfi(&rf, &cfg, args.nof_capture_sf)) {
    fprintf(stderr, "Could not capture control-region size (CFI)\n");
    exit_now(1);
  }

  if (go_exit) {
    exit_now(0);
  }

  // The clone must be self-consistent: PBCH's CRC mask and the CRS pattern
  // are both derived from nof_ports, so we can't fake a 4-port cell over 1
  // physical TX chain. Clamp to whatever hardware is actually available
  // (-P, default 1) rather than trying to match the source cell exactly.
  if (cfg.cell.nof_ports > args.max_tx_ports) {
    printf("Note: source cell has %d ports; transmitting as a self-consistent %u-port clone "
           "(limited by available TX chains, see -P).\n",
           cfg.cell.nof_ports,
           args.max_tx_ports);
    cfg.cell.nof_ports = args.max_tx_ports;
  }

  monitor_ctx_t mon_ctx;
  bool          monitoring = false;
  if (args.monitor) {
    monitoring = start_monitor(&rf, cfg.cell.nof_prb, 1, cfg.freq_hz, args.monitor_gain, "iq_capture", "loopback.fc32", &mon_ctx);
    if (!monitoring) {
      fprintf(stderr, "Could not start loopback monitor, continuing without it\n");
    }
  }

  transmit_channel(&rf, &cfg, args.tx_gain, args.nof_tx_frames);

  if (monitoring) {
    stop_monitor(&mon_ctx);
  }

  printf("Done\n");
  exit_now(0);
}
