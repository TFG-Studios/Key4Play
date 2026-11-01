OO_PS4_TOOLCHAIN ?=
GOLDHEN_SDK ?=
CC := clang
LD := ld.lld
CREATE_FSELF := $(OO_PS4_TOOLCHAIN)/bin/linux/create-fself

TARGET := build/key4play_usb_probe
OBJECTS := build/key4play_usb_probe.o build/key4play_pad_hook.o build/input_core.o build/hid_report.o build/input_snapshot.o
CRT := build/crtprx.o

CFLAGS := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -std=c11 \
  -isysroot $(OO_PS4_TOOLCHAIN) -isystem $(OO_PS4_TOOLCHAIN)/include \
  -I$(GOLDHEN_SDK)/include -Iinclude -I../../shared/include -Wall -Wextra
LDFLAGS := -m elf_x86_64 -pie --script $(OO_PS4_TOOLCHAIN)/link.x -e _init \
  --eh-frame-hdr -L$(OO_PS4_TOOLCHAIN)/lib \
  -lSceLibcInternal -lkernel -lm

.PHONY: all clean check-env
all: check-env $(TARGET).prx

check-env:
	@test -n "$(OO_PS4_TOOLCHAIN)" || (echo "Set OO_PS4_TOOLCHAIN"; exit 1)
	@test -n "$(GOLDHEN_SDK)" || (echo "Set GOLDHEN_SDK"; exit 1)

build:
	mkdir -p build

build/key4play_usb_probe.o: source/key4play_usb_probe.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/key4play_pad_hook.o: source/key4play_pad_hook.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/input_core.o: ../../shared/src/input_core.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/hid_report.o: ../../shared/src/hid_report.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/input_snapshot.o: ../../shared/src/input_snapshot.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/crtprx.o: $(GOLDHEN_SDK)/crt/crtprx.c | build
	$(CC) --target=x86_64-pc-linux-gnu -ffreestanding -nostdlib -fno-builtin \
	  -fPIC -isysroot $(OO_PS4_TOOLCHAIN) -isystem $(OO_PS4_TOOLCHAIN)/include \
	  -c $< -o $@

$(TARGET).prx: $(OBJECTS) $(CRT)
	$(LD) $(CRT) $(OBJECTS) -o $(TARGET).elf $(LDFLAGS)
	$(CREATE_FSELF) -in=$(TARGET).elf -out=$(TARGET).oelf --lib=$(TARGET).prx --paid 0x3800000000000011

clean:
	rm -rf build
