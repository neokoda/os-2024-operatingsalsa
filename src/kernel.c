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
#include "header/memory/paging.h"

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    initialize_filesystem_fat32();
    gdt_install_tss();
    set_tss_register();

    // Allocate first 4 MiB virtual memory
    paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t*) 0);

    // Write shell into memory
    struct FAT32DriverRequest request = {
        .buf                   = (uint8_t*) 0,
        .name                  = "shell",
        .ext                   = "\0\0\0",
        .parent_cluster_number = ROOT_CLUSTER_NUMBER,
        .buffer_size           = 0x100000,
    };
    read(request);

    // Set TSS $esp pointer and jump into shell 
    set_tss_kernel_current_stack();
    kernel_execute_user_program((uint8_t*) 0);

    while (true);
}


// void kernel_setup(void) {
//     load_gdt(&_gdt_gdtr);
//     pic_remap();
//     activate_keyboard_interrupt();
//     initialize_idt();
//     framebuffer_clear();
//     framebuffer_set_cursor(0, 0);
//     initialize_filesystem_fat32();

//     struct FAT32DriverRequest request;

//     // testing read
//     // memcpy(request.name, "nbuna", 8); 
//     // memcpy(request.ext, "", 3);
//     // request.parent_cluster_number = 2;
//     // request.buffer_size = 450000; 

//     // int8_t read_retval = read(request);
//     // read(request);

//     // write_clusters(request.buf, 32, 1);
//     // write_clusters(&read_retval, 32, 3);

//     // if (read_retval == 0) {
//     //     write_clusters(request.buf, 32, 3);
//     // }
//     // if (read_retval == 1) {
//     //     write_clusters("Not a file", 32, 3);
//     // } 
//     // if (read_retval == 2) {
//     //     write_clusters("Not enough buffer size", 32, 3);
//     // }
//     // if (read_retval == 3) {
//     //     write_clusters("File not found", 32, 3);
//     // }
//     // if (read_retval == -1) {
//     //     write_clusters("Unknown error", 32, 3);
//     // }

//     // testing read_directory
//     // memcpy(request.name, "folder2", 8); 
//     // memcpy(request.ext, "", 3);
//     // request.parent_cluster_number = 2;
//     // request.buffer_size = 450000; 

//     // int8_t read_retval = read_directory(request);
//     // read_directory(request);

//     // write_clusters(request.buf, 32, 1);
//     // write_clusters(&read_retval, 32, 3);

//     // if (read_retval == 0) {
//     //     write_clusters(request.buf, 32, 3);
//     // }
//     // if (read_retval == 1) {
//     //     write_clusters("Not a folder", 32, 3);
//     // } 
//     // if (read_retval == 2) {
//     //     write_clusters("Folder not found", 32, 3);
//     // }
//     // if (read_retval == -1) {
//     //     write_clusters("Unknown error", 32, 3);
//     // }

//     // testing write
//     memcpy(request.name, "f", 8); 
//     memcpy(request.ext, "", 3);
//     request.parent_cluster_number = 2;
//     request.buffer_size = 500;
//     char* string_to_write = "Neo Koda";
//     int l = strlen(string_to_write);
//     memset(request.buf, 1, bytes_to_cluster(l));
//     // write_clusters(request.buf, 32, 3);
//     memcpy(request.buf, "Muhammad Neo Cicero Koda", strlen(string_to_write));

//     // int8_t write_retval = write(request);
//     write(request);

//     // write_clusters(request.buf, 32, 1);
//     // write_clusters(&write_retval, 32, 3);

//     // if (read_retval == 0) {
//     //     write_clusters(request.buf, 32, 3);
//     // }
//     // if (read_retval == 1) {
//     //     write_clusters("Not a folder", 32, 3);
//     // } 
//     // if (read_retval == 2) {
//     //     write_clusters("Folder not found", 32, 3);
//     // }
//     // if (read_retval == -1) {
//     //     write_clusters("Unknown error", 32, 3);
//     // }


//     while (true);
// }
