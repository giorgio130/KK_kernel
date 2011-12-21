obj-$(CONFIG_MXC_IPU_V1) = mxc_ipu.o

mxc_ipu-objs := ipu_common.o ipu_sdc.o ipu_adc.o ipu_ic.o ipu_csi.o ipu_device.o

ifdef CONFIG_MACH_LUIGI_LAB126
mxc_ipu-objs += ipu_l126_detect.o 
endif

obj-$(CONFIG_MXC_IPU_PF)        +=   pf/
