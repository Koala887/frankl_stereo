/*
writeloop.c                Copyright frankl 2013-2015

This file is part of frankl's stereo utilities.
See the file License.txt of the distribution and
http://www.gnu.org/licenses/gpl.txt for license details.
*/


#define _GNU_SOURCE
#include "version.h"
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include "cprefresh.h"
#include "nf_io.h"

/* help page */
/* vim hint to remove resp. add quotes:
      s/^"\(.*\)\\n"$/\1/
      s/.*$/"\0\\n"/
*/
void usage( ) {
  fprintf(stderr,
          "writeloop (version %s of frankl's stereo utilities)\nUSAGE:\n",
          VERSION);
  fprintf(stderr,
"  writeloop --block-size=<bsize> --file-size=<fsize> <file1> <file2> [<file3>..]\n"
"\n"
"  This program 'writeloop' reads data from stdin or a file and writes them \n"
"  cyclically into the files whose names <file1>, ..., are given on the \n"
"  command line. (If the next file to be written exists then the program \n"
"  will wait until it is deleted.)\n"
"\n"
"  Some overhead can be saved by using shared memory files instead of files\n"
"  in a file system. See the --shared option below.\n"
"\n"
"  USAGE HINTS\n"
"  \n"
"  This can be used together with 'catloop' which does the converse\n"
"  (reading the files cyclically and writing a stream to stdout).\n"
"\n"
"  The given files should be on a ramdisk. writeloop/catloop provide\n"
"  a buffer for data read from a hard disk or from the network.\n"
"  \n"
"  OPTIONS\n"
"\n"
"  --block-size=intval, -b intval\n"
"      the size in bytes of the data chunks written to the given filenames.\n"
"      Default is 2000 bytes.\n"
"\n"
"  --file-size=intval, -f intval\n"
"      the maximal size of the written files (default is 64000).\n"
"  \n"
"  --from-file=fname,  -F fname\n"
"      read data from file fname instead of stdin.\n"
"\n"
"  --shared, -s\n"
"      use named shared memory instead of files. The convention is that\n"
"      the given names start with a slash like '/file1'.\n"
"      For large amounts of shared memory you may need to enlarge\n"
"      '/proc/sys/kernel/shmmax' directly or via sysctl.\n"
"\n"
"  --nfinfo=nfnam, -M nfnam\n"
"      with this and the following option input can be read from an nf file,\n"
"      this option specifies the nf info file.\n"
"\n"
"  --nfid=id, -N id\n"
"      this is the nf id of the file to read, the id must be found in the \n"
"      nf info file specified by the previous option.\n"
"\n"
"  --force-shm, -x\n"
"      if --shared is used writeloop may fail to start if 'semaphores' are\n"
"      left over from former calls or other programs. With this option these\n"
"      are overwritten.\n"
"\n"
"  --skip=intval, -S intval\n"
"      if input is a file then skip intval bytes at the beginning, the \n"
"      argument is rounded down to a multiple of 8.\n"
"\n"
"  --number-copies=intnum, -R intnum\n"
"      before writing data they are copied the specfied number of\n"
"      times through  cleaned temporary buffers in RAM.\n"
"      The default is 0. \n"
"\n"
"  --verbose, -v\n"
"      print some information during startup.\n"
"\n"
"  --version, -V\n"
"      print information about the version of the program and abort.\n"
"\n"
"  --help, -h\n"
"      print this help page and abort.\n"
"\n"
"  EXAMPLE\n"
"  \n"
"  If you want to play an audio file (CD format) on a hard disk or on a \n"
"  network file system, you can use writeloop and catloop as a buffer:\n"
"\n"
"       sox /my/disk/cd.flac -t raw | writeloop --block-size=4000 \\\n"
"                --file-size=20000 /ramdisk/aa /ramdisk/bb /ramdisk/cc &\n"
"\n"
"  and \n"
"       catloop --block-size=1000 /ramdisk/aa /ramdisk/bb /ramdisk/cc | \\\n"
"             aplay -t raw -f S16_LE -c 2 -r44100 -D \"hw:0,0\" \n"
"\n"
"  Or similarly, using less system resources, with shared memory:\n"
"\n"
"       sox /my/disk/cd.flac -t raw | writeloop --block-size=4000 \\\n"
"                --file-size=20000 --shared /aa /bb /cc &\n"
"\n"
"  and \n"
"       catloop --block-size=1000 --shared /aa /bb /cc | \\\n"
"             aplay -t raw -f S16_LE -c 2 -r44100 -D \"hw:0,0\" \n"
"\n"
"     \n"
"  In experiments I found that audio playback was improved compared to a \n"
"  direct playing. For larger file sizes (a few megabytes) the effect was\n"
"  similar to copying the file into RAM and playing that file.\n"
"  But even better was the effect with small file sizes (a few kilobytes)\n"
"  such that all files fit into the processor cache. \n"
"\n"
);
}



