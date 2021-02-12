#
# Makefile for all kinds of Fingerprint
#

obj-$(CONFIG_GOODIX_FINGERPRINT) += goodix/
obj-$(CONFIG_FPC_FINGERPRINT) += fpc/
# begin, prize-pengguangyi-20190119, add for goodix gf5118m fp
obj-$(CONFIG_GOODIX_GF5118M_FINGERPRINT) += goodix-gf5118m/
# end, prize-pengguangyi-20190119, add for goodix gf5118m fp
# prize-add-pengzhipeng-20191105 end
obj-$(CONFIG_MTK_FINGERPRINT_SUNWAVECORP) += sunwave_driver_all_in_one/
obj-$(CONFIG_FT9362_FINGERPRINT) += focaltech/
obj-$(CONFIG_FT9362_FINGERPRINT_TEE) += focaltech_tee/
# prize-add-pengzhipeng-20191105-end
# prize-add-xuxiaoming-20200112-start
obj-$(CONFIG_SILEAD_FP)		+= silead/
obj-$(CONFIG_SILEAD_FP_PLATFORM)		+= silead/
# prize-add-xuxiaoming-20200112-end

#prize added by huarui, add fp driver, 20190327-start
obj-$(CONFIG_CHIPONE_FINGERPRINT_REE) += chipone_ree/
#prize added by huarui, add fp driver, 20190327-end


#prize add by lipengpeng 20200628 start
obj-$(CONFIG_CHIPONE_FINGERPRINT_TEE) += chipone_tee/
#prize add by lipengpeng 20200628 end

#prize add by tangcong 20200715 start
obj-$(CONFIG_BETTERLIFE_FINGERPRINT) += betterlife_fp/

#prize add by tangcong 20200715 end