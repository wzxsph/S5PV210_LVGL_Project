#
# Makefile for module.
#

CROSS		?= arm-none-eabi-
NAME		:= template-key-with-led

#
# System environment variable.
ifeq ($(OS),Windows_NT)
HOSTOS		:= windows
else
ifneq (,$(findstring Linux, $(shell uname -s)))
HOSTOS		:= linux
else
$(error unable to determine host operation system)
endif
endif

#
# Load variables of flag.
#
ASFLAGS		:= -g -ggdb -Wall -O3
CFLAGS		:= -g -ggdb -Wall -O3
CXXFLAGS	:= -g -ggdb -Wall -O3
LDFLAGS		:= -T link.ld -nostdlib
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
CP			:= copy /y
RM			:= rmdir /s /q
CD			:= cd
FIND		:= find
else
MKDIR		:= mkdir -p
CP			:= cp -af
RM			:= rm -fr
CD			:= cd
FIND		:= find
endif

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

#
# Add startup object file (excluding s5pv210-irom.c for U-BOOT)
#
X_STARTUP	:= .obj/source/startup/start.o
X_OBJS		:= $(X_STARTUP) $(X_SOBJS) $(X_COBJS) $(X_CPPOBJS)

VPATH		:= $(X_OBJDIRS)

.PHONY:	all clean
all : $(X_NAME)

$(X_NAME) : $(X_OBJS)
	@echo [LD] Linking $@.elf
	@$(CC) $(X_LDFLAGS) $(X_LIBDIRS) -Wl,--cref,-Map=$@.map $^ -o $@.elf $(X_LIBS)
	@echo [OC] Objcopying $@.bin
	@$(OC) $(X_OCFLAGS) $@.elf $@.bin

clean:
ifeq ($(HOSTOS),windows)
	@powershell -Command "if (Test-Path '$(X_OUT)') { Remove-Item -Path '$(X_OUT)' -Recurse -Force }"
	@powershell -Command "if (Test-Path '.obj') { Remove-Item -Path '.obj' -Recurse -Force }"
else
	@$(RM) $(X_DEPS) $(X_OBJS) $(X_OBJDIRS) $(X_OUT)
endif
	@echo Clean complete.

#
# Startup file rule
#
$(X_STARTUP) : source/startup/start.S
	@echo [AS] $<
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	@$(AS) $(X_ASFLAGS) $(X_INCDIRS) -c $< -o $@
	@$(AS) $(X_ASFLAGS) -MD -MP -MF $@.d $(X_INCDIRS) -c $< -o $@

$(X_SOBJS) : .obj/%.o : %.S
	@echo [AS] $<
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	@$(AS) $(X_ASFLAGS) $(X_INCDIRS) -c $< -o $@
	@$(AS) $(X_ASFLAGS) -MD -MP -MF $@.d $(X_INCDIRS) -c $< -o $@

$(X_COBJS) : .obj/%.o : %.c
	@echo [CC] $<
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	@$(CC) $(X_CFLAGS) $(X_INCDIRS) -c $< -o $@
	@$(CC) $(X_CFLAGS) -MD -MP -MF $@.d $(X_INCDIRS) -c $< -o $@

$(X_CPPOBJS) : .obj/%.o : %.cpp
	@echo [CXX] $<
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	@$(CXX) $(X_CXXFLAGS) $(X_INCDIRS) -c $< -o $@	
	@$(CXX) $(X_CXXFLAGS) -MD -MP -MF $@.d $(X_INCDIRS) -c $< -o $@

#
# Create output directories
#
$(shell if not exist "$(X_OUT)" mkdir "$(X_OUT)")
$(shell if not exist ".obj" mkdir ".obj")
