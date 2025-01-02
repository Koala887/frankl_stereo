/*
playhrt.c                Copyright frankl 2013-2016
                         Copyright Andree Buschmann 2020

This file is part of frankl's stereo utilities and was reworked by Andree Buschmann.
See the file License.txt of the distribution and http://www.gnu.org/licenses/gpl.txt 
for license details.
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
    fprintf(stderr, "playhrt (version %s of frankl's stereo utilities", VERSION);
#ifdef ALSANC
    fprintf(stderr, ", with alsa-lib patch");
#endif
    fprintf(stderr, ", reworked by Andree Buschmann");
    fprintf(stderr, ", with PI control to compensate clock deviation");
    fprintf(stderr, ")\n\nUSAGE:\n");
    fprintf(stderr,
"\n"
"  playhrt [options] \n"
"\n"
"  This program reads raw(!) stereo audio data from stdin, a file or the \n"
"  network and plays it on a local (ALSA) sound device. \n"
"\n"
"  The program repeats in a given number of loops per second: reading\n"
"  a chunk of input data, preparing data for the audio driver, then it\n"
"  sleeps until a specific instant of time and after wakeup it hands data\n"
"  to the audio driver. In contrast to other player programs this is done\n"
"  with a very precise timing such that no buffers underrun or overrun and\n"
"  no reading or writing of data is blocking. Furthermore, the data is\n"
"  refreshed in RAM directly before copying it to the audio driver.\n"
"\n"
"  The Linux kernel needs the highres-timer functionality enabled (on most\n"
"  systems this is the case).\n"
"\n"
"  This reworked version only writes input data directly to the memory\n"
"  of the audio driver (mmap mode).\n"
"\n"
"  USAGE HINTS\n"
"  \n"
"  It is recommended to give this program a high priority and not to run\n"
"  too many other things on the same computer during playback. A high\n"
"  priority can be specified with the 'chrt' command:\n"
"\n"
"  'chrt -f 70 playhrt .....'\n"
"\n"
"  (Depending on the configuration of your computer you may need root\n"
"  privileges for this, in that case use 'sudo chrt -f 99 playhrt ....' \n"
"  or give 'chrt' setuid permissions.)\n"
"\n"
"  While running this program the computer should run as few other things\n"
"  as possible. In particular we recommend to generate the input data\n"
"  on a different computer and to send them via the network to 'playhrt'\n"
"  using the program 'bufhrt' which is also contained in this package. \n"
"  \n"
"  OPTIONS\n"
"\n"
"  --host=hostname, -H hostname\n"
"      the host from which to receive the data , given by name or\n"
"      ip-address.\n"
"\n"
"  --port=portnumber, -P portnumber\n"
"      the port number on the remote host from which to receive data.\n"
"\n"
"  --stdin, -S\n"
"      read data from stdin (instead of --host and --port).\n"
"\n"
"  --device=alsaname, -d alsaname\n"
"      the name of the sound device. A typical name is 'hw:0,0', maybe\n"
"      use 'aplay -l' to find out the correct numbers. It is recommended\n"
"      to use the hardware devices 'hw:...' if possible.\n"
"\n"
"  --rate=intval, -r intval\n"
"      the sample rate of the audio data. Default is 44100 as on CDs.\n"
"\n"
"  --format=formatstring, -f formatstring\n"
"      the format of the samples in the audio data. Currently recognised are\n"
"      'S16_LE'  (signed integer 16 bits, the sample format on CDs),\n"
"      'S24_LE'  (signed integer 24 bits, packed to 4 bytes, used by many DACs)\n" 
"      'S24_3LE' (signed integer 24 bits, using 3 bytes per sample), \n"
"      'S32_LE'  (signed integer 32 bits, true 32 bit samples).\n"
"      Default is 'S16_LE'.\n"
"\n"
"  --channels=intval, -c intval\n"
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
"  --hw-buffer-size=intval, -B intval\n"
"      the buffer size (number of frames) used on the sound device.\n"
"      It may be worth to experiment a bit with this, in particular\n"
"      to try some smaller values. When 'playhrt' is called with\n"
"      '--verbose' it will report on the range allowed by the device.\n"
"      Default is 4096.\n"
" \n"
"  --in-net-buffer-size=intval, -I intval\n"
"      when reading from the network this allows to set the buffer\n"
"      size for the incoming data. This is for finetuning only, normally\n"
"      the operating system chooses sizes to guarantee constant data\n"
"      flow. The actual fill of the buffer during playback can be checked\n"
"      with 'netstat -tpn', it can be up to twice as big as the given\n"
"      intval.\n"
"\n"
"  --sleep=intval, -D intval\n"
"      causes 'playhrt' to sleep for intval microseconds (1/1000000 sec)\n"
"      after opening the sound device and before starting playback.\n"
"      This may sometimes be useful to give other programs time to fill\n"
"      the input buffer of 'playhrt'. Default is no sleep, in this case\n"
"      'playhrt' waits for the input pipeline to provide data.\n"
"\n"
"  --verbose, -v\n"
"      print some information during startup and operation.\n"
"      This option can be given twice for more output about the auto-\n"
"      matic speed control and availability of the audio buffer.\n"
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
"  second (this is in particular a good choice for USB devices), our sound\n"
"  device is 'hw:0,0' and we want to write non-blocking to the device:\n"
"\n"
"  playhrt --host=myserver --port=5123 \\\n"
"      --loops-per-second=1000 \\\n"
"      --device=hw:0,0 --sample-rate=192000 --sample-format=S32_LE \\\n"
"      --non-blocking --verbose \n"
"\n"
"  To play a local CD quality flac file 'music.flac' you need another \n"
"  program to unpack the raw audio data. In this example we use 'sox':\n"
"\n"
"  sox musik.flac -t raw - | playhrt --stdin \\\n"
"          --loops-per-second=1000 --device=hw:0,0 --sample-rate=44100 \\\n"
"          --sample-format=S16_LE --non-blocking --verbose \n"
"\n"
"  ADJUSTING SPEED\n"
"\n"
"  This version of 'playhrt' is automatically adjusting the speed of\n"
"  writing the data to the hardware buffer. This is done via measuring\n"
"  the space left in the hardware buffer and tuning the interval time\n"
"  until the next data write occurs. The targeted value is hw-buffer/2.\n"
"  \n"
"  The automatic adjustment is implemented as PI-control which allows\n"
"  'playhrt' to adjust to fixed and variable deviation of the local clock\n"
"  against the consuming clock (typically a DAC).\n"
"\n"
);
}

const char *formattime(char* text, struct timespec mtime)
{
    int hrs  = mtime.tv_sec/3600;
    int min  = (mtime.tv_sec-3600*hrs)/60;
    int sec  = mtime.tv_sec%60;
    int msec = mtime.tv_nsec/1000000;
    int usec = (mtime.tv_nsec-1000000*msec)/1000;
    int nsec = mtime.tv_nsec%1000;

    /* formatting into <h:m:s.ms us ns> for better readability */
    sprintf(text, "%02d:%02d:%02d.%03d %03d %03d", hrs, min, sec, msec, usec, nsec);
    return text;
}

