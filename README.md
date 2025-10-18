This repository includes the source code for my papers
1. "On-demand Virtualization for Live Migration in Bare Metal Cloud", ACM International Symposium on Cloud Computing [SoCC 2017]
2. "On-demand Virtualization for Post-copy OS Migration in Bare-metal Cloud", IEEE Transactions on Cloud Computing [TCC 2022]

This repository includes bitvisor-server and bitvisor-client.
The VM in bitvisor-client could be migrated to bitvisor-server. We support stop-and-copy migration and postcopy migration.
For a successful migration, the hardwares of the two computers must be the same.

# How to Postcopy Migration

### at server
./dbgsh.sh\
echoctl\
server start 20

### at client
./dbgsh.sh\
echoctl\
client connect 192.168.0.2 20 # You can see the server IP at server booting\
client send\
// Run test application at client

### at server
./postcopy_mig_ready.sh

### at client
./postcopy_mig_start.sh

# Special notes
We use PXE booting in experiments. We needed DHCP, TFTP, and NFS servers. The details are in the papers above.
