#
# Makefile for the kernel flashlight device drivers.
#

flashlight-y := flashlight-core.o
flashlight-y += flashlight-device.o

obj-$(CONFIG_MTK_FLASHLIGHT)             += flashlight.o
obj-$(CONFIG_MTK_FLASHLIGHT_LED191)      += flashlights-led191.o
obj-$(CONFIG_MTK_FLASHLIGHT_LM3642)      += flashlights-lm3642.o
obj-$(CONFIG_MTK_FLASHLIGHT_LM3643)      += flashlights-lm3643.o
obj-$(CONFIG_MTK_FLASHLIGHT_LM3644)      += flashlights-lm3644.o
obj-$(CONFIG_MTK_FLASHLIGHT_RT4505)      += flashlights-rt4505.o
obj-$(CONFIG_MTK_FLASHLIGHT_RT5081)      += flashlights-rt5081.o
obj-$(CONFIG_MTK_FLASHLIGHT_MT6336)      += flashlights-mt6336.o
obj-$(CONFIG_MTK_FLASHLIGHT_MT6370)      += flashlights-mt6370.o
obj-$(CONFIG_MTK_FLASHLIGHT_MT6360)      += flashlights-mt6360.o
obj-$(CONFIG_MTK_FLASHLIGHT_DUMMY)       += flashlights-dummy.o
obj-$(CONFIG_MTK_FLASHLIGHT_DUMMY_GPIO)  += flashlights-dummy-gpio.o
#/*zhengjiang.zhu@prize.Camera.Driver  20200116  start for sub_flash_aw36402*/
obj-$(CONFIG_MTK_FLASHLIGHT_AW36402_GPIO)  += flashlights-aw36402-gpio.o
obj-$(CONFIG_MTK_FLASHLIGHT_AW36518)  += flashlights-aw36518.o
#/*zhengjiang.zhu@prize.Camera.Driver  20200116  start for sub_flash_aw36402*/

ccflags-$(CONFIG_MTK_FLASHLIGHT_DEBUG)     := -DDEBUG
ccflags-$(CONFIG_MTK_FLASHLIGHT_LED191)    += -DCONFIG_MTK_FLASHLIGHT_LED191
ccflags-$(CONFIG_MTK_FLASHLIGHT_PT)        += -DCONFIG_MTK_FLASHLIGHT_PT
ccflags-$(CONFIG_MTK_FLASHLIGHT_PT_STRICT) += -DCONFIG_MTK_FLASHLIGHT_PT_STRICT
ccflags-$(CONFIG_MTK_FLASHLIGHT_DLPT)      += -DCONFIG_MTK_FLASHLIGHT_DLPT

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
MTK_CUSTOM_PATH := $(srctree)/drivers/misc/mediatek

ccflags-y += -D$(MTK_PLATFORM) -D$(MTK_PROJECT)

subdir-ccflags-y += \
	-I$(MTK_CUSTOM_PATH)/include/mt-plat \
	-I$(MTK_CUSTOM_PATH)/include/mt-plat/$(MTK_PLATFORM)/include/mach \
	-I$(MTK_CUSTOM_PATH)/pmic/include \
	-I$(MTK_CUSTOM_PATH)/pmic


#prize added by huarui, flashlight support, 20190111-start
obj-$(CONFIG_MTK_FLASHLIGHT_AW3641_GPIO)     += flashlights-aw3641-gpio.o
obj-$(CONFIG_MTK_FLASHLIGHT_WD3100D_GPIO)     += flashlights-wd3100d-gpio.o
obj-$(CONFIG_MTK_FLASHLIGHT_ISINK)     += flashlights-isink.o
obj-$(CONFIG_MTK_FLASHLIGHT_SGM37898)      += flashlights-sgm37898.o
obj-$(CONFIG_MTK_FLASHLIGHT_SGM37895)      += flashlights-sgm37895.o
obj-$(CONFIG_MTK_FLASHLIGHT_SGM37895A)      += flashlights-sgm37895a.o
#prize added by huarui, flashlight support, 20190111-end