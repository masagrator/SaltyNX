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

saltysd_proc/saltysd_proc.nsp:
	@cd saltysd_proc && make

saltysd_bootstrap/saltysd_bootstrap.elf: libnx_min/nx/lib/libnx_min.a
	@cd saltysd_bootstrap && make -f ./Makefile.64.mk

saltysd_bootstrap/saltysd_bootstrap32.elf: libnx32_min/nx/lib/libnx_min.a
	@cd saltysd_bootstrap && make --ignore-errors -f ./Makefile.32.mk

saltysd_core/saltysd_core.elf: saltysd_bootstrap/saltysd_bootstrap.elf
	@cd saltysd_core && make -f ./Makefile.64.mk

saltysd_core/saltysd_core32.elf: saltysd_bootstrap/saltysd_bootstrap32.elf
	@cd saltysd_core && make --ignore-errors -f ./Makefile.32.mk

sdcard_out/atmosphere/contents/0000000000534C56/exefs.nsp: saltysd_core/saltysd_core.elf saltysd_core/saltysd_core32.elf saltysd_proc/saltysd_proc.nsp
	@mkdir -p sdcard_out/atmosphere/contents/0000000000534C56/flags
	@cp $< $@
	@touch sdcard_out/atmosphere/contents/0000000000534C56/flags/boot2.flag
	@mkdir -p sdcard_out/SaltySD/flags/
	@mkdir -p sdcard_out/SaltySD/patches/
	@mkdir -p sdcard_debug/SaltySD/
	@mkdir -p sdcard_debug/atmosphere/contents/0000000000534C56/
	@touch sdcard_out/SaltySD/flags/log.flag
	@cp exceptions.txt sdcard_out/SaltySD/exceptions.txt
	@cp saltysd_core/saltysd_core32.elf sdcard_out/SaltySD/saltysd_core32.elf
	@cp saltysd_core/saltysd_core32.dbg sdcard_debug/SaltySD/saltysd_core32.dbg
	@cp saltysd_core/saltysd_core.elf sdcard_out/SaltySD/saltysd_core.elf
	@cp saltysd_core/saltysd_core.dbg sdcard_debug/SaltySD/saltysd_core.dbg
	@cp saltysd_bootstrap/saltysd_bootstrap.elf sdcard_out/SaltySD/saltysd_bootstrap.elf
	@cp saltysd_bootstrap/saltysd_bootstrap.dbg sdcard_debug/SaltySD/saltysd_bootstrap.dbg
	@cp saltysd_bootstrap/saltysd_bootstrap32_3k.elf sdcard_out/SaltySD/saltysd_bootstrap32_3k.elf
	@cp saltysd_bootstrap/saltysd_bootstrap32_5k.elf sdcard_out/SaltySD/saltysd_bootstrap32_5k.elf
	@cp saltysd_bootstrap/saltysd_bootstrap32_3k.dbg sdcard_debug/SaltySD/saltysd_bootstrap32_3k.dbg
	@cp saltysd_bootstrap/saltysd_bootstrap32_5k.dbg sdcard_debug/SaltySD/saltysd_bootstrap32_5k.dbg
	@cp saltysd_proc/saltysd_proc.nsp sdcard_out/atmosphere/contents/0000000000534C56/exefs.nsp
	@cp saltysd_proc/saltysd_proc.elf sdcard_debug/atmosphere/contents/0000000000534C56/exefs.elf
	@cp saltysd_proc/toolbox.json sdcard_out/atmosphere/contents/0000000000534C56/toolbox.json
	@cp -r exefs_patches/ sdcard_out/atmosphere/

clean:
	@rm -f saltysd_proc/data/*
	@rm -r -f sdcard_out
	@cd libnx_min && make clean
	@cd libnx32_min && make clean
	@cd saltysd_core && make -f Makefile.64.mk clean && make -f Makefile.32.mk clean
	@cd saltysd_bootstrap && make -f Makefile.64.mk clean && make -f Makefile.32.mk clean
	@cd saltysd_proc && make clean
