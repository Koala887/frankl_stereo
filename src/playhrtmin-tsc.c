/*
playhrt.c                Copyright frankl 2013-2024

This file is part of frankl's stereo utilities.
See the file License.txt of the distribution and
http://www.gnu.org/licenses/gpl.txt for license details.
*/

#define _GNU_SOURCE
#include "version.h"
#include "net.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <alsa/asoundlib.h>
#include "cprefresh.h"
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <emmintrin.h>
#include <x86intrin.h>
#include <x86gprintrin.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <inttypes.h>

void usage()
{
  fprintf(stderr,
          "playhrt (version %s of frankl's stereo utilities",
          VERSION);
}

long long tsc_freq_hz;
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags)
{
  return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

long long get_tsc_freq(void)
{
  struct perf_event_attr pe = {
      .type = PERF_TYPE_HARDWARE,
      .size = sizeof(struct perf_event_attr),
      .config = PERF_COUNT_HW_INSTRUCTIONS,
      .disabled = 1,
      .exclude_kernel = 1,
      .exclude_hv = 1};
  int fd = perf_event_open(&pe, 0, -1, -1, 0);
  if (fd == -1)
  {
    perror("perf_event_open failed");
    return 1;
  }
  void *addr = mmap(NULL, 4 * 1024, PROT_READ, MAP_SHARED, fd, 0);
  if (!addr)
  {
    perror("mmap failed");
    return 1;
  }
  struct perf_event_mmap_page *pc = addr;
  if (pc->cap_user_time != 1)
  {
    fprintf(stderr, "Perf system doesn't support user time\n");
    return 1;
  }
  // printf("TSC-Mult: %u \n", pc->time_mult);
  // printf("TSC-Shift: %u \n", pc->time_shift);
  close(fd);

  __uint128_t x = 1000000000ull;
  x <<= pc->time_shift;
  x /= pc->time_mult;
  x += 1;
  return (x);
}

static inline unsigned long long read_tsc(void)
{
  unsigned long long tsc;
  _mm_lfence();
  tsc = __rdtsc();
  _mm_lfence();
  return (tsc);
}

static inline int tpause(long long end)
{
  int i, loops;
  long sleep;
  long long tsc = read_tsc();
  if (tsc > end) return (0);
  long step = (end - tsc);
  /* maximum sleep time for tpause is 100000 */
  loops = (step / 100000) + 1;
  sleep = (step / loops);
  for (i = 1; i < loops; i++)
  {
    _tpause(1, (tsc + (i * sleep)));
  }
  _tpause(1, end);
  return (0);
}

static inline int sleep_ns(int step)
{
  struct timespec mtime;
  clock_gettime(CLOCK_MONOTONIC, &mtime);
  mtime.tv_nsec += (step);
  if (mtime.tv_nsec > 999999999) {
    mtime.tv_sec++;
    mtime.tv_nsec -= 1000000000;
  }      
  while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &mtime, NULL) != 0);
  return (0);
}

long long ticks_to_ns(long long ticks)
{
  __uint128_t x = ticks;
  x *= 1000000000ull;
  x /= tsc_freq_hz;
  return (x);
}

long ns_to_ticks(long ns)
{
  __uint128_t x = ns;
  x *= tsc_freq_hz;
  x /= 1000000000ull;
  return (x);
}

