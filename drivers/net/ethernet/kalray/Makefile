#
# Makefile for the Kalray device drivers.
#
kvx_eth-y :=	kvx-net.o kvx-net-hw.o kvx-net-tx.o kvx-mac-hw.o kvx-phy-hw.o \
		kvx-net-parsing.o kvx-ethtool.o kvx-net-fs.o \
		kvx-net-def-filters.o

kvx_eth-$(CONFIG_DCB) += kvx-dcb.o

obj-$(CONFIG_NET_KVX_SOC) += kvx_eth.o
