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
//     gdt_install_tss();
//     set_tss_register();

    // Allocate first 4 MiB virtual memory
    // paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t*) 0);

    // testing read
    // struct FAT32DriverRequest request = {
    //     .buf                   = (uint8_t*) 0,
    //     .name                  = "kano",
    //     .ext                   = "\0\0\0",
    //     .parent_cluster_number = 2,
    //     .buffer_size           = 6000,
    // };

    // int8_t retval = read(request);

    // write_clusters(request.buf, 32, 3);

    // while (retval == 10) {

    // }

    // testing read_directory
    // struct FAT32DriverRequest request = {
    //     .buf                   = (uint8_t*) 0,
    //     .name                  = "folder1",
    //     .ext                   = "\0\0\0",
    //     .parent_cluster_number = 2,
    //     .buffer_size           = 6000,
    // };

    // int8_t retval = read_directory(request);

    // write_clusters(request.buf, 32, 3);

    // while (retval == 10) {

    // }

    // testing write
    // struct FAT32DriverRequest request = {
    //     .buf                   = (uint8_t*) 0,
    //     .name                  = "meong",
    //     .ext                   = "txt",
    //     .parent_cluster_number = 2,
    //     .buffer_size           = 4097,
    // };

    // memset(request.buf, 115, 4097);

    // int8_t retval = write(request);

    // while (retval == 10) {

    // }

    // testing delete
    // struct FAT32DriverRequest request = {
    //     .buf                   = (uint8_t*) 0,
    //     .name                  = "folder2",
    //     .ext                   = "\0\0\0",
    //     .parent_cluster_number = ROOT_CLUSTER_NUMBER,
    //     .buffer_size           = 0x100000,
    // };

    // int8_t retval = delete(request);

    // while (retval == 10) {

    // }

//     while (true);
// }
