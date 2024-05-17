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
void clear_cmd_args(char* command, char args[80][80]) {
    for (int i = 0; i < 80; i++) {
        command[i] = '\0';
        for (int j = 0; j < 80; j++) {
            args[i][j] = '\0';
        }
    }
}

// parses input from command to arguments
void parse_arguments(char* command, char args[80][80]) {
    int i = 0;
    int j = 0;
    for (int k = 0; k < strlen(command); k++) {
        if (command[k] == ' ') {
            if (j != 0) {
                i++;
            }
            j = 0;
        } else {
            args[i][j] = command[k];
            j++;
        }
    }
}

// clears current_folder
void clear_current_folder() {
    memset(&current_folder, 0, sizeof(struct FAT32DirectoryTable));
}

// update contents of current working directory
void update_cwd_contents() {
    uint32_t low = (uint32_t)(current_folder.table[0].cluster_low);
    uint32_t high = ((uint32_t) current_folder.table[0].cluster_high) << 16;
    uint32_t current_cluster = (low | high);
    
    struct FAT32DriverRequest current_folder_request = {
        .buf                   = &current_folder,
        .name                  = "",
        .ext                   = "\0\0\0",
        .parent_cluster_number = current_cluster,
        .buffer_size           = 2048,
    };
    memcpy(current_folder_request.name, current_folder.table[0].name, 8);

    syscall(1, (uint32_t) &current_folder_request, 0, 0);
}

// parse name and extension from argument file
void parse_file(char* file, char* name, char* ext) {
    memset(name, 0, 8);
    memset(ext, 0, 3);

    int dot_idx = -1;
    for (int i = 0; i < strlen(file); i++) {
        if (file[i] == '.') {
            dot_idx = i;
            break;
        }
    }

    if (dot_idx != -1) {
        memcpy(name, file, dot_idx);

        for (int i = dot_idx + 1; i < strlen(file); i++) {
            ext[i - (dot_idx + 1)] = file[i];
        } 
    } else {
        memcpy(name, file, strlen(file));
    }
}

// change directory
void cd(char* arg2) {
    uint32_t low, high;
    char* message;

    if (strings_equal(current_folder.table[0].name, arg2)) {
        bool found = false;
        for (int i = 2; i < 64; i++) {
            if (strings_equal(current_folder.table[i].name, arg2)) {
                if (current_folder.table[i].attribute == ATTR_SUBDIRECTORY) {
                    low = (uint32_t)(current_folder.table[i].cluster_low);
                    high = ((uint32_t) current_folder.table[i].cluster_high) << 16;
                    found = true;
                }
            }
        }
        if (!found) {
            message = "Folder not found\n";
            syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
            return;
        }
    } else {
        low = (uint32_t) (current_folder.table[0].cluster_low);
        high = ((uint32_t) current_folder.table[0].cluster_high) << 16;
    }

    uint32_t target_cluster = (low | high);

    if (target_cluster == ROOT_CLUSTER_NUMBER && strings_equal("..", arg2)) {
        message = "Already in root folder\n";
        syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
        return;
    }

    struct FAT32DriverRequest child_folder_request = {
        .buf                   = &current_folder,
        .name                  = "",
        .ext                   = "\0\0\0",
        .parent_cluster_number = target_cluster,
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
            for (int j = i + 1; j < 80; j++) {
                cwd[j] = '\0';
            }
        } else {
            int cwdlen = strlen(cwd);
            for (int i = cwdlen; i < cwdlen + strlen(arg2); i++) {
                cwd[i] = arg2[i - cwdlen];
            }
            cwd[cwdlen + strlen(arg2)] = '/';
        }
    }
}

