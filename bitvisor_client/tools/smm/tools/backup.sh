#!/bin/bash
VAR_BACKUP_RESULT="0"

sudo ./revirt
while :
do
	sudo ../../checkpoint/backup
	VAR_BACKUP_RESULT="$?"
	if [ "$VAR_BACKUP_RESULT" = "5" ] || [ "$VAR_BACKUP_RESULT" = "6" ] ; then
		break
	fi
done

if [ "$VAR_BACKUP_RESULT" = "5" ] ; then
	sudo ../../enable_devirt/enable_devirt
elif [ "$VAR_BACKUP_RESULT" = "6" ] ; then
# device initialization
#	sudo /etc/init.d/networking restart
	sudo service lightdm stop
	sudo sh -c "echo 0000:01:05.0 > /sys/bus/pci/drivers/radeon/unbind"
	sudo sh -c "echo 0000:01:05.0 > /sys/bus/pci/drivers/radeon/bind"
	sudo service lightdm start
# network initialization
#sudo /etc/init.d/gdm restart
#sudo /etc/init.d/udev restart
	sudo ../../enable_devirt/enable_devirt
	sudo service network-manager restart
fi
