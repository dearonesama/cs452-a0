XDIR:=/u/cs452/public/xdev
ARCH=cortex-a72
TRIPLE=aarch64-none-elf
XBINDIR:=$(XDIR)/bin
CC:=$(XBINDIR)/$(TRIPLE)-gcc
OBJCOPY:=$(XBINDIR)/$(TRIPLE)-objcopy
OBJDUMP:=$(XBINDIR)/$(TRIPLE)-objdump
OUTPUT := build

# COMPILE OPTIONS
WARNINGS=-Wall -Wextra -Wpedantic -Wno-unused-const-variable
CFLAGS:=-g -pipe -static $(WARNINGS) -ffreestanding -nostartfiles\
	-mcpu=$(ARCH) -static-pie -mstrict-align -fno-builtin -mgeneral-regs-only

# -Wl,option tells g++ to pass 'option' to the linker with commas replaced by spaces
# doing this rather than calling the linker ourselves simplifies the compilation procedure
LDFLAGS:=-Wl,-nmagic -Wl,-Tlinker.ld

# Source files and include dirs
SOURCES := $(wildcard *.c) $(wildcard *.S)
# Create .o and .d files for every .cc and .S (hand-written assembly) file
OBJECTS := $(patsubst %, $(OUTPUT)/%, $(patsubst %.c, %.o, $(patsubst %.S, %.o, $(SOURCES))))
DEPENDS := $(patsubst %, $(OUTPUT)/%, $(patsubst %.c, %.d, $(patsubst %.S, %.d, $(SOURCES))))

# The first rule is the default, ie. "make", "make all" and "make kernel8.img" mean the same
all: $(OUTPUT) $(OUTPUT)/kernel8.img

$(OUTPUT):
	mkdir -p $(OUTPUT)

clean:
	rm -rf $(OUTPUT)

$(OUTPUT)/kernel8.img: $(OUTPUT)/kernel8.elf
	$(OBJCOPY) $< -O binary $@

$(OUTPUT)/kernel8.elf: $(OBJECTS) linker.ld
	$(CC) $(CFLAGS) $(filter-out %.ld, $^) -o $@ $(LDFLAGS)
	@$(OBJDUMP) -d $(OUTPUT)/kernel8.elf | fgrep -q q0 && printf "\n***** WARNING: SIMD INSTRUCTIONS DETECTED! *****\n\n" || true

$(OUTPUT)/%.o: %.c Makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTPUT)/%.o: %.S Makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPENDS)
