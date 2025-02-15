/*
highrestest.c                Copyright frankl 2013-2024

This file is part of frankl's stereo utilities. 
See the file License.txt of the distribution and 
http://www.gnu.org/licenses/gpl.txt for license details.
*/
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/prctl.h>
#include <sys/prctl.h>

//#define MYCLOCK CLOCK_MONOTONIC_RAW
#define MYCLOCK CLOCK_MONOTONIC

/* t2 - t1 in nanoseconds */
long difftimens(struct timespec t1, struct timespec t2)
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

/* a simple test of the resolution of several CLOCKs */
int main(int argc, char *argv[]) {
  int ret, highresok, first, nloops, i, k, shift;
  long step, d, min, max, dev, dint, count[21];
  struct timespec res, last, tim, ttime;

  if (argc == 2 && (strcmp(argv[1],"-h") == 0 || strcmp(argv[1],"--help") == 0)) {
     fprintf(stderr, "Usage: no argument - simple test\n  highresttest intv shift - with statistics\n  intv: interval, shift: delay for more precision\n\n");
     return 0;
  }

  printf("----- Testing highres timer:\n");

  /* avoid waiting 50000 ns collecting more sleep requests */
  prctl(PR_SET_TIMERSLACK, 1L);

  ret = clock_getres(CLOCK_REALTIME, &res);
  printf("realtime resolution: %ld s %ld ns (%d)\n", res.tv_sec, res.tv_nsec, ret); 

  ret = clock_gettime(CLOCK_REALTIME, &tim);
  printf("realtime: %ld s %ld ns (%d)\n", tim.tv_sec, tim.tv_nsec, ret); 

  ret = clock_getres(CLOCK_MONOTONIC, &res);
  highresok = (res.tv_sec == 0 && res.tv_nsec == 1);
  printf("monotonic resolution: %ld s %ld ns (%d)\n", res.tv_sec, res.tv_nsec, ret); 

  ret = clock_gettime(CLOCK_MONOTONIC, &tim);
  printf("monotonic: %ld s %ld ns (%d)\n", tim.tv_sec, tim.tv_nsec, ret); 

  ret = clock_getres(CLOCK_PROCESS_CPUTIME_ID, &res);
  printf("process resolution: %ld s %ld ns (%d)\n", res.tv_sec, res.tv_nsec, ret); 

  ret = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tim);
  printf("process: %ld s %ld ns (%d)\n", tim.tv_sec, tim.tv_nsec, ret); 

  if (highresok && argc > 1) {
    
    printf("Measuring actual precision of monotonic clock for 10 seconds ...\n");
    step = 1000000;
    nloops = 10000;
    dint = atoi(argv[1]);
    if (dint <= 0)
      dint = 500;
    min = 0;
    max = 0;
    dev =0;
    clock_gettime(MYCLOCK, &res);
    last = res;
    
    for(i=0; i<21; count[i]=0, i++);
    /* avoid some startup jitter */
    for(first=100, i=0; i < nloops+99; i++) 
    {
      res.tv_nsec = res.tv_nsec+step;
      if (res.tv_nsec > 999999999) {
        res.tv_sec++;
        res.tv_nsec -= 1000000000;
      }
      while (clock_nanosleep(MYCLOCK, TIMER_ABSTIME, &res, NULL) != 0);
      clock_gettime(MYCLOCK, &tim);
      d = difftimens(last, tim)-step;
      k = d/dint;
      if (k < -10) k = -10;
      if (k > 10) k = 10;
      count[k+10]++;
      last = tim;

      //printf("%ld\n", d);
      if (first == 0) {
        if (d < min)
           min = d;
        if (d > max)
           max = d;
        dev += d;
      } else if (first == 1) {
        min = d;
        max = d;
        first = 0;
      } else 
        first--;
    }
    printf("    Min diff: %ld ns, max diff: %ld ns, \n"
           "    avg. diff: %ld ns\n",
           min, max, dev/nloops);
    printf("   diff in ns      count\n");
    printf(" < -10*%ld        %ld\n", dint, count[0]);
    for(i=-9; i<10; i++)
        printf("    %d*%ld        %ld\n", i, dint, count[i+10]);
    printf(" >  10*%ld        %ld\n", dint, count[20]);

    printf("--------------------------------------\n");
    printf("Measuring actual precision with correction loops for 10 seconds ...\n");

    /* calibrate sleep loop */
    clock_gettime(CLOCK_MONOTONIC, &res);
    nsloop(100000000);
    clock_gettime(CLOCK_MONOTONIC, &ttime);
    nsloopfactor = 1.0*100000000/(difftimens(res, ttime)-50);
    printf("nsloopfactor is %lf\n",nsloopfactor);

    step = 1000000;
    nloops = 10000;
    shift = 100000;
    if (argc > 2) 
      shift = atoi(argv[2]);
    if (shift <= 0)
      shift = 100000;
    min = 0;
    max = 0;
    dev =0;
    for(i=0; i<21; count[i]=0, i++);

    clock_gettime(MYCLOCK, &res);
    last = res;
    
    /* avoid some startup jitter */
    for(first=100, i=0; i < nloops+99; i++) 
    {
      res.tv_nsec = res.tv_nsec+step;
      if (res.tv_nsec > 999999999) {
        res.tv_sec++;
        res.tv_nsec -= 1000000000;
      }
      clock_nanosleep(MYCLOCK, TIMER_ABSTIME, &res, NULL);
      clock_gettime(CLOCK_MONOTONIC, &ttime);
      nsloop(shift - difftimens(res, ttime));
      clock_gettime(MYCLOCK, &tim);
      d = difftimens(last, tim)-step;

      last = tim;

      //printf("%ld\n", d);
      if (first == 0) {
        k = d/dint;
        if (k < -10) k = -10;
        if (k > 10) k = 10;
        count[k+10]++;
        if (d < min)
           min = d;
        if (d > max)
           max = d;
        dev += d;
      } else if (first == 1) {
        min = d;
        max = d;
        first = 0;
      } else 
        first--;
    }
    printf("    Min diff: %ld ns, max diff: %ld ns, \n"
           "    avg. diff: %ld ns\n",
           min, max, dev/nloops);
    printf("   diff in ns      count\n");
    printf(" < -10*%ld        %ld\n", dint, count[0]);
    for(i=-9; i<10; i++)
        printf("    %d*%ld        %ld\n", i, dint, count[i+10]);
    printf(" >  10*%ld        %ld\n", dint, count[20]);

  }

  if (highresok) 
     printf("\nHighres timer seems enabled!\n");
  else
     printf("\nHighres is NOT enabled!\n");

  return 0;

}