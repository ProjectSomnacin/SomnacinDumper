SDK_INCLUDE_DIR=$(shell xcrun -sdk iphoneos --show-sdk-path)/usr/include

CC=xcrun -sdk iphoneos clang
CFLAGS=-target armv7-linux-gnueabi-elf -marm -D__arm__ -Wall -static -nostdlib -I$(SDK_INCLUDE_DIR) -Iinclude -O0 -fpic -fpie

LDFLAGS=-target arm-linux-gnueabi-elf -fuse-ld=/opt/homebrew/bin/ld.lld -static -nostdlib linkerScript.ld -Llib -fpic -fpie

DEPS=

OBJS=start.o main.o utils.o arm32_aeabi_divmod.o arm32_aeabi_ldivmod.o printf.o

NAME=EL3Payload

ELF=$(NAME).elf
BIN=$(NAME).bin
HDR=spi_payload.h payload_el3.h

all: $(HDR)


spi_payload.h: $(NAME).bin
	xxd -n spi_payload -c 8 -i $< >$@

payload_el3.h: $(NAME).bin
	xxd -n payload_el3 -c 8 -i $< >$@

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.S $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME).elf: $(OBJS) linkerScript.ld
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.bin: %.elf
	/opt/homebrew/opt/binutils/bin/gobjcopy -Obinary $< $@
	dd if=/dev/zero bs=1 count=4 >>$@

PHONY: clean
clean:
	rm -rf $(ELF) $(BIN) $(OBJS) $(HDR)
