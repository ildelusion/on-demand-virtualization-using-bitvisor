#!/bin/bash
# This script should be run by a user whose public key has been
# registered to the monitor machine.

WATCH_PERIOD=1
OUTPUT_PATH="/boot/results/heartbeat.log.default"

# Usage
display_usage() {
	echo -e "\nYou must set the period of heartbeat in second using -p option."
	echo -e "If -o option is not passed, the output file will be saved into default path: $OUTPUT_PATH"
	echo -e "\nUsage:"
	echo -e "watch_heartbeat.sh [-p <watch_period_in_second>] [-o <output_file_path>]"
}

# Check parameters
if [ "$#" -eq 0 ] 		# there is no argument.
then
	display_usage
	exit 1
fi

while [ "$1" != "" ]; do # argument exists.
	case $1 in
		-p )	shift
			WATCH_PERIOD=$1
			;;
		-o )	shift
			OUTPUT_PATH=$1
			;;
		-h | --help )	display_usage
			exit
			;;	
		* )		display_usage
			exit 1
	esac
	shift
done

echo "[HEARTBEAT] WATCH_PERIOD : $WATCH_PERIOD, OUTPUT_PATH: $OUTPUT_PATH"

cd "$(dirname $0)"	# Get into the dir of this script file.
while true; do
	./heartbeat.sh >> $OUTPUT_PATH 2>&1
	sleep $WATCH_PERIOD
done
