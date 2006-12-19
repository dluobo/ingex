psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "CREATE DATABASE prodautodb"
psql -v ON_ERROR_STOP=yes -U postgres -d template1 -c "GRANT CREATE ON DATABASE prodautodb TO ingex"
psql -v ON_ERROR_STOP=yes -U ingex -d prodautodb -f ProdAutoDatabase.sql

