#!/bin/sh

cd /home/stuartc/nexus
./nexus_sdi -c 3 &
res=$?
pid=$!

sleep 5
kill -0 $pid 2> /dev/null
