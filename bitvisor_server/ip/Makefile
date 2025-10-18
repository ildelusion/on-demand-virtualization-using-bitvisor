# LWIP_VERSION=1.4.1
LWIP_VERSION=2.0.0

CFLAGS += -I$(DIR)/include -I$(DIR)/include/arch -I$(DIR)/include/lwip \
	-I$(DIR)/lwip-$(LWIP_VERSION)/src/include \
	-I$(DIR)/lwip-$(LWIP_VERSION)/src/include/ipv4 \
	# -I$(DIR)/lwip-$(LWIP_VERSION)/src/include/lwip \  # For lwIP threading. Not completed.


# For lwIP threading. Not completed.
# CFLAGS := $(filter-out -nostdinc, $(CFLAGS))
# LDFLAGS := $(filter-out -nostdlib, $(LDFLAGS))
# LDFLAGS += -static-libgcc -L/lib/x86_64-linux-gnu \
# LDFLAGS += -shared-libgcc -L/lib/x86_64-linux-gnu -lgcc \
# LDFLAGS += -static-libgcc -L/lib/x86_64-linux-gnu -lgcc \
# LDFLAGS += -L/lib/x86_64-linux-gnu -lgcc\
# flags-data := $(filter-out -nostdinc -nostdlib, $(flags-data))


ifeq ($(LWIP_VERSION), 1.4.1)
    objs-1 += lwip-$(LWIP_VERSION)/src/core/def.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/dhcp.o	
    objs-1 += lwip-$(LWIP_VERSION)/src/core/dns.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/init.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/mem.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/memp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/netif.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/pbuf.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/raw.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/stats.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/sys.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/tcp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/tcp_in.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/tcp_out.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/udp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/timers.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/autoip.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/igmp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/inet_chksum.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/ip.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/icmp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/inet.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/ip_addr.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/ip_frag.o
    objs-1 += lwip-$(LWIP_VERSION)/src/netif/etharp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/api_lib.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/api_msg.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/err.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/netbuf.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/netdb.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/netifapi.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/sockets.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/tcpip.o

else ifeq ($(LWIP_VERSION), 2.0.0)
    objs-1 += lwip-$(LWIP_VERSION)/src/core/def.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/dhcp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/dns.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/init.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/mem.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/memp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/netif.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/pbuf.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/raw.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/stats.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/sys.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/tcp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/tcp_in.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/tcp_out.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/udp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/autoip.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/igmp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/inet_chksum.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ip.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/icmp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/ip4_addr.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/etharp.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/ip4.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/ip4_frag.o
    objs-1 += lwip-$(LWIP_VERSION)/src/core/timeouts.o
    objs-1 += lwip-$(LWIP_VERSION)/src/netif/ethernet.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/api_lib.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/api_msg.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/err.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/netbuf.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/netdb.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/netifapi.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/sockets.o
    objs-1 += lwip-$(LWIP_VERSION)/src/api/tcpip.o

else
    @echo "Incorrect lwIP version."
    exit 1
endif

objs-1 += ip_sys.o arch/sys_arch.o
objs-1 += ip_main.o net_main.o
objs-1 += echo-server.o echo-client.o echoctl.o
