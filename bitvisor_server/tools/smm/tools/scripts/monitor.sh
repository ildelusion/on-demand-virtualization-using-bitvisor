#!/bin/bash
# Author : Jongyul Kim. Computer Architecture Lab. KAIST.
# This script is to monitor the hearbeat of a checkpoint machine.
# If the machine does not send a heartbeat, it is regarded that
# the machine has been crashed and this script will inject SMI 
# to the checkpoint machine.
# Prerequisite: minicom

# Contants ######################################################
# If the difference between current_time and file_modified_time is 
# greater than TRIGGER_THRESHOLD_SEC, trigger SMI injection.
TRIGGER_THRESHOLD_SEC=5	
WATCHED_FILE_PATH="/home/yulistic/dev/bitvisors/bitvisor_checkpointing/tools/smm/tools/scripts/heartbeat"

# Usage #########################################################
display_usage() {
	echo -e "\nYou must contruct serial connection between two machine using minicom."
	echo -e "\nUsage:"
	echo -e "monitor.sh [-f <file_path>]"
	echo -e "Options' default values:"
	echo -e	-n "\t<file_path>="
	echo -e $WATCHED_FILE_PATH
}

# Check parameters ##############################################
while [ "$1" != "" ]; do
	case $1 in
		-f )	shift
			WATCHED_FILE_PATH=$1
			;;
		-h | --help )	display_usage
			exit
			;;	
		* )		display_usage
			exit 1
	esac
	shift
done

# Compare current time with the last modified time of the WATCHED_FILE.
CURRENT_TIME=$(date +%s)
WATCHED_FILE_MODIFIED_TIME=$(stat -c "%Y" $WATCHED_FILE_PATH)

echo "Current Time: $CURRENT_TIME"
echo "File Modified Time: $WATCHED_FILE_MODIFIED_TIME"

TIME_DIFF=$(($CURRENT_TIME - $WATCHED_FILE_MODIFIED_TIME))
echo "Time difference (sec): $TIME_DIFF"

if [ $TIME_DIFF -gt $TRIGGER_THRESHOLD_SEC ];
then
	echo "Trigger SMI!"
	echo -n '1' > /dev/ttyUSB0 # Invoke SMM (trigger SMI injection) 
else
	echo "Do nothing!"
fi

