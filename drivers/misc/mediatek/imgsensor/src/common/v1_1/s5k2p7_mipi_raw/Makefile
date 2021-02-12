#include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y += s5k2p7mipiraw_Sensor.o
#obj-y += s5k2p7_pdafotp.o
#obj-y += s5k2p7_otp.o
# [AGOLD][hbb][20190215] stereo cam 
ifeq ($(AGOLD_STEREOCAM_SUPPORT_XAPI_S5K2P7), yes)
obj-y += xapi_3d_cal.o
endif
