#!/bin/bash

#########################################################################
##  frankl (C) 2015              playfrommpd
##
##  See README or http://frank_l.bitbucket.org/stereoutils/player.html
##  for explanations.
#########################################################################
#!/bin/bash
# wget $3 -qO /dev/shm/play.wav &
CARD="hw:1,0"
VERBOSE="--verbose --verbose"
SAMPLERATE=$1
BITLENGTH=$2
FILENAME=$3
#echo "Original Bitdepth is ${BITLENGTH}"
#echo "Original Sample rate is ${SAMPLERATE}"
#echo "File name is ${FILENAME}"

BITLENGTH=32
SAMPLEFORMAT=S32_LE
if [ "${SAMPLERATE}" = "96000" ]; then
  FILESIZE=10240
  LOOPSPERSECOND=1500
  BLOCKSIZE=256
  HW_BUFFER=8192
  EXTRABYTESPLAY=24

elif [ "${SAMPLERATE}" = "48000" ]; then
  FILESIZE=5120
  LOOPSPERSECOND=1500
  BLOCKSIZE=256
  HW_BUFFER=8192 #4096
  EXTRABYTESPLAY=12

elif [ "${SAMPLERATE}" = "88200" ]; then
  FILESIZE=6720
  LOOPSPERSECOND=1050
  BLOCKSIZE=336
  HW_BUFFER=16800
  EXTRABYTESPLAY=18 #9

elif [ "${SAMPLERATE}" = "44100" ]; then
  FILESIZE=3360 #336
  LOOPSPERSECOND=1050 #1470
  BLOCKSIZE=336
  HW_BUFFER=16800 #8064 #4033
  EXTRABYTESPLAY=9 #8

elif [ "${SAMPLERATE}" = "192000" ]; then
  FILESIZE=15360 #10240 #15360 #10240
  LOOPSPERSECOND=1500 #3000
  BLOCKSIZE=256 #1536 #1024 #256
  HW_BUFFER=16384 #8192
  EXTRABYTESPLAY=47
fi

rm -f /dev/shm/bl{1,2,3} /dev/shm/sem.bl{1,2,3}*

trap "trap - SIGTERM && kill -- -$$ " SIGINT SIGTERM EXIT

taskset -c 2 cat /dev/shm/play.raw | \
chrt -f 97 taskset -c 2 writeloop --force-shm \
                        --block-size=${BLOCKSIZE} \
                        --file-size=${FILESIZE} --shared /bl1 /bl2 /bl3 &
sleep 0.1
chrt -f 99 taskset -c 3 playshm \
			--device=${CARD} \
                        --sample-rate=${SAMPLERATE} \
                        --sample-format=${SAMPLEFORMAT} \
                        ${VERBOSE} \
			--non-blocking-write \
                        --extra-bytes-per-second=${EXTRABYTESPLAY} \
                        --loops-per-second=${LOOPSPERSECOND} \
                        --hw-buffer=${HW_BUFFER} \
                        /bl1 /bl2 /bl3
