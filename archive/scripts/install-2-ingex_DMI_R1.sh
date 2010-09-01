#! /bin/sh
# INGEX installation script


INGEXHOME=/home/ingex
VIDEOCACHEDIR=/video
POSTGRESHOME=/home/postgres
POSTGRESDATA=$POSTGRESHOME/data
POSTGRESCONF=$POSTGRESDATA/postgresql.conf
POSTGRESSYSCONF=/etc/sysconfig/postgresql
POSTGRESHBA=$POSTGRESDATA/pg_hba.conf

ROOT_UID=0     # Only users with $UID 0 have root privileges.

# Exit error codes
E_NOTROOT=1        # Non-root exit error.
E_CREATERULES=2    # Cannot create Ingex udev rules.
E_NODVSCARD=3      # Did not find DVS card or install drivers
E_NOVIDEOCACHE=4   # Didi not find video cache directory
E_CREATEPOSTGRESSYSCONF=5

# Run as root, of course.
if [ "$UID" -ne "$ROOT_UID" ]
then
  echo "Must be root to run this script."
  exit $E_NOTROOT
fi  


# -----------------------------------------------------------
# Install Ingex RPMs
cd $INGEXHOME/rpms
rpm -U codecs-for-ffmpeg-20081009-1.x86_64.rpm
rpm -U ffmpeg-DNxHD-h264-aac-0.5-3.x86_64.rpm
rpm -U shttpd-20070828-1.x86_64.rpm
rpm -U dvsdriver-3.4.0.2-1.x86_64.rpm
rpm -U ingex_DMI_R1-0.10.1-1.x86_64.rpm


# -----------------------------------------------------------
# Add ingex user to uucp group to give access to serial
# devices
usermod -A uucp ingex


# -----------------------------------------------------------
# Add ingex user to sudoers
sed -i '$a\ingex   ALL = (ALL) NOPASSWD: ALL' /etc/sudoers


# -----------------------------------------------------------
# Disable the firewall
/sbin/SuSEfirewall2 off


# -----------------------------------------------------------
# Set up the video cache
if [ -d $VIDEOCACHEDIR ]
then
  mkdir -p /video/recorder/cache
  mkdir -p /video/recorder/browse
  mkdir -p /video/recorder/pse
  chown -R ingex.users /video
else
  echo "Video cache directory not found"
  exit $E_NOVIDEOCACHE
fi

# -----------------------------------------------------------
# Install postgres
zypper --no-cd -n ref
zypper --no-cd -n in portaudio postgresql postgresql-server libpqxx perl-DBD-Pg postgresql-libs

# -----------------------------------------------------------
# Move the postgres data directory
(
cat <<'ENDOFPGSYSCONF'
## Path:	   Applications/PostgreSQL
## Description:    The PostgreSQL Database System
## Type:	   string()
## Default:	   "~postgres/data"
## ServiceRestart: postgresql
#
# In which directory should the PostgreSQL database reside?
#
POSTGRES_DATADIR=/home/postgres/data

## Path:	   Applications/PostgreSQL
## Description:    The PostgreSQL Database System
## Type:	   string()
## Default:        ""
## ServiceRestart: postgresql
#
# The options that are given to the PostgreSQL master daemon on startup.
# See the manual pages for postmaster and postgres for valid options.
#
# Don't put "-D datadir" here since it is set by the startup script
# based on the variable POSTGRES_DATADIR above.
#
POSTGRES_OPTIONS=""

## Path:	   Applications/PostgreSQL
## Description:    The PostgreSQL Database System
## Type:           string()
## Default:        "C"
## ServiceRestart: ""
#
# Specifies the locale under which the PostgreSQL database location
# should be initialized and run. If needed, it has to be changed
# before PostgreSQL is started for the first time. To change the
# locale of an existsing PostgreSQL database location, it must be
# dumped, removed and initialized from scratch using the new locale.
#
# If unset or empty $RC_LANG from /etc/sysconfig/language is used.
#
POSTGRES_LANG=""
ENDOFPGSYSCONF
) > $POSTGRESSYSCONF


if [ -f "$POSTGRESSYSCONF" ]
then
  chmod 644 $POSTGRESSYSCONF
  # Enable the postgres service
  chkconfig -a postgresql

  # Start the postgres service so that it creates the data dir and conf files
  /sbin/service postgresql start