const char *hms(char* text, struct timespec mtime)
{
    int hrs  = mtime.tv_sec/3600;
    int min  = (mtime.tv_sec-3600*hrs)/60;
    int sec  = mtime.tv_sec%60;

    /* formatting into <h:m:s> for better readability */
    sprintf(text, "%02d:%02d:%02d", hrs, min, sec);
    return text;
}

int main(int argc, char *argv[])
{
    int sfd, readbytes, readthis, verbose, nrchannels, startcount, sumavg, innetbufsize;
    long loopspersec, sleep, nsec, extransec, count, avgav;
    long long bytecount;
    void *iptr;
    struct timespec mtime;
    struct timespec mtimecheck;
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_format_t format;
    char *host, *port, *pcm_name;
    int optc, nonblock, rate, bytespersample, bytesperframe;
    snd_pcm_uframes_t hwbufsize, offset, frames;
    snd_pcm_sframes_t avail;
    const snd_pcm_channel_area_t *areas;
    char text[32];

    /* define variables and default for PI control */
    #define LOOPS_AVG 16        /* amount of averaged buffer measurement */
    #define LOOPS_CADENCE 4000  /* measure each LOOPS_CADENCE loops */
    double bufavg = 0;
    double buferr = 0;
    double buferr_i = 0;
    double Ta = 0.0;	/* will be calculated later */
    double Kp = 1.0;	/* value based on tests */
    double Ki = 0.05;	/* value based on tests */

    /**********************************************************************/
    /* read and set parameters                                            */
    /**********************************************************************/

    /* no parameter given */
    if (argc == 1) {
        usage();
        exit(0);
    }

    /* read command line options */
    static struct option longoptions[] = {
        {"host",               required_argument, 0, 'H' },
        {"port",               required_argument, 0, 'P' },
        {"loops-per-second",   required_argument, 0, 'n' },
        {"rate",               required_argument, 0, 'r' },
        {"format",             required_argument, 0, 'f' },
        {"channels",           required_argument, 0, 'c' },
        {"hw-buffer-size",     required_argument, 0, 'B' },
        {"device",             required_argument, 0, 'd' },
        {"sleep",              required_argument, 0, 'D' },
        {"in-net-buffer-size", required_argument, 0, 'I' },
        {"mmap",               no_argument,       0, 'M' },
        {"stdin",              no_argument,       0, 'S' },
        {"non-blocking-write", no_argument,       0, 'N' },
        {"verbose",            no_argument,       0, 'v' },
        {"version",            no_argument,       0, 'V' },
        {"help",               no_argument,       0, 'h' },
        {0,                    0,                 0,  0  }
    };

    /* set defaults */
    host = NULL;
    port = NULL;
    loopspersec = 1000;
    rate = 44100;
    format = SND_PCM_FORMAT_S16_LE;
    bytespersample = 2;
    hwbufsize = 4096;
    pcm_name = NULL;
    sfd = -1;
    nrchannels = 2;
    extransec = 0;
    sleep = 0;
    nonblock = 0;
    innetbufsize = 0;
    verbose = 0;
    sumavg = 0;
    avgav = 0;
    buferr_i = 0;
    bytecount = 0;

    /* read parameters */
    while ((optc = getopt_long(argc, argv, "H:P:n:r:f:c:B:d:D:I:MSNvVh", longoptions, &optind)) != -1) {
        switch (optc) {
        case 'H':
            host = optarg;
          	break;
        case 'P':
          	port = optarg;
          	break;
        case 'S':
          	sfd = 0;
          	break;
        case 'n':
          	loopspersec = atoi(optarg);
          	break;
        case 'r':
          	rate = atoi(optarg);
          	break;
        case 'f':
            if        (strcmp(optarg, "S16_LE" )==0) {
                format = SND_PCM_FORMAT_S16_LE;
                bytespersample = 2;
            } else if (strcmp(optarg, "S24_LE" )==0) {
                format = SND_PCM_FORMAT_S24_LE;
                bytespersample = 4;
            } else if (strcmp(optarg, "S24_3LE")==0) {
                format = SND_PCM_FORMAT_S24_3LE;
                bytespersample = 3;
            } else if (strcmp(optarg, "S32_LE" )==0) {
                format = SND_PCM_FORMAT_S32_LE;
                bytespersample = 4;
            } else {
                fprintf(stderr, "playhrt: Error. Sample format %s not recognized.\n", optarg);
                exit(1);
            }
            break;
        case 'c':
            nrchannels = atoi(optarg);
            break;
        case 'B':
            hwbufsize = atoi(optarg);
            break;
        case 'M':
            /* ignore, just kept for compatibility */
            break;
        case 'd':
            pcm_name = optarg;
            break;
        case 'D':
            sleep = atoi(optarg);
            break;
        case 'I':
            innetbufsize = atoi(optarg);
            if (innetbufsize != 0 && innetbufsize < 128)
                innetbufsize = 128;
            break;
        case 'N':
            nonblock = 1;
            break;
        case 'v':
            verbose += 1;
            break;
        case 'V':
            fprintf(stderr, "playhrt (version %s of frankl's stereo utilities", VERSION);
#ifdef ALSANC
            fprintf(stderr, ", with alsa-lib patch");
#endif
            fprintf(stderr, ", reworked by Andree Buschmann");
            fprintf(stderr, ", with PI control to compensate clock deviation)\n");
            exit(2);
	    default:
            usage();
            exit(3);
        }
    }

    /**********************************************************************/
    /* calculate and check values from given parameters                   */
    /**********************************************************************/

    /* calculate some values from the parameters */
    bytesperframe = bytespersample*nrchannels;  /* bytes per frame */
    frames = rate/loopspersec;                  /* frames per loop */
    nsec = (int) (1000000000/loopspersec);      /* compute nanoseconds per loop (wrt local clock) */
    Ta = (1.0*LOOPS_CADENCE)/loopspersec;       /* delta T seconds */
    Ta = (Ki*Ta>0.2) ? 0.2/Ki : Ta;             /* limit Ta to avoid oscallation */

    /* set hwbuffer to a multiple of frames per loop (needed for mmap!) */
    hwbufsize = hwbufsize - (hwbufsize % frames);

    /* amount of loops to fill half buffer */
    startcount = hwbufsize/(2*frames);
		
    /* check some arguments and set some parameters */
    if ((host == NULL || port == NULL) && sfd < 0) {
        fprintf(stderr, "playhrt: Error. Must specify --host and --port or --stdin.\n");
        exit(4);
    }

    /**********************************************************************/
    /* show playhrt configuration                                         */
    /**********************************************************************/

    /* show configuration */
    if (verbose) {
        fprintf(stderr, "playhrt: Version %s\n", VERSION);
        fprintf(stderr, "playhrt: Using mmap access.\n");
        fprintf(stderr, "playhrt: Step size is %ld nsec.\n", nsec);
        fprintf(stderr, "playhrt: %d channels with %d bytes per sample at %d Hz\n", nrchannels, bytespersample, rate);
    }

    /**********************************************************************/
    /* setup network connection                                           */
    /**********************************************************************/

    /* setup network connection */
    if (host != NULL && port != NULL) {
        sfd = fd_net(host, port);
        if (innetbufsize != 0) {
            if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, (void*)&innetbufsize, sizeof(int)) < 0) {
                fprintf(stderr, "playhrt: Error setting buffer size for network socket to %d.\n", innetbufsize);
                exit(5);
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
        exit(6);
    }
    if (nonblock) {
        if (snd_pcm_nonblock(pcm_handle, 1) < 0) {
            fprintf(stderr, "playhrt: Error setting non-block mode.\n");
            exit(7);
        } else if (verbose) {
            fprintf(stderr, "playhrt: Using card in non-block mode.\n");
        }
    }
    if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
        fprintf(stderr, "playhrt: Error configuring this PCM device.\n");
        exit(8);
    }
    if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_MMAP_INTERLEAVED) < 0) {
        fprintf(stderr, "playhrt: Error setting MMAP access.\n");
        exit(9);
    }
    if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, format) < 0) {
        fprintf(stderr, "playhrt: Error setting format.\n");
        exit(10);
    }
    if (snd_pcm_hw_params_set_rate(pcm_handle, hwparams, rate, 0) < 0) {
        fprintf(stderr, "playhrt: Error setting rate.\n");
        exit(11);
    }
    if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, nrchannels) < 0) {
        fprintf(stderr, "playhrt: Error setting channels to %d.\n", nrchannels);
        exit(12);
    }
    if (verbose) {
        snd_pcm_uframes_t min=1, max=100000000;
        snd_pcm_hw_params_set_buffer_size_minmax(pcm_handle, hwparams, &min, &max);
        fprintf(stderr, "playhrt: Min and max buffer size of device %ld .. %ld - ", min, max);
    }
    if (snd_pcm_hw_params_set_buffer_size(pcm_handle, hwparams, hwbufsize) < 0) {
        fprintf(stderr, "\nplayhrt: Error setting buffersize to %ld.\n", hwbufsize);
        exit(13);
    }
    snd_pcm_hw_params_get_buffer_size(hwparams, &hwbufsize);
    if (verbose) {
        fprintf(stderr, "using %ld.\n", hwbufsize);
    }
    if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
        fprintf(stderr, "playhrt: Error setting HW params.\n");
        exit(14);
    }
    snd_pcm_hw_params_free(hwparams);
    if (snd_pcm_sw_params_malloc (&swparams) < 0) {
        fprintf(stderr, "playhrt: Error allocating SW params.\n");
        exit(15);
    }
    if (snd_pcm_sw_params_current(pcm_handle, swparams) < 0) {
        fprintf(stderr, "playhrt: Error getting current SW params.\n");
        exit(16);
    }
    if (snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams, hwbufsize/2) < 0) {
        fprintf(stderr, "playhrt: Error setting start threshold.\n");
        exit(17);
    }
    if (snd_pcm_sw_params(pcm_handle, swparams) < 0) {
        fprintf(stderr, "playhrt: Error applying SW params.\n");
        exit(18);
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
    if (verbose)
        fprintf(stderr, "playhrt: Wait for pipeline (%s).\n", formattime(text, mtime));
    
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
    if (verbose)
        fprintf(stderr, "playhrt: Pipeline ready    (%s).\n", formattime(text, mtime));

    /**********************************************************************/
    /* main loop                                                          */
    /**********************************************************************/

    for (count=1; 1; count++) {

        /* start playing when half of hwbuffer is filled */
        if (count == startcount) {
            snd_pcm_start(pcm_handle);
            if (verbose) {
                clock_gettime(CLOCK_MONOTONIC, &mtimecheck);
                fprintf(stderr, "playhrt: Start playback    (%s).\n", formattime(text, mtimecheck));
            }
        }

        /* read amount of frames which can be written to hardware buffer */
        avail = snd_pcm_avail(pcm_handle);
        if (avail < 0) {
            fprintf(stderr, "playhrt: Error on snd_pcm_avail(): %ld.\n", avail);
            exit(22);
       	}

        /* get address for mmap access, we will write to iptr */
        if (snd_pcm_mmap_begin(pcm_handle, &areas, &offset, &frames) < 0) {
            fprintf(stderr, "playhrt: Error getting mmap address.\n");
            exit(23);
        }
        iptr = areas[0].addr + offset * bytesperframe;

        /**********************************************************************/
        /* automatic rate adaption                                            */
        /**********************************************************************/

       	/* start measurement/adaption in time to finish when LOOPS_CADENCE loops were done */
        if (count > startcount && (count+LOOPS_AVG) % LOOPS_CADENCE == 0) {
            sumavg = LOOPS_AVG;
            avgav = 0;
       	}

        /* add up buffer level for an amount of LOOPS_AVG measurements */
        if (sumavg) {
            avgav += avail;
            if (sumavg == 1) {
                bufavg = (double)avgav/LOOPS_AVG;   /* average buffer level */
                buferr = bufavg - hwbufsize/2;      /* error against target (hwbufsize/2) */
                buferr_i = buferr_i + buferr;       /* integrated error */

                /* calculate amount of time to be added to default step time */
                /* to overall match the local clock to the outgoing clock */
                extransec = (long)(-(Kp * buferr + Ki * Ta * buferr_i) + 0.5);
                nsec = (int)(1000000000/loopspersec + extransec);

                if (verbose > 1) {
                    /* deviation: >0 local clock it too fast, <0 local clock too slow */
                    double deviation = nsec / (1000000000.0/loopspersec) - 1.0;
                    fprintf(stderr, "playhrt: (%s) buf: %6.1f e: % 6.1f ei: % 6.1f dt: % 4ld ns (%+6.4f%%)\n", 
                            hms(text,mtime), bufavg, buferr, buferr_i, extransec, deviation*100);
                }
            }
            sumavg--;
        }

        /**********************************************************************/
        /* read data directly from pipeline into mmap´ed area (iptr)          */
        /**********************************************************************/

        /* memset(iptr, 0, frames * bytesperframe); commented out to save time */
//AB        readbytes = read(sfd, iptr, frames * bytesperframe);

        /* important: we might need to read several times to get targeted amount of data. */
        readbytes = 0;
        int nloops = 0;
        do {
            readthis = read(sfd, iptr+readbytes, (frames * bytesperframe)-readbytes);
            readbytes += readthis;
            nloops++;
        } while (readbytes < frames * bytesperframe && readthis > 0);

        if (verbose && readbytes != frames * bytesperframe)
            fprintf(stderr, "playhrt: Incomplete read (pipe end): read=%d targeted=%ld\n", 
                    readbytes, frames * bytesperframe);
        if (nloops>1)
            fprintf(stderr, "playhrt: Multiple reads required (nloops=%d).\n", nloops);

        /**********************************************************************/
        /* calculate next wakeup                                              */
        /**********************************************************************/

        /* compute time for next wakeup */
        mtime.tv_nsec += nsec;
        if (mtime.tv_nsec > 999999999) {
            mtime.tv_nsec -= 1000000000;
            mtime.tv_sec++;
        }

        /**********************************************************************/
        /* sleep until defined wakeup, refresh data, commit data              */
        /**********************************************************************/

        /* refreshmem(iptr, readbytes); commented out as called again before commit */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &mtime, NULL);
        refreshmem(iptr, readbytes); /* refresh memory */
        snd_pcm_mmap_commit(pcm_handle, offset, frames);
        bytecount += readbytes;
        if (readthis == 0) /* done */
            break;
    }

    /**********************************************************************/
    /* playhrt end, cleanup                                               */
    /**********************************************************************/

    /* cleanup network connection and sound device */
    close(sfd);
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    if (verbose) {
        fprintf(stderr, "playhrt: Loops: %ld, bytes: %lld. \n", count, bytecount);
    }
    return 0;
}
