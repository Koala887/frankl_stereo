/*
bufhrt.c                Copyright frankl 2013-2024

This file is part of frankl's stereo utilities.
See the file License.txt of the distribution and
http://www.gnu.org/licenses/gpl.txt for license details.
*/

#define _GNU_SOURCE
#include "version.h"
#include "net.h"
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include "cprefresh.h"
#include <x86intrin.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <inttypes.h>
#include <netinet/tcp.h>

#define TBUF 134217728 /* 2^27 */

void usage()
{
  fprintf(stderr,
          "bufhrt (version %s of frankl's stereo utilities)\nUSAGE:\n",
          VERSION);
}

/* difference t2 - t1 in ns */
inline long difftimens(struct timespec t1, struct timespec t2)
{
  long long l1, l2;
  l1 = t1.tv_sec * 1000000000 + t1.tv_nsec;
  l2 = t2.tv_sec * 1000000000 + t2.tv_nsec;
  return (long)(l2 - l1);
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
  //printf("TSC-Mult: %u \n", pc->time_mult);
  //printf("TSC-Shift: %u \n", pc->time_shift);
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

long ns_to_ticks(long ns)
{
  __uint128_t x = ns;
  x *= tsc_freq_hz;
  x /= 1000000000ull;
  return (x);
}

static inline long nsloop(long cnt)
{
  if (cnt < 0) return 0;
  unsigned long long tsc = read_tsc();
  unsigned long long end = tsc + ns_to_ticks(cnt);
  while (end > __rdtsc());
  return 0;
}

int main(int argc, char *argv[])
{
  struct sockaddr_in serv_addr;
  int listenfd, connfd, ifd, s, moreinput, optval = 1, rate,
                                           bytesperframe, optc, interval, innetbufsize, nrcp,
                                           outnetbufsize, tcpnodelay, flag;
  long blen, hlen, ilen, olen, outpersec, loopspersec, nsec, count, wnext,
      outcopies, rambps, ramlps, shift,
      ramchunk;
  long long icount, ocount;
  void *buf, *iptr, *optr, *max;
  char *port, *inhost, *inport, *outfile, *infile;
  void *obufs[1024];
  struct timespec mtime, ttime;
  double looperr, extraerr, extrabps, off;

  int i;

  /* read command line options */
  static struct option longoptions[] = {
      {"port-to-write", required_argument, 0, 'p'},
      /* for backward compatibility */
      {"port", required_argument, 0, 'p'},
      {"outfile", required_argument, 0, 'o'},
      {"buffer-size", required_argument, 0, 'b'},
      {"input-size", required_argument, 0, 'i'},
      {"loops-per-second", required_argument, 0, 'n'},
      {"bytes-per-second", required_argument, 0, 'm'},
      {"ram-loops-per-second", required_argument, 0, 'X'},
      {"ram-bytes-per-second", required_argument, 0, 'Y'},
      {"dsyncs-per-second", required_argument, 0, 'D'},
      {"sample-rate", required_argument, 0, 's'},
      {"sample-format", required_argument, 0, 'f'},
      {"dsync", no_argument, 0, 'd'},
      {"file", required_argument, 0, 'F'},
      {"shift", required_argument, 0, 'x'},
      {"number-copies", required_argument, 0, 'R'},
      {"out-copies", required_argument, 0, 'c'},
      {"host-to-read", required_argument, 0, 'H'},
      {"port-to-read", required_argument, 0, 'P'},
      {"stdin", no_argument, 0, 'S'},
      {"shared", no_argument, 0, 'M'},
      {"extra-bytes-per-second", required_argument, 0, 'e'},
      {"in-net-buffer-size", required_argument, 0, 'K'},
      {"out-net-buffer-size", required_argument, 0, 'L'},
      {"tcp-nodelay", no_argument, 0, 'T' },
      {"overwrite", required_argument, 0, 'O'}, /* not used, ignored */
      {"interval", no_argument, 0, 'I'},
      {"verbose", no_argument, 0, 'v'},
      {"version", no_argument, 0, 'V'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  if (argc == 1)
  {
    usage();
    exit(0);
  }
  /* defaults */
  flag = 1;
  port = NULL;

  outfile = NULL;
  blen = 65536;
  /* default input is stdin */
  ifd = 0;
  /* default output is stdout */
  connfd = 1;
  ilen = 0;
  loopspersec = 1000;
  outpersec = 0;
  ramlps = 0;
  rambps = 0;
  ramchunk = 0;
  outcopies = 0;

  rate = 0;
  bytesperframe = 0;
  inhost = NULL;
  inport = NULL;
  infile = NULL;
  interval = 0;
  extrabps = 0.0;
  nrcp = 0;
  innetbufsize = 0;
  outnetbufsize = 0;
  shift = 100000; 
  tcpnodelay = 0;

  while ((optc = getopt_long(argc, argv, "p:o:b:i:D:n:m:X:Y:s:f:F:R:c:H:P:e:x:TvVIhd",
                             longoptions, &optind)) != -1)
  {
    switch (optc)
    {
    case 'p':
      port = optarg;
      break;
    case 'd':
      break;
    case 'o':
      outfile = optarg;
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
    case 'm':
      outpersec = atoi(optarg);
      break;
    case 'X':
      ramlps = atoi(optarg);
      break;
    case 'Y':
      rambps = atoi(optarg);
      break;
    case 's':
      rate = atoi(optarg);
      break;
    case 'R':
      nrcp = atoi(optarg);
      if (nrcp < 0 || nrcp > 1000)
        nrcp = 0;
      break;
    case 'c':
      outcopies = atoi(optarg);
      if (outcopies < 0 || outcopies > 1000)
        outcopies = 0;
      break;
    case 'f':
      if (strcmp(optarg, "S16_LE") == 0)
      {
        bytesperframe = 4;
      }
      else if (strcmp(optarg, "S24_LE") == 0)
      {
        bytesperframe = 8;
      }
      else if (strcmp(optarg, "S24_3LE") == 0)
      {
        bytesperframe = 6;
      }
      else if (strcmp(optarg, "S32_LE") == 0)
      {
        bytesperframe = 8;
      }
      else
      {
        exit(1);
      }
      break;
    case 'F':
      infile = optarg;
      break;
    case 'H':
      inhost = optarg;
      break;
    case 'P':
      inport = optarg;
      break;
    case 'S':
      ifd = 0;
      break;
    case 'M':
      break;
    case 'e':
      extrabps = atof(optarg);
      break;
    case 'D':
      break;
    case 'K':
      innetbufsize = atoi(optarg);
      if (innetbufsize != 0 && innetbufsize < 128)
        innetbufsize = 128;
      break;
    case 'L':
      outnetbufsize = atoi(optarg);
      if (outnetbufsize != 0 && outnetbufsize < 128)
        outnetbufsize = 128;
      break;
    case 'x':
      shift = atoi(optarg); 
    case 'O':
      break;   /* ignored */
    case 'T':
      tcpnodelay = 1;
      break;
    case 'I':
      interval = 1;
      break;
    case 'v':
      break;
    case 'V':
      fprintf(stderr,
              "bufhrt (version %s of frankl's stereo utilities)\n",
              VERSION);
      exit(0);
    default:
      usage();
      exit(3);
    }
  }
  /* check some arguments, open files and set some parameters */
  if (outfile)
  {
    connfd = open(outfile, O_WRONLY | O_CREAT | O_NOATIME, 00644);
    if (connfd == -1)
    {
      exit(3);
    }
  }
  if (infile)
  {
    if ((ifd = open(infile, O_RDONLY | O_NOATIME)) == -1)
    {
      exit(2);
    }
  }

  if (outpersec == 0)
  {
    if (rate != 0 && bytesperframe != 0)
    {
      outpersec = rate * bytesperframe;
    }
    else
    {
      exit(5);
    }
  }
  if (inhost != NULL && inport != NULL)
  {
    ifd = fd_net(inhost, inport);
    if (innetbufsize != 0 &&
            setsockopt(ifd, SOL_SOCKET, SO_RCVBUF, (void*)&innetbufsize, sizeof(int)) < 0) {
                exit(23);
        }
        if (tcpnodelay != 0 && setsockopt(ifd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int)));    
        {
            exit(31);
        }
  }
  if (ramlps != 0 && rambps != 0)
  {

    ramchunk = rambps / ramlps;
    while (ramchunk % 16 != 0)
      ramchunk++;
    if (ramchunk > TBUF)
      ramchunk = TBUF;
  }

  /* avoid waiting 50000 ns collecting more sleep requests */
  prctl(PR_SET_TIMERSLACK, 1L);

  /* get tsc frequency */
  tsc_freq_hz = get_tsc_freq();

  extraerr = 1.0 * outpersec / (outpersec + extrabps);
  nsec = (int)(1000000000 * extraerr / loopspersec);
  olen = outpersec / loopspersec;
  if (olen <= 0)
    olen = 1;
  if (interval)
  {
    if (ilen == 0)
      ilen = 16384;
  }
  else if (ilen < olen)
  {
    ilen = olen + 2;
    if (olen * loopspersec == outpersec)
      ilen = olen;
    else
      ilen = olen + 1;
  }
  if (blen < 3 * (ilen + olen))
    blen = 3 * (ilen + olen);
  hlen = blen / 2;
  if (olen * loopspersec == outpersec)
    looperr = 0.0;
  else
    looperr = (1.0 * outpersec) / loopspersec - 1.0 * olen;
  moreinput = 1;
  icount = 0;
  ocount = 0;

  /* we want buf % 8 = 0 */
  if (!(buf = malloc(blen + ilen + 2 * olen + 8)))
  {
    exit(6);
  }
  while (((uintptr_t)buf % 8) != 0)
    buf++;
  buf = buf + 2 * olen;
  max = buf + blen;
  iptr = buf;
  optr = buf;

  /* buffers for loop of output copies */
  if (outcopies > 0)
  {
    /* spend half of loop duration with copies of output chunk */

    for (i = 1; i < outcopies; i++)
    {
      if (posix_memalign(obufs + i, 4096, 2 * olen))
      {
        exit(20);
      }
    }
  }

  /* outgoing socket */
  if (port != 0)
  {
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
      exit(9);
    }
    if (setsockopt(listenfd,
                   SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1)
    {
      exit(10);
    }
    if (outnetbufsize != 0 && setsockopt(listenfd,
                                         SOL_SOCKET, SO_SNDBUF, &outnetbufsize, sizeof(int)) == -1)
    {
      exit(30);
    }
    if (tcpnodelay != 0 && setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int)));    
    {  
        exit(31);
    }
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(port));
    if (bind(listenfd, (struct sockaddr *)&serv_addr,
             sizeof(serv_addr)) == -1)
    {
      exit(11);
    }
    listen(listenfd, 1);
    if ((connfd = accept(listenfd, (struct sockaddr *)NULL, NULL)) == -1)
    {
      exit(12);
    }
  }

  /* default mode, no shared memory input and no interval mode */
  /* fill at least half buffer */
  memclean(buf, 2 * hlen);
  for (; iptr < buf + 2 * hlen - ilen;)
  {
    s = read(ifd, iptr, ilen);
    if (s < 0)
    {
      exit(13);
    }
    icount += s;
    if (s == 0)
    {
      moreinput = 0;
      break;
    }
    iptr += s;
  }
  if (iptr - optr < olen)
    wnext = iptr - optr;
  else
    wnext = olen;

  if (clock_gettime(CLOCK_MONOTONIC, &mtime) < 0)
  {
    exit(14);
  }

  /* main loop */

  for (count = 1, off = looperr; 1; count++, off += looperr)
  {
    /* once cache is filled and other side is reading we reset time */
    if (count == 500)
      clock_gettime(CLOCK_MONOTONIC, &mtime);
    mtime.tv_nsec += nsec;
    if (mtime.tv_nsec > 999999999)
    {
      mtime.tv_nsec -= 1000000000;
      mtime.tv_sec++;
    }
    refreshmem((char *)optr, wnext);
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &mtime, NULL) != 0)
      ;
    clock_gettime(CLOCK_MONOTONIC, &ttime);
    nsloop(shift-difftimens(mtime, ttime));      
    /* write a chunk, this comes first after waking from sleep */
    s = write(connfd, optr, wnext);
    if (s < 0)
    {
      exit(15);
    }

    ocount += s;
    optr += s;
    wnext = olen + wnext - s;
    if (off >= 1.0)
    {
      off -= 1.0;
      wnext++;
    }
    if (wnext >= 2 * olen)
    {
      wnext = 2 * olen - 1;
    }
    s = (iptr >= optr ? iptr - optr : iptr + blen - optr);
    if (s <= wnext)
    {
      wnext = s;
    }
    if (optr + wnext >= max)
    {
      optr -= blen;
    }
    /* read if buffer not half filled */
    if (moreinput && (iptr > optr ? iptr - optr : iptr + blen - optr) < hlen)
    {
      memclean(iptr, ilen);
      s = read(ifd, iptr, ilen);
      if (s < 0)
      {
        exit(16);
      }

      icount += s;
      iptr += s;
      if (iptr >= max)
      {
        memcpy(buf - 2 * olen, max - 2 * olen, iptr - max + 2 * olen);
        iptr -= blen;
      }
      if (s == 0)
      { /* input complete */
        moreinput = 0;
      }
    }
    if (wnext == 0)
      break; /* done */
  }
  close(connfd);
  shutdown(listenfd, SHUT_RDWR);
  close(listenfd);
  close(ifd);

  return 0;
}
