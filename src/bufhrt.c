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
#include <semaphore.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include "cprefresh.h"
#include <netinet/tcp.h>

#define TBUF 134217728 /* 2^27 */

/* help page */
/* vim hint to remove resp. add quotes:
      s/^"\(.*\)\\n"$/\1/
      s/.*$/"\0\\n"/
*/
void usage( ) {
  fprintf(stderr,
          "bufhrt (version %s of frankl's stereo utilities)\nUSAGE:\n",
          VERSION);
  fprintf(stderr,
"\n"
"  bufhrt [options] [--shared <snam1> <snam2> [...]]\n"
"\n"
"  This program reads data from stdin, a file or a network connection and\n"
"  writes it to stdout, a file or the network in precisely timed chunks.\n"
"  To be accurate, the Linux kernel should have the highres-timer enabled\n"
"  (which is the case in most Linux distributions).\n"
"\n"
"  The default input is stdin. Otherwise specify a file or a network\n"
"  connection. Furthermore, the number of bytes to be written per second\n"
"  must be given. In a given number of loops per second the program\n"
"  reads a chunk of data (if needed), sleeps until a specific instant of\n"
"  time and writes a chunk of data after waking up. The data which are\n"
"  buffered in RAM are refreshed just before writing them.\n"
"\n"
"  The default output is stdout, otherwise specify a network port or file\n"
"  name.\n"
"\n"
"  There are also two special modes. One is to read the data from shared\n"
"  memory, written by the 'writeloop' utility (see --shared option). The\n"
"  other is to first collect input into a buffer and then to write data\n"
"  without reading more input, see the --interval option.\n"
"\n"
"  USAGE HINTS\n"
"\n"
"  For critical applications (e.g., sending audio data to our player program\n"
"  'playhrt') 'bufhrt' may be started with a high priority for the real\n"
"  time scheduler:  chrt -f 99 bufhrt .....\n"
"  See the documentation of 'playhrt' for more details.\n"
"\n"
"  OPTIONS\n"
"\n"
"  --port-to-write=intval, -p intval\n"
"      the network port number to which data are written instead of stdout.\n"
"\n"
"  --outfile=fname, -o fname\n"
"      write to this file instead of stdout.\n"
"\n"
"  --bytes-per-second=intval, -m intval\n"
"      the number of bytes to be written to the output per second.\n"
"      (Alternatively, in case of stereo audio data, the options\n"
"       --sample-rate and --sample-format can be given instead.)\n"
"\n"
"  --loops-per-second=intval, -n intval\n"
"      the number of loops per second in which this program is reading\n"
"      data, sleeping and then writing a chunk of data. Default is 1000.\n"
"\n"
"  --buffer-size=intval, -b intval\n"
"      the size of the buffer between reading and writing data in bytes.\n"
"      Default is 65536. If reading the input is instable then a larger\n"
"      buffer can be useful. Otherwise, it should be small to fit in\n"
"      memory cache.\n"
"\n"
"  --file=fname, -F fname\n"
"      the name of a file that should be read instead of standard input.\n"
"\n"
"  --host-to-read=hname, -H hname\n"
"      the name or ip-address of a machine. If given, you have to specify\n"
"      --port-to-read as well. In this case data are not read from stdin\n"
"      but from this host and port.\n"
"\n"
"  --port-to-read=intval, -P intval\n"
"      a port number, see --host-to-read.\n"
"\n"
"  --stdin, -S\n"
"      read data from stdin. This is the default.\n"
"\n"
"  --shared <snam1> <snam2> ...\n"
"      input is read from shared memory. The names <snam1>, ..., of the\n"
"      memory files must be specified after all other options. This\n"
"      option can be used together with 'writeloop' which writes the\n"
"      data to the memory files (see documentation of 'writeloop').\n"
"      For transfering music data this option may lead to better sound\n"
"      quality than using 'catloop' and a pipe to 'bufhrt' with --stdin\n"
"      input. We have made good experience with using three shared memory\n"
"      chunks whose combined length is a bit smaller than the CPUs\n"
"      L1-cache. In this mode 'bufhrt' does not combine data from\n"
"      several memory files as output in one loop. Therefore,\n"
"      'writeloop' should be used with a '--file-size' parameter\n"
"      which is a multiple of the output per loop of 'bufhrt'.\n"
"\n"
"  --input-size=intval, -i intval\n"
"      the number of bytes to be read per loop (when needed). The default\n"
"      is to use the smallest amount needed for the output.\n"
"\n"
"  --extra-bytes-per-second=floatval, -e floatval\n"
"      sometimes the clocks in the sending machine and the receiving\n"
"      machine are not absolutely synchronous. This option allows\n"
"      to specify an amount of extra bytes to be written to the output\n"
"      per second (negativ for fewer and positive for more bytes).\n"
"      The program adjusts the duration of the read-sleep-write rounds.\n"
"\n"
"  --interval, -I\n"
"      use interval mode, typically together with a large --buffer-size.\n"
"      Per interval the buffer is filled without writing data, and then\n"
"      the buffer content is written out in a sleep-write loop (without\n"
"      reading input). See below for an example.\n"
"\n"
"  --number-copies=intnum, -R intnum\n"
"      this option is only used in --interval mode which was mentioned before.\n"
"      Before writing out data they are copied the specfied number of\n"
"      times to a cleaned temporary buffer in RAM and then back to the \n"
"      cleaned original buffer. The default is 0. This option may improve \n"
"      sound quality in applications like 'improvefile' (see example script).\n"
"  \n"
"  --ram-loops-per-second=intnum, -X intnum\n"
"  --ram-bytes-per-second=intnum, -Y intnum\n"
"      by default the copies in RAM specified by the previous option \n"
"      --number-copies are written as fast as possible. Optionally these two\n"
"      options can be used for slower copy loops with timing.\n"
"\n"
"  --out-copies=intnum, -c intnum\n"
"      with this option the content of the next output chunk will be copied\n"
"      through a loop of intnum buffers. The copies will be done in equal time\n"
"      intervals during the first half of the waiting time in the output loop.\n"
"      This option can be used instead or together with --number-copies.\n"
"      The default is 0, so no copies.\n"
"\n"
"  --dsyncs-per-second=intval, -D intval\n"
"      in interval mode data of output file will be transfered to storage\n"
"      hardware with this frequency. Sensible values depend on hardware, can\n"
"      be between 1 and 200.\n"
"\n"
"  --dsync, -d\n"
"      abbreviation to set --dsyncs-per-second to same as --loops-per-second.\n"
"\n"
"  --in-net-buffer-size=intval, -K intval\n"
"  --out-net-buffer-size=intval, -L intval\n"
"      this if for finetuning only. It specifies the buffer size to\n"
"      use for data read from the network, or data written to the\n"
"      network, respectively. The actual fill of the buffers while\n"
"      the program is running can be checked with 'netstat -tpn'.\n"
"      Usually the operation system chooses sensible values itself.\n"
"\n"
"  --shift=intval, -x intval\n"
"      sleeping to some specific time has limited precision (depending\n"
"      on many parameters). This paramter appends a loop after each sleep\n"
"      which runs until the given wakeup time plus intval ns. This can make\n"
"      the intervals between writes more precise.\n"
"      Default is 0, so no extra loops.\n"
"\n"
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
"  Sending stereo audio data (192/32 format) from a filter program to\n"
"  a remote machine, using a small buffer and smallest possible input\n"
"  chunks (here 32 bit means 4 byte per channel and sample, so we have\n"
"  2x4x192000 = 1536000 bytes of audio data per second):\n"
"\n"
"  ...(filter)... | bufhrt --port-to-write 5888 \\\n"
"                    --bytes-per-second=1536000 --loops-per-second=2000 \\\n"
"                    --buffer-size=8096\n"
"\n"
"  A network buffer, reading from and writing to network:\n"
"\n"
"  bufhrt --host-to-read=myserver --port-to-read=5888 \\\n"
"               --buffer-size=20000 --bytes-per-second=1536000 \\\n"
"               --loops-per-second=2000 --port-to-write=5999\n"
"\n"
"  We use interval mode to copy music files to a hard disk. (Yes, different\n"
"  copies of a music file on the same disk can sound differently . . .):\n"
"\n"
"  bufhrt --interval --file music.flac --outfile=music_better.flac \\\n"
"         --buffer-size=536870912 --loops-per-second=1024 \\\n"
"         --bytes-per-second=8290304 --number-copies=32 \\\n"
"         --dsyncs-per-second=32\n"
"\n"
"  And here is an example how 'bufhrt' can work together with 'writeloop'\n"
"  (note that the memory file size is 6 * 1536, 6 times the data to be\n"
"  written per loop).\n"
"\n"
"  ...(filter)... | writeloop --block-size=1536 \\\n"
"                             --file-size=9216 --shared /bl1 /bl2 /bl3 &\n"
"\n"
"  bufhrt --bytes-per-second=1536000 --loops-per-second=1000 \\\n"
"         --port-to-write=5999 --extra-bytes-per-second=12 \\\n"
"         --shared /bl1 /bl2 /bl3\n"
"\n"
  );
}

