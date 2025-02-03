/*
highrestest.c                Copyright frankl 2013-2024

This file is part of frankl's stereo utilities. 
See the file License.txt of the distribution and 
http://www.gnu.org/licenses/gpl.txt for license details.
*/
#include <asm/unistd.h>
#include <inttypes.h>
#include <unistd.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <emmintrin.h> 
#include <x86intrin.h>

//#define MYCLOCK CLOCK_MONOTONIC_RAW
#define MYCLOCK CLOCK_MONOTONIC

static inline unsigned long long read_tsc(void)
{ unsigned long long tsc;
  _mm_lfence();
  tsc = __rdtsc();
  _mm_lfence();
  return (tsc);
}

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
           int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

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
  long i;
  for (i = (long)(cnt*nsloopfactor); i > 0; i--) {
    __asm volatile ("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
  }  
  return i;
}

/* a simple test of the resolution of several CLOCKs */
int main(int argc, char *argv[]) {
  int ret, err, highresok, first, nloops, i, k, shift;
  long multi, shifti, step, d, min, max, dev, dint, count[21];
  struct timespec res, last, tim, ttime;
  long long start_ticks, end_ticks, last_ticks;
  long long tsc_freq_sec;
  struct perf_event_attr pe = {
      .type = PERF_TYPE_HARDWARE,
      .size = sizeof(struct perf_event_attr),
      .config = PERF_COUNT_HW_INSTRUCTIONS,
      .disabled = 1,
      .exclude_kernel = 1,
      .exclude_hv = 1
  };
  int fd = perf_event_open(&pe, 0, -1, -1, 0);
  if (fd == -1) {
      perror("perf_event_open failed");
      return 1;
  }
  void *addr = mmap(NULL, 4*1024, PROT_READ, MAP_SHARED, fd, 0);
  if (!addr) {
      perror("mmap failed");
      return 1;
  }
  struct perf_event_mmap_page *pc = addr;
  if (pc->cap_user_time != 1) {
      fprintf(stderr, "Perf system doesn't support user time\n");
      return 1;
  }
  printf("%16s   %5s\n", "mult", "shift");
  printf("%16" PRIu32 "   %5" PRIu16 "\n", pc->time_mult, pc->time_shift);
  close(fd);
  multi = pc->time_mult;
  shifti = pc->time_shift;
  tsc_freq_sec = 1000000000;
  tsc_freq_sec <<= shifti;
  tsc_freq_sec /= multi;
  printf("tsc freq Hz: %lld\n", tsc_freq_sec); 

  
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
    
    printf("Measuring actual precision of rdtsc() for 10 seconds ...\n");
    step = 1000000;
    nloops = 10000;
    dint = atoi(argv[1]);
    if (dint <= 0)
      dint = 500;
    min = 0;
    max = 0;
    dev =0;
    clock_gettime(MYCLOCK, &res);
    start_ticks = read_tsc();
    last = res;
    last_ticks = start_ticks;
    for(i=0; i<21; count[i]=0, i++);
    /* avoid some startup jitter */
    for(first=100, i=0; i < nloops+99; i++) 
    {
      end_ticks = start_ticks + step*tsc_freq_sec/1000000000ull;
      do {
        start_ticks = read_tsc();
      } while (start_ticks < end_ticks);

      clock_gettime(MYCLOCK, &tim);
      d = difftimens(last, tim)-step;
      k = d/dint;
      if (k < -10) k = -10;
      if (k > 10) k = 10;
      count[k+10]++;
      last = tim;
      start_ticks = end_ticks;

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
    clock_gettime(MYCLOCK, &res);
    nsloop(10000);
    clock_gettime(MYCLOCK, &ttime);
    nsloopfactor = 1.0*10000/(difftimens(res, ttime));
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
      do {
        err = clock_nanosleep(MYCLOCK, TIMER_ABSTIME, &res, NULL);
      } while (err !=0);
      clock_gettime(MYCLOCK, &ttime);
      nsloop(shift - difftimens(res, ttime));
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

  }

  if (highresok) 
     printf("\nHighres timer seems enabled!\n");
  else
     printf("\nHighres is NOT enabled!\n");

  return 0;

}
