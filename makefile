# Compiler & linker
ASM           = nasm
LIN           = ld
CC            = gcc

# Directory
SOURCE_FOLDER = src
OUTPUT_FOLDER = bin
ISO_NAME      = os2024
DISK_NAME     = sample-image

# Flags
WARNING_CFLAG = -Wall -Wextra -Werror
DEBUG_CFLAG   = -fshort-wchar -g
STRIP_CFLAG   = -nostdlib -fno-stack-protector -nostartfiles -nodefaultlibs -ffreestanding
CFLAGS        = $(DEBUG_CFLAG) $(WARNING_CFLAG) $(STRIP_CFLAG) -m32 -c -I$(SOURCE_FOLDER)
AFLAGS        = -f elf32 -g -F dwarf
LFLAGS        = -T $(SOURCE_FOLDER)/linker.ld -melf_i386


run: all
	@qemu-system-i386 -s -drive file=$(OUTPUT_FOLDER)/$(DISK_NAME).bin,format=raw,if=ide,index=0,media=disk -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso
all: build
build: iso kernel
clean:
	@rm -rf *.o *.iso $(OUTPUT_FOLDER)/kernel

kernel: gdt framebuffer interrupt keyboard stdlib filesystem memory inserter user-shell insert-shell
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/kernel-entrypoint.s -o $(OUTPUT_FOLDER)/kernel-entrypoint.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/kernel.c -o $(OUTPUT_FOLDER)/kernel.o
	@$(LIN) $(LFLAGS) bin/*.o -o $(OUTPUT_FOLDER)/kernel
	@echo Linking object files and generate elf32...
	@rm -f *.o

iso: kernel
	@mkdir -p $(OUTPUT_FOLDER)/iso/boot/grub
	@cp $(OUTPUT_FOLDER)/kernel     $(OUTPUT_FOLDER)/iso/boot/
	@cp other/grub1                 $(OUTPUT_FOLDER)/iso/boot/grub/
	@cp $(SOURCE_FOLDER)/menu.lst   $(OUTPUT_FOLDER)/iso/boot/grub/
	@genisoimage -R                   \
		-b boot/grub/grub1         \
		-no-emul-boot              \
		-boot-load-size 4          \
		-A os                      \
		-input-charset utf8        \
		-quiet                     \
		-boot-info-table           \
		-o $(OUTPUT_FOLDER)/$(ISO_NAME).iso              \
		$(OUTPUT_FOLDER)/iso
	@rm -r $(OUTPUT_FOLDER)/iso
	
stdlib:
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/stdlib/string.c -o $(OUTPUT_FOLDER)/string.o

gdt:
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/gdt.c -o $(OUTPUT_FOLDER)/gdt.o

framebuffer:
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/portio.c -o $(OUTPUT_FOLDER)/portio.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/framebuffer.c -o $(OUTPUT_FOLDER)/framebuffer.o

interrupt:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/intsetup.s -o $(OUTPUT_FOLDER)/intsetup.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/idt.c -o $(OUTPUT_FOLDER)/idt.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/interrupt.c -o $(OUTPUT_FOLDER)/interrupt.o

keyboard:
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/keyboard.c -o $(OUTPUT_FOLDER)/keyboard.o

disk:
	@qemu-img create -f raw $(OUTPUT_FOLDER)/$(DISK_NAME).bin 4M

filesystem:
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/disk.c -o $(OUTPUT_FOLDER)/disk.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/filesystem/fat32.c -o $(OUTPUT_FOLDER)/fat32.o

memory:
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/paging.c -o $(OUTPUT_FOLDER)/paging.o

inserter:
	@$(CC) -Wno-builtin-declaration-mismatch -g -I$(SOURCE_FOLDER) \
		$(SOURCE_FOLDER)/stdlib/string.c \
		$(SOURCE_FOLDER)/filesystem/fat32.c \
		$(SOURCE_FOLDER)/external/external-inserter.c \
		-o $(OUTPUT_FOLDER)/inserter

user-shell:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-shell.c -o user-shell.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-shell.o -o $(OUTPUT_FOLDER)/shell
	@echo Linking object shell object files and generate flat binary...
	@size --target=binary $(OUTPUT_FOLDER)/shell
	@rm -f *.o

insert-shell: inserter user-shell
	@echo "Inserting shell into root directory..."
	@cd $(OUTPUT_FOLDER); ./inserter shell 2 $(DISK_NAME).bin
