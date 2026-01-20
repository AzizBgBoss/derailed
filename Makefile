# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Antonio Niño Díaz, 2024-2025

COMMIT_HASH := $(shell git describe --always)
CFLAGS += -DCOMMIT_HASH=\"$(COMMIT_HASH)\"

BLOCKSDS	?= /opt/blocksds/core

# User config

NAME 		:= derailed
GAME_TITLE	:= Derailed
GAME_SUBTITLE	:= By AzizBgBoss
GAME_AUTHOR	:= github.com/AzizBgBoss/derailed

GAME_ICON    := media/icon.png

GFXDIRS		:= gfx
AUDIODIRS	:= sfx
NITROFSDIR	:= nitrofs

# Libraries

ARM7ELF	:= $(BLOCKSDS)/sys/arm7/main_core/arm7_dswifi_maxmod.elf
LIBS	:= -ldswifi9_noip -lnds9 -lmm9
    
LIBDIRS		:= $(BLOCKSDS)/libs/dswifi \
			   $(BLOCKSDS)/libs/libnds \
               $(BLOCKSDS)/libs/maxmod

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