int main(int argc, char *argv[])
{
  int sfd, s, nrchannels, startcount,
      stripped, innetbufsize, nrcp, slowcp, k, i;
  long blen, ilen, olen, extra, loopspersec, sleep,
      nsec, csec, shift;

  long long count, start_ticks, nsec_ticks, copy_ticks, csec_ticks, sleep_ticks;
  void *buf, *iptr, *tbuf, *tbufs[102];
  double looperr, extraerr, extrabps;
  snd_pcm_t *pcm_handle;
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_sw_params_t *swparams;
  snd_pcm_format_t format;
  char *host, *port, *pcm_name;
  int optc, nonblock, rate, bytespersample, bytesperframe;
  snd_pcm_uframes_t hwbufsize, periodsize, offset, frames;
  snd_pcm_access_t access;
  const snd_pcm_channel_area_t *areas;

  /* read command line options */
  static struct option longoptions[] = {
      {"host", required_argument, 0, 'r'},
      {"port", required_argument, 0, 'p'},
      {"stdin", no_argument, 0, 'S'},
      {"buffer-size", required_argument, 0, 'b'},
      {"input-size", required_argument, 0, 'i'},
      {"loops-per-second", required_argument, 0, 'n'},
      {"sample-rate", required_argument, 0, 's'},
      {"sample-format", required_argument, 0, 'f'},
      {"number-channels", required_argument, 0, 'k'},
      {"mmap", no_argument, 0, 'M'},
      {"hw-buffer", required_argument, 0, 'c'},
      {"period-size", required_argument, 0, 'P'},
      {"device", required_argument, 0, 'd'},
      {"extra-bytes-per-second", required_argument, 0, 'e'},
      {"shift", required_argument, 0, 'x'},
      {"number-copies", required_argument, 0, 'R'},
      {"slow-copies", no_argument, 0, 'C'},
      {"sleep", required_argument, 0, 'D'},
      {"max-bad-reads", required_argument, 0, 'm'},
      {"in-net-buffer-size", required_argument, 0, 'K'},
      {"extra-frames-out", required_argument, 0, 'o'},
      {"non-blocking-write", no_argument, 0, 'N'},
      {"stripped", no_argument, 0, 'X'},
      {"overwrite", required_argument, 0, 'O'},
      {"verbose", no_argument, 0, 'v'},
      {"no-buf-stats", no_argument, 0, 'y'},
      {"no-delay-stats", no_argument, 0, 'j'},
      {"version", no_argument, 0, 'V'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  if (argc == 1)
  {
    usage();
    exit(0);
  }
  /* defaults */
  host = NULL;
  port = NULL;
  blen = 65536;
  ilen = 0;
  loopspersec = 1000;
  rate = 44100;
  format = SND_PCM_FORMAT_S16_LE;
  bytespersample = 2;
  hwbufsize = 16384;
  periodsize = 0;
  /* nr of frames that wnext can be larger than olen */
  extra = 24;
  pcm_name = NULL;
  sfd = -1;
  nrchannels = 2;
  access = SND_PCM_ACCESS_RW_INTERLEAVED;
  extrabps = 0;
  nrcp = 0;
  slowcp = 0;
  csec = 0;
  sleep = 0;
  nonblock = 0;
  innetbufsize = 0;
  shift = 1000;
  stripped = 1;
  while ((optc = getopt_long(argc, argv, "r:p:Sb:D:i:n:s:f:k:Mc:P:d:R:Ce:x:m:K:o:NXO:vyjVh",
                             longoptions, &optind)) != -1)
  {
    switch (optc)
    {
    case 'r':
      host = optarg;
      break;
    case 'p':
      port = optarg;
      break;
    case 'S':
      sfd = 0;
      break;
    case 'b':
      blen = atoi(optarg);
      break;
    case 'i':
      ilen = atoi(optarg);
      break;
    case 'n':
      loopspersec = atoi(optarg);
      break;
    case 'R':
      nrcp = atoi(optarg);
      if (nrcp < 0 || nrcp > 100)
        nrcp = 0;
      break;
    case 'C':
      slowcp = 1;
      break;
    case 's':
      rate = atoi(optarg);
      break;
    case 'f':
      if (strcmp(optarg, "S16_LE") == 0)
      {
        format = SND_PCM_FORMAT_S16_LE;
        bytespersample = 2;
      }
      else if (strcmp(optarg, "S24_LE") == 0)
      {
        format = SND_PCM_FORMAT_S24_LE;
        bytespersample = 4;
      }
      else if (strcmp(optarg, "S24_3LE") == 0)
      {
        format = SND_PCM_FORMAT_S24_3LE;
        bytespersample = 3;
      }
      else if (strcmp(optarg, "S32_LE") == 0)
      {
        format = SND_PCM_FORMAT_S32_LE;
        bytespersample = 4;
      }
      else if (strcmp(optarg, "DSD_U32_BE") == 0)
      {
        format = SND_PCM_FORMAT_DSD_U32_BE;
        bytespersample = 4;
      }
      else
      {
        exit(1);
      }
      break;
    case 'k':
      nrchannels = atoi(optarg);
      break;
    case 'M':
      access = SND_PCM_ACCESS_MMAP_INTERLEAVED;
      break;
    case 'c':
      hwbufsize = atoi(optarg);
      break;
    case 'P':
      periodsize = atoi(optarg);
      break;
    case 'd':
      pcm_name = optarg;
      break;
    case 'e':
      extrabps = atof(optarg);
      break;
    case 'D':
      sleep = atoi(optarg);
      break;
    case 'm':
      break;
    case 'K':
      innetbufsize = atoi(optarg);
      if (innetbufsize != 0 && innetbufsize < 128)
        innetbufsize = 128;
      break;
    case 'o':
      extra = atoi(optarg);
      break;
    case 'N':
      nonblock = 1;
      break;
    case 'x':
      shift = atoi(optarg);
      break;
    case 'O':
      break;
    case 'v':
      break;
    case 'X':
      stripped = 1;
      break;
    case 'y':
      break;
    case 'j':
      break;
    case 'V':
      fprintf(stderr,
              "playhrt (version %s of frankl's stereo utilities",
              VERSION);
      fprintf(stderr, ")\n");
      exit(0);
    default:
      usage();
      exit(2);
    }
  }

  /* avoid waiting 50000 ns collecting more sleep requests */
  prctl(PR_SET_TIMERSLACK, 1L);

  /* get tsc frequency */
  tsc_freq_hz = get_tsc_freq();

  bytesperframe = bytespersample * nrchannels;
  /* check some arguments and set some parameters */
  if ((host == NULL || port == NULL) && sfd < 0)
  {
    exit(3);
  }
  /* compute nanoseconds per loop (wrt local clock) */
  extraerr = 1.0 * bytesperframe * rate;
  extraerr = extraerr / (extraerr + extrabps);
  nsec = (int)(1000000000 * extraerr / loopspersec);
  // calculate ticks per step
  nsec_ticks = ns_to_ticks(nsec);

  if (slowcp)
  {
    csec = nsec / (8 * nrcp);
    csec_ticks = ns_to_ticks(csec);
  }
  /* olen in frames written per loop */
  olen = rate / loopspersec;
  if (olen <= 0)
    olen = 1;
  if (ilen < bytesperframe * (olen))
  {
    if (olen * loopspersec == rate)
      ilen = bytesperframe * olen;
    else
      ilen = bytesperframe * (olen + 1);
  }

  /* temporary buffers */
  for (i=1; i < nrcp; i++) {
      if (posix_memalign(tbufs+i, 4096, 2*olen*bytesperframe)) {
          fprintf(stderr, "myplayhrt: Cannot allocate buffer for cleaning.\n");
          exit(2);
      }
  }
  tbuf = tbufs[1];

  /* need big enough input buffer */
  if (blen < 3 * ilen)
  {
    blen = 3 * ilen;
  }

  if (olen * loopspersec == rate)
    looperr = 0.0;
  else
    looperr = (1.0 * rate) / loopspersec - 1.0 * olen;

  /* for mmap try to set hwbuffer to multiple of output per loop */
  if (access == SND_PCM_ACCESS_MMAP_INTERLEAVED)
  {
    hwbufsize = hwbufsize - (hwbufsize % olen);
  }

  /* need blen plus some overlap for (circular) input buffer */
  if (!(buf = malloc(blen + ilen + (olen + extra) * bytesperframe)))
  {
    exit(2);
  }
  /* we put some overlap before the reference pointer */
  buf = buf + (olen + extra) * bytesperframe;

  /* the pointers for next input and next output */
  iptr = buf;

  /**********************************************************************/
  /* setup network connection                                           */
  /**********************************************************************/

  if (host != NULL && port != NULL)
  {
    sfd = fd_net(host, port);
    if (innetbufsize != 0)
    {
      if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, (void *)&innetbufsize, sizeof(int)) < 0)
      {
        exit(23);
      }
    }
  }

  /**********************************************************************/
  /* setup sound device                                                 */
  /**********************************************************************/

  snd_pcm_hw_params_malloc(&hwparams);
  if (snd_pcm_open(&pcm_handle, pcm_name, SND_PCM_STREAM_PLAYBACK, 0) < 0)
  {
    exit(5);
  }
  if (nonblock)
  {
    if (snd_pcm_nonblock(pcm_handle, 1) < 0)
    {
      exit(6);
    }
  }
  if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0)
  {
    exit(7);
  }
  if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, access) < 0)
  {
    exit(8);
  }
  if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, format) < 0)
  {
    exit(9);
  }
  if (snd_pcm_hw_params_set_rate(pcm_handle, hwparams, rate, 0) < 0)
  {
    exit(10);
  }
  if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, nrchannels) < 0)
  {
    exit(11);
  }
  if (periodsize != 0)
  {
    if (snd_pcm_hw_params_set_period_size(
            pcm_handle, hwparams, periodsize, 0) < 0)
    {
      exit(11);
    }
  }
  if (snd_pcm_hw_params_set_buffer_size(pcm_handle, hwparams,
                                        hwbufsize) < 0)
  {
    exit(12);
  }
  snd_pcm_hw_params_get_buffer_size(hwparams, &hwbufsize);

  if (snd_pcm_hw_params(pcm_handle, hwparams) < 0)
  {
    exit(13);
  }
  snd_pcm_hw_params_free(hwparams);
  if (snd_pcm_sw_params_malloc(&swparams) < 0)
  {
    exit(14);
  }
  if (snd_pcm_sw_params_current(pcm_handle, swparams) < 0)
  {
    exit(15);
  }
  if (snd_pcm_sw_params_set_start_threshold(pcm_handle,
                                            swparams, hwbufsize / 2) < 0)
  {
    exit(16);
  }
  if (snd_pcm_sw_params(pcm_handle, swparams) < 0)
  {
    exit(17);
  }
  snd_pcm_sw_params_free(swparams);
  /**********************************************************************/
  /* wait to allow filling input pipeline                               */
  /**********************************************************************/

  /* get time */
  start_ticks = read_tsc();

  /* use defined sleep (us) to allow input process to fill pipeline */
  if (sleep > 0)
  {
    sleep_ticks = ns_to_ticks(sleep * 1000);

    tpause(start_ticks + sleep_ticks);

    /* waits until pipeline is filled */
  }
  else
  {
    fd_set rdfs;
    FD_ZERO(&rdfs);
    FD_SET(sfd, &rdfs);

    /* select() waits until pipeline is ready */
    if (select(sfd + 1, &rdfs, NULL, NULL, NULL) <= 0)
    {
      exit(20);
    };

    /* now sleep until the pipeline is filled */
    sleep = (long)((fcntl(sfd, F_GETPIPE_SZ) / bytesperframe) * 1000000.0 / rate); /* us */
    sleep_ticks = ns_to_ticks(sleep * 1000);
    tpause(start_ticks + sleep_ticks);
  }

  /**********************************************************************/
  /* main loop                                                          */
  /**********************************************************************/
  /* main loop */

  if (access == SND_PCM_ACCESS_MMAP_INTERLEAVED && looperr == 0.0 &&
      stripped)
  {
    /* this is the same code as below, but with all verbosity, printf,
       offset and statistics code removed */
    startcount = hwbufsize / (2 * olen);
    /* get time */
    start_ticks = read_tsc();

    while (1)
    {
      /* start playing when half of hwbuffer is filled */
      if (count == startcount)  snd_pcm_start(pcm_handle);

      frames = olen;
      snd_pcm_avail(pcm_handle);
      snd_pcm_mmap_begin(pcm_handle, &areas, &offset, &frames);
      ilen = frames * bytesperframe;
      iptr = areas[0].addr + offset * bytesperframe;
      memclean(iptr, ilen);
      s = read(sfd, iptr, ilen);
      if (s == 0) /* done */
        break;      
      sleep_ns(nsec/2);
      if (slowcp) {
        copy_ticks = start_ticks + (nsec_ticks/8*6);
        tbufs[0] = iptr;
        tbufs[nrcp] = iptr;
        for (k=1; k <= nrcp; k++) {
          /* short active pause before before cprefresh
            (too short for sleeps) */
          tpause(copy_ticks);
          memclean((char*)(tbufs[k]), ilen);
          cprefresh((char*)(tbufs[k]), (char*)(tbufs[k-1]), ilen);
          memclean((char*)(tbufs[k-1]), ilen);
          copy_ticks += csec_ticks;
        }
      } else {
        for (k=nrcp; k; k--) {
          memclean((char*)tbuf, ilen);
          cprefresh((char*)tbuf, (char*)iptr, ilen);
          memclean((char*)iptr, ilen);
          cprefresh((char*)iptr, (char*)tbuf, ilen);
        }
      }

      start_ticks += nsec_ticks;

      tpause(start_ticks - shift);
      while (start_ticks > __rdtsc());
      snd_pcm_mmap_commit(pcm_handle, offset, frames);
      count++;

    }
  }
  /* cleanup network connection and sound device */
  close(sfd);
  snd_pcm_drain(pcm_handle);
  snd_pcm_close(pcm_handle);
  return 0;
}
