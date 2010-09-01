#!/bin/sh


echo "** Create 'ingex' user"
psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "CREATE USER ingex WITH PASSWORD 'ingex'" || echo "** Ignoring previous error"

echo "** Create 'ingexrecorderdb' database"
psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "CREATE DATABASE ingexrecorderdb"
psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "GRANT CREATE ON DATABASE ingexrecorderdb TO ingex"
psql -v ON_ERROR_STOP=yes -U postgres -d ingexrecorderdb -c "CREATE LANGUAGE plpgsql"
psql -v ON_ERROR_STOP=yes -U ingex -d ingexrecorderdb -f IngexRecorderDb.sql

echo "** Vacuum"
psql -U ingex -d ingexrecorderdb -c "VACUUM ANALYSE;"