void cat(char* arg2) {
    // Initialize the current cluster number to the cluster of the current directory
    uint32_t low = (uint32_t) (current_folder.table[0].cluster_low);
    uint32_t high = ((uint32_t) current_folder.table[0].cluster_high) << 16;
    uint32_t current_cluster_number = (low | high);

    char* message;
    struct ClusterBuffer cl[8];
    // Create the request to find the file
    struct FAT32DriverRequest request = {
        .buf                   = &cl,
        .parent_cluster_number = current_cluster_number,
        .buffer_size           = 2048,
    };

    // Extract the filename and extension from the provided argument
    int filenamelen = 0;
    for (int i = 0; i < strlen(arg2); i++) {
        if (arg2[i] == '.') {
            request.ext[0] = arg2[i + 1];
            request.ext[1] = arg2[i + 2];
            request.ext[2] = arg2[i + 3];
            break;
        } else {
            filenamelen++;
        }
    }

    memcpy(request.name, (void*) arg2, filenamelen);

    // Attempt to find the file
    int retcode;
    syscall(0, (uint32_t)&request, (uint32_t)&retcode, 0);

    // Handle errors
    switch (retcode) {
        case -1:
            message = "An unknown error occurred\n";
            break;
        case 1:
            message = "Provided argument is not a file\n";
            break;
        case 2:
            message = "Not enough buffer\n";
            break;
        case 3:
            message = "File not found in this directory\n";
            break;
    }

    if (retcode != 0) {
        syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
    } else {
        // If the file is found, read its content
        struct ClusterBuffer cl[8];

        struct FAT32DriverRequest readRequest = {
            .buf = &cl,
            .parent_cluster_number = current_cluster_number,
            .buffer_size = 2048,
        };

        memcpy(readRequest.name, request.name, 8);
        memcpy(readRequest.ext, request.ext, 3);

        // Read the file content
        int retCode = 0;
        syscall(0, (uint32_t) &readRequest, (uint32_t) &retCode, 0);

        if (retCode == 0) {
            int bufLen = 0;
            while (cl->buf[bufLen] != '\0') {
                bufLen++;
            }

            syscall(6, (uint32_t) cl->buf, bufLen, 0x07);
            syscall(6, (uint32_t) "\n", (uint32_t) 1, 0x07);
        } else {
            switch (retCode) {
                case 1:
                    message = "Not a file.\n";
                    break;
                case 2:
                    message = "Not enough buffer.\n";
                    break;
                case 3:
                    message = "No such file.\n";
                    break;
                default:
                    message = "Unknown error.\n";
                    break;
            }
            syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
            syscall(6, (uint32_t) "\n", (uint32_t) 1, 0x4);
        }
    }
}

// copy file to another destination
void cp(char* src, char* dest) {
    uint32_t low = (uint32_t) (current_folder.table[0].cluster_low);
    uint32_t high = ((uint32_t) current_folder.table[0].cluster_high) << 16;
    uint32_t cluster_number = (low | high);

    // read file from src
    char file_name[8];
    char file_ext[3];
    parse_file(src, file_name, file_ext);

    uint32_t filesize = 0;
    for (int i = 0; i < 64; i++) {
        if (memcmp(current_folder.table[i].name, file_name, 8) == 0 && memcmp(current_folder.table[i].ext, file_ext, 3) == 0) {
            filesize = current_folder.table[i].filesize;
        }
    }

    char* temp[filesize];
    struct FAT32DriverRequest src_request = {
        .buf                   = temp,
        .name                  = "",
        .ext                   = "",
        .parent_cluster_number = cluster_number,
        .buffer_size           = filesize,
    };
    memcpy(src_request.name, file_name, 8);
    memcpy(src_request.ext, file_ext, 3);

    char* message;
    uint32_t retcode;
    syscall(0, (uint32_t) &src_request, (uint32_t) &retcode, 0);

    switch (retcode) {
        case -1:
            message = "An unknown error occurred\n";
            break;
        case 1:
            message = "Provided argument is not a file\n";
            break;
        case 2:
            message = "Not enough buffer size\n";
            break;
        case 3:
            message = "File not found\n";
            break;
    }
    
    if (retcode != 0) {
        syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
        return;
    }

    // copy read file to dest
    char dest_name[8];
    char dest_ext[3];
    parse_file(dest, dest_name, dest_ext);

    bool found = false;
    cluster_number = (low | high);
    for (int i = 0; i < 64; i++) {
        if (strings_equal(current_folder.table[i].name, dest)) {
            low = (uint32_t) (current_folder.table[i].cluster_low);
            high = ((uint32_t) current_folder.table[i].cluster_high) << 16;
            cluster_number = (low | high);
            found = true;
        }
    }

    if (found) {
        struct FAT32DriverRequest dest_request = {
            .buf                   = temp,
            .name                  = "",
            .ext                   = "",
            .parent_cluster_number = cluster_number,
            .buffer_size           = filesize,
        };
        memcpy(dest_request.name, file_name, 8);
        memcpy(dest_request.ext, file_ext, 3);

        syscall(2, (uint32_t) &dest_request, (uint32_t) &retcode, 0);
    } else {
        struct FAT32DriverRequest dest_request = {
            .buf                   = temp,
            .name                  = "",
            .ext                   = "",
            .parent_cluster_number = cluster_number,
            .buffer_size           = filesize,
        };
        memcpy(dest_request.name, dest_name, 8);
        memcpy(dest_request.ext, dest_ext, 3);

        syscall(2, (uint32_t) &dest_request, (uint32_t) &retcode, 0);
    }

    switch (retcode) {
        case -1:
            message = "An unknown error occurred\n";
            break;
        case 0:
            message = "File successfully copied\n";
            break;
        case 1:
            message = "File/folder with the same name + ext already exists\n";
            break;
        case 2:
            message = "Invalid parent folder\n";
            break;
    }
    
    syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
}

