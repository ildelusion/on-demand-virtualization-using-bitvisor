#!/bin/bash
cd bitvisor_server/tools/postcopy_migration_ready_kernel_module
sudo insmod migrationReadyModule.ko

cd bitvisor_server/tools/after_migration
./after_migration_vmmcall
