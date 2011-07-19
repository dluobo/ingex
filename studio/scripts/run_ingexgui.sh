#! /bin/sh

# -g option runs program under gdb so that a crash displays a stack trace
PRERUN=""
if [ "$1" = "-g" ] ; then
	PRERUN="gdb -q -ex run -ex where --args"
	shift			# swallow option
fi

# Set defaults for Ingex parameters
# These can be overidden by ~/ingex.conf or /etc/ingex.conf
INGEX_DIR="/home/ingex/ap-workspace/ingex"
NAMESERVER=":8888"
DOTTED_DECIMAL=1

# Get the required configuration
if [ -r $HOME/ingex.conf ]
then
        # Read in ingex mode configuration
        . $HOME/ingex.conf

elif [ -r /etc/ingex.conf ]
then
        # Read in ingex mode configuration
        . /etc/ingex.conf
fi

# set corba options
OPTIONS="-ORBDefaultInitRef corbaloc:iiop:$NAMESERVER -ORBDottedDecimalAddresses $DOTTED_DECIMAL"

# add file server root option
if [ -n "${INGEXGUI_FILE_SERVER_ROOT}" ] ; then
  OPTIONS="${OPTIONS} -r ${INGEXGUI_FILE_SERVER_ROOT}"
fi

# add snapshot directory option
if [ -n "${INGEXGUI_SNAPSHOT_DIR}" ] ; then
  OPTIONS="${OPTIONS} -s ${INGEXGUI_SNAPSHOT_DIR}"
fi

$PRERUN $INGEX_DIR/studio/ace-tao/Ingexgui/src/ingexgui $OPTIONS