// list file in a directory
void ls(char* arg2) {
    syscall(6, (uint32_t) arg2, (uint32_t) strlen(arg2), 0x4);
    char * foldername = current_folder.table->name;

    // initiate a request, do syscall
    uint32_t low = (uint32_t) (current_folder.table[0].cluster_low);
    uint32_t high = ((uint32_t) current_folder.table[0].cluster_high) << 16;
    uint32_t current_cluster_number = (low | high);
    
    struct FAT32DirectoryTable table = {};
    struct FAT32DriverRequest request = {
        .buf                   = &table,
        .parent_cluster_number = current_cluster_number,
        .buffer_size           = 2048,
    };
    memcpy(request.name, foldername, 8);

    char* message;
    uint32_t retcode;
    // syscall to read directory
    syscall(1, (uint32_t) &request, (uint32_t) &retcode, 0);

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
        // iterate through each entry in directory table
        for (int i = 2; i < 64; i++){
            char * name = table.table[i].name;

            if (table.table[i].name[0] == 0x00){
                continue;
            }

            syscall(6, (uint32_t) name, (uint32_t) strlen(name), 0xF);
            syscall(6, (uint32_t) "\n", (uint32_t) 1, 0xF);
        }
    }
}

void mkdir(char args[80][80]) {
    uint32_t low = (uint32_t) (current_folder.table[0].cluster_low);
    uint32_t high = ((uint32_t) current_folder.table[0].cluster_high) << 16;
    uint32_t current_cluster_number = (low | high);

    char* message;

    int argIdx = 1;
    while (memcmp(args[argIdx], "", 1) != 0) {
        struct FAT32DriverRequest child_folder_request = {
            .buf                   = &current_folder,
            .name                  = "",
            .ext                   = "\0\0\0",
            .parent_cluster_number = current_cluster_number,
            .buffer_size           = 0,
        };
        memcpy(child_folder_request.name, args[argIdx], 8);

        uint32_t retcode;
        syscall(2, (uint32_t) &child_folder_request, (uint32_t) &retcode, 0);

        switch (retcode) {
            case 0:
                message = "Folder created\n";
                break;
            case 1:
                message = "Folder already exists\n";
                break;
            case 2:
                message = "Invalid parent cluster\n";
                break;
            default:
                message = "An unknown error occurred\n";
                break;
        }

        syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
        argIdx++;
    }
}

// execute command from arg1 and arg2
void execute_command(char args[80][80]) {
    if (strings_equal(args[0], "cd")) {
        cd(args[1]);
    } else if (strings_equal(args[0], "cat")) {
        cat(args[1]);
    } else if (strings_equal(args[0], "ls")) {
        ls(args[1]);
    } else if (strings_equal(args[0], "mkdir")) {
        mkdir(args);
    } else if (strings_equal(args[0], "cp"))  {
        cp(args[1], args[2]);
    } else {
        char* message = "Command not found\n";
        syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
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
    char args[80][80];

    syscall(7, 0, 0, 0);
    while (true) {
        update_cwd_contents();
        print_terminal_cwd(cwd);
        get_command(command);
        parse_arguments(command, args);
        execute_command(args);
        clear_cmd_args(command, args);
    }

    return 0;
}