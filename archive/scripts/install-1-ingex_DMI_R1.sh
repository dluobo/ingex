#! /bin/sh
# INGEX installation script

# Run as root, of course.
if [ "$UID" -ne "$ROOT_UID" ]
then
  echo "Must be root to run this script."
  exit 1
fi  

# -----------------------------------------------------------
# Add the NVIDIA repo and install graphics drivers
zypper -n --no-gpg-checks ar http://download.nvidia.com/opensuse/11.1/ NVIDIA
zypper --no-cd -n --no-gpg-checks in X11-video-nvidiaG02 nvidia-gfxG02-kmp-default

# Remove everything to do with the beagle indexing service
zypper --no-cd -n rm kdebase3-beagle beagle beagle-index kde4-kerry kerry

# Do a non-interactive patch first in case zypper needs updating
zypper --no-cd -n patch -l

# Then do an interactive one so the kernel can be updated
zypper --no-cd patch -l

exit 0