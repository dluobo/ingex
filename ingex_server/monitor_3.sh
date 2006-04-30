#!/bin/sh

./nexus_xv -q -c 2 &
pid2=$!
./nexus_xv -q -c 1 &
pid3=$!

trap kill "kill $pid2 $pid3" TERM INT
echo pid2=$pid2 pid3=$pid3
./nexus_xv -q -c 0

kill $pid2
kill $pid3
