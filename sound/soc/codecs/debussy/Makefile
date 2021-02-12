# Release Version 0.2.0

# Add config define "CONFIG_SND_SOC_DEBUSSY" to "y"
# Add to Makefile of prented folder: obj-$(CONFIG_SND_SOC_DEBUSSY) += debussy/

# Platform


# Basic
snd-soc-debussy-objs += debussy.o debussy_snd_ctrl.o debussy_kws.o
snd-soc-debussy-objs += debussy_intf.o debussy_intf_i2c.o debussy_intf_spi.o
snd-soc-debussy-objs += debussy_flash.o debussy_utility.o debussy_customer.o

# Option - Depend on PCBA, Chip Version and FW Function
snd-soc-debussy-objs += debussy_psram.o
snd-soc-debussy-objs += debussy_spidev.o
snd-soc-debussy-objs += debussy_genetlink.o

obj-y += snd-soc-debussy.o
