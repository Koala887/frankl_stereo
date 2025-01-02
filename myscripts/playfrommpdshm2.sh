#!/bin/bash

#########################################################################
##  frankl (C) 2015              playfrommpd
##
##  See README or http://frank_l.bitbucket.org/stereoutils/player.html
##  for explanations.
#########################################################################
#!/bin/bash
rm -f /dev/shm/play.wav
wget $3 -qO /dev/shm/play.wav &
CARD="hw:0,0"
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
  HW_BUFFER=8192
  EXTRABYTESPLAY=-1

elif [ "${SAMPLERATE}" = "88200" ]; then
  FILESIZE=6720
  LOOPSPERSECOND=1050
  BLOCKSIZE=336
  HW_BUFFER=16800
  EXTRABYTESPLAY=0#-4

elif [ "${SAMPLERATE}" = "44100" ]; then
  FILESIZE=3360
  LOOPSPERSECOND=1050
  BLOCKSIZE=336
  HW_BUFFER=16800
  EXTRABYTESPLAY=0#-2

elif [ "${SAMPLERATE}" = "192000" ]; then
  FILESIZE=15360
  LOOPSPERSECOND=1500
  BLOCKSIZE=256
  HW_BUFFER=16384
  EXTRABYTESPLAY=0#47
fi

rm -f /dev/shm/bl{1,2,3} /dev/shm/sem.bl{1,2,3}*

trap "trap - SIGTERM && kill -- -$$ " SIGINT SIGTERM EXIT
datei=0
while [ "${datei}" -lt "20000" ]
do
  if [ -f /dev/shm/play.wav ]
  then
    datei=$(stat -c %s /dev/shm/play.wav)
  fi
sleep 0.1
done
sox /dev/shm/play.wav -t raw -r ${SAMPLERATE} \
                        -c 2 -e signed -b ${BITLENGTH} - | \
chrt -f 81 taskset -c 2 writeloop --force-shm \
                        --block-size=${BLOCKSIZE} \
                        --file-size=${FILESIZE} --shared /bl1 /bl2 /bl3 &

chrt -f 82 taskset -c 3 playshm \
			--device=${CARD} \
                        --sample-rate=${SAMPLERATE} \
                        --sample-format=${SAMPLEFORMAT} \
                        ${VERBOSE} \
			--non-blocking-write \
                        --extra-bytes-per-second=${EXTRABYTESPLAY} \
                        --loops-per-second=${LOOPSPERSECOND} \
                        --hw-buffer=${HW_BUFFER} \
                        /bl1 /bl2 /bl3
