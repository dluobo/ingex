#!/bin/sh
# $Id: play_quadsplit.sh,v 1.1 2007/09/11 14:08:12 stuart_hc Exp $
#
# Given a single argument, work out the other 3 video MXF files to pass
# to the MXF player

if [ -z "$1" ] ; then
	echo "Usage: play_quadsplit.sh [-p player] [-s sed_cmd] video_file.mxf"
	exit 0
fi

# default player is simply 'player' but -p can be used for alternative
player=player
if [ "$1" = "-p" ] ; then
	player=$2
	shift
	shift
fi

# sed substition command to convert part of the 1st filename into a pattern
# to match all related filenames.
sed_cmd='s/_card._/_card?_/'
if [ "$1" = "-s" ] ; then
	sed_cmd=$2
	shift
	shift
fi
pattern=`echo $1 | sed $sed_cmd`

# loop through all matching files, building up final command args
args="--dual --vqswitch"
for file in $pattern
do
	args="$args -m $file"
done

echo $player $args
exec $player $args
