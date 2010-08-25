#!/bin/bash
# single-input viewer

# Check whether process is running and kill if it is
if killall -q -0 -e player
then
	killall -e player
	while killall -q -0 -e player
	do
		usleep 100000
	done
fi

# -g option runs program under gdb so that a crash displays a stack trace
PRERUN=""
if [ "$1" = "-g" ] ; then
	PRERUN="gdb -q -ex run -ex where --args"
	shift			# swallow option
fi

# play shm channel 3
$PRERUN /usr/local/bin/player --disable-shuttle --audio-lineup -18 --audio-mon 2 --source-aspect 16:9 --hide-progress-bar --shm-in 3p

