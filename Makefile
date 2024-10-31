#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

all: sdcard_out/atmosphere/contents/0000000000534C56/exefs.nsp

libnx_min/nx/lib/libnx_min.a:
	@cd libnx_min && make

libnx32_min/nx/lib/libnx_min.a:
	@cd libnx32_min && make

saltysd_proc/saltysd_proc.nsp: libnx_min/nx/lib/libnx_min.a
	@cd saltysd_proc && make

saltysd_bootstrap/saltysd_bootstrap.elf: saltysd_proc/saltysd_proc.nsp
	@cd saltysd_bootstrap && make

saltysd_bootstrap32/saltysd_bootstrap32.elf: libnx32_min/nx/lib/libnx_min.a
	@cd saltysd_bootstrap32 && make --ignore-errors

saltysd_core/saltysd_core.elf: saltysd_bootstrap/saltysd_bootstrap.elf
	@cd saltysd_core && make

saltysd_core32/saltysd_core32.elf: saltysd_bootstrap32/saltysd_bootstrap32.elf
	@cd saltysd_core32 && make --ignore-errors

sdcard_out/atmosphere/contents/0000000000534C56/exefs.nsp: saltysd_core/saltysd_core.elf saltysd_core32/saltysd_core32.elf
	@mkdir -p sdcard_out/atmosphere/contents/0000000000534C56/flags
	@cp $< $@
	@touch sdcard_out/atmosphere/contents/0000000000534C56/flags/boot2.flag
	@mkdir -p sdcard_out/SaltySD/flags/
	@mkdir -p sdcard_out/SaltySD/patches/
	@touch sdcard_out/SaltySD/flags/log.flag
	@cp exceptions.txt sdcard_out/SaltySD/exceptions.txt
	@cp toolbox.json sdcard_out/atmosphere/contents/0000000000534C56/toolbox.json
	@cp saltysd_core32/saltysd_core32.elf sdcard_out/SaltySD/saltysd_core32.elf
	@cp saltysd_core/saltysd_core.elf sdcard_out/SaltySD/saltysd_core.elf
	@cp saltysd_bootstrap/saltysd_bootstrap.elf sdcard_out/SaltySD/saltysd_bootstrap.elf
	@cp saltysd_bootstrap32/saltysd_bootstrap32_3k.elf sdcard_out/SaltySD/saltysd_bootstrap32_3k.elf
	@cp saltysd_bootstrap32/saltysd_bootstrap32_5k.elf sdcard_out/SaltySD/saltysd_bootstrap32_5k.elf
	@cp saltysd_proc/saltysd_proc.nsp sdcard_out/atmosphere/contents/0000000000534C56/exefs.nsp

clean:
	@rm -f saltysd_proc/data/*
	@rm -r -f sdcard_out
	@cd libnx_min && make clean
	@cd libnx32_min && make clean
	@cd saltysd_core && make clean
	@cd saltysd_core32 && make clean
	@cd saltysd_bootstrap && make clean
	@cd saltysd_bootstrap32 && make clean
	@cd saltysd_proc && make clean
