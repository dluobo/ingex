#!/bin/bash
# quad-split viewer

# Check whether process is running and kill if it is
if killall -q -0 -e player
then
	killall -e player
	while killall -q -0 -e player
	do
		usleep 100000
	done
fi

#
# --x11 is necessary when graphics card / driver cannot suport Xv extension
# audio device 1 was found to work on evan
# assumption is that the audio is in shared mem 0

# normal quad view
/usr/local/bin/player --show-tc LTC.0 --audio-lineup -18 --audio-mon 2 --source-aspect 16:9 --quad-split --hide-progress-bar --shm-in 0p --shm-in 1p --shm-in 2p --shm-in 3p

# quad view with director's cut logging
#/usr/local/bin/player --show-tc LTC.0 --audio-lineup -18 --audio-mon 2 --source-aspect 16:9 --quad-split --hide-progress-bar --split-select --vswitch-tc LTC.0 --vswitch-db /video/aaf/directors_cut.db --shm-in 0p --shm-in 1p --shm-in 2p --shm-in 3p

