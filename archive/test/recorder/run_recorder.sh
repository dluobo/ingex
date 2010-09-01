# The player fails to open the display if ingex_recorder is run using the
# root user. To get around this the set-user-id bit is set on the ingex_recorder
# executable
sudo chown root ../../src/recorder/ingex_recorder
sudo chmod u+s ../../src/recorder/ingex_recorder

../../src/recorder/ingex_recorder --config ingex.cfg

