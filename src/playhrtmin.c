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
#include <alsa/asoundlib.h>
#include "cprefresh.h"

/* help page */
/* vim hint to remove resp. add quotes:
      s/^"\(.*\)\\n"$/\1/
      s/.*$/"\0\\n"/
*/
void usage( ) {
  fprintf(stderr,
          "playhrt (version %s of frankl's stereo utilities",
          VERSION);
}


int main(int argc, char *argv[])
{
    int sfd, s, moreinput, err, verbose, nrchannels, startcount, sumavg,
        stripped, innetbufsize, dobufstats, countdelay, maxbad, nrcp, slowcp, k;
    long blen, hlen, ilen, olen, extra, loopspersec, nrdelays, sleep,
         nsec, csec, count, wnext, badloops, badreads, readmissing, avgav, checkav;
    long long icount, ocount, badframes;
    void *buf, *iptr, *optr, *tbuf, *max;
    struct timespec mtime, ctime;
    struct timespec mtimecheck;
    double looperr, off, extraerr, extrabps, morebps;
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_format_t format;
    char *host, *port, *pcm_name;
    int optc, nonblock, rate, bytespersample, bytesperframe;
    snd_pcm_uframes_t hwbufsize, periodsize, offset, frames;
    snd_pcm_access_t access;
    snd_pcm_sframes_t avail;
    const snd_pcm_channel_area_t *areas;
    double checktime;
    long corr;

    /* read command line options */
    static struct option longoptions[] = {
        {"host", required_argument, 0,  'r' },
        {"port", required_argument,       0,  'p' },
        {"stdin", no_argument,       0,  'S' },
        {"buffer-size", required_argument,       0,  'b' },
        {"input-size",  required_argument, 0, 'i'},
        {"loops-per-second", required_argument, 0,  'n' },
        {"sample-rate", required_argument, 0,  's' },
        {"sample-format", required_argument, 0, 'f' },
        {"number-channels", required_argument, 0, 'k' },
        {"mmap", no_argument, 0, 'M' },
        {"hw-buffer", required_argument, 0, 'c' },
        {"period-size", required_argument, 0, 'P' },
        {"device", required_argument, 0, 'd' },
        {"extra-bytes-per-second", required_argument, 0, 'e' },
        {"number-copies", required_argument, 0, 'R' },
        {"slow-copies", no_argument, 0, 'C' },
        {"sleep", required_argument, 0, 'D' },
        {"max-bad-reads", required_argument, 0, 'm' },
        {"in-net-buffer-size", required_argument, 0, 'K' },
        {"extra-frames-out", required_argument, 0, 'o' },
        {"non-blocking-write", no_argument, 0, 'N' },
        {"stripped", no_argument, 0, 'X' },
        {"overwrite", required_argument, 0, 'O' },
        {"verbose", no_argument, 0, 'v' },
        {"no-buf-stats", no_argument, 0, 'y' },
        {"no-delay-stats", no_argument, 0, 'j' },
        {"version", no_argument, 0, 'V' },
        {"help", no_argument, 0, 'h' },
        {0,         0,                 0,  0 }
    };

    if (argc == 1) {
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
    maxbad = 4;
    nonblock = 0;
    innetbufsize = 0;
    corr = 0;
    verbose = 0;
    stripped = 1;
    dobufstats = 1;
    countdelay = 1;
    while ((optc = getopt_long(argc, argv, "r:p:Sb:D:i:n:s:f:k:Mc:P:d:R:Ce:m:K:o:NXO:vyjVh",
            longoptions, &optind)) != -1) {
        switch (optc) {
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
          if (nrcp < 0 || nrcp > 1000) nrcp = 0;
          break;
        case 'C':
          slowcp = 1;
          break;
        case 's':
          rate = atoi(optarg);
          break;
        case 'f':
          if (strcmp(optarg, "S16_LE")==0) {
             format = SND_PCM_FORMAT_S16_LE;
             bytespersample = 2;
          } else if (strcmp(optarg, "S24_LE")==0) {
             format = SND_PCM_FORMAT_S24_LE;
             bytespersample = 4;
          } else if (strcmp(optarg, "S24_3LE")==0) {
             format = SND_PCM_FORMAT_S24_3LE;
             bytespersample = 3;
          } else if (strcmp(optarg, "S32_LE")==0) {
             format = SND_PCM_FORMAT_S32_LE;
             bytespersample = 4;
          } else if (strcmp(optarg, "DSD_U32_BE")==0) {
             format = SND_PCM_FORMAT_DSD_U32_BE;
             bytespersample = 4;
          } else {
             fprintf(stderr, "playhrt: Sample format %s not recognized.\n", optarg);
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
          maxbad = atoi(optarg);
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
        case 'O':
          break;
        case 'v':
          verbose += 1;
          break;
        case 'X':
          stripped = 1;
          break;
        case 'y':
          dobufstats = 0;
          break;
        case 'j':
          countdelay = 0;
          break;
        case 'V':
          fprintf(stderr,
                  "playhrt (version %s of frankl's stereo utilities",
                  VERSION);
#ifdef ALSANC
          fprintf(stderr, ", with alsa-lib patch");
#endif
          fprintf(stderr, ")\n");
          exit(0);
        default:
          usage();
          exit(2);
        }
    }
    bytesperframe = bytespersample*nrchannels;
    /* check some arguments and set some parameters */
    if ((host == NULL || port == NULL) && sfd < 0) {
       fprintf(stderr, "playhrt: Must specify --host and --port or --stdin.\n");
       exit(3);
    }
    /* compute nanoseconds per loop (wrt local clock) */
    extraerr = 1.0*bytesperframe*rate;
    extraerr = extraerr/(extraerr+extrabps);
    nsec = (int) (1000000000*extraerr/loopspersec);
    if (slowcp) csec = nsec / (4*nrcp);

    /* olen in frames written per loop */
    olen = rate/loopspersec;
    if (olen <= 0)
        olen = 1;
    if (ilen < bytesperframe*(olen)) {
        if (olen*loopspersec == rate)
            ilen = bytesperframe * olen;
        else
            ilen = bytesperframe * (olen+1);
    }
    /* temporary buffer */
    tbuf = NULL;     
    if (posix_memalign(&tbuf, 4096, 2*olen*bytesperframe)) {
        fprintf(stderr, "myplayhrt: Cannot allocate buffer for cleaning.\n");
        exit(6);
    }        

    /* need big enough input buffer */
    if (blen < 3*ilen) {
        blen = 3*ilen;
    }
    hlen = blen/2;
    if (olen*loopspersec == rate)
        looperr = 0.0;
    else
        looperr = (1.0*rate)/loopspersec - 1.0*olen;
    moreinput = 1;
    icount = 0;
    ocount = 0;
    /* for mmap try to set hwbuffer to multiple of output per loop */
    if (access == SND_PCM_ACCESS_MMAP_INTERLEAVED) {
        hwbufsize = hwbufsize - (hwbufsize % olen);
    }

    /* need blen plus some overlap for (circular) input buffer */
    if (! (buf = malloc(blen+ilen+(olen+extra)*bytesperframe)) ) {
        fprintf(stderr, "playhrt: Cannot allocate buffer of length %ld.\n",
                blen+ilen+(olen+extra)*bytesperframe);
        exit(2);
    }
    /* we put some overlap before the reference pointer */
    buf = buf + (olen+extra)*bytesperframe;
    max = buf + blen;
    /* the pointers for next input and next output */
    iptr = buf;
    optr = buf;

    /**********************************************************************/
    /* setup network connection                                           */
    /**********************************************************************/
    /* setup network connection */
    if (host != NULL && port != NULL) {
        sfd = fd_net(host, port);
        if (innetbufsize != 0) {
            if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, (void*)&innetbufsize, sizeof(int)) < 0) {
                fprintf(stderr, "playhrt: Cannot set buffer size for network socket to %d.\n",
                        innetbufsize);
                exit(23);
            }
        }
    }

    /**********************************************************************/
    /* setup sound device                                                 */
    /**********************************************************************/
    /* setup sound device */
    snd_pcm_hw_params_malloc(&hwparams);
    if (snd_pcm_open(&pcm_handle, pcm_name, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "playhrt: Error opening PCM device %s\n", pcm_name);
        exit(5);
    }
    if (nonblock) {
        if (snd_pcm_nonblock(pcm_handle, 1) < 0) {
            fprintf(stderr, "playhrt: Cannot set non-block mode.\n");
            exit(6);
        } 
    }
    if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
        fprintf(stderr, "playhrt: Cannot configure this PCM device.\n");
        exit(7);
    }
    if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, access) < 0) {
        fprintf(stderr, "playhrt: Error setting access.\n");
        exit(8);
    }
    if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, format) < 0) {
        fprintf(stderr, "playhrt: Error setting format.\n");
        exit(9);
    }
    if (snd_pcm_hw_params_set_rate(pcm_handle, hwparams, rate, 0) < 0) {
        fprintf(stderr, "playhrt: Error setting rate.\n");
        exit(10);
    }
    if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, nrchannels) < 0) {
        fprintf(stderr, "playhrt: Error setting channels to %d.\n", nrchannels);
        exit(11);
    }
    if (periodsize != 0) {
      if (snd_pcm_hw_params_set_period_size(
                                pcm_handle, hwparams, periodsize, 0) < 0) {
          fprintf(stderr, "playhrt: Error setting period size to %ld.\n", periodsize);
          exit(11);
      }
    }
    if (snd_pcm_hw_params_set_buffer_size(pcm_handle, hwparams,
                                                      hwbufsize) < 0) {
        fprintf(stderr, "\nplayhrt: Error setting buffersize to %ld.\n", hwbufsize);
        exit(12);
    }
    snd_pcm_hw_params_get_buffer_size(hwparams, &hwbufsize);

    if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
        fprintf(stderr, "playhrt: Error setting HW params.\n");
        exit(13);
    }
    snd_pcm_hw_params_free(hwparams);
    if (snd_pcm_sw_params_malloc (&swparams) < 0) {
        fprintf(stderr, "playhrt: Cannot allocate SW params.\n");
        exit(14);
    }
    if (snd_pcm_sw_params_current(pcm_handle, swparams) < 0) {
        fprintf(stderr, "playhrt: Cannot get current SW params.\n");
        exit(15);
    }
    if (snd_pcm_sw_params_set_start_threshold(pcm_handle,
                                          swparams, hwbufsize/2) < 0) {
        fprintf(stderr, "playhrt: Cannot set start threshold.\n");
        exit(16);
    }
    if (snd_pcm_sw_params(pcm_handle, swparams) < 0) {
        fprintf(stderr, "playhrt: Cannot apply SW params.\n");
        exit(17);
    }
    snd_pcm_sw_params_free (swparams);
    /**********************************************************************/
    /* wait to allow filling input pipeline                               */
    /**********************************************************************/

    /* get time */
    if (clock_gettime(CLOCK_MONOTONIC, &mtime) < 0) {
        fprintf(stderr, "playhrt: Error getting monotonic clock.\n");
       	exit(19);
    }

    /* use defined sleep to allow input process to fill pipeline */
    if (sleep > 0) {
        mtime.tv_sec = sleep/1000000;
        mtime.tv_nsec = 1000*(sleep - mtime.tv_sec*1000000);
        nanosleep(&mtime, NULL);
    /* waits until pipeline is filled */
    } else {
        fd_set rdfs;
        FD_ZERO(&rdfs);
        FD_SET(sfd, &rdfs);

        /* select() waits until pipeline is ready */
        if (select(sfd+1, &rdfs, NULL, NULL, NULL) <=0 ) {
            fprintf(stderr, "playhrt: Error waiting for pipeline data.\n");
            exit(20);
        };

        /* now sleep until the pipeline is filled */
        sleep = (long)((fcntl(sfd, F_GETPIPE_SZ)/bytesperframe)*1000000.0/rate); /* us */
        mtime.tv_sec = 0;
        mtime.tv_nsec = sleep*1000;
        nanosleep(&mtime, NULL);
    }
	
    /* get time */
    if (clock_gettime(CLOCK_MONOTONIC, &mtime) < 0) {
        fprintf(stderr, "playhrt: Error getting monotonic clock.\n");
       	exit(21);
    }

    /**********************************************************************/
    /* main loop                                                          */
    /**********************************************************************/
    /* main loop */
    badloops = 0;
    badframes = 0;
    badreads = 0;
    readmissing = 0;
    nrdelays = 0;

    if (access == SND_PCM_ACCESS_MMAP_INTERLEAVED && looperr == 0.0 &&
               stripped) {
     /* this is the same code as below, but with all verbosity, printf,
        offset and statistics code removed */
     startcount = hwbufsize/(2*olen);
     if (clock_gettime(CLOCK_MONOTONIC, &mtime) < 0) {
          exit(19);
      }
      for (count=1; count <= startcount; count++) {
          /* start playing when half of hwbuffer is filled */
          if (count == startcount)  snd_pcm_start(pcm_handle);

          frames = olen;
          avail = snd_pcm_avail_update(pcm_handle);
          snd_pcm_mmap_begin(pcm_handle, &areas, &offset, &frames);
          ilen = frames * bytesperframe;
          iptr = areas[0].addr + offset * bytesperframe;
          memclean(iptr, ilen);
          s = read(sfd, iptr, ilen);
          mtime.tv_nsec += nsec;
          if (mtime.tv_nsec > 999999999) {
            mtime.tv_nsec -= 1000000000;
            mtime.tv_sec++;
          }
          refreshmem(iptr, s);
          clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &mtime, NULL);
          snd_pcm_mmap_commit(pcm_handle, offset, frames);
          icount += s;
          ocount += s;
          if (s == 0) /* done */
              break;
      }
      while (1) {
          frames = olen;
          avail = snd_pcm_avail_update(pcm_handle);
          snd_pcm_mmap_begin(pcm_handle, &areas, &offset, &frames);
          ilen = frames * bytesperframe;
          iptr = areas[0].addr + offset * bytesperframe;
          memclean(iptr, ilen);
          s = read(sfd, iptr, ilen);
          if (slowcp) {
              ctime.tv_nsec = mtime.tv_nsec;
              ctime.tv_sec = mtime.tv_sec;
              for (k=nrcp; k; k--) {
                  ctime.tv_nsec += csec;
                  if (ctime.tv_nsec > 999999999) {
                    ctime.tv_nsec -= 1000000000;
                    ctime.tv_sec++;
                  }
                  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ctime, NULL);
                  memclean((char*)tbuf, ilen);
                  cprefresh((char*)tbuf, (char*)iptr, ilen);
                  ctime.tv_nsec += csec;
                  if (ctime.tv_nsec > 999999999) {
                    ctime.tv_nsec -= 1000000000;
                    ctime.tv_sec++;
                  }
                  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ctime, NULL);
                  memclean((char*)iptr, ilen);
                  cprefresh((char*)iptr, (char*)tbuf, ilen);
              }
          } else {
              for (k=nrcp; k; k--) {
                  memclean((char*)tbuf, ilen);
                  cprefresh((char*)tbuf, (char*)iptr, ilen);
                  memclean((char*)iptr, ilen);
                  cprefresh((char*)iptr, (char*)tbuf, ilen);
              }
          }
          mtime.tv_nsec += nsec;
          if (mtime.tv_nsec > 999999999) {
            mtime.tv_nsec -= 1000000000;
            mtime.tv_sec++;
          }
          refreshmem(iptr, s);
          clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &mtime, NULL);
          snd_pcm_mmap_commit(pcm_handle, offset, frames);
          icount += s;
          ocount += s;
          if (s == 0) /* done */
              break;
      }
    } 
    /* cleanup network connection and sound device */
    close(sfd);
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    return 0;
}

