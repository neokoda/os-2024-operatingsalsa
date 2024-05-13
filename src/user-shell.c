#include <stdint.h>
#include "header/stdlib/string.h"
#include "header/filesystem/fat32.h"

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

void print_terminal_cwd(char* cwd) {
    char* terminal = "operatingsalsa:";
    syscall(6, (uint32_t) terminal, strlen(terminal), 2);
    syscall(6, (uint32_t) cwd, strlen(cwd), 1);
}

void get_command(char* command) {
    int i = 0;
    while (true) {
        syscall(4, (uint32_t) &(command[i]), 0, 0);
        syscall(5, command[i], 0xF, 0);
        if (command[i] == '\n') {
            break;
        }
        if (command[i] != '\0' && command[i] != '\n' && command[i] != '\b') {
            i++;
        }
        if (command[i] == '\b') {
            command[i] = '\0';
            i--;
        }
    }
    for (int i = 0; i < 80; i++) {
        command[i] = '\0';
    }
}

int main(void) {
    struct ClusterBuffer      cl[2]   = {0};
    struct FAT32DriverRequest request = {
        .buf                   = &cl,
        .name                  = "shell",
        .ext                   = "\0\0\0",
        .parent_cluster_number = ROOT_CLUSTER_NUMBER,
        .buffer_size           = CLUSTER_SIZE,
    };
    int32_t retcode;
    syscall(0, (uint32_t) &request, (uint32_t) &retcode, 0);

    char command[80];
    char* cwd = "/root/";
    syscall(7, 0, 0, 0);
    while (true) {
        print_terminal_cwd(cwd);
        get_command(command);
    }

    // while (true) {
    //     print current working directory
    //     get input
    //     process input
    //     print output
    // }

    return 0;
}
