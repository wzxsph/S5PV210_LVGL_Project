#
# Makefile for module.
#

# Enable parallel builds by default (use MAKEFLAGS=-jN to override)
MAKEFLAGS += -j24

CROSS		?= arm-none-eabi-
NAME		:= template-framebuffer-gui

#
# System environment variable.
#
ifeq ($(OS),Windows_NT)
HOSTOS		:= windows
else
ifneq (,$(findstring Linux, $(shell uname -s)))
HOSTOS		:= linux
endif
ifneq (,$(findstring windows, $(shell uname -s)))
HOSTOS		:= windows
endif
endif

ifeq ($(strip $(HOSTOS)),)
$(error unable to determine host operation system)
endif

#
# Load variables of flag.Makefile
#
ASFLAGS		:= -g -ggdb -Wall -O3
CFLAGS		:= -g -ggdb -Wall -O3 -ffunction-sections -fdata-sections -fno-common -Wno-unused-function -Wno-unused-variable -Wno-missing-field-initializers
CXXFLAGS	:= -g -ggdb -Wall -O3
LDFLAGS		:= -T link.ld -nostdlib -static -Wl,--gc-sections
ARFLAGS		:= -rcs
OCFLAGS		:= -v -O binary
ODFLAGS		:=
MCFLAGS		:= -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -ftree-vectorize -ffast-math -mfloat-abi=softfp

LIBDIRS		:=
LIBS 		:=
INCDIRS		:=
SRCDIRS		:=

#
# Add necessary directory for INCDIRS and SRCDIRS.
#
INCDIRS		+= include \
			   include/hardware \
			   include/library
SRCDIRS		+= source \
			   source/startup \
			   source/hardware \
			   source/arm \
			   source/library \
			   source/library/ctype \
			   source/library/errno \
			   source/library/exit \
			   source/library/malloc \
			   source/library/stdlib \
			   source/library/string \
			   source/library/stdio \
			   source/library/math \
			   source/graphic \
			   source/graphic/maps/software

#
# Legacy GUI library (unused by LVGL, kept for reference)
#
INCDIRS		+= source/gui
#SRCDIRS	+= source/gui

#
# LVGL include paths
#
INCDIRS		+= . \
			   lvgl \
			   lvgl/src

#
# LVGL source directories
# Only directories for enabled features are included.
# Disabled features (see lv_conf.h):
#   - GPU backends: vg_lite, dma2d, opengles, sdl, nema_gfx, nxp, renesas, espressif
#   - Image/video codecs: ffmpeg, freetype, gif, libjpeg_turbo, libpng, libwebp,
#                        lodepng, lz4, rlottie, svg, thorvg, tiny_ttf, tjpgd, bmp, bin_decoder, barcode, qrcode, rle, fsdrv
#   - Themes: mono, simple (default theme enabled)
#   - Debug: debugging, monkey, test
#   - Enabled Debug: sysmon (perf_monitor, mem_monitor)
#   - Widgets: lottie (LV_USE_LOTTIE 0), 3dtexture (LV_USE_3DTEXTURE 0)
#   - Others: file_explorer, fragment (disabled in lv_conf.h)
#
SRCDIRS		+= lvgl/src \
			   lvgl/src/core \
			   lvgl/src/display \
			   lvgl/src/draw \
			   lvgl/src/draw/sw \
			   lvgl/src/draw/sw/blend \
			   lvgl/src/draw/sw/blend/neon \
			   lvgl/src/draw/sw/blend/helium \
			   lvgl/src/draw/sw/arm2d \
			   lvgl/src/draw/convert \
			   lvgl/src/draw/convert/neon \
			   lvgl/src/draw/convert/helium \
			   lvgl/src/draw/snapshot \
			   lvgl/src/font \
			   lvgl/src/font/fmt_txt \
			   lvgl/src/font/binfont_loader \
			   lvgl/src/font/font_manager \
			   lvgl/src/font/imgfont \
			   lvgl/src/indev \
			   lvgl/src/layouts \
			   lvgl/src/layouts/flex \
			   lvgl/src/layouts/grid \
			   lvgl/src/misc \
			   lvgl/src/misc/cache \
			   lvgl/src/misc/cache/class \
			   lvgl/src/misc/cache/instance \
			   lvgl/src/osal \
			   lvgl/src/libs/bin_decoder \
			   lvgl/src/others \
			   lvgl/src/others/translation \
			   lvgl/src/stdlib \
			   lvgl/src/stdlib/builtin \
			   lvgl/src/stdlib/clib \
			   lvgl/src/themes \
			   lvgl/src/themes/default \
			   lvgl/src/themes/simple \
			   lvgl/src/tick \
			   lvgl/src/widgets/animimage \
			   lvgl/src/widgets/arc \
			   lvgl/src/widgets/arclabel \
			   lvgl/src/widgets/bar \
			   lvgl/src/widgets/button \
			   lvgl/src/widgets/buttonmatrix \
			   lvgl/src/widgets/calendar \
			   lvgl/src/widgets/canvas \
			   lvgl/src/widgets/chart \
			   lvgl/src/widgets/checkbox \
			   lvgl/src/widgets/dropdown \
			   lvgl/src/widgets/gif \
			   lvgl/src/widgets/image \
			   lvgl/src/widgets/imagebutton \
			   lvgl/src/widgets/ime \
			   lvgl/src/widgets/keyboard \
			   lvgl/src/widgets/label \
			   lvgl/src/widgets/led \
			   lvgl/src/widgets/line \
			   lvgl/src/widgets/list \
			   lvgl/src/widgets/menu \
			   lvgl/src/widgets/msgbox \
			   lvgl/src/widgets/objx_templ \
			   lvgl/src/widgets/property \
			   lvgl/src/widgets/roller \
			   lvgl/src/widgets/scale \
			   lvgl/src/widgets/slider \
			   lvgl/src/widgets/span \
			   lvgl/src/widgets/spinbox \
			   lvgl/src/widgets/spinner \
			   lvgl/src/widgets/switch \
			   lvgl/src/widgets/table \
			   lvgl/src/widgets/tabview \
			   lvgl/src/widgets/textarea \
			   lvgl/src/widgets/tileview \
			   lvgl/src/widgets/win \
			   lvgl/src/drivers \
			   lvgl/src/drivers/display \
			   lvgl/src/debugging/sysmon \
			   lvgl/demos \
			   lvgl/demos/widgets \
			   lvgl/demos/widgets/assets

