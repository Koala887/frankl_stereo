#!/bin/bash

#########################################################################
##  frankl (C) 2015              play_shm
##
##  See README or http://frank_l.bitbucket.org/stereoutils/player.html
##  for explanations.
#########################################################################

CARD="hw:1,0"
EXTRABYTESPLAY=400
VERBOSE="--verbose --verbose"

wget $3 -O /dev/shm/play.wav &
sleep 0.3
chrt -r 98 taskset -c 2 sox /dev/shm/play.wav -t raw -r 44100 -c 2 -e signed -b 16 - | \
chrt -r 99 taskset -c 3 playhrt \
      --stdin \
      --device=${CARD} \
      --sample-rate=44100 \
      --sample-format=S16_LE \
      --loops-per-second=1000 \
      --mmap \
      --non-blocking-write \
      ${VERBOSE} \
      --extra-bytes-per-second=${EXTRABYTESPLAY} \
      --hw-buffer=4096
