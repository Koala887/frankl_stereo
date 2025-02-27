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
#include <x86gprintrin.h>

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
  printf("TSC-Mult: %u \n", pc->time_mult);
  printf("TSC-Shift: %u \n", pc->time_shift);
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

long ticks_to_ns(long ticks)
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

/* a simple test of the resolution of several CLOCKs */
int main(int argc, char *argv[])
{
  int ret, highresok, first, nloops, i, k, shift;
  long step, d, min, max, dev, dint, count[21];
  struct timespec res, tim;
  long long start_ticks, end_ticks, last_ticks, step_ticks;

  if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
  {
    fprintf(stderr, "Usage: no argument - simple test\n  highresttest intv shift - with statistics\n  intv: interval, shift: delay for more precision\n\n");
    return 0;
  }

  printf("----- Testing highres timer:\n");

  /* avoid waiting 50000 ns collecting more sleep requests */
  prctl(PR_SET_TIMERSLACK, 1L);

  /* get tsc frequency */
  tsc_freq_hz = get_tsc_freq();
  printf("TSC Frequency: %lld Hz\n", tsc_freq_hz);

  ret = clock_getres(CLOCK_MONOTONIC, &res);
  highresok = (res.tv_sec == 0 && res.tv_nsec == 1);
  printf("monotonic resolution: %ld s %ld ns (%d)\n", res.tv_sec, res.tv_nsec, ret);

  ret = clock_gettime(CLOCK_MONOTONIC, &tim);
  printf("monotonic: %ld s %ld ns (%d)\n", tim.tv_sec, tim.tv_nsec, ret);

  if (highresok && argc > 1)
  {

    printf("Measuring actual precision of rdtsc() for 10 seconds ...\n");
    step = 1000000;
    nloops = 10000;
    shift = 1000;
    if (argc > 2)
      shift = atoi(argv[2]);
    if (shift <= 0)
      shift = 1000;
    dint = atoi(argv[1]);
    if (dint <= 0)
      dint = 500;
    min = 0;
    max = 0;
    dev = 0;
    // calculate ticks per step
    step_ticks = ns_to_ticks(step);
    for (i = 0; i < 21; count[i] = 0, i++);

    start_ticks = read_tsc();
    last_ticks = start_ticks;

    /* avoid some startup jitter */
    for (first = 100, i = 0; i < nloops + 99; i++)
    {
      start_ticks += step_ticks;
      sleep_ns(step/4*3);
      tpause(start_ticks - shift);
      do
      {
        end_ticks = __rdtsc();
      } while (start_ticks > end_ticks);

      d = (ticks_to_ns(end_ticks - last_ticks) - step);
      last_ticks = end_ticks;

      // printf("%ld\n", d);
      if (first == 0)
      {
        k = d / dint;
        if (k < -10)
          k = -10;
        if (k > 10)
          k = 10;
        count[k + 10]++;
        if (d < min)
          min = d;
        if (d > max)
          max = d;
        dev += d;
      }
      else if (first == 1)
      {
        min = d;
        max = d;
        first = 0;
      }
      else
        first--;
    }
    printf("    Min diff: %ld ns, max diff: %ld ns, \n"
           "    avg. diff: %ld ns\n",
           min, max, dev / nloops);
    printf("   diff in ns      count\n");
    printf(" < -10*%ld        %ld\n", dint, count[0]);
    for (i = -9; i < 10; i++)
      printf("    %d*%ld        %ld\n", i, dint, count[i + 10]);
    printf(" >  10*%ld        %ld\n", dint, count[20]);
  }

  if (highresok)
    printf("\nHighres timer seems enabled!\n");
  else
    printf("\nHighres is NOT enabled!\n");

  return 0;
}
