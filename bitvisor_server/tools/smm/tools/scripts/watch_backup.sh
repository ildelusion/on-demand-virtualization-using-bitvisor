#!/bin/bash

WATCH_PERIOD=1
OUTPUT_PATH="/boot/results/backup.log.default"

# Usage
display_usage() {
	echo -e "\nYou must set the period of WATCH in second using -p option and the path for output file using -o option."
	echo -e "If -o option is not passed, the output file will be saved into default path: $OUTPUT_PATH"
	echo -e "\nUsage:"
	echo -e "watch_backup.sh [-p <watch_period_in_second>] [-o <output_file_path>]"
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

# Change to root.
#[ `whoami` = root ] || exec su -c "$0 -p $WATCH_PERIOD -o $OUTPUT_PATH" root

echo "[BACKUP] WATCH_PERIOD : $WATCH_PERIOD, OUTPUT_PATH: $OUTPUT_PATH"

cd "$(dirname $0)/.."	# backup.sh exists in the parent directory. Get into the parent dir.
while true; do
	./backup.sh >> $OUTPUT_PATH 2>&1 && echo "backup success at $(date +%Y)-$(date +%m)-$(date +%d) $(date +%H):$(date +%M):$(date +%S)" >> $OUTPUT_PATH
	sleep $WATCH_PERIOD
done
