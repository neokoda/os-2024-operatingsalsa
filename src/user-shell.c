#include <stdint.h>
#include "header/stdlib/string.h"
#include "header/filesystem/fat32.h"

// stores directorytable of current workiing directory
static struct FAT32DirectoryTable current_folder;
// displays current working directory, initially /root/ 
char cwd[80] = "/root/";

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

// used for processing user input
bool strings_equal(char* arg1, char* arg2) {
    return (strlen(arg1) == strlen(arg2) && memcmp(arg1, arg2, strlen(arg2)) == 0);
}

// prints out cwd to terminal every line
void print_terminal_cwd(char* cwd) {
    char* terminal = "operatingsalsa:";
    syscall(6, (uint32_t) terminal, strlen(terminal), 2);
    syscall(6, (uint32_t) cwd, strlen(cwd), 1);
}

// gets user input and stores it in command
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
    command[i] = '\0';
}

// clears command and argument buffers
void clear_cmd_args(char* command, char* arg1, char* arg2) {
    for (int i = 0; i < 80; i++) {
        command[i] = '\0';
        arg1[i] = '\0';
        arg2[i] = '\0';
    }
}

// parses input from command to first and second argument
void parse_arguments(char* command, char* arg1, char* arg2) {
    int i;
    int j = 0;
    for (i = 0; i < strlen(command); i++) {
        if (command[i] == ' ') {
            break;
        }
        arg1[j] = command[i];
        j++;
    }
    int k;
    j = 0;
    for (k = i + 1; k < strlen(command); k++) {
        if (command[k] == ' ') {
            break;
        }
        arg2[j] = command[k];
        j++;
    }
}

// clears current_folder
void clear_current_folder() {
    memset(&current_folder, 0, sizeof(struct FAT32DirectoryTable));
}

// change directory
void cd(char* arg2) {
    uint32_t low = (uint32_t) (current_folder.table[0].cluster_low);
    uint32_t high = ((uint32_t) current_folder.table[0].cluster_high) << 16;
    uint32_t current_cluster_number = (low | high);

    char* message;

    if (current_cluster_number == ROOT_CLUSTER_NUMBER && strings_equal("..", arg2)) {
        message = "Already in root folder\n";
        syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
        return;
    }
    if (strings_equal(arg2, current_folder.table[0].name)) {
        return;
    }

    struct FAT32DriverRequest child_folder_request = {
        .buf                   = &current_folder,
        .name                  = "",
        .ext                   = "\0\0\0",
        .parent_cluster_number = current_cluster_number,
        .buffer_size           = 2048,
    };
    memcpy(child_folder_request.name, arg2, 8);
    
    uint32_t retcode;
    syscall(1, (uint32_t) &child_folder_request, (uint32_t) &retcode, 0);

    switch (retcode) {
        case -1:
            message = "An unknown error occurred\n";
            break;
        case 1:
            message = "Provided argument is not a folder\n";
            break;
        case 2:
            message = "Folder not found\n";
            break;
    }

    if (retcode != 0) {
        syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
    } else {
        if (memcmp("..", arg2, strlen(arg2)) == 0) {
            int i = strlen(cwd) - 2;
            while (cwd[i] != '/') {
                i--;
            }
            cwd[i + 1] = '\0';
        } else {
            int cwdlen = strlen(cwd);
            for (int i = cwdlen; i < cwdlen + strlen(arg2); i++) {
                cwd[i] = arg2[i - cwdlen];
            }
            cwd[cwdlen + strlen(arg2)] = '/';
        }
    }
}

// execute command from arg1 and arg2
void execute_command(char* arg1, char* arg2) {
    if (memcmp(arg1, "cd", 2) == 0 && strlen(arg1) == 2) {
        cd(arg2);
    }
}

// main
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

    struct FAT32DriverRequest root_folder_request = {
        .buf                   = &current_folder,
        .name                  = "root",
        .ext                   = "\0\0\0",
        .parent_cluster_number = 2,
        .buffer_size           = 2048,
    };
    syscall(1, (uint32_t) &root_folder_request, (uint32_t) &retcode, 0);

    char command[80];
    char arg1[80];
    char arg2[80];

    syscall(7, 0, 0, 0);
    while (true) {
        print_terminal_cwd(cwd);
        get_command(command);
        parse_arguments(command, arg1, arg2);
        execute_command(arg1, arg2);
        clear_cmd_args(command, arg1, arg2);
    }

    // while (true) {
    //     print current working directory
    //     get input
    //     process input
    //     print output
    // }

    return 0;
}
