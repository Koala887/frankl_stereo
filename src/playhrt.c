/*
playhrt.c                Copyright frankl 2013-2014

This file is part of frankl's stereo utilities.
See the file License.txt of the distribution and
http://www.gnu.org/licenses/gpl.txt for license details.
*/

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


/* help page */
/* vim hint to remove resp. add quotes:
      s/^"\(.*\)\\n"$/\1/
      s/.*$/"\0\\n"/
*/
void usage( ) {
  fprintf(stderr,
          "playhrt (version %s of frankl's stereo utilities",
          VERSION);
#ifdef ALSANC
  fprintf(stderr, ", with alsa-lib patch");
#endif
  fprintf(stderr, ")\nUSAGE:\n");
  fprintf(stderr,
"\n"
"  playhrt [options] \n"
"\n"
"  This program reads raw(!) stereo audio data from stdin, a file or the \n"
"  network and plays it on a local (ALSA) sound device. \n"
"\n"
"  The program repeats in a given number of loops per second: reading a\n"
"  chunk of input data into a buffer, preparing the next output chunk, then\n"
"  it sleeps until a specific instant of time and after wakeup it writes\n"
"  an output chunk. \n"
"\n"
"  The Linux kernel needs the highres-timer functionality enabled (on most\n"
"  systems this is the case).\n"
"\n"
"  The program optionally supports direct writes to the sound device via \n"
"  mmap'ed memory, which sometimes results in improved sound.\n"
"\n"
"  USAGE HINTS\n"
"  \n"
"  It is recommended to give this program a high priority and not to run\n"
"  too many other things on the same computer during playback. A high\n"
"  priority can be specified with the 'chrt' command:\n"
"\n"
"  chrt -f 99 playhrt .....\n"
"\n"
"  (Depending on the configuration of your computer you may need root\n"
"  priviledges for this, in that case use 'sudo chrt -f 99 playhrt ....' \n"
"  or give 'chrt' setuid permissions.)\n"
"\n"
"  While running this program the computer should run as few other things\n"
"  as possible. In particular we recommend to generate the input data\n"
"  on a different computer and to send them via the network to 'playhrt'\n"
"  using the program 'bufhrt' which is also contained in this package. \n"
"  \n"
"  OPTIONS\n"
"\n"
"  --host=hostname, -r hostname\n"
"      the host from which we receive the data , given by name or\n"
"      ip-address.\n"
"\n"
"  --port=portnumber, -p portnumber\n"
"      the port number on the remote host from which we receive data.\n"
"\n"
"  --stdin, -S\n"
"      read data from stdin (instead of --host and --port).\n"
"\n"
"  --device=alsaname, -d alsaname\n"
"      the name of the sound device. A typical name is 'hw:0,0', maybe\n"
"      use 'aplay -l' to find out the correct numbers. \n"
"\n"
"  --sample-rate=intval, -s intval\n"
"      the sample rate of the audio data. Default is 44100 as on CDs.\n"
"\n"
"  --sample-format=formatstring, -f formatstring\n"
"      the format of the samples in the audio data. Currently recognised\n"
"      are 'S16_LE' (the sample format on CDs), 'S24_LE' \n"
"      (signed integer data with 24 bits packed into 32 bit words, used by\n"
"      many DACs), 'S24_3LE' (also 24 bit integers but only using 3 bytes\n"
"      per sample), 'S32_LE' (true 32 bit signed integer samples).\n"
"      Default is 'S16_LE'.\n"
"\n"
"  --number-channels=intval, -k intval\n"
"      the number of channels in the (interleaved) audio stream. The \n"
"      default is 2 (stereo).\n"
"\n"
"  --loops-per-second=intval, -n intval\n"
"      the number of loops per second in which 'playhrt' reads some\n"
"      data from the network into a buffer, sleeps until a precise\n"
"      moment and then writes a chunk of data to the sound device. \n"
"      Typical values would be 1000 or 2000. Default is 1000.\n"
"\n"
"  --non-blocking-write, -N\n"
"      write data to sound device in a non-blocking fashion. This can\n"
"      improve sound quality, but the timing must be very precise.\n"
"\n"
"  --mmap, -M\n"
"      write data directly to the sound device via an mmap'ed memory\n"
"      area. In this mode --buffer-size and --input-size are ignored.\n"
"      If you hear clicks enlarge the --hw-buffer.\n"
"\n"
"  --buffer-size=intval, -b intval\n"
"      the size of the internal buffer for incoming data in bytes.\n"
"      It can make sense to play around with this value, a larger\n"
"      value (say up to some or many megabytes) can be useful if the \n"
"      network is busy. Smaller values (a few kilobytes) may enable\n"
"      'playhrt' to mainly operate in memory cache and improve sound\n"
"      quality. Default is 65536 bytes. You may specify 0 or some small\n"
"      number, in which case 'playhrt' will compute and use a minimal\n"
"      amount of memory it needs, depending on the other parameters.\n"
"\n"
"  --input-size=intval, -i intval\n"
"      the amount of data in bytes 'playhrt' tries to read from the\n"
"      network during each loop (if needed). If not given or small\n"
"      'playhrt' uses the smallest amount it needs. You may try some\n"
"      larger value such that it is not necessary to read data during\n"
"      every loop.\n"
"\n"
"  --hw-buffer=intval, -c intval\n"
"      the buffer size (number of frames) used on the sound device.\n"
"      It may be worth to experiment a bit with this,\n"
"      in particular to try some smaller values. When 'playhrt' is\n"
"      called with --verbose it should report on the range allowed by\n"
"      the device. Default is 16384.\n"
" \n"
"  --period-size=intval -P intval\n"
"      the period size is the chunk size (number of frames) read by the\n"
"      sound device. The default is chosen by the hardware driver.\n"
"      Use only for final tuning (or not at all), this option can cause\n"
"      strange behaviour.\n"
"\n"
"  --extra-bytes-per-second=floatval, -e floatval\n"
"      usually the number of bytes per second that need to written\n"
"      to the sound device is computed as the sample rate times the\n"
"      number of bytes per frame (i.e., one sample per channel).\n"
"      But the clocks of the computer and the DAC/Soundcard may\n"
"      differ a little bit. This parameter allows to specify a \n"
"      correction of this number of bytes per second (negative means\n"
"      to write a little bit slower, and positive to write a bit\n"
"      faster to the sound device). The default is 0.\n"
"\n"
"  --extra-frames-out=intval, -o intval\n"
"      when in one loop not all data were written the program needs to\n"
"      write some additional frames the next time. This specifies the\n"
"      maximal extra amount before an underrun of data is assumed.\n"
"      Default is 24.\n"
"  \n"
"  --verbose, -v\n"
"      print some information during startup and operation.\n"
"\n"
"  --version, -V\n"
"      print information about the version of the program and abort.\n"
"\n"
"  --help, -h\n"
"      print this help page and abort.\n"
"\n"
"  EXAMPLES\n"
"\n"
"  We read from myserver on port 5123 stereo data in 32-bit integer\n"
"  format with a sample rate of 192000. We want to run 1000 loops per \n"
"  second (in particular a good choice for USB devices), our only sound\n"
"  device is 'hw:0,0' and we want to write non-blocking to the device:\n"
"\n"
"  chrt -f 99 playhrt --host=myserver --port=5123 --loops-per-second=1000 \\\n"
"      --device=hw:0,0 --sample-rate=192000 --sample-format=S32_LE \\\n"
"      --non-blocking --verbose \n"
"\n"
"  To play a local CD quality flac file 'music.flac' you need to use \n"
"  another program to unpack the raw audio data. In this example we use \n"
"  'sox':\n"
"\n"
"  sox musik.flac -t raw - | chrt -f 99 playhrt --stdin \\\n"
"          --loops-per-second=1000 --device=hw:0,0 --sample-rate=44100 \\\n"
"          --sample-format=S16_LE --non-blocking --verbose \n"
"\n"
"  You may also add the --mmap option to each of these examples.\n"
"\n"
"  If playback has an underrun after a while use \n"
"      (value of --hw-buffer) x (number of bytes per frame) \n"
"                             / (2 x number of seconds until underrun)\n"
"  as an estimate for the argument --extra-bytes-per-second.\n"
"  (And use a negative argument in case of overruns.)\n"
"  With --mmap --verbose it is shown with time stamps if the hardware \n"
"  buffer is filled less than 20%% or more than 80%%, which can be used \n"
"  to find a sensible value for --extra-bytes-per-second.\n"
"\n"
);
}


