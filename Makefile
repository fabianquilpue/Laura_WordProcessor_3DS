
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "No se encuentra DEVKITARM. Comprueba la instalacion de devkitPro.")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITARM)/3ds_rules

# --------------------------------------------------
# Configuracion general
# --------------------------------------------------

TARGET := Laura

BUILD := build
SOURCES := source
DATA :=
INCLUDES := include
GRAPHICS := gfx
GFXBUILD := $(BUILD)
ROMFS := romfs

# --------------------------------------------------
# Metadatos
# --------------------------------------------------

APP_TITLE := Laura
APP_DESCRIPTION := Procesador de texto minimalista para Nintendo 3DS
APP_AUTHOR := Laura Dev / Fabian Contreras

ICON := assets/icon.png

# --------------------------------------------------
# Rutas de libctru
# --------------------------------------------------

CTRULIB ?= $(DEVKITPRO)/libctru

LIBDIRS := $(CTRULIB)

export INCLUDE := -I$(TOPDIR)/include \
                  -I$(CTRULIB)/include \
                  -I$(TOPDIR)/$(BUILD)

# --------------------------------------------------
# Arquitectura Nintendo 3DS
# --------------------------------------------------

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS := -g -Wall -Wextra -O2 -mword-relocations \
          -ffunction-sections -fdata-sections \
          $(ARCH) \
          $(INCLUDE) \
          -D__3DS__

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS := -g $(ARCH) $(INCLUDE)

LDFLAGS = -specs=3dsx.specs -g $(ARCH) \
          -Wl,-Map,$(notdir $*.map)

LIBS := -lctru -lm

# --------------------------------------------------
# Primera fase: preparacion
# --------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)

export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                $(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
                $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

PICAFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.shlist)))

GFXFILES := $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

# --------------------------------------------------
# Seleccion del enlazador
# --------------------------------------------------

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

# --------------------------------------------------
# Recursos graficos
# --------------------------------------------------

ifeq ($(GFXBUILD),$(BUILD))
export T3XFILES := $(GFXFILES:.t3s=.t3x)
else
export ROMFS_T3XFILES := $(patsubst %.t3s,$(GFXBUILD)/%.t3x,$(GFXFILES))
export T3XHFILES := $(patsubst %.t3s,$(BUILD)/%.h,$(GFXFILES))
endif

# --------------------------------------------------
# Archivos objeto
# --------------------------------------------------

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) \
                         $(CFILES:.c=.o) \
                         $(SFILES:.s=.o)

export OFILES_BIN := $(addsuffix .o,$(BINFILES)) \
                     $(PICAFILES:.v.pica=.shbin.o) \
                     $(SHLISTFILES:.shlist=.shbin.o) \
                     $(addsuffix .o,$(T3XFILES))

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(PICAFILES:.v.pica=_shbin.h) \
                 $(SHLISTFILES:.shlist=_shbin.h) \
                 $(addsuffix .h,$(subst .,_,$(BINFILES))) \
                 $(GFXFILES:.t3s=.h)

# --------------------------------------------------
# Bibliotecas
# --------------------------------------------------

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# --------------------------------------------------
# Configuracion SMDH
# --------------------------------------------------

export _3DSXDEPS := $(if $(NO_SMDH),,$(OUTPUT).smdh)

export APP_ICON := $(TOPDIR)/$(ICON)

ifeq ($(strip $(NO_SMDH)),)
export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

# --------------------------------------------------
# Objetivos principales
# --------------------------------------------------

.PHONY: all clean

all: $(BUILD) $(GFXBUILD) $(DEPSDIR) $(ROMFS_T3XFILES) $(T3XHFILES)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

ifneq ($(GFXBUILD),$(BUILD))
$(GFXBUILD):
	@mkdir -p $@
endif

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

# --------------------------------------------------
# Limpiar
# --------------------------------------------------

clean:
	@echo "Limpiando compilacion..."
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(GFXBUILD)

# --------------------------------------------------
# Conversion de graficos
# --------------------------------------------------

$(GFXBUILD)/%.t3x $(BUILD)/%.h: %.t3s
	@echo "Convirtiendo recurso grafico: $(notdir $<)"
	@tex3ds -i $< -H $(BUILD)/$*.h -d $(DEPSDIR)/$*.d -o $(GFXBUILD)/$*.t3x

# --------------------------------------------------
# Segunda fase: compilacion
# --------------------------------------------------

else

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES): $(HFILES)

$(OUTPUT).elf: $(OFILES)

%.bin.o %_bin.h: %.bin
	@echo "Convirtiendo archivo: $(notdir $<)"
	@$(bin2o)

.PRECIOUS: %.t3x %.shbin

%.t3x.o %_t3x.h: %.t3x
	@$(SILENTMSG) $(notdir $<)
	@$(bin2o)

%.shbin.o %_shbin.h: %.shbin
	@$(SILENTMSG) $(notdir $<)
	@$(bin2o)

-include $(DEPSDIR)/*.d

endif
