Summary: DMI R1 Ingex Recorder
Name: ingex_DMI_R1
Version: 0.11.0
Release: 1
License: GPL
Group: System Environment/Daemons
Url: http://ingex.sourceforge.net
BuildRoot: %{_tmppath}/%{name}-root
PreReq: /sbin/chkconfig /sbin/service
Requires: dvsdriver = 3.4.0.2
Requires: ffmpeg-DNxHD-h264-aac = 0.5-4
Requires: codecs-for-ffmpeg = 20081215-2
Requires: portaudio
Requires: postgresql
Requires: postgresql-server

%description
Ingex Archive / DMI Release 1 tape ingest system.


%prep
rm -rf $RPM_BUILD_ROOT


%build

%install
# get and check $INGEX_DIR environment variable
%define INGEX_DIR %(echo $INGEX_DIR)
if [ "$INGEX_DIR" = "" ]
then
	echo "Error: INGEX_DIR environment variable not set"
	exit 1
fi
export INGEX_ARCHIVE_DIR=$INGEX_DIR/archive


# Sample Database
mkdir -p $RPM_BUILD_ROOT/home/ingex/install/database
cp -R $INGEX_ARCHIVE_DIR/database/* $RPM_BUILD_ROOT/home/ingex/install/database

# The recorder
mkdir -p $RPM_BUILD_ROOT/home/ingex/install/recorder/logs
cp -a $INGEX_ARCHIVE_DIR/src/recorder/ingex_recorder $RPM_BUILD_ROOT/home/ingex/install/recorder/ingex_recorder
cp -R $INGEX_ARCHIVE_DIR/web/content $RPM_BUILD_ROOT/home/ingex/install/recorder/www
cp -a $INGEX_ARCHIVE_DIR/config/ingex.cfg.dmi $RPM_BUILD_ROOT/home/ingex/install/recorder/ingex.cfg
cp -a $INGEX_ARCHIVE_DIR/scripts/run_recorder.sh $RPM_BUILD_ROOT/home/ingex/install/recorder/run_recorder.sh

# Player executables
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
cp -a $INGEX_DIR/player/ingex_player/player $RPM_BUILD_ROOT/usr/local/bin
cp -a $INGEX_DIR/player/ingex_player/qc_player $RPM_BUILD_ROOT/usr/local/bin

# PSE library
#cp -a $INGEX_ARCHIVE_DIR/bbc-internal/pse/libfpacore2.so $RPM_BUILD_ROOT/usr/lib

# Desktop
mkdir -p $RPM_BUILD_ROOT/home/ingex/Desktop
cp -a $INGEX_ARCHIVE_DIR/kde-desktop/Ingex.desktop $RPM_BUILD_ROOT/home/ingex/Desktop/Ingex.desktop

# init scripts
mkdir -p $RPM_BUILD_ROOT/etc/init.d
cp $INGEX_ARCHIVE_DIR/scripts/ingex_recorder_setup $RPM_BUILD_ROOT/etc/init.d

%post

# -----------------------------------------------------------
# Create Ingex udev rules
UDEVRULESFILE=/etc/udev/rules.d/45-ingex.rules

(
cat <<'ENDOFUDEVRULES'
# Rules for the Ingex Archive system
# Read/write access to the LTO tape drive
KERNEL=="st[0-9]*", NAME="%k", MODE="0666"
KERNEL=="nst[0-9]*", NAME="%k", MODE="0666"
# Read access to the jog-shuttle control device
KERNEL=="event*", NAME="input/%k", MODE="0644"
ENDOFUDEVRULES
) > $UDEVRULESFILE


if [ -f "$UDEVRULESFILE" ]
then
  # Restart the udev service
  /sbin/service boot.udev restart
else
  echo "Problem creating file: \"$UDEVRULESFILE\""
  exit $E_CREATERULES
fi

chown root /home/ingex/install/recorder/ingex_recorder
chmod u+s /home/ingex/install/recorder/ingex_recorder
chmod 744 /etc/init.d/ingex_recorder_setup

# remove old version if any
/sbin/chkconfig --del ingex_recorder_setup 2>/dev/null || true
# add boot-time script
/sbin/chkconfig --add ingex_recorder_setup

%preun

if [ "$1" = "0" ]; then
	/sbin/service ingex_recorder_setup stop > /dev/null 2>&1
	/sbin/chkconfig --del ingex_recorder_setup
fi

exit 0

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/local/bin/
%defattr(-,ingex,users)
/home/ingex/install
/home/ingex/Desktop/Ingex.desktop
/etc/init.d/ingex_recorder_setup


%changelog
* Tue May 18 2010 Philip de Nier 0.11.0
- Added ingest Profiles
- Added support for IMX-50
- Reduced out-of-date description
- Updated ffmpeg-DNxHD-h264-aac and codecs-for-ffmpeg dependency versions

* Thu Jan 28 2010 Tim Jobling (tim@cambridgeimaging.co.uk) 0.10.0
- Updated for DMI R1 operations.
- Recorder runs as a deamon/service.
- KDE-desktop icon links to open the web interface added.
- Package dependencies added.

* Thu Jun 19 2008 Philip de Nier 0.9.5
- Added config option to set the max files to put on a LTO for the auto
  transfer method
- Added config options to set the max total size to transfer to LTO
- Fixed LTO Infax data duration value for the MXF file which was out by factor 25

* Mon Jun 9 2008 Philip de Nier 0.9.4
- Changes for new ingex archive structure

* Tue May 20 2008 Philip de Nier 0.9.3
- Fix name for LTO and Digibeta barcode prefixes option in config file
- Replaced internal use of infax data strings with infax data structs
- Added a second progress bar to the player for displaying marks
- PSE failures and VTR errors are shown on the second progress bar in the player
- Pressing the play item button in the cache page will now seek to start or play
- Added client side shuttle control for the player
- Throttling chunking speed when a tape transfer is in progress
- Warning user when a digibeta was used in a previously completed (not only 
  started) session
- Original (temporary) file ingested from multi-item tape is not shown in the 
  recorder cache page
- Distinguishing between channels and testbed machine configurations and 
  filling in the channel name in the config file for channels
- Fix bug which resulted in log files not being deleted
- Separated framed review page into review and item page
- Added (player, marks, chunking) progress bar to the review and item pages
- Added link to item page from the record page
- Changed cache MXF, browse and PSE filenames
- Added junk item for junking the end of the MXF file during chunking
- Changing browse info background colour in the record page when the buffer overflows
- Changing eta background colour when it had reached 0
- Non-playing items are greyed out in the items page

* Mon Apr 7 2008 Philip de Nier 0.9.2
- Added support for multi-item tapes
- Removed unused barcode scanner startup from the desktop
- Prevent usage of LTO tapes that have been used before in a completed session
- Allow multiple Digibeta and LTO barcode prefixes to be set in the config
- Storing the material, file and tape package UIDs in the database
- Storing the browse copy file size in the database

* Fri Feb 22 2008 Philip de Nier 0.9.1
- removed hard-coded content package size from d3_mxf_info and from the 
  remaining recording time displayed in the home pages

* Mon Feb 18 2008 Philip de Nier 0.9.0
- added support for cue audio track configurable through num_audio_tracks in config file
- recorder home page reports the the number of audio tracks for ingest

* Wed Feb 13 2008 Philip de Nier 0.8.2
- replace space characters in barcode/spool no with &nbsp; for display in browser
- separated source info into 2 tables in preparation of multi-item tape support
- using the monotonic clock for timing and sleeping to avoid sleeping longer/shorter 
  when the wall clock time is changed, eg. time server resync
- removed edit option from the recorder source page to make it easier to implement
  support for multi-item tape
- added config option to enable the server side barcode scanner
- fix bug where browse encode thread was left running after an abort because a signal
  only reached the mxf store thread (ie. signal should have been broadcast to all threads)
- abort the capture if the mxf store buffer overflows
- no longer polling the Digibeta for its extended VTR state
- added detailed tape device status to the prepare web page
- made progress bar wider for imrpoved mark resolution
- reformatted the pse report

* Tue Dec 11 2007 Philip de Nier 0.8.1
- changed background colours for better contrast on LCD touch screens
- fix auto-correction of D3 barcodes with a single character prefix
- convert barcodes to uppercase to workaround issue with keyboard capslock
- checking that digibeta barcodes start with a prefix declared in the config
- added capture abort when starting a new session to abort a capture which has 
  been left running when the previous controlling session was completed or aborted
- the PSE result is shown in the record page to simplify the process for operators
- fix runaway capture (stop it) resulting from digibeta failing to go into record
- cleanup capture when aborting after the capture stopped because of a buffer overflow
- allowing more than 2 VITC/LTC lines and setting VITC default to also include 
  line 18, 20 and 22
- added LTO barcode prefix to the config file
- checking that the Digibeta and LTO barcodes are valid

* Wed Dec 5 2007 Philip de Nier 0.8.0
- added version information to the index web page
- added review button to confidence replay
- moved pse report frames to external www and using ssi to fill in pse report name
- logging detailed VTR state during recording if it is not recognized
- checking serial port message checksums to allow invalid VTR return messages to be rejected
- changed binary browse timecode format to text format
- made link to pse report document into a pseudo button
- added file playback in cache page
- recording session will (also) be stopped when digibeta VTR stops/pauses
- reintroduced (disabled) record button to Digibeta VTR control
- reading the alternate timecode line if the primary could not be read
- the config file now specifies the alternate timecode lines
- set different page background colours for the recorder and tape export apps
- auto-correcting D3 barcodes

* Wed Nov 28 2007 Philip de Nier 0.7.2
- stop ingest when D3 VTR changes state to 'stopped' only

* Tue Nov 27 2007 Philip de Nier 0.7.1
- show the VITC in the confidence replay when starting up
- removed redundant 'Select All' button from tape export prepare page
- added config option to set the VITC and LTC VBI lines
- set keyboard focus away from 'Stop', 'Abort' etc buttons to avoid accidents
- set comments text box colour to yellow when it has focus
- remove unneccessary VTR log messages when the VTRs are switched off
- allowing the recorder to start without any ttyUSB serial devices
- added a 'Print' button in the PSE report page 

* Wed Nov 21 2007 Philip de Nier 0.7.0
- Improved browse generation performance by 10%
- VTR control disconnects or remote lockouts do not cause a recording session to stop
- Logging reason for stopped recording session
- fix endless loop in capture when SDI signal is lost when starting a capture
- moving to a new log file at the start of each day
- added configure option to disable PSE
- added auto select button to manual tape transfer
- disabled selection of items that are part of the current transfer session
- Moved position of overlays in player to safe area
- Allow shuttle control to be hot plugged in player

* Wed Nov 14 2007 Philip de Nier 0.6.0
- Bump version for installation at WMR

* Wed Oct 31 2007 Philip de Nier 0.5.0
- Update for new http recorder

* Fri Jun  8 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.4.0
- Wholesale changes for http control, replacing Corba control

* Fri Jun  8 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.3.3
- Fix mxf log redirection
- Use more robust infax metadata writing functions

* Thu May 16 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.3.2
- Fix SV_ERROR_FIFO_PUTBUFFER error by restarting fifo
- Add logging of ObserveStatus() and ObserveRecording()

* Wed May 15 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.3.1
- Added logging of MXF warnings and errors
- Fix possible recorder lock-up state when bad SDI signal received.
- Add PSE report generation which creates an HTML file.

* Fri Apr 19 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.3.0
- Tape block size is 256kB for much faster tape I/O
- Add PSE analysis during record with data added to MXF file
- Updated to new libMXF with updated keys
- player application can now play on X11 (RGB) output window

* Fri Jan 26 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.2.5
- Catch no-audio-in-video error during capture, which is triggered by
  bad SDI signal, and switch video raster to restore AV sync

* Wed Dec 6 2006 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.2.4
- Fix audio wrapping
- Correctly fix spurious StoreStatus after abort and reattach
- Catch unexpected corba exceptions
- Include MXF playback program 'player'

* Wed Nov 22 2006 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.2.3
- Fix spurious StoreStatus after abort and reattach
- Add "get now" UpdateGeneralStatus and UpdateStoreStatus on DataTape
- Fix spurious status when aborting store during rewind and before eject

* Wed Nov 22 2006 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.2.2
- Fix spurious status messages when controller disappears

* Wed Nov 22 2006 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.2.1
- Renames files before going to tape
- Recorder and DataTape abort if their controller disappears
- Minor fixes

* Tue Nov 21 2006 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.2.0
- Many more fixes
- Using 1.5.3 libraries

* Mon Nov 20 2006 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.1.1
- Many fixes

* Sun Oct  3 2006 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.1.0
- First version