int main(int argc, char *argv[])
{
    int sfd, s, moreinput, err, verbose, overwrite, nrchannels, startcount;
    uint *uptr;
    long blen, hlen, ilen, olen, extra, loopspersec,
         nsec, count, wnext, badloops, badreads, readmissing;
    long long icount, ocount, badframes;
    void *buf, *wbuf, *wbuf2, *iptr, *optr, *max;
    struct timespec mtime;
    double looperr, off, extraerr, extrabps;
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
        {"extra-frames-out", required_argument, 0, 'o' },
        {"non-blocking-write", no_argument, 0, 'N' },
        {"overwrite", required_argument, 0, 'O' },
        {"verbose", no_argument, 0, 'v' },
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
    nonblock = 0;
    overwrite = 0;
    verbose = 0;
    while ((optc = getopt_long(argc, argv, "r:p:Sb:i:n:s:f:k:Mc:P:d:e:o:NvVh",
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
          } else {
             fprintf(stderr, "Sample format %s not recognized.\n", optarg);
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
        case 'o':
          extra = atoi(optarg);
          break;
        case 'N':
          nonblock = 1;
          break;
        case 'O':
          overwrite = atoi(optarg);
          break;
        case 'v':
          verbose = 1;
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
       fprintf(stderr, "Must give --host and --port or --stdin.\n");
       exit(3);
    }
    /* compute nanoseconds per loop (wrt local clock) */
    extraerr = 1.0*bytesperframe*rate;
    extraerr = extraerr/(extraerr+extrabps);
    nsec = (int) (1000000000*extraerr/loopspersec);
    if (verbose) {
        fprintf(stderr, "playhrt: Step size is %ld nsec.\n", nsec);
    }
    /* olen in frames written per loop */
    olen = rate/loopspersec;
    if (olen <= 0)
        olen = 1;
    if (ilen < bytesperframe*(olen)) {
        if (olen*loopspersec == rate)
            ilen = bytesperframe * olen;
        else
            ilen = bytesperframe * (olen+1);
        if (verbose)
            fprintf(stderr, "playhrt: Setting input chunk size to %ld bytes.\n", ilen);
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
        fprintf(stderr, "Cannot allocate buffer of length %ld.\n",
                blen+ilen+(olen+extra)*bytesperframe);
        exit(2);
    }
    if (verbose) {
        fprintf(stderr, "playhrt: Input buffer size is %ld bytes.\n",
                blen+ilen+(olen+extra)*bytesperframe);
    }
    /* we put some overlap before the reference pointer */
    buf = buf + (olen+extra)*bytesperframe;
    max = buf + blen;
    /* the pointers for next input and next output */
    iptr = buf;
    optr = buf;
    if (! (wbuf = malloc(2*olen*bytesperframe)) ) {
        fprintf(stderr, "Cannot allocate buffer of length %ld.\n",
                2*olen*bytesperframe);
        exit(3);
    }
    if (! (wbuf2 = malloc(2*olen*bytesperframe)) ) {
        fprintf(stderr, "Cannot allocate another buffer of length %ld.\n",
                2*olen*bytesperframe);
        exit(4);
    }

    /* setup network connection */
    if (host != NULL && port != NULL)
        sfd = fd_net(host, port);

    /* setup sound device */
    snd_pcm_hw_params_malloc(&hwparams);
    if (snd_pcm_open(&pcm_handle, pcm_name, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "Error opening PCM device %s\n", pcm_name);
        exit(5);
    }
    if (nonblock) {
        if (snd_pcm_nonblock(pcm_handle, 1) < 0) {
            fprintf(stderr, "Cannot set non-block mode.\n");
            exit(6);
        } else if (verbose) {
            fprintf(stderr, "playhrt: Accessing card in non-block mode.\n");
        }
    }
    if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
        fprintf(stderr, "Can not configure this PCM device.\n");
        exit(7);
    }
    if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, access) < 0) {
        fprintf(stderr, "Error setting access.\n");
        exit(8);
    }
    if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, format) < 0) {
        fprintf(stderr, "Error setting format.\n");
        exit(9);
    }
    if (snd_pcm_hw_params_set_rate(pcm_handle, hwparams, rate, 0) < 0) {
        fprintf(stderr, "Error setting rate.\n");
        exit(10);
    }
    if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, nrchannels) < 0) {
        fprintf(stderr, "Error setting channels to %d.\n", nrchannels);
        exit(11);
    }
    if (periodsize != 0) {
      if (snd_pcm_hw_params_set_period_size(
                                pcm_handle, hwparams, periodsize, 0) < 0) {
          fprintf(stderr, "Error setting period size to %ld.\n", periodsize);
          exit(11);
      }
      if (verbose) {
          fprintf(stderr, "playhrt: Setting period size explicitly to %ld frames.\n",
                          periodsize);
      }
    }
    if (verbose) {
        snd_pcm_uframes_t min=1, max=100000000;
        snd_pcm_hw_params_set_buffer_size_minmax(pcm_handle, hwparams,
                                                                &min, &max);
        fprintf(stderr,
                "playhrt: Min and max buffer size of device %ld .. %ld - ", min, max);
    }
    if (snd_pcm_hw_params_set_buffer_size(pcm_handle, hwparams,
                                                      hwbufsize) < 0) {
        fprintf(stderr, "Error setting buffersize to %ld.\n", hwbufsize);
        exit(12);
    }
    snd_pcm_hw_params_get_buffer_size(hwparams, &hwbufsize);
    if (verbose) {
        fprintf(stderr, " using %ld.\n", hwbufsize);
    }
    if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
        fprintf(stderr, "Error setting HW params.\n");
        exit(13);
    }
    snd_pcm_hw_params_free(hwparams);
    if (snd_pcm_sw_params_malloc (&swparams) < 0) {
        fprintf(stderr, "Cannot allocate SW params.\n");
        exit(14);
    }
    if (snd_pcm_sw_params_current(pcm_handle, swparams) < 0) {
        fprintf(stderr, "Cannot get current SW params.\n");
        exit(15);
    }
    if (snd_pcm_sw_params_set_start_threshold(pcm_handle,
                                          swparams, hwbufsize/2) < 0) {
        fprintf(stderr, "Cannot set start threshold.\n");
        exit(16);
    }
    if (snd_pcm_sw_params(pcm_handle, swparams) < 0) {
        fprintf(stderr, "Cannot apply SW params.\n");
        exit(17);
    }
    snd_pcm_sw_params_free (swparams);

    /* main loop */
    badloops = 0;
    badframes = 0;
    badreads = 0;
    readmissing = 0;

  if (access == SND_PCM_ACCESS_RW_INTERLEAVED) {
    /* fill half buffer */
    for (; iptr < buf + 2*hlen - ilen; ) {
        s = read(sfd, iptr, ilen);
        if (s < 0) {
            fprintf(stderr, "Read error.\n");
            exit(18);
        }
        icount += s;
        if (s == 0) {
            moreinput = 0;
            break;
        }
        iptr += s;
    }
    if (iptr - optr < olen*bytesperframe)
        wnext = (iptr-optr)/bytesperframe;
    else
        wnext = olen;

    if (clock_gettime(CLOCK_MONOTONIC, &mtime) < 0) {
        fprintf(stderr, "Cannot get monotonic clock.\n");
        exit(19);
    }
    if (verbose)
       fprintf(stderr, "playhrt: Start time (%ld sec %ld nsec)\n", 
                       mtime.tv_sec, mtime.tv_nsec);
    memcpy(wbuf, optr, wnext*bytesperframe);
    for (count=1, off=looperr; 1; count++, off+=looperr) {
        /* compute time for next wakeup */
        mtime.tv_nsec += nsec;
        if (mtime.tv_nsec > 999999999) {
          mtime.tv_nsec -= 1000000000;
          mtime.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &mtime, NULL);
        /* write a chunk, this comes first immediately after waking up */
#ifdef ALSANC
        /* here we use snd_pcm_writei_nc (if available in patched ALSA
           library. This avoids some error checks and high cpu usage with
           small hardware buffer sizes */
        s = snd_pcm_writei_nc(pcm_handle, wbuf, wnext);
#else
        /* otherwise we use the standard snd_pcm_writei  */
        s = snd_pcm_writei(pcm_handle, wbuf, wnext);
#endif
        while (s < 0) {
            s = snd_pcm_recover(pcm_handle, s, 0);
            if (s < 0) {
                snd_pcm_prepare(pcm_handle);
                fprintf(stderr, "<<<<< Cannot write, resetted >>>>\n");
            }
            clock_gettime(CLOCK_MONOTONIC, &mtime);
            if (verbose)
               fprintf(stderr, "playhrt: bad write at (%ld sec %ld nsec)\n",
                       mtime.tv_sec, mtime.tv_nsec);
#ifdef ALSANC
            s = snd_pcm_writei_nc(pcm_handle, wbuf, wnext);
#else
            s = snd_pcm_writei(pcm_handle, wbuf, wnext);
#endif
        }
        /* we count output and bad loops */
        if (s < wnext) {
            badloops++;
            badframes += (wnext - s);
        }
        ocount += s*bytesperframe;
        optr += s*bytesperframe;
        wnext = olen + wnext - s;
        if (off >= 1.0) {
           off -= 1.0;
           wnext++;
        }
        if (wnext >= olen+extra) {
           if (verbose)
              fprintf(stderr, "playhrt: Underrun by %ld bytes at (%ld sec %ld nsec).\n",
                      wnext - olen - extra, mtime.tv_sec, mtime.tv_nsec);
           wnext = olen+extra-1;
        }
        s = (iptr >= optr ? iptr - optr : iptr+blen-optr);
        if (s <= wnext*bytesperframe) {
            wnext = s/bytesperframe;
        }
        if (optr+wnext*bytesperframe >= max) {
            optr -= blen;
        }
        /* read if buffer not half filled */
        if (moreinput && (iptr > optr ? iptr-optr : iptr+blen-optr) < hlen) {
            s = read(sfd, iptr, ilen);
            if (s < 0) {
                fprintf(stderr, "Read error.\n");
                exit(20);
            } else if (s < ilen) {
                badreads++;
                readmissing += (ilen-s);
            }
            icount += s;
            iptr += s;
            /* copy input to beginning if we reach end of buffer */
            if (iptr >= max) {
                memcpy(buf-(olen+extra)*bytesperframe,
                                         max-(olen+extra)*bytesperframe,
                                         iptr-max+(olen+extra)*bytesperframe);
                iptr -= blen;
            }
            if (s == 0) { /* input complete */
                moreinput = 0;
            }
        }
        if (wnext == 0)
            break;    /* done */
        /* copy data for next write:
           they are always lying in the same memory address and so hopefully
           in processor cache     */
        for (s = 0, uptr=(uint*)wbuf; s < wnext*bytesperframe/sizeof(uint)+1; s++)
             *uptr++ = 2863311530u;
        for (s = 0, uptr=(uint*)wbuf; s < wnext*bytesperframe/sizeof(uint)+1; s++)
             *uptr++ = 4294967295u;
        for (s = 0, uptr=(uint*)wbuf; s < wnext*bytesperframe/sizeof(uint)+1; s++)
             *uptr++ = 0;
        memcpy(wbuf2, optr, wnext*bytesperframe);
        memcpy(wbuf, wbuf2, wnext*bytesperframe);
        /* overwrite multiple times (does this make a difference?)  */
        for (s = 0; s < overwrite; s++)
            memcpy(wbuf, wbuf2, wnext*bytesperframe);
    }
  } else if (access == SND_PCM_ACCESS_MMAP_INTERLEAVED) {
    /* mmap access */
    /* why does start threshold not work ??? */
   if (verbose)
       fprintf(stderr, "playhrt: Using mmap access.\n");
   startcount = hwbufsize/(2*olen);
   if (clock_gettime(CLOCK_MONOTONIC, &mtime) < 0) {
        fprintf(stderr, "Cannot get monotonic clock.\n");
        exit(19);
    }
    if (verbose)
       fprintf(stderr, "playhrt: Start time (%ld sec %ld nsec)\n", 
                       mtime.tv_sec, mtime.tv_nsec);
    for (count=1, off=looperr; 1; count++, off+=looperr) {
        if (count == startcount)  snd_pcm_start(pcm_handle);
        /* compute time for next wakeup */
        mtime.tv_nsec += nsec;
        if (mtime.tv_nsec > 999999999) {
          mtime.tv_nsec -= 1000000000;
          mtime.tv_sec++;
        }
        avail = snd_pcm_avail_update(pcm_handle);
        if (verbose) {
          if (5*avail < hwbufsize && count > startcount)
              fprintf(stderr,
                      "playhrt: available mmap buffer small: %ld (at %ld s %ld ns)\n",
                      avail, mtime.tv_sec, mtime.tv_nsec);
          if (5*avail > 4*hwbufsize && count > startcount)
              fprintf(stderr,
                      "playhrt: available mmap buffer large: %ld (at %ld s %ld ns)\n",
                      avail, mtime.tv_sec, mtime.tv_nsec);
        }
        frames = olen;
        if (off > 1.0) {
            frames++;
            off -= 1.0;
        }
        err = snd_pcm_mmap_begin(pcm_handle, &areas, &offset, &frames);
        if (err < 0) {
            fprintf(stderr, "Don't get mmap address.\n");
            exit(21);
        }
        ilen = frames * bytesperframe;
        iptr = areas[0].addr + offset * bytesperframe;
        /* clean memory */
        for (s = 0, uptr=(uint*)buf; s < ilen/sizeof(uint); s++)
             *uptr++ = 2863311530u;
        for (s = 0, uptr=(uint*)buf; s < ilen/sizeof(uint); s++)
             *uptr++ = 4294967295u;
        for (s = 0, uptr=(uint*)buf; s < ilen/sizeof(uint); s++)
             *uptr++ = 0;
        for (s = 0, uptr=(uint*)iptr; s < ilen/sizeof(uint); s++)
             *uptr++ = 2863311530u;
        for (s = 0, uptr=(uint*)iptr; s < ilen/sizeof(uint); s++)
             *uptr++ = 4294967295u;
        for (s = 0, uptr=(uint*)iptr; s < ilen/sizeof(uint); s++)
             *uptr++ = 0;
        /* we have first tried the following sequence: 
              s = read(sfd, buf, ilen);
              clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &mtime, NULL);
              memcpy((void*)iptr, (void*)buf, s);
              snd_pcm_mmap_commit(pcm_handle, offset, frames);
           but a direct read without buffer buf and an immediate commit 
           after sleep seems better */
        s = read(sfd, iptr, ilen);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &mtime, NULL);
        snd_pcm_mmap_commit(pcm_handle, offset, frames);
        if (s < 0) {
            fprintf(stderr, "Read error.\n");
            exit(22);
        } else if (s < ilen) {
            badreads++;
            readmissing += (ilen-s);
        }
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
    if (verbose) {
        fprintf(stderr, "playhrt: Loops: %ld, total bytes: %lld in %lld out. \n      Bad loops/frames written: %ld/%lld,  bad reads/bytes: %ld/%ld\n",
                    count, icount, ocount, badloops, badframes, badreads, readmissing);
    }
    return 0;
}