inline long difftimens(struct timespec t1, struct timespec t2)
{ 
   long long l1, l2;
   l1 = t1.tv_sec*1000000000 + t1.tv_nsec;
   l2 = t2.tv_sec*1000000000 + t2.tv_nsec;
   return (long)(l2-l1);
}

/* after calibration nsloop will take cnt ns */
double nsloopfactor = 1.0;

static inline long  nsloop(long cnt) {
  long i, j;
  for (j = 1, i = (long)(cnt*nsloopfactor); i > 0; i--) j = j+i;
  return j;
}

int main(int argc, char *argv[])
{
    struct sockaddr_in serv_addr;
    int listenfd, connfd, ifd, s, moreinput, optval=1, verbose, rate,
        bytesperframe, optc, interval, shared, innetbufsize, nrcp,
        outnetbufsize, dsync, tcpnodelay, flag;
    long blen, hlen, ilen, olen, outpersec, loopspersec, nsec, count, wnext,
         badreads, badreadbytes, badwrites, badwritebytes, lcount, 
         dcount, dsyncfreq, fsize, e, a, outtime, outcopies, rambps, ramlps, 
         ramtime, ramchunk, shift;
    long long icount, ocount;
    void *buf, *iptr, *optr, *max;
    char *port, *inhost, *inport, *outfile, *infile, *ptmp, *tbuf;
    void *obufs[1024];
    struct timespec mtime, mtime1, ttime;
    double looperr, extraerr, extrabps, off, dsyncpersec;
    /* variables for shared memory input */
    char **fname, *fnames[100], **tmpname, *tmpnames[100], **mem, *mems[100],
         *ptr;
    sem_t **sem, *sems[100], **semw, *semsw[100];
    int fd[100], i, k, flen, size, c, sz;
    struct stat sb;

    /* read command line options */
    static struct option longoptions[] = {
        {"port-to-write", required_argument,       0,  'p' },
        /* for backward compatibility */
        {"port", required_argument,       0,  'p' },
        {"outfile", required_argument, 0, 'o' },
        {"buffer-size", required_argument,       0,  'b' },
        {"input-size",  required_argument, 0, 'i'},
        {"loops-per-second", required_argument, 0,  'n' },
        {"bytes-per-second", required_argument, 0,  'm' },
        {"ram-loops-per-second", required_argument, 0,  'X' },
        {"ram-bytes-per-second", required_argument, 0,  'Y' },
        {"dsyncs-per-second", required_argument, 0,  'D' },
        {"sample-rate", required_argument, 0,  's' },
        {"sample-format", required_argument, 0, 'f' },
        {"dsync", no_argument, 0, 'd' },
        {"file", required_argument, 0, 'F' },
        {"shift", required_argument, 0, 'x' },
        {"number-copies", required_argument, 0, 'R' },
        {"out-copies", required_argument, 0, 'c' },
        {"host-to-read", required_argument, 0, 'H' },
        {"port-to-read", required_argument, 0, 'P' },
        {"stdin", no_argument, 0, 'S' },
        {"shared", no_argument, 0, 'M' },
        {"extra-bytes-per-second", required_argument, 0, 'e' },
        {"in-net-buffer-size", required_argument, 0, 'K' },
        {"out-net-buffer-size", required_argument, 0, 'L' },
        {"tcp-nodelay", no_argument, 0, 'T' },
        {"overwrite", required_argument, 0, 'O' }, /* not used, ignored */
        {"interval", no_argument, 0, 'I' },
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
    flag = 1;
    port = NULL;
    dsync = 0;
    dsyncpersec = 0;
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
    ramtime = 0;
    ramchunk = 0;
    outcopies = 0;
    outtime = 0;
    rate = 0;
    bytesperframe = 0;
    inhost = NULL;
    inport = NULL;
    infile = NULL;
    shared = 0;
    interval = 0;
    extrabps = 0.0;
    nrcp = 0;
    innetbufsize = 0;
    outnetbufsize = 0;
    shift = 0;
    verbose = 0;
    tcpnodelay = 0;
    while ((optc = getopt_long(argc, argv, "p:o:b:i:D:n:m:X:Y:s:f:F:R:c:H:P:e:x:TvVIhd",
            longoptions, &optind)) != -1) {
        switch (optc) {
        case 'p':
          port = optarg;
          break;
        case 'd':
          dsync = 1;
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
          if (nrcp < 0 || nrcp > 1000) nrcp = 0;
          break;
        case 'c':
          outcopies = atoi(optarg);
          if (outcopies < 0 || outcopies > 1000) outcopies = 0;
          break;
        case 'f':
          if (strcmp(optarg, "S16_LE")==0) {
             bytesperframe = 4;
          } else if (strcmp(optarg, "S24_LE")==0) {
             bytesperframe = 8;
          } else if (strcmp(optarg, "S24_3LE")==0) {
             bytesperframe = 6;
          } else if (strcmp(optarg, "S32_LE")==0) {
             bytesperframe = 8;
          } else {
             fprintf(stderr, "bufhrt: Sample format %s not recognized.\n", optarg);
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
          shared = 1;
          break;
        case 'e':
          extrabps = atof(optarg);
          break;
        case 'D':
          dsyncpersec = atof(optarg);
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
          verbose = 1;
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
    if (outfile) {
        connfd = open(outfile, O_WRONLY | O_CREAT | O_NOATIME, 00644);
        if (connfd == -1) {
            fprintf(stderr, "bufhrt: Cannot open output file %s.\n   %s\n",
                             outfile, strerror(errno));
            exit(3);
        }
    }
    if (infile) {
        if ((ifd = open(infile, O_RDONLY | O_NOATIME)) == -1) {
            fprintf(stderr, "bufhrt: Cannot open input file %s: %s.\n", 
                            infile, strerror(errno));
            exit(2);
        }
    }
    /* translate --dsync */
    if (dsync) dsyncfreq = 1;
    if (dsyncpersec) dsyncfreq = (long) (loopspersec/dsyncpersec);
    if (outpersec == 0) {
       if (rate != 0 && bytesperframe != 0) {
           outpersec = rate * bytesperframe;
       } else {
           fprintf(stderr, "bufhrt: Specify --bytes-per-second (or rate and "
                           "format of audio data).\n");
           exit(5);
       }
    }
    if (inhost != NULL && inport != NULL) {
       ifd = fd_net(inhost, inport);
        if (innetbufsize != 0  &&
            setsockopt(ifd, SOL_SOCKET, SO_RCVBUF, (void*)&innetbufsize, sizeof(int)) < 0) {
                fprintf(stderr, "bufhrt: Cannot set buffer size for network socket to %d.\n",
                        innetbufsize);
                exit(23);
        }
        if (tcpnodelay != 0 && setsockopt(ifd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));    
        {  
            fprintf(stderr, "bufhrt: set TCP_NODELAY failed! \n");
            exit(31);
        }
    }
    if (ramlps != 0 && rambps != 0) {
        ramtime = 1000000000/(2*ramlps);
        ramchunk = rambps/ramlps;
        while (ramchunk % 16 != 0) ramchunk++;
        if (ramchunk > TBUF) ramchunk = TBUF;
    }
    if (verbose) {
       fprintf(stderr, "bufhrt: Writing %ld", outpersec);
       if (extrabps < 0.0)
           fprintf(stderr, "%.1lf", extrabps);
       else if (extrabps > 0.0)
           fprintf(stderr, "+%.1lf", extrabps);
       fprintf(stderr, " bytes per second to ");
       if (port != NULL)
          fprintf(stderr, "port %s.\n", port);
       else if (connfd == 1)
          fprintf(stderr, "stdout.\n");
       else
          fprintf(stderr, " file %s.\n", outfile);
       fprintf(stderr, "bufhrt: Input from ");
       if (shared)
          fprintf(stderr, "shared memory");
       else if (ifd == 0)
          fprintf(stderr, "stdin");
       else if (inhost != NULL)
          fprintf (stderr, "host %s (port %s)", inhost, inport);
       else
          fprintf(stderr, "file %s", infile);
       fprintf(stderr, ", output in %ld loops per second.\n", loopspersec);
       if (dsyncfreq)
          fprintf(stderr, "Data sync'ed every %ld loops.\n", dsyncfreq);
    }

    /* avoid waiting 50000 ns collecting more sleep requests */
    prctl(PR_SET_TIMERSLACK, 1L);

    /* calibrate sleep loop */
    clock_gettime(CLOCK_MONOTONIC, &mtime);
    nsloop(10000000);
    clock_gettime(CLOCK_MONOTONIC, &ttime);
    nsloopfactor = 1.0*10000000/(difftimens(mtime, ttime)-50);

    extraerr = 1.0*outpersec/(outpersec+extrabps);
    nsec = (int) (1000000000*extraerr/loopspersec);
    olen = outpersec/loopspersec;
    if (olen <= 0)
        olen = 1;
    if (interval) {
        if (ilen == 0)
            ilen = 16384;
    }
    else if (ilen < olen) {
        ilen = olen + 2;
        if (olen*loopspersec == outpersec)
            ilen = olen;
        else
            ilen = olen + 1;
        if (verbose) {
           fprintf(stderr, "bufhrt: Choosing %ld as length of input chunks.\n", ilen);
           fflush(stderr);
        }
    }
    if (blen < 3*(ilen+olen))
        blen = 3*(ilen+olen);
    hlen = blen/2;
    if (olen*loopspersec == outpersec)
        looperr = 0.0;
    else
        looperr = (1.0*outpersec)/loopspersec - 1.0*olen;
    moreinput = 1;
    icount = 0;
    ocount = 0;

    /* we want buf % 8 = 0 */
    if (! (buf = malloc(blen+ilen+2*olen+8)) ) {
        fprintf(stderr, "bufhrt: Cannot allocate buffer of length %ld.\n",
                blen+ilen+olen);
        exit(6);
    }
    while (((uintptr_t)buf % 8) != 0) buf++;
    buf = buf + 2*olen;
    max = buf + blen;
    iptr = buf;
    optr = buf;

    /* buffers for loop of output copies */
    if (outcopies > 0) {
        /* spend half of loop duration with copies of output chunk */
        outtime = nsec/(8*outcopies);
        for (i=1; i < outcopies; i++) {
            if (posix_memalign(obufs+i, 4096, 2*olen)) {
                fprintf(stderr, "bufhrt: Cannot allocate buffer for output cleaning.\n");
                exit(20);
            }
        }
    }

    /* outgoing socket */
    if (port != 0) {
        listenfd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenfd < 0) {
            fprintf(stderr, "bufhrt: Cannot create outgoing socket.\n");
            exit(9);
        }
        if (setsockopt(listenfd,
                       SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(int)) == -1)
        {
            fprintf(stderr, "bufhrt: Cannot set REUSEADDR.\n");
            exit(10);
        }
        if (outnetbufsize != 0 && setsockopt(listenfd,
                       SOL_SOCKET,SO_SNDBUF,&outnetbufsize,sizeof(int)) == -1)
        {
            fprintf(stderr, "bufhrt: Cannot set outgoing network buffer to %d.\n",
                    outnetbufsize);
            exit(30);
        }
        if (tcpnodelay != 0 && setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));    
        {  
            fprintf(stderr, "bufhrt: set TCP_NODELAY failed! \n");
            exit(31);
        }
        memset(&serv_addr, '0', sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        serv_addr.sin_port = htons(atoi(port));
        if (bind(listenfd, (struct sockaddr*)&serv_addr,
                                              sizeof(serv_addr)) == -1) {
            fprintf(stderr, "bufhrt: Cannot bind outgoing socket.\n");
            exit(11);
        }
        listen(listenfd, 1);
        if ((connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1) {
            fprintf(stderr, "bufhrt: Cannot accept outgoing connection.\n");
            exit(12);
        }
    }
    /* shared memory input */
    if (shared) {
      size = 0;
      badwrites = 0;
      badwritebytes = 0;
      /* prepare shared memory */
      for (i=optind; i < argc; i++) {
         if (i>100) {
            fprintf(stderr, "bufhrt: Too many filenames.");
            exit(6);
         }
         fnames[i-optind] = argv[i];
         if (shared) {
             /* open semaphore with same name as memory */
             if ((sems[i-optind] = sem_open(fnames[i-optind], O_RDWR))
                                                          == SEM_FAILED) {
                 fprintf(stderr, "bufhrt: Cannot open semaphore.");
                 exit(20);
             }
             /* also semaphore for write lock */
             tmpnames[i-optind] = (char*)malloc(strlen(argv[i])+5);
             strncpy(tmpnames[i-optind], fnames[i-optind], strlen(argv[i]));
             strncat(tmpnames[i-optind], ".TMP", 5);
             if ((semsw[i-optind] = sem_open(tmpnames[i-optind], O_RDWR))
                                                           == SEM_FAILED) {
                 fprintf(stderr, "bufhrt: Cannot open write semaphore.");
                 exit(21);
             }
             /* open shared memory */
             if ((fd[i-optind] = shm_open(fnames[i-optind],
                                 O_RDWR, S_IRUSR | S_IWUSR)) == -1){
                 fprintf(stderr, "bufhrt: Cannot open shared memory %s.\n", fnames[i-optind]);
                 exit(22);
             }
             if (size == 0) { /* find size of shared memory chunks */
                 if (fstat(fd[i-optind], &sb) == -1) {
                     fprintf(stderr, "bufhrt: Cannot stat shared memory %s.\n", fnames[i-optind]);
                     exit(24);
                 }
                 size = sb.st_size - sizeof(int);
             }
             /* map the memory (will be on page boundary, so 0 mod 8) */
             mems[i-optind] = mmap(NULL, sizeof(int)+size,
                             PROT_WRITE | PROT_READ, MAP_SHARED, fd[i-optind], 0);
             if (mems[i-optind] == MAP_FAILED) {
                 fprintf(stderr, "bufhrt: Cannot map shared memory.");
                 exit(24);
             }
         }
      }
      fnames[argc-optind] = NULL;
      fname = fnames;
      tmpname = tmpnames;
      mem = mems;
      sem = sems;
      semw = semsw;
      clock_gettime(CLOCK_MONOTONIC, &mtime);
      lcount = 0;
      off = looperr;
      i = 0; /* counter to update mtime */
      while (1) {
         i++;
         if (*fname == NULL) {
            fname = fnames;
            tmpname = tmpnames;
            mem = mems;
            sem = sems;
            semw = semsw;
         }
         /* get lock */
         sem_wait(*sem);
         /* find length of relevant memory chunk */
         flen = *((int*)(*mem));
         icount += flen;
         if (flen == 0) {
             /* done, unlink semaphores and shared memory */
             fname = fnames;
             tmpname = tmpnames;
             while (*fname != NULL) {
                 shm_unlink(*fname);
                 sem_unlink(*fname);
                 sem_unlink(*tmpname);
                 fname++;
                 tmpname++;
             }
             exit(0);
         }
         /* write shared memory content to output */
         ptr = *mem + sizeof(int);
         sz = 0;
         if (i == 100)
           clock_gettime(CLOCK_MONOTONIC, &mtime);
         while (sz < flen) {
             mtime.tv_nsec += nsec;
             if (mtime.tv_nsec > 999999999) {
               mtime.tv_nsec -= 1000000000;
               mtime.tv_sec++;
             }
             if (flen - sz <= olen) {
                 c = flen - sz;
                 off += (olen-c);
             } else {
                 c = olen;
                 if (off >= 1.0) {
                    off -= 1.0;
                    c++;
                 }
             }
             refreshmem((char*)ptr, c);
             while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                                                &mtime, NULL)
                    != 0) ;
             /* write a chunk, this comes first after waking from sleep */
             s = write(connfd, ptr, c);
             if (s < 0) {
                 fprintf(stderr, "bufhrt (from shared): Write error: %s.\n",
                                 strerror(errno));
                 exit(15);
             }
             if (s < c) {
                 badwrites++;
                 badwritebytes += (c-s);
                 off += (c-s);
             }
             ocount += c;
             ptr += c;
             sz += c;
             lcount++;
             off += looperr;
         }
         /* mark as writable */
         sem_post(*semw);
         fname++;
         tmpname++;
         mem++;
         sem++;
         semw++;
      }
      close(connfd);
      shutdown(listenfd, SHUT_RDWR);
      close(listenfd);
      if (verbose)
        fprintf(stderr, "bufhrt: Loops: %ld, total bytes: %lld in (shared mem) %lld out.\n"
                        "bufhrt: bad writes: %ld (%ld bytes)\n",
                        lcount, icount, ocount, badwrites, badwritebytes);
      return 0;
    }

    /* interval mode */
    if (interval) {
       tbuf = NULL;
       if (nrcp > 0) {
           /* page size aligned temporary buffer */
           ptmp = (char *)malloc(TBUF+4096);
           for (tbuf = ptmp; (long)tbuf % 4096 != 0; tbuf++);
       }
       count = 0;
       /* counter for fdatasync */
       dcount = 0;
       while (moreinput) {
          count++;
          /* fill buffer */
          memclean(buf, 2*hlen);
          for (iptr = buf; iptr < buf + 2*hlen - ilen; ) {
              s = read(ifd, iptr, ilen);
              if (s < 0) {
                  fprintf(stderr, "bufhrt: Read error.\n");
                  exit(18);
              }
              icount += s;
              if (s == 0) {
                  moreinput = 0;
                  break;
              }
              iptr += s;
          }

          /* maybe make refreshed copies */
          if (nrcp > 0 && ramchunk > 0) {
              fsize = iptr - buf;
              for (i=nrcp;  i; i--) {
                   clock_gettime(CLOCK_MONOTONIC, &mtime);
                   for (a = 0, e = a+ramchunk; a < fsize; a += ramchunk, 
                                                          e += ramchunk) {
                       if (e > fsize) e = fsize;
                       mtime.tv_nsec += ramtime;
                       if (mtime.tv_nsec > 999999999) {
                         mtime.tv_nsec -= 1000000000;
                         mtime.tv_sec++;
                       }
                       while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                                       &mtime, NULL) != 0) ;
                       clock_gettime(CLOCK_MONOTONIC, &ttime);
                       nsloop(shift - difftimens(mtime, ttime));
                       memclean(tbuf, e-a);
                       cprefresh(tbuf, buf+a, e-a);
                       mtime.tv_nsec += ramtime;
                       if (mtime.tv_nsec > 999999999) {
                         mtime.tv_nsec -= 1000000000;
                         mtime.tv_sec++;
                       }
                       while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                                       &mtime, NULL) != 0) ;
                       clock_gettime(CLOCK_MONOTONIC, &ttime);
                       nsloop(shift - difftimens(mtime, ttime));
                       memclean(buf+a, e-a);
                       cprefresh(buf+a, tbuf, e-a);
                   }
              }
          }
          if (nrcp > 0 && ramchunk == 0) {
              fsize = iptr - buf;
              for (i=nrcp;  i; i--) {
                   for (a = 0, e = a+TBUF; a < fsize; a += TBUF, e += TBUF) {
                       if (e > fsize) e = fsize;
                       memclean(tbuf, e-a);
                       cprefresh(tbuf, buf+a, e-a);
                       memclean(buf+a, e-a);
                       cprefresh(buf+a, tbuf, e-a);
                   }
              }
          }

          /* write out */
          optr = buf;
          wnext = (iptr - optr <= olen) ? (iptr - optr) : olen;
          clock_gettime(CLOCK_MONOTONIC, &mtime1);
          clock_gettime(CLOCK_MONOTONIC, &mtime);
          for (lcount=0, off=looperr; optr < iptr; lcount++, off+=looperr) {
              /* once cache is filled and other side is reading we reset time */
              if (lcount == 50) clock_gettime(CLOCK_MONOTONIC, &mtime);
              mtime.tv_nsec += nsec;
              if (mtime.tv_nsec > 999999999) {
                mtime.tv_nsec -= 1000000000;
                mtime.tv_sec++;
              }
              clock_gettime(CLOCK_MONOTONIC, &mtime1);
              if ((mtime1.tv_sec > mtime.tv_sec)
                  || (mtime1.tv_sec == mtime.tv_sec
                      && mtime1.tv_nsec > mtime.tv_nsec)) {
                 if (verbose)
                      fprintf(stderr, "bufhrt: delayed block %ld\n", lcount);
                 /* reset to current time + 0.3 sec */
                 clock_gettime(CLOCK_MONOTONIC, &mtime);
                 mtime.tv_nsec += (300000000);
                 if (mtime.tv_nsec > 999999999) {
                   mtime.tv_nsec -= 1000000000;
                   mtime.tv_sec++;
                 }
              }
              /* refresh output chunk before sleep */
              if (outcopies > 0) {
                  obufs[0] = optr;
                  obufs[outcopies] = optr;
                  for (k=1; k <= outcopies; k++) {
                      nsloop(outtime);
                      memclean((char*)(obufs[k]), wnext);
                      cprefresh((char*)(obufs[k]), (char*)(obufs[k-1]), wnext);
                      memclean((char*)(obufs[k-1]), wnext);
                  }
              } else {
                  refreshmem((char*)optr, wnext);
              }
              while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                                                 &mtime, NULL)
                     != 0) ;
              /* write at mtime + shift ns */
              clock_gettime(CLOCK_MONOTONIC, &ttime);
              nsloop(shift - difftimens(mtime, ttime));
              /* write a chunk, this comes first after waking from sleep */
              s = write(connfd, optr, wnext);
              if (s < 0) {
                  fprintf(stderr, "bufhrt: Write error.\n");
                  exit(15);
              }
              if (dsyncfreq) {
                  dcount++;
                  if (dcount == dsyncfreq) {
                     fdatasync(connfd);
                     dcount = 0;
                     /* duration of sync depends on hardware, reset timer */
                     clock_gettime(CLOCK_MONOTONIC, &mtime);
                  }
              }
              ocount += s;
              optr += s;
              wnext = olen + wnext - s;
              if (off >= 1.0) {
                 off -= 1.0;
                 wnext++;
              }
              if (wnext >= 2*olen) {
                 fprintf(stderr, "bufhrt: Underrun by %ld (%ld sec %ld nsec).\n",
                           wnext - 2*olen, mtime.tv_sec, mtime.tv_nsec);
                 wnext = 2*olen-1;
              }
              s = iptr - optr;
              if (s <= wnext) {
                  wnext = s;
              }
          }
          fdatasync(connfd);
       }

       if (outfile)
           posix_fadvise(connfd, 0, 0, POSIX_FADV_DONTNEED);
       close(connfd);
       shutdown(listenfd, SHUT_RDWR);
       close(listenfd);
       close(ifd);
       if (verbose)
           fprintf(stderr, "bufhrt: Intervals: %ld, total bytes: %lld in %lld out.\n",
                            count, icount, ocount);
       exit(0);
    }

    /* default mode, no shared memory input and no interval mode */
    /* fill at least half buffer */
    memclean(buf, 2*hlen);
    for (; iptr < buf + 2*hlen - ilen; ) {
        s = read(ifd, iptr, ilen);
        if (s < 0) {
            fprintf(stderr, "bufhrt: Read error.\n");
            exit(13);
        }
        icount += s;
        if (s == 0) {
            moreinput = 0;
            break;
        }
        iptr += s;
    }
    if (iptr - optr < olen)
        wnext = iptr-optr;
    else
        wnext = olen;

    if (clock_gettime(CLOCK_MONOTONIC, &mtime) < 0) {
        fprintf(stderr, "bufhrt: Cannot get monotonic clock.\n");
        exit(14);
    }
    if (verbose) {
        fprintf(stderr, "bufhrt: Starting at %ld sec %ld nsec,\n",
                                           mtime.tv_sec, mtime.tv_nsec);
        fprintf(stderr,
                "bufhrt:    insize %ld, outsize %ld, buflen %ld, interval %ld nsec\n",
                                     ilen, olen, blen+ilen+2*olen, nsec);
    }

    /* main loop */
    badreads = 0;
    badwrites = 0;
    badreadbytes = 0;
    badwritebytes = 0;
    for (count=1, off=looperr; 1; count++, off+=looperr) {
        /* once cache is filled and other side is reading we reset time */
        if (count == 500) clock_gettime(CLOCK_MONOTONIC, &mtime);
        mtime.tv_nsec += nsec;
        if (mtime.tv_nsec > 999999999) {
          mtime.tv_nsec -= 1000000000;
          mtime.tv_sec++;
        }
        refreshmem((char*)optr, wnext);
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &mtime, NULL)
               != 0) ;
        /* write a chunk, this comes first after waking from sleep */
        s = write(connfd, optr, wnext);
        if (s < 0) {
            fprintf(stderr, "bufhrt: Write error.\n");
            exit(15);
        }
        if (s < wnext) {
            badwrites++;
            badwritebytes += (wnext-s);
        }
        ocount += s;
        optr += s;
        wnext = olen + wnext - s;
        if (off >= 1.0) {
           off -= 1.0;
           wnext++;
        }
        if (wnext >= 2*olen) {
           fprintf(stderr, "bufhrt: Underrun by %ld (%ld sec %ld nsec).\n",
                     wnext - 2*olen, mtime.tv_sec, mtime.tv_nsec);
           wnext = 2*olen-1;
        }
        s = (iptr >= optr ? iptr - optr : iptr+blen-optr);
        if (s <= wnext) {
            wnext = s;
        }
        if (optr+wnext >= max) {
            optr -= blen;
        }
        /* read if buffer not half filled */
        if (moreinput && (iptr > optr ? iptr-optr : iptr+blen-optr) < hlen) {
            memclean(iptr, ilen);
            s = read(ifd, iptr, ilen);
            if (s < 0) {
                fprintf(stderr, "bufhrt: Read error.\n");
                exit(16);
            }
            if (s < ilen) {
                badreads++;
                badreadbytes += (ilen-s);
            }
            icount += s;
            iptr += s;
            if (iptr >= max) {
                memcpy(buf-2*olen, max-2*olen, iptr-max+2*olen);
                iptr -= blen;
            }
            if (s == 0) { /* input complete */
                moreinput = 0;
            }
        }
        if (wnext == 0)
            break;    /* done */
    }
    close(connfd);
    shutdown(listenfd, SHUT_RDWR);
    close(listenfd);
    close(ifd);
    if (verbose)
        fprintf(stderr, "bufhrt: Loops: %ld, total bytes: %lld in %lld out.\n"
                        "bufhrt: Bad reads/bytes %ld/%ld and writes/bytes %ld/%ld.\n",
                        count, icount, ocount, badreads, badreadbytes,
                        badwrites, badwritebytes);
    return 0;
}

