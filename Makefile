
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "No se encuentra DEVKITARM. Comprueba la instalación de devkitPro.")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITARM)/3ds_rules

TARGET := Laura

BUILD := build
SOURCES := source
DATA :=
INCLUDES := include
GRAPHICS := gfx
ROMFS := romfs

APP_TITLE := Laura
APP_DESCRIPTION := Procesador de texto minimalista para Nintendo 3DS
APP_AUTHOR := Laura Dev / Fabián Contreras

ICON := assets/icon.png

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS := -g -Wall -Wextra -O2 -mword-relocations \
          -ffunction-sections \
          $(ARCH) \
          -D__3DS__

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS := -g $(ARCH)

LDFLAGS := -specs=3dsx.specs -g $(ARCH) \
           -Wl,-Map,$(notdir $*.map)

LIBS := -lctru -lm

LIBDIRS := $(CTRULIB)
