#
# Makefile for S5PV210 LVGL Bare-metal Project
#

CROSS		?= arm-none-eabi-
NAME		:= s5pv210_lvgl_demo

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
# Compiler Flags
#
ASFLAGS		:= -g -ggdb -Wall -O3
CFLAGS		:= -g -ggdb -Wall -O3
CXXFLAGS	:= -g -ggdb -Wall -O3
LDFLAGS		:= -T s5pv210.lds -nostdlib -static
ARFLAGS		:= -rcs
OCFLAGS		:= -v -O binary
MCFLAGS		:= -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -ftree-vectorize -ffast-math -mfloat-abi=softfp

LIBDIRS		:=
LIBS 		:=
INCDIRS		:=
SRCDIRS		:=

#
# Add necessary directory for INCDIRS and SRCDIRS.
#
INCDIRS		+= include \
			   include/library \
			   include/hardware \
			   arch \
			   bsp \
			   app \
			   porting \
			   . \
			   lvgl
			   
SRCDIRS		+= arch \
			   bsp \
			   app \
			   porting \
			   library/ctype \
			   library/errno \
			   library/exit \
			   library/malloc \
			   library/stdlib \
			   library/string \
			   library/stdio \
			   library/math

#
# LVGL Inclusion
#
LVGL_CFILES := $(wildcard lvgl/src/*.c) \
             $(wildcard lvgl/src/*/*.c) \
             $(wildcard lvgl/src/*/*/*.c) \
             $(wildcard lvgl/src/*/*/*/*.c) \
             $(wildcard lvgl/src/*/*/*/*/*.c) \
             $(wildcard lvgl/src/*/*/*/*/*/*.c)
             
CSRCS := $(LVGL_CFILES)
# Do not include lvgl.mk on Windows to avoid 'find' errors

#
# Toolchains
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
CP			:= copy
RM			:= del /Q /F
else
MKDIR		:= mkdir -p
CP			:= cp -af
RM			:= rm -fr
endif

#
# Setup Sources
#
X_ASFLAGS	:= $(MCFLAGS) $(ASFLAGS)
X_CFLAGS	:= $(MCFLAGS) $(CFLAGS) -std=c99
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

X_SFILES	:= $(foreach dir, $(X_SRCDIRS), $(wildcard $(dir)/*.S))
X_CFILES	:= $(foreach dir, $(X_SRCDIRS), $(wildcard $(dir)/*.c)) $(CSRCS)
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

.PHONY:	all clean

all : $(X_NAME)
	@echo [Done] Build complete.

$(X_NAME) : $(X_OBJS)
	@echo [LD] Linking $@.elf
	@if not exist $(X_OUT) $(MKDIR) $(X_OUT)
	@$(CC) $(X_LDFLAGS) $(X_LIBDIRS) -Wl,--cref,-Map=$@.map $(X_OBJS) -o $@.elf $(X_LIBS)
	@echo [OC] Objcopying $@.bin
	@$(OC) $(X_OCFLAGS) $@.elf $@.bin

.obj/%.o : %.S
	@echo [AS] $<
	@if not exist $(subst /,\,$(dir $@)) (mkdir $(subst /,\,$(dir $@)))
	@$(AS) $(X_ASFLAGS) $(X_INCDIRS) -c $< -o $@

.obj/%.o : %.c
	@echo [CC] $<
	@if not exist $(subst /,\,$(dir $@)) (mkdir $(subst /,\,$(dir $@)))
	@$(CC) $(X_CFLAGS) $(X_INCDIRS) -c $< -o $@

.obj/%.o : %.cpp
	@echo [CXX] $<
	@if not exist $(subst /,\,$(dir $@)) (mkdir $(subst /,\,$(dir $@)))
	@$(CXX) $(X_CXXFLAGS) $(X_INCDIRS) -c $< -o $@	

clean:
ifeq ($(HOSTOS),windows)
	@if exist $(X_OUT) rmdir /s /q $(X_OUT)
	@if exist .obj rmdir /s /q .obj
else
	@$(RM) $(X_DEPS) $(X_OBJS) $(X_OBJDIRS) $(X_OUT)
endif
	@echo Clean complete.