int main(int argc, char *argv[])
{
    char **fname, *fnames[100], **tmpname, *tmpnames[100], **mem, *mems[100],
         *ptr, *nfinfo;
    sem_t **sem, *sems[100], **semw, *semsw[100];
    void *buf, *tbufs[1024];
    long nfid;
    int outfile, fd[100], inp, i, shared, verbose, force, blocksize,
        semflag, size, ret, sz, c, optc, nrcp;
    off_t skip, checkskip;
    struct nfrec *nfr;

    /* read command line options */
    static struct option longoptions[] = {
        {"block-size", required_argument, 0,  'b' },
        {"from-file", required_argument, 0, 'F' },
        {"file-size", required_argument,       0,  'f' },
        {"skip", required_argument,       0,  'S' },
        {"shared", no_argument, 0, 's' },
        {"force-shm", no_argument, 0, 'x' },
        {"nfinfo", required_argument, 0, 'M' },
        {"nfid", required_argument, 0, 'N' },
        {"number-copies", required_argument, 0, 'R' },
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
    blocksize = 2000;
    size = 64000;
    shared = 0;
    verbose = 0;
    semflag = O_CREAT | O_EXCL;
    force = 0;
    nfinfo = NULL;
    nfid = -2;
    inp = 0;  /* stdin */
    skip = 0;
    nrcp = 0;
    buf = NULL;
    while ((optc = getopt_long(argc, argv, "b:f:F:M:N:R:sVh",
            longoptions, &optind)) != -1) {
        switch (optc) {
        case 'b':
          blocksize = atoi(optarg);
          break;
        case 'f':
          size = atoi(optarg);
          break;
        case 'F':
          if ((inp = open(optarg, O_RDONLY)) == -1) {
            fprintf(stderr, "writeloop: Cannot open input file %s.\n", optarg);
            exit(3);
          }
          break;
        case 'S':
          skip = (off_t)atoll(optarg);
          while ((skip % 8) != 0)
            skip--;
          if (skip < 0)
            skip = 0;
          break;
        case 's':
          shared = 1;
          break;
        case 'x':
          force = 1;
          break;
        case 'M':
          nfinfo = strdup(optarg);
          break;
        case 'N':
          nfid = atoi(optarg);
          break;
        case 'R':
          nrcp = atoi(optarg);
          if (nrcp < 0 || nrcp > 1000) nrcp = 0;
          break;
        case 'v':
          verbose = 1;
          break;
        case 'V':
          fprintf(stderr,
                  "writeloop (version %s of frankl's stereo utilities)\n",
                  VERSION);
          exit(0);
        default:
          usage();
          exit(2);
        }
    }
    if (skip && inp) { /* seek if skip is nonzero and inp is a file */
        checkskip = lseek(inp, skip, SEEK_SET);
        if (checkskip != skip) {
          fprintf(stderr, "writeloop: cannot seek to %lld \n", (long long)skip);
          exit(17);
        }
    }
    if (blocksize > size) {
        fprintf(stderr, "writeloop: Block size must be smaller than file size.\n");
        exit(3);
    }
    posix_memalign(&buf, 4096, blocksize);
    if (! buf) {
       fprintf(stderr, "writeloop: Cannot allocate buffer.\n");
       exit(4);
    }
    if (argc-optind < 1) {
       fprintf(stderr, "writeloop: Specify at least two filenames.\n");
       exit(5);
    }
    if (shared) {
       if (force) 
          semflag = O_CREAT;
       else
          semflag = O_CREAT | O_EXCL;
    }
    if (nrcp && nfinfo == NULL) {
        /* temporary buffers */
        for (i=1; i < nrcp; i++) {
            if (posix_memalign(tbufs+i, 4096, blocksize)) {
                fprintf(stderr, "writeloop: Cannot allocate buffer for cleaning.\n");
                exit(2);
            }
        }
    }

    for (i=optind; i < argc; i++) {
       if (i>100) {
          fprintf(stderr, "writeloop: Too many filenames.");
          exit(6);
       }
       fnames[i-optind] = argv[i];
       if (shared) {
           /* open semaphore with same name as memory */
           if ((sems[i-optind] = sem_open(fnames[i-optind], semflag,
                                                    0666, 0)) == SEM_FAILED) {
               fprintf(stderr, "writeloop: Cannot open semaphore.");
               exit(20);
           }
           /* open shared memory */
           if ((fd[i-optind] = shm_open(fnames[i-optind],
                               O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) == -1){
               fprintf(stderr, "writeloop: Cannot open shared memory %s.\n", fnames[i-optind]);
               exit(22);
           }
           /* truncate to size plus info about used memory */
           if (force && ftruncate(fd[i-optind], 0) == -1) {
               fprintf(stderr, "writeloop: Cannot truncate to 0.");
               exit(23);
           }
           if (ftruncate(fd[i-optind], sizeof(long)+size) == -1) {
               fprintf(stderr, "writeloop: Cannot truncate to %d.", size);
               exit(23);
           }
           /* map the memory */
           mems[i-optind] = mmap(NULL, sizeof(long)+size,
                           PROT_READ | PROT_WRITE, MAP_SHARED, fd[i-optind], 0);
           if (mems[i-optind] == MAP_FAILED) {
               fprintf(stderr, "writeloop: Cannot map shared memory.");
               exit(24);
           }
       } else
           unlink(fnames[i-optind]);

       tmpnames[i-optind] = (char*)malloc(strlen(argv[i])+5);
       strncpy(tmpnames[i-optind], fnames[i-optind], strlen(argv[i])+5);
       strncat(tmpnames[i-optind], ".TMP", 5);
       if (shared) {
           /* open semaphore with TMP name for write lock */
           if ((semsw[i-optind] = sem_open(tmpnames[i-optind], semflag,
                                                    0666, 0)) == SEM_FAILED) {
               fprintf(stderr, "writeloop: Cannot open write semaphore.");
               exit(21);
           }
           sem_post(semsw[i-optind]);
       } else
           unlink(tmpnames[i-optind]);
    }
    fnames[argc-optind] = NULL;
    fname = fnames;
    tmpname = tmpnames;

    if (verbose) {
      if (inp == 0) 
        fprintf(stderr, "writeloop: Reading from stdin, ");
      else if (nfinfo)
        fprintf(stderr, "writeloop: Reading from nf file, ");
      else
        fprintf(stderr, "writeloop: Reading from file, ");
    } 
    if (nfinfo != NULL) {
       /* input from nf info, in that case we also set block size and file size to PS */
        nfr = nfopen(nfinfo, nfid, O_RDONLY, 0);
        if (nfr == NULL) {
            fprintf(stderr, "writeloop: cannot open nf file: %s id=%ld.",
                   nfinfo, nfid);
            exit(15);
        }
        if (skip != 0) {
            nfclose();
            nfr = nfopen(nfinfo, nfid, O_RDONLY, skip);
        }
        size = PS;
        blocksize = PS;
        posix_memalign(&buf, 4096, blocksize);
        if (nrcp) {
            /* temporary buffers */
            for (i=1; i < nrcp; i++) {
                if (posix_memalign(tbufs+i, 4096, blocksize)) {
                    fprintf(stderr, "writeloop: Cannot allocate buffer for cleaning.\n");
                    exit(2);
                }
            }
        }
        sz = (skip/PS)*PS;
        mem = mems;
        sem = sems;
        semw = semsw;
        memclean(buf, blocksize);
        c = nfreadpage(buf);
        //refreshmem((char*)buf, blocksize);
        sz += c;
        while (c > 0) {
           if (*fname == NULL) {
              fname = fnames;
              tmpname = tmpnames;
              mem = mems;
              sem = sems;
              semw = semsw;
           }
           /* get write lock */
           sem_wait(*semw);
           ptr = *mem+sizeof(long);
           memclean(ptr, size);
           cprefresh(ptr, buf, c);
           if (nrcp) {
               tbufs[0] = ptr;
               tbufs[nrcp] = ptr;
               for (i=1; i <= nrcp; i++) {
                   memclean((char*)(tbufs[i]), c);
                   cprefresh((char*)(tbufs[i]), (char*)(tbufs[i-1]), c);
                   memclean((char*)(tbufs[i-1]), c);
               }
           }

           *((long*)(*mem)) = (long)c;
           sem_post(*sem);

           /* read next page */
           memclean(buf, blocksize);
           c = nfreadpage(buf);
           sz += c;
           fname++;
           tmpname++;
           mem++;
           sem++;
           semw++;
        }
        /* done, indicate by empty memory */
        nfclose();
        if (*fname == NULL) {
           fname = fnames;
           tmpname = tmpnames;
           mem = mems;
           sem = sems;
           semw = semsw;
        }
        sem_wait(*semw);
        *((int*)(*mem)) = 0;
        sem_post(*sem);
        exit(0);

    } else if (shared) {
        if (verbose)
          fprintf(stderr, "writing to shared memory.\n");
        mem = mems;
        sem = sems;
        semw = semsw;
        memclean(buf, blocksize);
        c = read(inp, buf, blocksize);
        sz = c;
        while (1) {
           if (*fname == NULL) {
              fname = fnames;
              tmpname = tmpnames;
              mem = mems;
              sem = sems;
              semw = semsw;
           }
           /* get write lock */
           sem_wait(*semw);
           if (c == 0) {
              /* done, indicate by empty memory */
              *((int*)(*mem)) = 0;
              sem_post(*sem);
              exit(0);
           }
           ptr = *mem+sizeof(long);
           while (c > 0 && sz <= size) {
              memclean(ptr, c);
              cprefresh(ptr, buf, c);
              if (nrcp) {
                  tbufs[0] = ptr;
                  tbufs[nrcp] = ptr;
                  for (i=1; i <= nrcp; i++) {
                      memclean((char*)(tbufs[i]), c);
                      cprefresh((char*)(tbufs[i]), (char*)(tbufs[i-1]), c);
                      memclean((char*)(tbufs[i-1]), c);
                  }
              }
              ptr += c;
              memclean(buf, blocksize);
              c = read(inp, buf, blocksize);
              sz += c;
           }
           *((long*)(*mem)) = (long)(sz - c);
           sz = c;
           sem_post(*sem);
           fname++;
           tmpname++;
           mem++;
           sem++;
           semw++;
        }
    } else {
        if (verbose)
          fprintf(stderr, "writing to files.\n");
        sz = 0;
        memclean(buf, blocksize);
        c = read(inp, buf, blocksize);
        sz += c;
        while (1) {
           if (*fname == NULL) {
              fname = fnames;
              tmpname = tmpnames;
           }
           /* take short naps until next file can be written */
           while (access(*fname, R_OK) == 0)
             usleep(50000);
           outfile = open(*tmpname, O_WRONLY|O_CREAT|O_NOATIME, 00444);
           if (!outfile) {
              fprintf(stderr, "writeloop: Cannot open for writing: %s.\n", *fname);
              exit(4);
           }
           if (c == 0) {
              /* done, indicate by empty file */
              close(outfile);
              rename(*tmpname, *fname);
              exit(0);
           }
           while (c > 0 && sz <= size) {
              ret = write(outfile, buf, c);
              if (ret == -1) {
                  fprintf(stderr, "writeloop: Write error: %s\n", strerror(errno));
                  exit(5);
              }
              if (ret < c) {
                  fprintf(stderr, "writeloop: Could not write full buffer.\n");
                  exit(6);
              }
              memclean(buf, blocksize);
              c = read(inp, buf, blocksize);
              sz += c;
           }
           sz = c;
           close(outfile);
           rename(*tmpname, *fname);
           fname++;
           tmpname++;
        }
    }
}

