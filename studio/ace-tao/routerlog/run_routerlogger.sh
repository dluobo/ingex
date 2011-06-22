#! /bin/sh

# Choose config depending on location
LOCATION=studio
#LOCATION=stage
#LOCATION=lot
#LOCATION=dummy
case $LOCATION in

  ( studio )

./routerlogger --dbuser bamzooki --dbpass bamzooki \
  -v -r 192.168.100.12:9001 -s -l \
  -n Router-StudioA \
  -m 20 \
  -d "StudioA-Sx1" -p 7 \
  -d "StudioA-Sx2" -p 8 \
  -d "StudioA-Sx3" -p 9 \
  -d "StudioA-Sx4" -p 10 \
  -c "StudioA multi-camera clip" \
  -a corbaloc:iiop:192.168.100.10:8888/NameService \
  -ORBDottedDecimalAddresses 1
;;

  ( stage )

./routerlogger --dbuser bamzooki --dbpass bamzooki \
  -v -r 192.168.100.22:9001 -s -l \
  -n Router-Stage \
  -m 20 \
  -d "Stage-Sx1" -p 7 \
  -d "Stage-Sx2" -p 8 \
  -d "Stage-Sx3" -p 9 \
  -d "Stage-Sx4" -p 10 \
  -c "Stage multi-camera clip" \
  -a corbaloc:iiop:192.168.100.20:8888/NameService \
  -ORBDottedDecimalAddresses 1
;;

  ( lot)

./routerlogger --dbuser bamzooki --dbpass bamzooki \
  -v -r 192.168.100.32:9001 -s -l \
  -n Router-Lot \
  -m 20 \
  -d "Lot-Sx1" -p 7 \
  -d "Lot-Sx2" -p 8 \
  -d "Lot-Sx3" -p 9 \
  -d "Lot-Sx4" -p 10 \
  -c "Lot multi-camera clip" \
  -a corbaloc:iiop:192.168.100.30:8888/NameService \
  -ORBDottedDecimalAddresses 1
;;

  ( example)

# Example with two areas using same router
# router connection via serial port
# and timecode from recorder shared memory
./routerlogger --dbuser bamzooki --dbpass bamzooki \
  -v \
  -r /dev/ttyS0 \
  -l \
  -n Router-Lot \
  -m 12 \
  -d "Lot-VT1" -p 5 \
  -d "Lot-VT2" -p 6 \
  -d "Lot-VT3" -p 7 \
  -d "Lot-VT4" -p 8 \
  -c "Lot multi-camera clip" \
  -a corbaloc:iiop:192.168.1.181:8888/NameService \
  -n Router-Stage \
  -m 11 \
  -d "Stage-VT1" -p 1 \
  -d "Stage-VT2" -p 2 \
  -d "Stage-VT3" -p 3 \
  -d "Stage-VT4" -p 4 \
  -c "Stage multi-camera clip" \
  -a corbaloc:iiop:192.168.1.130:8888/NameService \
  -ORBDottedDecimalAddresses 1
;;

  ( * )
echo "Unknown location"
;;

esac


