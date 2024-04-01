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

    // testing read
    // memcpy(request.name, "nbuna", 8); 
    // memcpy(request.ext, "", 3);
    // request.parent_cluster_number = 2;
    // request.buffer_size = 450000; 

    // int8_t read_retval = read(request);
    // read(request);

    // write_clusters(request.buf, 32, 1);
    // write_clusters(&read_retval, 32, 3);

    // if (read_retval == 0) {
    //     write_clusters(request.buf, 32, 3);
    // }
    // if (read_retval == 1) {
    //     write_clusters("Not a file", 32, 3);
    // } 
    // if (read_retval == 2) {
    //     write_clusters("Not enough buffer size", 32, 3);
    // }
    // if (read_retval == 3) {
    //     write_clusters("File not found", 32, 3);
    // }
    // if (read_retval == -1) {
    //     write_clusters("Unknown error", 32, 3);
    // }

    // testing read_directory
    // memcpy(request.name, "folder2", 8); 
    // memcpy(request.ext, "", 3);
    // request.parent_cluster_number = 2;
    // request.buffer_size = 450000; 

    // int8_t read_retval = read_directory(request);
    // read_directory(request);

    // write_clusters(request.buf, 32, 1);
    // write_clusters(&read_retval, 32, 3);

    // if (read_retval == 0) {
    //     write_clusters(request.buf, 32, 3);
    // }
    // if (read_retval == 1) {
    //     write_clusters("Not a folder", 32, 3);
    // } 
    // if (read_retval == 2) {
    //     write_clusters("Folder not found", 32, 3);
    // }
    // if (read_retval == -1) {
    //     write_clusters("Unknown error", 32, 3);
    // }

    // testing write
    memcpy(request.name, "neofile", 8); 
    memcpy(request.ext, "txt", 3);
    request.parent_cluster_number = 2;
    request.buffer_size = 4100; 
    memset(request.buf, 'v', 4100);

    int8_t write_retval = write(request);
    // read_directory(request);

    // write_clusters(request.buf, 32, 1);
    write_clusters(&write_retval, 32, 3);

    // if (read_retval == 0) {
    //     write_clusters(request.buf, 32, 3);
    // }
    // if (read_retval == 1) {
    //     write_clusters("Not a folder", 32, 3);
    // } 
    // if (read_retval == 2) {
    //     write_clusters("Folder not found", 32, 3);
    // }
    // if (read_retval == -1) {
    //     write_clusters("Unknown error", 32, 3);
    // }


    while (true);
}
