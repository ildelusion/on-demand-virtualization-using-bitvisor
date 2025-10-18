#!/bin/bash
# Author : Jongyul Kim. Computer Architecture Lab. KAIST.
# This script is to send a heartbeat to the monitoring machine.
# You need to register the ssh-key of the local machine to the monitoring machine for crontab.
# Prerequisites: ssh

HEARTBEAT_PATH="/home/yulistic/dev/bitvisors/bitvisor_checkpointing/tools/smm/tools/scripts/heartbeat"
USERNAME="yulistic"
MONITOR_MACHINE="143.248.140.179"

# Usage
display_usage() {
	echo -e "\nYou must register the local ssh public key to the monitoring machine for crontab."
	echo -e "\nUsage:"
	echo -e "heartbeat.sh [-f <file_path>] [-u <username_of_monitor_machine>] [-m <monitor_machine>]"
	echo -e "Options' default values:"
	echo -e -n	"\t<file_path>="
	echo -e $HEARTBEAT_PATH
	echo -e -n	"\t<username_of_monitor_machine>="
	echo -e $USERNAME
	echo -e -n	"\t<monitor_machine>="
	echo -e $MONITOR_MACHINE
}

# Check parameters
while [ "$1" != "" ]; do
	case $1 in
		-f )	shift
			HEARTBEAT_PATH=$1
			;;
		-u )	shift
			USERNAME=$1
			;;
		-m )	shift
			MONITOR_MACHINE=$1
			;;
		-h | --help )	display_usage
			exit
			;;	
		* )		display_usage
			exit 1
	esac
	shift
done

#echo -e "ssh $USERNAME@$MONITOR_MACHINE \"touch $HEARTBEAT_PATH\""
ssh $USERNAME@$MONITOR_MACHINE "touch $HEARTBEAT_PATH"
