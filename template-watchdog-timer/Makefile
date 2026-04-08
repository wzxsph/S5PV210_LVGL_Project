#
# Makefile for module.
#

CROSS		?= arm-none-eabi-
NAME		:= template-watchdog-timer

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
# Load variables of flag.
#
ASFLAGS		:= -g -ggdb -Wall -O3
CFLAGS		:= -g -ggdb -Wall -O3
CXXFLAGS	:= -g -ggdb -Wall -O3
LDFLAGS		:= -T link.ld -nostdlib -static
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
# You shouldn't need to change anything below this point.
#
AS			:= $(CROSS)gcc -x assembler-with-cpp
CC			:= $(CROSS)gcc
CXX			:= $(CROSS)g++
LD			:= $(CROSS)ld
AR			:= $(CROSS)ar
OC			:= $(CROSS)objcopy
OD			:= $(CROSS)objdump
MKDIR		:= mkdir -p
RM			:= rm -fr
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
X_NAME		:= $(NAME)
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
X_OBJS		:= $(X_SOBJS) $(X_COBJS) $(X_CPPOBJS)

VPATH		:= $(X_OBJDIRS)

.PHONY:	all clean
all : $(X_OUT)/$(X_NAME).bin

$(X_OUT)/$(X_NAME).bin : $(X_OBJS)
	@echo [LD] Linking $(X_OUT)/$(X_NAME).elf
	@$(CC) $(X_LDFLAGS) $(X_LIBDIRS) -Wl,--cref,-Map=$(X_OUT)/$(X_NAME).map $^ -o $(X_OUT)/$(X_NAME).elf $(X_LIBS)
	@echo [OC] Objcopying $(X_OUT)/$(X_NAME).bin
	@$(OC) $(X_OCFLAGS) $(X_OUT)/$(X_NAME).elf $(X_OUT)/$(X_NAME).bin
	@echo [TFTP] TFTP bootable file created: $(X_OUT)/$(X_NAME).bin
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
ifeq ($(HOSTOS),windows)
	@if exist $(X_OUT) rmdir /s /q $(X_OUT)
	@if exist .obj rmdir /s /q .obj
else
	@$(RM) $(X_DEPS) $(X_OBJS) $(X_OBJDIRS) $(X_OUT)
endif
	@echo Clean complete.

#
# Include the dependency files, should be place the last of makefile
#
# Create directories before including dependencies
ifeq ($(HOSTOS),windows)
$(shell if not exist $(X_OUT) mkdir $(X_OUT))
$(shell if not exist .obj mkdir .obj)
$(shell if not exist .obj\source mkdir .obj\source)
$(shell if not exist .obj\source\startup mkdir .obj\source\startup)
$(shell if not exist .obj\source\hardware mkdir .obj\source\hardware)
$(shell if not exist .obj\source\arm mkdir .obj\source\arm)
$(shell if not exist .obj\source\library mkdir .obj\source\library)
$(shell if not exist .obj\source\library\ctype mkdir .obj\source\library\ctype)
$(shell if not exist .obj\source\library\errno mkdir .obj\source\library\errno)
$(shell if not exist .obj\source\library\exit mkdir .obj\source\library\exit)
$(shell if not exist .obj\source\library\malloc mkdir .obj\source\library\malloc)
$(shell if not exist .obj\source\library\stdlib mkdir .obj\source\library\stdlib)
$(shell if not exist .obj\source\library\string mkdir .obj\source\library\string)
$(shell if not exist .obj\source\library\stdio mkdir .obj\source\library\stdio)
$(shell if not exist .obj\source\library\math mkdir .obj\source\library\math)
$(shell if not exist .obj\source\graphic mkdir .obj\source\graphic)
$(shell if not exist .obj\source\graphic\maps mkdir .obj\source\graphic\maps)
$(shell if not exist .obj\source\graphic\maps\software mkdir .obj\source\graphic\maps\software)
endif
sinclude $(X_DEPS)
