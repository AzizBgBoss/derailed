# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Antonio Niño Díaz, 2024-2025

COMMIT_HASH := $(shell git describe --always --dirty)
CFLAGS += -DCOMMIT_HASH=\"$(COMMIT_HASH)\"

BLOCKSDS	?= /opt/blocksds/core

# User config

NAME 		:= derailed
GAME_TITLE	:= Derailed
GAME_SUBTITLE	:= By AzizBgBoss

GFXDIRS		:= gfx

# Libraries
# ---------

ifeq ($(DEBUG),1)
    DEFINES := -DDSWIFI_LOGS
    ARM7ELF	:= $(BLOCKSDS)/sys/arm7/main_core/arm7_dswifi_debug.elf
    LIBS	:= -ldswifi9d_noip -lnds9d
else
    ARM7ELF	:= $(BLOCKSDS)/sys/arm7/main_core/arm7_dswifi.elf
    LIBS	:= -ldswifi9_noip -lnds9
endif

LIBDIRS		:= $(BLOCKSDS)/libs/dswifi \
			   $(BLOCKSDS)/libs/libnds

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
