#!/bin/bash

#########################################################################
##  frankl (C) 2015              playfrommpd
##
##  See README or http://frank_l.bitbucket.org/stereoutils/player.html
##  for explanations.
#########################################################################
#!/bin/bash
rm -f /dev/shm/play.raw
#wget $3 -qO /dev/shm/play.wav &
CARD="hw:0,0"
#VERBOSE="--verbose --verbose"
SAMPLERATE=$1
BITLENGTH=$2
FILENAME=$3
#echo "Original Bitdepth is ${BITLENGTH}"
#echo "Original Sample rate is ${SAMPLERATE}"
#echo "File name is ${FILENAME}"

BITLENGTH=32
SAMPLEFORMAT=S32_LE
if [ "${SAMPLERATE}" = "96000" ]; then
  FILESIZE=5120
  LOOPSPERSECOND=1500
  BLOCKSIZE=512
  HW_BUFFER=4096
  EXTRABYTESPLAY=0

elif [ "${SAMPLERATE}" = "48000" ]; then
  FILESIZE=2560
  LOOPSPERSECOND=1500
  BLOCKSIZE=256
  HW_BUFFER=2048
  EXTRABYTESPLAY=0

elif [ "${SAMPLERATE}" = "88200" ]; then
  FILESIZE=4800 #6720
  LOOPSPERSECOND=1470 #1050
  BLOCKSIZE=480 #672
  HW_BUFFER=4032
  EXTRABYTESPLAY=0

elif [ "${SAMPLERATE}" = "44100" ]; then
  FILESIZE=2400 #6720 #3360
  LOOPSPERSECOND=1470 #525 #1050
  BLOCKSIZE=240 #672 #336
  HW_BUFFER=2016
  EXTRABYTESPLAY=0

elif [ "${SAMPLERATE}" = "192000" ]; then
  FILESIZE=10240
  LOOPSPERSECOND=1500
  BLOCKSIZE=1024
  HW_BUFFER=8192
  EXTRABYTESPLAY=0
fi

rm -f /dev/shm/bl{1,2,3,4,5} /dev/shm/sem.bl{1,2,3,4,5}*

trap "trap - SIGTERM && kill -- -$$ " SIGINT SIGTERM EXIT
wget -q -O - $3 | sox - -t raw -r ${SAMPLERATE} -c 2 \
		-e signed -b ${BITLENGTH} /dev/shm/play.raw &
#datei=0
#while [ "${datei}" -lt "100000" ]
#do
#  if [ -f /dev/shm/play.raw ]
#  then
#   datei=$(stat -c %s /dev/shm/play.raw)
#  fi
#sleep 0.1
#done
chrt -f 95 taskset -c 2 writeloop --from-file=/dev/shm/play.raw --force-shm \
			--block-size=${BLOCKSIZE} \
			--file-size=${FILESIZE} --shared /bl1 /bl2 /bl3 &
#sleep 0.2
chrt -f 96 taskset -c 3 playshmmin \
			--device=${CARD} \
                        --sample-rate=${SAMPLERATE} \
                        --sample-format=${SAMPLEFORMAT} \
                        ${VERBOSE} \
			--non-blocking-write \
                        --extra-bytes-per-second=${EXTRABYTESPLAY} \
                        --loops-per-second=${LOOPSPERSECOND} \
                        --hw-buffer=${HW_BUFFER} \
                        /bl1 /bl2 /bl3
