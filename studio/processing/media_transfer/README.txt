COPYING MATERIAL TO A SERVER
----------------------------

Each recorder's ENCODE1_DEST, ENCODE2_DEST etc should point to directories on the server(s).

There should be an instance of xferserver.pl running, which will take commands from each recorder.

This in turn will call cpfs to copy each file, sending signals to switch rate control on and off as recordings start and stop.

The location of cpfs is defined in xferserver.pl.  After making cpfs, it will need to be copied to this location if it is not the current directory.

Alternatively, FTP can be used to transfer material - see xferserver.pl for command-line options

(The present scheme does not use media_transfer.pl.)

xferserver.pl maintains a file called paths, which it uses to keep tabs on what it has copied (to prevent re-copying) and to continue copying automatically after restarting.  Its format is described in xferserver.pl.


AUTOMATICALLY GENERATING SAMBA EXPORTS FOR AVID
-----------------------------------------------

export_for_avidd.pl needs to be run as a system service:

1) Put export_for_avidd.pl in /usr/local/sbin, and export_for_avidd in /etc/init.d, change ownerships to root and make exectable.

2) sudo /sbin/chkconfig -a export_for_avidd

The export_for_avid.pl script sets up links in the samba shares.
If your client is unable to follow these links, try the following
options in /etc/samba/smb.conf

[global]

...

	wide links = Yes
	unix extensions = No


AUTOMATICALLY IMPORTING MATERIAL AND CUT DATA INTO SERVER DATABASE
-------------------------------------------------------------

import_db_infod.pl needs to be run as a system service:

1) Put import_db_infod.pl in /usr/local/sbin, and import_db_infod in /etc/init.d, change ownerships to root and make exectable.

2) sudo /sbin/chkconfig -a import_db_infod


ADDITIONAL PERL MODULES
-----------------------

export_for_avidd.pl and import_dv_info need:
  Proc::Daemon
  Linux::Inotify2

xferserver.pl needs:
  IPC::ShareLite
  Storable
  Filesys::DfPortable
  Term::ANSIColor


