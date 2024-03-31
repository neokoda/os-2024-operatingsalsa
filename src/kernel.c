#include <stdint.h>
#include <stdbool.h>
#include "header/stdlib/string.h"
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/driver/framebuffer.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/driver/keyboard.h"
#include "header/filesystem/fat32.h"
#include "header/driver/disk.h"

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    activate_keyboard_interrupt();
    initialize_idt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    initialize_filesystem_fat32();

    // struct FAT32DirectoryEntry fde;
    struct FAT32DriverRequest request;

    memcpy(request.name, "kano", 8); // Copy "filename" to the name array
    memcpy(request.ext, "", 3); // Copy "txt" to the ext array
    request.parent_cluster_number = 2;
    request.buffer_size = 5; 

    int8_t read_retval = read(request);

    if (read_retval == 0) {
        write_clusters(request.buf, 32, 1);
    } else {
        // if (read_retval == 1) {
        //     write_clusters("Not a file", 32, 3);
        // } else if (read_retval == 2) {
        //     write_clusters("Not enough buffer size", 32, 3);
        // } else if (read_retval == 3) {
        //     write_clusters("File not kontol", 32, 3);
        // }
    } 


    while (true);
}
