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
#include "header/process/process.h"

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

    // Shell request
    struct FAT32DriverRequest request = {
        .buf                   = (uint8_t*) 0,
        .name                  = "shell",
        .ext                   = "\0\0\0",
        .parent_cluster_number = ROOT_CLUSTER_NUMBER,
        .buffer_size           = 0x100000,
    };

    // Set TSS.esp0 for interprivilege interrupt
    set_tss_kernel_current_stack();

    // Create & execute process 0
    process_create_user_process(request);
    paging_use_page_directory((struct PageDirectory*) _process_list[0].context.page_directory_virtual_addr);
    kernel_execute_user_program((void*) 0x0);
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
//     struct FAT32DriverRequest root_folder_request = {
//         .buf                   = (uint8_t*) 0,
//         .name                  = "root",
//         .ext                   = "\0\0\0",
//         .parent_cluster_number = 2,
//         .buffer_size           = 2048,
//     };
//     read_directory(root_folder_request);
//     write_clusters(root_folder_request.buf, 32, 1);

    // struct FAT32DriverRequest root_folder_request = {
    //     .buf                   = (uint8_t*) 0,
    //     .name                  = "root",
    //     .ext                   = "\0\0\0",
    //     .parent_cluster_number = 2,
    //     .buffer_size           = 2048,
    // };

    // read_directory(root_folder_request);

    // write_clusters(root_folder_request.buf, 32, 3);

    // testing write
    // char brainrot[894] = "! Alright, so let's break it down in a way that hits home, fam. We're talking about the lognormal distribution, a concept that Livy Dunne and the crew would vibe with, for sure. Picture this: imagine a world where everything's got a kind of natural rhythm, like baby Gronk running his routes or Kai Cenat hitting the gym. Now, the lognormal distribution is like a funky twist on that rhythm, a bit of Gyatt mixed with Adin Ross vibes, you feel me? It's all about how things pan out when you're looking at the logarithms of variables, like Skibidi Toilet's gaming stats or even Fanum Tax's stock market moves. It's like finding the sweet spot between edging, gooning, and mewing, but in the world of numbers. And when you're trying to maximize your gains or minimize your risks, knowing how to ride that lognormal wave could be your ticket to becoming the ultimate Rizzler, straight outta Ohio!";
    // struct FAT32DriverRequest request = {
    //     .buf                   = (uint8_t*) 0,
    //     .name                  = "115",
    //     .ext                   = "pdf",
    //     .parent_cluster_number = 5,
    //     .buffer_size           = 4097,
    // };

    // memset(request.buf, 115, 4097);

    // int8_t retval = write(request);

    // while (retval == 10) {

    // }

    // testing delete
    // struct FAT32DriverRequest request = {
    //     .buf                   = (uint8_t*) 0,
    //     .name                  = "115",
    //     .ext                   = "\0\0\0",
    //     .parent_cluster_number = 5,
    //     .buffer_size           = 0x100000,
    // };

    // int8_t retval = delete(request);

    // while (retval == 10) {

    // }

//     while (true);
// }