else
  echo "Problem creating file: \"$POSTGRESSYSCONF\""
  exit $E_CREATEPOSTGRESSYSCONF
fi


# -----------------------------------------------------------
# Set the access privileges to allow access to the database
# from the Recorder and Tape Export servers.
(
cat <<'ENDOFPGHBA'
# PostgreSQL Client Authentication Configuration File
# ===================================================
#
# Refer to the "Client Authentication" section in the
# PostgreSQL documentation for a complete description
# of this file.  A short synopsis follows.
#
# This file controls: which hosts are allowed to connect, how clients
# are authenticated, which PostgreSQL user names they can use, which
# databases they can access.  Records take one of these forms:
#
# local      DATABASE  USER  METHOD  [OPTION]
# host       DATABASE  USER  CIDR-ADDRESS  METHOD  [OPTION]
# hostssl    DATABASE  USER  CIDR-ADDRESS  METHOD  [OPTION]
# hostnossl  DATABASE  USER  CIDR-ADDRESS  METHOD  [OPTION]
#
# (The uppercase items must be replaced by actual values.)
#
# The first field is the connection type: "local" is a Unix-domain socket,
# "host" is either a plain or SSL-encrypted TCP/IP socket, "hostssl" is an
# SSL-encrypted TCP/IP socket, and "hostnossl" is a plain TCP/IP socket.
#
# DATABASE can be "all", "sameuser", "samerole", a database name, or
# a comma-separated list thereof.
#
# USER can be "all", a user name, a group name prefixed with "+", or
# a comma-separated list thereof.  In both the DATABASE and USER fields
# you can also write a file name prefixed with "@" to include names from
# a separate file.
#
# CIDR-ADDRESS specifies the set of hosts the record matches.
# It is made up of an IP address and a CIDR mask that is an integer
# (between 0 and 32 (IPv4) or 128 (IPv6) inclusive) that specifies
# the number of significant bits in the mask.  Alternatively, you can write
# an IP address and netmask in separate columns to specify the set of hosts.
#
# METHOD can be "trust", "reject", "md5", "crypt", "password", "gss", "sspi",
# "krb5", "ident", "pam" or "ldap".  Note that "password" sends passwords
# in clear text; "md5" is preferred since it sends encrypted passwords.
#
# OPTION is the ident map or the name of the PAM service, depending on METHOD.
#
# Database and user names containing spaces, commas, quotes and other special
# characters must be quoted. Quoting one of the keywords "all", "sameuser" or
# "samerole" makes the name lose its special character, and just match a
# database or username with that name.
#
# This file is read on server startup and when the postmaster receives
# a SIGHUP signal.  If you edit the file on a running system, you have
# to SIGHUP the postmaster for the changes to take effect.  You can use
# "pg_ctl reload" to do that.

# Put your actual configuration here
# ----------------------------------
#
# If you want to allow non-local connections, you need to add more
# "host" records. In that case you will also need to make PostgreSQL listen
# on a non-local interface via the listen_addresses configuration parameter,
# or via the -i or -h command line switches.
#

# Trusts required for Ingex follow. If you wish to access Ingex from another
# host you should add an entry for the host or subnet.

# TYPE  DATABASE    USER        CIDR-ADDRESS          METHOD

# "local" is for Unix domain socket connections only
local   all         all                               trust
# IPv4 local connections:
host    all         all         127.0.0.1/32          trust
# IPv6 local connections:
host    all         all         ::1/128               trust

ENDOFPGHBA
) > $POSTGRESHBA

chown postgres.postgres $POSTGRESHBA
chmod 600 $POSTGRESHBA

# -----------------------------------------------------------
# Modify the postgres conforming settings in postgresql.conf
sed -i 's/^#backslash_quote/backslash_quote/' $POSTGRESCONF
sed -i 's/^#escape_string_warning/escape_string_warning/' $POSTGRESCONF
sed -i 's/^#standard_conforming_strings = off/standard_conforming_strings = on/' $POSTGRESCONF

# Restart the postgres service
/sbin/service postgresql restart

# Create the database folder
mkdir -p /video/database

# Create the sample database and ingex user
cd $INGEXHOME/install/database
./create_local_infax_db_example.sh
./create_recorder_db_example.sh

# Start the recorder service
/sbin/service ingex_recorder_setup start

exit 0