#
# You shouldn't need to change anything below this point.
#
AS			:= $(CROSS)gcc -x assembler-with-cpp
CC			:= $(CROSS)gcc
CXX			:= $(CROSS)g++
LD			:= $(CROSS)ld
AR			:= $(CROSS)ar
OC			:= $(CROSS)objcopy
OD			:= $(CROSS)objdump
ifeq ($(HOSTOS),windows)
MKDIR		:= mkdir
else
MKDIR		:= mkdir -p
endif
CP			:= cp -af
RM			:= rm -rf
CD			:= cd
FIND		:= find

#
# X variables
#
X_ASFLAGS	:= $(MCFLAGS) $(ASFLAGS)
X_CFLAGS	:= $(MCFLAGS) $(CFLAGS)
X_CXXFLAGS	:= $(MCFLAGS) $(CXXFLAGS)
X_LDFLAGS	:= $(LDFLAGS)
X_OCFLAGS	:= $(OCFLAGS)
X_LIBDIRS	:= $(LIBDIRS)
X_LIBS		:= $(LIBS) -lgcc

X_OUT		:= output
X_NAME		:= $(patsubst %, $(X_OUT)/%, $(NAME))
X_INCDIRS	:= $(patsubst %, -I %, $(INCDIRS))
X_SRCDIRS	:= $(patsubst %, %, $(SRCDIRS))
X_OBJDIRS	:= $(patsubst %, .obj/%, $(X_SRCDIRS))
EMPTY		:=
SPACE		:= $(EMPTY) $(EMPTY)
COMMA		:= ,
ifeq ($(HOSTOS),windows)
X_MKDIRS	:= $(subst /,\,$(X_OBJDIRS)) $(subst /,\,$(X_OUT))
X_MKDIRS_PS	:= $(subst $(SPACE),$(COMMA),$(foreach d,$(X_MKDIRS),'$(d)'))
else
X_MKDIRS	:= $(X_OBJDIRS) $(X_OUT)
endif

X_SFILES	:= $(foreach dir, $(X_SRCDIRS), $(wildcard $(dir)/*.S))
X_CFILES	:= $(foreach dir, $(X_SRCDIRS), $(wildcard $(dir)/*.c))
X_CPPFILES	:= $(foreach dir, $(X_SRCDIRS), $(wildcard $(dir)/*.cpp))

X_SDEPS		:= $(patsubst %, .obj/%, $(X_SFILES:.S=.o.d))
X_CDEPS		:= $(patsubst %, .obj/%, $(X_CFILES:.c=.o.d))
X_CPPDEPS	:= $(patsubst %, .obj/%, $(X_CPPFILES:.cpp=.o.d))
X_DEPS		:= $(X_SDEPS) $(X_CDEPS) $(X_CPPDEPS)

X_SOBJS		:= $(patsubst %, .obj/%, $(X_SFILES:.S=.o))
X_COBJS		:= $(patsubst %, .obj/%, $(X_CFILES:.c=.o))
X_CPPOBJS	:= $(patsubst %, .obj/%, $(X_CPPFILES:.cpp=.o))
X_OBJS		:= $(X_SOBJS) $(X_COBJS) $(X_CPPOBJS)

VPATH		:= $(X_OBJDIRS)

.PHONY:	all clean dirs

all : dirs $(X_NAME)

dirs:
ifeq ($(HOSTOS),windows)
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path @($(X_MKDIRS_PS)) | Out-Null"
else
	@$(MKDIR) $(X_MKDIRS)
endif

$(X_NAME) : $(X_OBJS)
	@echo [LD] Linking $@.elf
	@$(CC) $(X_LDFLAGS) $(X_LIBDIRS) -Wl,--cref,-Map=$@.map $^ -o $@.elf $(X_LIBS)
	@echo [OC] Objcopying $@.bin
	@$(OC) $(X_OCFLAGS) $@.elf $@.bin
	@echo [TFTP] TFTP bootable file created: $@.bin
	@echo Ready for U-BOOT tftpboot command

$(X_SOBJS) : .obj/%.o : %.S
	@echo [AS] $<
	@$(AS) $(X_ASFLAGS) $(X_INCDIRS) -c $< -o $@
	@$(AS) $(X_ASFLAGS) -MD -MP -MF $@.d $(X_INCDIRS) -c $< -o $@

$(X_COBJS) : .obj/%.o : %.c
	@echo [CC] $<
	@$(CC) $(X_CFLAGS) $(X_INCDIRS) -c $< -o $@
	@$(CC) $(X_CFLAGS) -MD -MP -MF $@.d $(X_INCDIRS) -c $< -o $@

$(X_CPPOBJS) : .obj/%.o : %.cpp
	@echo [CXX] $<
	@$(CXX) $(X_CXXFLAGS) $(X_INCDIRS) -c $< -o $@
	@$(CXX) $(X_CXXFLAGS) -MD -MP -MF $@.d $(X_INCDIRS) -c $< -o $@

clean:
	@$(RM) .obj output
	@echo Clean complete.

sinclude $(X_DEPS)
