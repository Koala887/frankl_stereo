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
EXTRABYTESBUF=0
AUDIOCOMPUTER=192.168.10.2
HOST=192.168.10.1
PORT=5570
#VERBOSE="--verbose --verbose"
SAMPLERATE=$1
BITLENGTH=$2

FILENAME=`echo "$3" | sed -e "s\.*musik\/mnt/media/musik\g;s/*20/ /g"` 
#echo "Original Bitdepth is ${BITLENGTH}"
#echo "Original Sample rate is ${SAMPLERATE}"
echo $3
echo "File name is ${FILENAME}"

BITLENGTH=32
SAMPLEFORMAT=S32_LE
if [ "${SAMPLERATE}" = "96000" ]; then
  FILESIZE=5120
  LOOPSPERSECOND=1500
  BLOCKSIZE=512
  HW_BUFFER=4096
  EXTRABYTESPLAY=0
  BYTESPERSECOND=768000

elif [ "${SAMPLERATE}" = "48000" ]; then
  FILESIZE=2560
  LOOPSPERSECOND=1500
  BLOCKSIZE=256
  HW_BUFFER=2048
  EXTRABYTESPLAY=0
  BYTESPERSECOND=384000

elif [ "${SAMPLERATE}" = "88200" ]; then
  FILESIZE=4800 #6720
  LOOPSPERSECOND=1050
  BLOCKSIZE=480 #672
  HW_BUFFER=4032
  EXTRABYTESPLAY=0
  BYTESPERSECOND=705600

elif [ "${SAMPLERATE}" = "44100" ]; then
  FILESIZE=3360
  LOOPSPERSECOND=1050
  BLOCKSIZE=336
  HW_BUFFER=7680
  EXTRABYTESPLAY=0
  BYTESPERSECOND=352800

elif [ "${SAMPLERATE}" = "192000" ]; then
  FILESIZE=10240
  LOOPSPERSECOND=1500
  BLOCKSIZE=1024
  HW_BUFFER=8192
  EXTRABYTESPLAY=0
  BYTESPERSECOND=1536000
fi

rm -f /dev/shm/bl{1,2,3,4,5} /dev/shm/sem.bl{1,2,3,4,5}*

trap "trap - SIGTERM && kill -- -$$ " SIGINT SIGTERM EXIT

#wget -q -O - $3 | sox - -t raw -r ${SAMPLERATE} -c 2 \
#sox "${FILENAME}" -t raw -r ${SAMPLERATE} -c 2 -e signed -b ${BITLENGTH} /dev/shm/play.raw
#datei=0
#while [ "${datei}" -lt "100000" ]
#do
#  if [ -f /dev/shm/play.raw ]
#  then
#   datei=$(stat -c %s /dev/shm/play.raw)
#  fi
#sleep 1.1
#done


#cptoshm --file="$3" --shmname=/play.flac

rm -f /dev/shm/bl{1,2,3} /dev/shm/sem.bl{1,2,3}* 

#shmcat --shmname=/play.flac |
taskset -c 2 sox  "${FILENAME}" -t raw -r ${SAMPLERATE} -c 2 -e signed -b 32 - | \
taskset -c 2 writeloop --block-size=${BLOCKSIZE} --file-size=${FILESIZE} --shared /bl1 /bl2 /bl3 &
#taskset -c 2 writeloop --from-file=/dev/shm/play.raw --block-size=${BLOCKSIZE} --file-size=${FILESIZE} --shared /bl1 /bl2 /bl3 &

sleep 0.5

# tell audio computer what to do, short sleep as startup time for sender
REMOTE="sleep 0.3;\
        chrt -f 90 taskset -c 3 playhrtmin ${VERBOSE} --host=${HOST} --port=${PORT} \
                     --sample-rate=${SAMPLERATE} \
                     --sample-format=${SAMPLEFORMAT} \
                     --loops-per-second=${LOOPSPERSECOND} --number-copies=2 --slow-copies \
                     --device=${CARD} --hw-buffer=${HW_BUFFER} \
                     --extra-bytes-per-second=${EXTRABYTESPLAY} \
                     --non-blocking-write --mmap"

echo "${REMOTE}" |  nc -q 0 ${AUDIOCOMPUTER} 5501 


taskset -c 3 catloop --block-size=${BLOCKSIZE} --shared /bl1 /bl2 /bl3 | \
chrt -f 90 taskset -c 3 bufhrtmin --bytes-per-second=${BYTESPERSECOND} \
                               --loops-per-second=$[${LOOPSPERSECOND}*2] --port-to-write=${PORT} \
                               --extra-bytes-per-second=${EXTRABYTESBUF} ${VERBOSE} 

