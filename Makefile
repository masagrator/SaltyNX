#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/devkitA64/base_rules

all: libnx_min/nx/lib/libnx_min.a sdcard_out/SaltySD/saltysd_core.elf sdcard_out/atmosphere/contents/0000000000534C56/exefs.nsp

libnx_min/nx/lib/libnx_min.a:
	@cd libnx_min && make

saltysd_proc/saltysd_proc.nsp: saltysd_proc/data/saltysd_bootstrap.elf
	@cd saltysd_proc && make

saltysd_bootstrap/saltysd_bootstrap.elf:
	@cd saltysd_bootstrap && make

saltysd_core/saltysd_core.elf: libnx_min/nx/lib/libnx_min.a
	@cd saltysd_core && make

sdcard_out/SaltySD/saltysd_bootstrap.elf: saltysd_bootstrap/saltysd_bootstrap.elf
	@mkdir -p sdcard_out/SaltySD/
	@cp $< $@

sdcard_out/SaltySD/saltysd_core.elf: saltysd_core/saltysd_core.elf
	@mkdir -p sdcard_out/SaltySD/
	@cp $< $@

sdcard_out/atmosphere/contents/0000000000534C56/exefs.nsp: saltysd_proc/saltysd_proc.nsp
	@mkdir -p sdcard_out/atmosphere/contents/0000000000534C56/flags
	@cp $< $@
	@touch sdcard_out/atmosphere/contents/0000000000534C56/flags/boot2.flag
	@mkdir -p sdcard_out/SaltySD/flags/
	@mkdir -p sdcard_out/SaltySD/patches/
	@touch sdcard_out/SaltySD/flags/log.flag
	@cp exceptions.txt sdcard_out/SaltySD/exceptions.txt
	@cp toolbox.json sdcard_out/atmosphere/contents/0000000000534C56/toolbox.json


clean:
	@rm -f saltysd_proc/data/*
	@rm -r -f sdcard_out
	@cd libnx_min && make clean
	@cd saltysd_core && make clean
	@cd saltysd_bootstrap && make clean
	@cd saltysd_proc && make clean
