#
# Atheros WiFi driver
#

INCLDIR=$(srctree)/drivers/net/wireless/ar6000/include
LINUX_INCLDIR=$(srctree)/drivers/net/wireless/ar6000/os/linux/include
EXTRA_CFLAGS += -I${INCLDIR} -I${LINUX_INCLDIR} -DSEND_EVENT_TO_APP -DHTC_RAW_INTERFACE -DAR6000REV2

obj-$(CONFIG_ATHEROS_AR6000)		+= ar6000.o

ar6000-objs := hif.o ar6k.o ar6k_events.o htc_send.o htc_recv.o \
              htc_services.o htc.o bmi.o ar6000_drv.o ar6000_raw_if.o \
              netbuf.o wireless_ext.o ioctl.o engine.o common_drv.o   \
              credit_dist.o wmi.o wlan_node.o wlan_recv_beacon.o      \
              wlan_utils.o

ATH_BUILD_TYPE := NATIVEMMC
ATH_BUS_TYPE := SDIO
ATH_BUS_SUBTYPE := linux_sdio
ATH_OS_SUB_TYPE := linux_2_6
export ATH_BUS_SUBTYPE

