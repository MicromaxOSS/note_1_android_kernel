subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
# In case the platform does NOT support this type of sensors
obj-y   +=  mtk_tpd.o
obj-y   +=  tpd_button.o
obj-y   +=  tpd_calibrate.o
obj-y   +=  tpd_debug.o
obj-y   +=  tpd_default.o
obj-y   +=  tpd_misc.o
obj-y   +=  tpd_setting.o
obj-$(CONFIG_TOUCHSCREEN_MTK_SOLOMON)	+=  SOLOMON/
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS)	+=  focaltech_touch/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT1151)	+=  GT1151/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT911)	+=  GT911/
obj-$(CONFIG_TOUCHSCREEN_MTK_GSLX680)	+=  gslX680/
obj-$(CONFIG_TOUCHSCREEN_MTK_NT36xxx)	+=  NT36xxx/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT9XX_LM)	+=  GT9XX_LM/
obj-$(CONFIG_TOUCHSCREEN_FT5X26)	+=  FT5X26/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT5688)	+=  GT5688/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_I2C)	+=  hxchipset/
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)	+=  focaltech_fhd_touch/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT1X_V14)	+=  GT1X_V14/
# begin, prize-lifenfen-20181206, add for new touchscreen goodix_gtx8
obj-$(CONFIG_TOUCHSCREEN_GOODIX_GTX8) += goodix_gtx8/
# end, prize-lifenfen-20181206, add for new touchscreen goodix_gtx8
#prize-add HX83112-pengguangyi-20190119-start
obj-$(CONFIG_TOUCHSCREEN_MTK_HX83112)	+=  HX83112/
#prize-add HX83112-pengzhipeng-20190119-end
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM)	+=  synaptics_tcm/
obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_touch_spi/

#prize-add-pengzhipeng-20191029-start
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS_V3) +=  focaltech_touch_V3/
#prize-add-pengzhipeng-20191029-end

#prize-add-pengzhipeng-20191029-start
obj-$(CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI) +=  NVT_Touch_HostDL/
#prize-add-pengzhipeng-20191029-end

#prize added by chenjiaxi, add GSL1691 driver, 20190327-start
obj-$(CONFIG_TOUCHSCREEN_MTK_GSL1691)  +=  GSL1691/
#prize added by chenjiaxi, add GSL1691 driver, 20190327-end
#prize added by huarui, add cst3xx driver, 20190327-start
obj-$(CONFIG_TOUCHSCREEN_MTK_CST3XX)  +=  cst3xx/
#prize added by huarui, add cst3xx driver, 20190327-end
#prize added by huarui, add tp driver, 20190327-start
obj-$(CONFIG_TOUCHSCREEN_MTK_ILITEK_V2)  +=  ilitek_v2/
#prize added by huarui, add tp driver, 20190327-end

#prize add by lipengpeng 20200716 start
obj-$(CONFIG_TOUCHSCREEN_MTK_ILI9882_V3)  +=  ili9882_v3/
#prize add by lipengpeng 20200716 end
#prize-add-songhezhu-20191231-start
obj-$(CONFIG_TOUCHSCREEN_CHIPONE) += chipone-tddi/
#prize-add-songhezhu-20191231-end
