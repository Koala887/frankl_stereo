/* reads 2-channel audio in 64 bit float samples and output every
   fourth frame as int32 samples */
/* we use this to reduce sample rate 192000 to 48000 with a signal
   that has no content above 24kHz */
#include "version.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include "cprefresh.h"

int main(int argc, char *argv[])
{
    double inp[1024], *ip;
    int32_t out[256], *op;
    int i, mlen, nrcp;
    void *tbufs[1024];

    nrcp = 0;
    if (argc == 2) {
        nrcp = atoi(argv[1]);
        if (nrcp < 0 || nrcp > 5000)
            nrcp = 0;
    }
    if (nrcp) {
        /* temporary buffers */
        for (i=1; i < nrcp; i++) {
            if (posix_memalign(tbufs+i, 4096, 4096)) {
                fprintf(stderr, "cptoshm: Cannot allocate buffer for cleaning.\n");
                exit(2);
            }
        }
    }
    tbufs[0] = out;
    tbufs[nrcp] = out;


    while (1) {
       memclean((char*)inp, 8192);  
       mlen = fread((void*)inp, sizeof(double), 1024, stdin);
       if (mlen == 0) break;
       memclean((char*)out, 1024);
       for (i=mlen/8, ip=inp, op=out; i; ip+=8, op+=2, i--) {
         *op = (int32_t) (*ip * 2147483647);
         *(op+1) = (int32_t) (*(ip+1) * 2147483647);
       }
       if (nrcp) {
           for (i=1; i <= nrcp; i++) {
               memclean((char*)(tbufs[i]), mlen);
               cprefresh((char*)(tbufs[i]), (char*)(tbufs[i-1]), mlen);
               memclean((char*)(tbufs[i-1]), mlen);
           }
       }
       fwrite((void*)out, sizeof(int32_t), mlen/4, stdout);
    }
    return 0;
}
