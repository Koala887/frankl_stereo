#!/bin/bash

#########################################################################
##  frankl (C) 2015              playfrommpdremote
##
##  See README or http://frank_l.bitbucket.org/stereoutils/player.html
##  for explanations.
#########################################################################
#!/bin/bash

CARD="hw:0,0"
EXTRABYTESBUF=0
AUDIOCOMPUTER=192.168.10.2
HOST=192.168.10.1
PORT=5570
#VERBOSE="--verbose --verbose"
SAMPLERATE=$1
BITLENGTH=$2
BITLENGTH=32
SAMPLEFORMAT=S32_LE

FILENAME=`echo "$3" | sed -e "s\.*musik\/mnt/media/musik\g;s/*20/ /g"` 

echo $3
echo "File name is ${FILENAME}"

if [ "${SAMPLERATE}" = "96000" ]; then
  LOOPSPERSECOND=1500
  HW_BUFFER=4096
  EXTRABYTESPLAY=0
  BYTESPERSECOND=768000

elif [ "${SAMPLERATE}" = "48000" ]; then
  LOOPSPERSECOND=1500
  HW_BUFFER=2048
  EXTRABYTESPLAY=0
  BYTESPERSECOND=384000

elif [ "${SAMPLERATE}" = "88200" ]; then
  LOOPSPERSECOND=1050
  HW_BUFFER=4032
  EXTRABYTESPLAY=0
  BYTESPERSECOND=705600

elif [ "${SAMPLERATE}" = "44100" ]; then
  LOOPSPERSECOND=1050
  HW_BUFFER=7680
  EXTRABYTESPLAY=0
  BYTESPERSECOND=352800

elif [ "${SAMPLERATE}" = "192000" ]; then
  LOOPSPERSECOND=1500
  HW_BUFFER=8192
  EXTRABYTESPLAY=0
  BYTESPERSECOND=1536000
fi

trap "trap - SIGTERM && kill -- -$$ " SIGINT SIGTERM EXIT

cpupower frequency-set -u 3400MHz >/dev/null 2>&1
sox "$3" -t raw -e signed -b 32 /dev/shm/play.raw
cpupower frequency-set -u 800MHz -d 800MHz >/dev/null 2>&1

# tell audio computer what to do, short sleep as startup time for sender
REMOTE="sleep 0.3;\
        taskset -c 3 playhrtmin-tsc --host=${HOST} --port=${PORT} \
                     --sample-rate=${SAMPLERATE} \
                     --sample-format=${SAMPLEFORMAT} \
                     --loops-per-second=${LOOPSPERSECOND} --number-copies=2 --slow-copies \
                     --device=${CARD} --hw-buffer=${HW_BUFFER} \
                     --extra-bytes-per-second=${EXTRABYTESPLAY} \
                     --non-blocking-write --mmap"

echo "${REMOTE}" |  nc -q 0 ${AUDIOCOMPUTER} 5501 

taskset -c 3 bufhrtmin-tsc --file=/dev/shm/play.raw --bytes-per-second=${BYTESPERSECOND} \
       --loops-per-second=${LOOPSPERSECOND} --port-to-write=${PORT} \
       --extra-bytes-per-second=${EXTRABYTESBUF} \
       ${VERBOSE} #--out-net-buffer-size=67200

