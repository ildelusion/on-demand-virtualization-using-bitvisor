#!/bin/bash

# Stop services.
#sudo service cups stop
#sudo service avahi-daemon stop

# Devirt
#../../enable_devirt/enable_devirt

# Prepare for SMM
sudo modprobe msr
sudo ./copy_smm
sudo ./invoke_smi
sudo ./serial_enable
sudo ./invoke_test

echo "Open minicom on the AMD machine."
echo "And then, send a message from remote machine through minicom."
