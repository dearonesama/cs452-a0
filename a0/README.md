## Overview
This program gives a terminal UI of the connected train track. First, in the current folder, you compile the program and upload it to the Pi:

```bash
make
/u/cs452/public/tools/setupTFTP.sh CS017541 build/kernel8.img
```

where `CS017541` can be other IDs, and `build/kernel8.img` is the compiled artifact relative to the current folder.

Then you need to restart the Pi, after the program loads, you will see from top to bottom
* A clock
* A line for giving commands
* A table of train speeds (if known)
* A table of switch positions (either S, C, or unknown)
* A list of most active sensors
* Real time timings and their max values, where
  * IT measures iteration time
  * FB measures time from requesting the sensor data to the time when first byte is received
  * FF measures time from requesting the sensor data to the time when last byte is received

In particular, the commands are:
* `tr <train number> <train speed>`: set any train in motion at the desired speed (0 for stop). The program assumes train number is at most 2 digits.
* `rv <train number>`: the train should reverse direction.
* `sw <switch number> <switch direction>`: throw the given switch to straight (S) or curved (C). The program assumes switch number is at most 3 digits.
* `q`: reboot.

You will need to press `Enter` to confirm. Some commands will take longer time to execute and render the command prompt unavailable until they are finished.

Illegal commands not matching any of above will be discarded.
