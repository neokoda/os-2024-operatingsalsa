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

    struct FAT32DriverRequest request;

    memcpy(request.name, "Neo", 8); // Copy "filename" to the name array
    memcpy(request.ext, "", 3); // Copy "txt" to the ext array
    request.parent_cluster_number = 2;
    request.buffer_size = 2080; 

    memset(request.buf, 'n', 2050);
    // // int8_t read_retval = read(request);
    // read_directory(request);
    write(request);

    // write_clusters(request.buf, 32, 3);

    // write_clusters("fef", 32, 1);
    // if (read_retval == 0) {
    //     write_clusters(request.buf, 32, 3);
    // } else {
    //     if (read_retval == 1) {
    //         write_clusters("Not a folder", 32, 1);
    //     }
    // } 


    while (true);
}
