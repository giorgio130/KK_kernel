#
# Atheros WiFi driver
#

EXTRA_CFLAGS += -DSEND_EVENT_TO_APP -DHTC_RAW_INTERFACE -DAR6000REV2 -Idrivers/net/wireless/ath6k22.133/

obj-$(CONFIG_ATHEROS_AR6000)		+= ar6000.o

ar6000-objs := \
	ar6000_drv.o \
	ar6000_raw_if.o \
	ar6k.o \
	ar6k_events.o \
	bmi.o \
	common_drv.o \
	credit_dist.o \
	eeprom.o \
	engine.o \
	hif.o \
	htc.o \
	htc_recv.o \
	htc_send.o \
	htc_services.o \
	ioctl.o \
	netbuf.o \
	wireless_ext.o \
	wlan_node.o \
	wlan_recv_beacon.o \
	wlan_utils.o \
	wmi.o

ATH_BUILD_TYPE := NATIVEMMC
ATH_BUS_TYPE := SDIO
ATH_BUS_SUBTYPE := linux_sdio
ATH_OS_SUB_TYPE := linux_2_6
export ATH_BUS_SUBTYPE

