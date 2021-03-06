#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#find build target
PLATFORM ?= stm32f4xx
CPU ?= cortexm4f
VARIANT ?= lunchbox

APP = nanohub

#bad words
BADWORDS += strcpy strcat atoi
BADWORDS += "rsaPrivOp=RSA private ops should never be compiled into firmware. You *ARE* doing something wrong! Stop!"

#find makefiles
MAKE_PLAT = misc/platform/$(PLATFORM)/Makefile
MAKE_CPU = misc/cpu/$(CPU)/Makefile

#link paths
ifdef VARIANT_PATH
LINK_PATH_MISC = $(VARIANT_PATH)/misc
LINK_PATH_SRC = $(VARIANT_PATH)/src
LINK_PATH_INC = $(VARIANT_PATH)/inc
else
LINK_PATH_MISC = misc/variant/$(VARIANT)
LINK_PATH_SRC = src/variant/$(VARIANT)
LINK_PATH_INC = inc/variant/$(VARIANT)
endif

#variant makefile
MAKE_VAR = $(LINK_PATH_MISC)/Makefile

#top make target
SRCS := 
DELIVERABLES := 
real: all

#include makefiles for plat and cpu
include $(MAKE_PLAT)
include $(MAKE_CPU)
include $(MAKE_VAR)

FLAGS += -Wall -Werror -Iinc -Ilinks -Iexternal/freebsd/inc -fshort-double
#help avoid commmon embedded C mistakes
FLAGS += -Wmissing-declarations -Wlogical-op -Waddress -Wempty-body -Wpointer-arith -Wenum-compare -Wdouble-promotion -Wfloat-equal -Wshadow -fno-strict-aliasing

OSFLAGS += -g -ggdb3 -D_OS_BUILD_ -O2 
APPFLAGS += -Os

#debug mode
FLAGS += -DDEBUG

#frameworks
SRCS += src/printf.c src/timer.c src/seos.c src/heap.c src/slab.c src/spi.c src/trylock.c
SRCS += src/hostIntf.c src/hostIntfI2c.c src/hostIntfSpi.c src/nanohubCommand.c src/sensors.c src/syscall.c
SRCS += src/eventQ.c src/sha2.c src/rsa.c src/aes.c src/osApi.c src/appSec.c src/simpleQ.c src/floatRt.c

ifndef PLATFORM_HAS_HARDWARE_CRC
SRCS += src/softcrc.c
endif

#app code
include $(wildcard app/*/Makefile)


#extra deps
DEPS += $(wildcard app/*.h)
DEPS += $(wildcard inc/*.h)
DEPS += Makefile $(MAKE_PLAT) $(MAKE_CPU) $(MAKE_VAR)

all: symlinks $(DELIVERABLES)

symlinks: links/p_$(PLATFORM) links/c_$(CPU) links/v_$(VARIANT)

links/p_$(PLATFORM):
	rm -rf links/plat links/p_*
	mkdir -p links/plat
	ln -s ../../misc/platform/$(PLATFORM) links/plat/misc
	ln -s ../../src/platform/$(PLATFORM) links/plat/src
	ln -s ../../inc/platform/$(PLATFORM) links/plat/inc
	touch links/p_$(PLATFORM)

links/c_$(CPU):
	rm -rf links/cpu links/c_*
	mkdir -p links/cpu
	ln -s ../../misc/cpu/$(CPU) links/cpu/misc
	ln -s ../../src/cpu/$(CPU) links/cpu/src
	ln -s ../../inc/cpu/$(CPU) links/cpu/inc
	touch links/c_$(CPU)

links/v_$(VARIANT):
	rm -rf links/variant links/v_*
	mkdir -p links/variant
	ln -s ../../$(LINK_PATH_MISC) links/variant/misc
	ln -s ../../$(LINK_PATH_SRC) links/variant/src
	ln -s ../../$(LINK_PATH_INC) links/variant/inc
	touch links/v_$(VARIANT)

$(APP).unchecked.elf: symlinks $(SRCS) $(DEPS)
	$(GCC) -o $(APP).unchecked.elf $(SRCS) $(OSFLAGS) $(FLAGS) -lm -lc -lgcc

$(APP).elf: $(APP).unchecked.elf symcheck.sh
	./symcheck.sh $(APP).unchecked.elf $(APP).elf $(BADWORDS)

clean:
	rm -rf $(DELIVERABLES) $(APP).elf $(APP).unchecked.elf links $(CLEANFILES)

