#!/usr/bin/python
import subprocess
import signal
import os
import mpd
import time
import math

# Needed: mpd server
MPD_SERVER=u"localhost"

# with 3 parameters "samplerate" (e.g. "44100"), "bitdepth" (e.g. "32"), and the full file name
#PLAY_CMD = u"/usr/local/bin/playfrommpdsimple.sh"
PLAY_CMD = u"/usr/local/bin/playfrommpdremote.sh"
client = mpd.MPDClient(use_unicode=True)
client.connect(MPD_SERVER, 6600)
event = None
playProcess = None

while True:
   event = client.idle()

   print("Event is %s" % event)

   if event == ["player"]:
      status = client.status()
      state = status["state"]
      currentsong = client.currentsong()

      try:
        file = currentsong["file"]
      except:
         file = None
      print("command: %s , currentsong: %s" % (state, file))
      if state == u"play":
         if not playProcess is None:
           os.killpg(os.getpgid(playProcess.pid), signal.SIGTERM)


         # audio should look like: "44100:24:2"
         audio = status["audio"]
         audioArray = audio.split(':')
#         print("Sample rate and bit depth are: %s and %s" % (audioArray[0], audioArray[1]))
         if not file is None:
           playCmdList = [PLAY_CMD, audioArray[0], audioArray[1], file]
           playProcess = subprocess.Popen(playCmdList, shell=False, preexec_fn=os.setsid)

      if state == u"pause" or state == u"stop":
        if not playProcess is None:
          time.sleep(0.2)
          os.killpg(os.getpgid(playProcess.pid), signal.SIGTERM)
          playProcess = None
