#!/bin/sh


echo "** Create 'ingex' user"
psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "CREATE USER ingex WITH PASSWORD 'ingex'" || echo "** Ignoring previous error"

echo "** Create 'localinfaxdb' database"
psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "CREATE DATABASE localinfaxdb"
psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "GRANT CREATE ON DATABASE localinfaxdb TO ingex"
psql -v ON_ERROR_STOP=yes -U postgres -d localinfaxdb -c "CREATE LANGUAGE plpgsql"
psql -v ON_ERROR_STOP=yes -U ingex -d localinfaxdb -f LocalInfaxDb.sql

echo "** Load test data"
psql -v ON_ERROR_STOP=yes -U ingex -d localinfaxdb < test_local_infax.sql > /dev/null

echo "** load the real data here..."

echo "** Count rows in SourceItem table"
psql -U ingex -d localinfaxdb -c "select count(*) from sourceitem;"

echo "** Count rows in SourceItems table, but not including test items which have sit_identifier <= 100"
psql -U ingex -d localinfaxdb -c "select count(*) from sourceitem where sit_identifier > 100;"

echo "** Vacuum"
psql -U ingex -d localinfaxdb -c "VACUUM ANALYSE;"

