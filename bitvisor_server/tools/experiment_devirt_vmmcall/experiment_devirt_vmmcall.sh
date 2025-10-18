#!/bin/bash

for (( i=1; i<=100; i++ ))
do

	cd ~/BitVisor/bitvisor_recent/tools/devirt_vmmcall
	./devirt_vmmcall

	sleep 2

	cd ~/BitVisor/bitvisor_recent/tools/revirt_kernel_module/vt
	sudo insmod revirtModule.ko

	sleep 2

	sudo rmmod revirtModule

	sleep 2
done
