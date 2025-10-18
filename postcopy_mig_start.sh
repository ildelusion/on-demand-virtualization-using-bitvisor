#!/bin/bash
cd bitvisor_client/tools/migration_kernel_module/vt_postcopy
sudo insmod migrationModule.ko
sudo /etc/init.d/ssh restart
sudo service cron restart