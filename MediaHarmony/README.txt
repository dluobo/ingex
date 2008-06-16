To build MediaHarmony you must have the Samba source code installed.
  rpm -i samba-3.0.23d-6.src.rpm
  (tip: use yast to install the src rpm so dependencies will be installed)

MediaHarmony depends on header files generated during compilation of Samba,
so start building Samba from source.  You can optionally stop this process
with Control-C after proto.h is created to save time.
  rpmbuild -bc /usr/src/packages/SPECS/samba.spec

Build MediaHarmony from source:
  cd ingex/MediaHarmony
  make SAMBA_DIR=/usr/src/packages/BUILD/samba-3.0.23d/

The resulting *.so files need to be installed in Samba's vfs directory:
  sudo cp *.so /usr/lib/samba/vfs/

Edit /etc/samba/smb.conf to enable MediaHarmony features:
[avid]
        comment = Multiple Avid client access to media files for Windows
        path = /video
        valid users = archive
        vfs objects = avid_full_audit media_harmony
        avid_full_audit:prefix = windows|%u|%I
        avid_full_audit:success = all
        avid_full_audit:failure = all
        veto files = /.*/
        delete veto files = yes
        writeable = yes
        browseable = yes
[fcp]
         comment = Exposes essence data in MXF files as virtual files
         path = /video
         vfs objects = mxf_harmony
         writeable = yes
         browseable = yes
