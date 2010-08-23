#! /bin/sh

# Choose config depending on location
LOCATION=studio
#LOCATION=stage

if [ "$LOCATION" = "studio" ]
then
# Studio
# Example with just one area using the router
# and communication via PC serial port.
./routerlogger --dbuser bamzooki --dbpass bamzooki \
  -v \
  -r /dev/ttyS0 -l \
  -n Ingex-Router \
  -m 9 \
  -d "Cam1" -p 1 \
  -d "Cam2" -p 2 \
  -d "Cam3" -p 3 \
  -d "Cam4" -p 4 \
  -c "Studio multicam" \
  -a corbaloc:iiop:172.29.154.23:8888/NameService \
  -ORBDottedDecimalAddresses 1

elif [ "$LOCATION" = "stage" ]
then

# Lot and Stage
# Example with two areas using same router
# and communication via ethernet adapter.
./routerlogger --dbuser bamzooki --dbpass bamzooki \
  -v \
  -r 192.168.1.252:4098 -s \
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

else

echo Unknown location.

fi


