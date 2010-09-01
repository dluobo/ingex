#!/bin/sh


echo "** Drop existing 'localinfaxdb' database"
psql -U postgres -d template1 -c "DROP DATABASE localinfaxdb"

