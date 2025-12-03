SDK_INCLUDE_DIR=$(shell xcrun -sdk iphoneos --show-sdk-path)/usr/include

CC=xcrun -sdk iphoneos clang
CFLAGS=-target armv7-linux-gnueabi-elf -marm -D__arm__ -Wall -static -nostdlib -I$(SDK_INCLUDE_DIR) -Iinclude -O1 -fpic -fpie

LDFLAGS=-target arm-linux-gnueabi-elf -fuse-ld=/opt/homebrew/bin/ld.lld -static -nostdlib linkerScript.ld -Llib -fpic -fpie

DEPS=

OBJS=start.o main.o

NAME=DMA

ELF=$(NAME).elf
BIN=$(NAME).bin
HDR=payload_abl0.h

all: $(HDR)

payload_abl0.h: $(NAME)_comp.bin
	xxd -n payload_abl0 -c 8 -i $< >$@

$(NAME)_comp.bin: $(BIN)
	echo "compressing"
	python3 makecomp.py $< $@

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.S $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME).elf: $(OBJS) linkerScript.ld
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.bin: %.elf
	/opt/homebrew/opt/binutils/bin/gobjcopy -Obinary $< $@

PHONY: clean
clean:
	rm -rf $(ELF) $(BIN) $(OBJS) $(NAME)_comp.bin $(HDR)
