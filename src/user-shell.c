#include <stdint.h>
#include "header/stdlib/string.h"
#include "header/filesystem/fat32.h"

// stores directorytable of current workiing directory
static struct FAT32DirectoryTable current_folder;
// displays current working directory, initially /root/ 
char cwd[80] = "/root/";

void execute_command(char args[80][80]);

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
void addPadding(char *str, int idx1, int idx2) {
    for (int i = idx1; i < idx2; i++)
        str[i] = '\0';
}

int parseNameAndExt(char *filename, char *name, char *ext) {
    int i = 0;
    while (i < 8 && filename[i] != '.' && filename[i] != '\0'){
        name[i] = filename[i];
        i++;
    }
    addPadding(name, i, 8);

    if (filename[i] == '\0') {
        addPadding(ext, 0, 3);
        return 0;
    }

    if (filename[i] != '.')
        return 1;
    
    int j;
    for (j = i + 1; j <= i + 3; j++){
        ext[j-i-1] = filename[j];
    }
    addPadding(ext, j - i - 1, 3);

    if (filename[j] != '\0')
        return 1;

    return 0;
}

void rmHelper(char* deletedName, char* ext, uint32_t parent_cluster_number, uint32_t deleted_cluster_number){
    uint32_t retcode;
    struct ClusterBuffer cl;
    struct FAT32DriverRequest request = {
        .buf = &cl,
        .name = "\0\0\0\0\0\0\0",
        .ext = "\0\0",
        .buffer_size = 0,
        .parent_cluster_number = parent_cluster_number
    };
    memcpy(request.name, deletedName, 8);
    memcpy(request.ext, ext, 3);
    // delete, file will immediately deleted
    syscall(3, (uint32_t)&request, (uint32_t)&retcode, 0);

    if (retcode == 2) {
        // cant immediately delete folder, need to remove child first 
        struct FAT32DirectoryTable target_table;
        request.buf = &target_table;
        request.buffer_size = 2048;
        // read
        syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);
        
        for (int i = 1; i < 64; i++) {
            // remove all child recursively
            if (target_table.table[i].user_attribute == UATTR_NOT_EMPTY) {
                uint32_t child_cluster_number = target_table.table[i].cluster_low | (target_table.table[i].cluster_high << 16);
                rmHelper(target_table.table[i].name, target_table.table[i].ext, deleted_cluster_number, child_cluster_number);
            }
        }

        // delete the folder itself
        memcpy(request.name, deletedName, 8);
        memcpy(request.ext, ext, 3);
        request.parent_cluster_number = parent_cluster_number;
        syscall(3, (uint32_t)&request, (uint32_t)&retcode, 0);
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
    if (strlen(arg2) == 0) {
        syscall(6, (uint32_t)"Missing destination file", strlen("Missing destination file"), 0x4);
        syscall(6, (uint32_t)"\n", 1, 0x4);
        return;
    }
 
    uint32_t low = (uint32_t) (current_folder.table[0].cluster_low);
    uint32_t high = ((uint32_t) current_folder.table[0].cluster_high) << 16;
    uint32_t current_cluster_number = (low | high);

    char* message;
    struct ClusterBuffer cl[8];
    struct FAT32DriverRequest request = {
        .buf                   = &cl,
        .parent_cluster_number = current_cluster_number,
        .buffer_size           = 2048,
    };

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

    int retcode;
    syscall(0, (uint32_t)&request, (uint32_t)&retcode, 0);

    switch (retcode) {
        case 1:
            message = "Not a file\n";
            break;
        case 2:
            message = "Not enough buffer\n";
            break;
        case 3:
            message = "File not found in this directory\n";
            break;
        default:
            message = "Unknown error.\n";
            break;
    }

    if (retcode != 0) {
        syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
    } else {
        struct ClusterBuffer cl[8];

        struct FAT32DriverRequest readRequest = {
            .buf = &cl,
            .parent_cluster_number = current_cluster_number,
            .buffer_size = 2048,
        };

        memcpy(readRequest.name, request.name, 8);
        memcpy(readRequest.ext, request.ext, 3);

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
                    message = "File not found in this directory\n";
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

void cp(char* src, char* dest) {
    uint32_t low = (uint32_t) (current_folder.table[0].cluster_low);
    uint32_t high = ((uint32_t) current_folder.table[0].cluster_high) << 16;
    uint32_t cluster_number = (low | high);

    char file_name[8];
    char file_ext[3];
    parseNameAndExt(src, file_name, file_ext);

    uint32_t filesize = 0;
    for (int i = 0; i < 64; i++) {
        if (memcmp(current_folder.table[i].name, file_name, 8) == 0 && memcmp(current_folder.table[i].ext, file_ext, 3) == 0) {
            filesize = current_folder.table[i].filesize;
        }
    }

    struct ClusterBuffer cl[filesize];
    struct FAT32DriverRequest src_request = {
        .buf                   = &cl,
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
    parseNameAndExt(dest, dest_name, dest_ext);

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
            .buf                   = &cl,
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
            .buf                   = &cl,
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
void ls() {
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
            char name[8];
            memcpy(name, table.table[i].name, 8);
            // if entry is empty, skip
            if (name[0] == 0x00){
                continue;
            }
            syscall(6, (uint32_t) name, (uint32_t) strlen(name), 0xF);
            if (table.table[i].ext[0] != 0x00){
                char * extension = table.table[i].ext;
                syscall(6, (uint32_t) ".", (uint32_t) 1, 0xF);
                syscall(6, (uint32_t) extension, (uint32_t) 3, 0xF);
            }
            syscall(6, (uint32_t) "\n", (uint32_t) 1, 0xF);
        }
    }
}

// remove file or folder
void rm(char args[80][80]){
    int arg_idx = 1; // starting idx of arguments
    if (strings_equal(args[1], "-r")){
        arg_idx = 2; //is recursive
    }

    char* message;
    // handle invalid rm command
    if ((arg_idx == 1 && args[1][0] == '\0') || (arg_idx == 2 && args[2][0] == '\0')){
        message = "usage: rm [-r] file ...\n";
        syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
        return;
    }

    // initiate a request
    uint32_t low = (uint32_t) (current_folder.table[0].cluster_low);
    uint32_t high = ((uint32_t) current_folder.table[0].cluster_high) << 16;
    uint32_t current_cluster_number = (low | high);
    uint32_t retcode;
    
    struct FAT32DirectoryTable table = {};
    struct FAT32DriverRequest request = {
        .buf                   = &table,
        .parent_cluster_number = current_cluster_number,
        .buffer_size           = sizeof(struct FAT32DirectoryTable),
    };

    // iterate through every file/folder to be removed
    for (int i = arg_idx; args[i][0] != '\0' ; i++){
        char filename[9];
        char ext[4];
        if (parseNameAndExt(args[i], filename, ext)){
            message = "filename or extension too long\n";
            syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
            return;
        }

        // check existence of file/folder
        memcpy(request.name, filename, 8);
        memcpy(request.ext, ext, 3);
        syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);

        if (retcode == 0 && arg_idx == 1){
            // -r not specified in args
            message = ": is a directory (usage: rm -r folder ...)\n";
            syscall(6, (uint32_t) filename, (uint32_t) strlen(filename), 0x4);
            syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
            continue;
        }
        else if (retcode == 2){
            // the specified file/folder is not found within current directory
            message = ": No such file or directory\n";

            syscall(6, (uint32_t) args[i], (uint32_t) strlen(args[i]), 0x4);
            syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
            continue;
        }
        uint32_t child_cluster_number;
        // if (memcmp(table.table[i].ext, ext, 3) == 0  && memcmp(table.table[i].name, filename, 8) == 0 && table.table[i].name[0] != 0x00) {
        //     child_cluster_number = (table.table[i].cluster_high << 16) | table.table[i].cluster_low;
        // }
        child_cluster_number = table.table[i].cluster_low | (table.table[i].cluster_high << 16);
        // child_cluster_number = (table.table[i].cluster_high << 16) | table.table[i].cluster_low;
        rmHelper(filename, ext, current_cluster_number, child_cluster_number);
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

        if (retcode != 0) {
            syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
        }
        argIdx++;
    }
}

void mv_folder(char* src, char* dest) {
    uint32_t current_low = (uint32_t) (current_folder.table[0].cluster_low);
    uint32_t current_high = ((uint32_t) current_folder.table[0].cluster_high) << 16;
    uint32_t current_cluster_number = (current_low | current_high);

    uint32_t target_cluster_number, target_low, target_high;
    for (int i = 0; i < 64; i++) {
        if (memcmp(current_folder.table[i].name, dest, 8) == 0) {
            target_low = (uint32_t) (current_folder.table[i].cluster_low);
            target_high = ((uint32_t) current_folder.table[i].cluster_high) << 16;
            target_cluster_number = (target_low | target_high);    
            break;
        }
        if (memcmp(current_folder.table[i].name, src, 8) == 0) {
            memset(&current_folder.table[i], 0, 32);
        }
    }

    // read src folder
    struct FAT32DirectoryTable temp_table = {0};
    struct FAT32DriverRequest src_folder_request = {
        .buf                   = &temp_table,
        .name                  = "",
        .ext                   = "",
        .parent_cluster_number = current_cluster_number,
        .buffer_size           = 2048,
    };
    memcpy(src_folder_request.name, src, 8);
    
    uint32_t retcode;
    syscall(1, (uint32_t) &src_folder_request, (uint32_t) &retcode, 0);

    // update entry in cwd
    struct FAT32DriverRequest update_cwd_request = {
        .buf                   = &current_folder,
        .name                  = "",
        .ext                   = "",
        .parent_cluster_number = current_cluster_number,
        .buffer_size           = 2048,
    };
    syscall(8, (uint32_t) &update_cwd_request, (uint32_t) &retcode, 0);
    
    // update src parent directory
    temp_table.table[1].cluster_low = target_low;
    temp_table.table[1].cluster_high = target_high;

    uint32_t src_low = (uint32_t) (temp_table.table[0].cluster_low);
    uint32_t src_high = ((uint32_t) temp_table.table[0].cluster_high) << 16;
    uint32_t src_cluster_number = (src_low | src_high);

    struct FAT32DriverRequest update_src_parent_directory_request = {
        .buf                   = &temp_table,
        .name                  = "",
        .ext                   = "",
        .parent_cluster_number = src_cluster_number,
        .buffer_size           = 2048,
    };
    syscall(8, (uint32_t) &update_src_parent_directory_request, (uint32_t) &retcode, 0);

    // write folder to cwd
    struct FAT32DriverRequest write_new_folder_request = {
        .buf                   = (uint8_t*) 0,
        .name                  = "",
        .ext                   = "",
        .parent_cluster_number = target_cluster_number,
        .buffer_size           = 0,
    };
    memcpy(write_new_folder_request.name, src, 8);

    syscall(2, (uint32_t) &write_new_folder_request, (uint32_t) &retcode, 0); 

    // read dest folder
    memset(&temp_table, 0, sizeof(struct FAT32DirectoryTable));
    struct FAT32DriverRequest dest_folder_request = {
        .buf                   = &temp_table,
        .name                  = "",
        .ext                   = "",
        .parent_cluster_number = current_cluster_number,
        .buffer_size           = 2048,
    };
    memcpy(dest_folder_request.name, dest, 8);

    syscall(1, (uint32_t) &dest_folder_request, (uint32_t) &retcode, 0);

    // update src cluster number in dest
    for (int i = 0; i < 64; i++) {
        if (memcmp(temp_table.table[i].name, src, 8) == 0) {
            temp_table.table[i].cluster_low = src_low;
            temp_table.table[i].cluster_high = src_high;   
            break;
        }
    }

    struct FAT32DriverRequest update_src_cluster_number_request = {
        .buf                   = &temp_table,
        .name                  = "",
        .ext                   = "",
        .parent_cluster_number = target_cluster_number,
        .buffer_size           = 2048,
    };
    syscall(8, (uint32_t) &update_src_cluster_number_request, (uint32_t) &retcode, 0);
}

int lastIndex(char args[80][80]) {
    int i = 0;
    while (args[i][0] != '\0') {
        i++;
    }
    return i - 1;
}

void mv (char args[80][80]) {
    uint32_t low = (uint32_t)(current_folder.table[0].cluster_low);
    uint32_t high = ((uint32_t)current_folder.table[0].cluster_high) << 16;
    uint32_t current_cluster_number = (low | high);

    struct FAT32DirectoryTable table_buf = {0};
    struct FAT32DriverRequest request = {
        .buf = &table_buf,
        .name = "\0\0\0\0\0\0\0",
        .ext = "\0\0\0",
        .parent_cluster_number = current_cluster_number,
        .buffer_size = sizeof(struct FAT32DirectoryEntry)
    };
    uint32_t retcode;
    char* message;

    if (lastIndex(args) < 2) {
        syscall(6, (uint32_t) "Not enough arguments\n", (uint32_t) strlen("Not enough arguments\n"), 0x4);
        return;
    } else {
        memcpy(request.name, args[lastIndex(args)], strlen(args[lastIndex(args)]));
        syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);

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
            return;
        } else {
            // Checking if all files are found
            for (int i = 1; i < lastIndex(args); i++) {
                char file_name[8];
                char file_ext[3];
                parseNameAndExt(args[i], file_name, file_ext);
                memcpy(request.name, file_name, 8);
                memcpy(request.ext, file_ext, 3);
                syscall(1, (uint32_t) &request, (uint32_t) &retcode, 0);

                if (retcode == 2) {
                    message = "File/Folder not found\n";
                    syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
                    return;
                }                    
            }

            int p = lastIndex(args);
            // Starts moving
            for (int i = 1; i < p; i++) {
                char file_name[8];
                char file_ext[3];
                parseNameAndExt(args[i], file_name, file_ext);
                memcpy(request.name, file_name, 8);
                memcpy(request.ext, file_ext, 3);
                syscall(1, (uint32_t) &request, (uint32_t) &retcode, 0);

                if (retcode == 2) {
                    message = "File/Folder not found\n";
                    syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
                    continue;
                }

                char rmCommand[80][80];
                if (retcode == 0) { // Is a folder
                    memcpy(rmCommand[0], "rm", 2);
                    memcpy(rmCommand[1], "-r", 2);
                    memcpy(rmCommand[2], args[i], strlen(args[i]));
                    
                    mv_folder(args[i], args[lastIndex(args)]);
                } else {
                    memcpy(rmCommand[0], "rm", 2);
                    memcpy(rmCommand[1], args[i], strlen(args[i]));
                    char cpCommand[80][80];
                    memcpy(cpCommand[0], "cp", 2);
                    memcpy(cpCommand[1], args[i], strlen(args[i]));
                    memcpy(cpCommand[2], args[lastIndex(args)], strlen(args[lastIndex(args)]));
                    syscall(6, (uint32_t) "\n", (uint32_t) 1, 0x4);
                    execute_command(cpCommand);
                    execute_command(rmCommand);
                }
            }
        }

    }
}

void getFullPath(uint32_t clusterAddress, char* folderName, char* result) {
    int32_t retcode;
    struct FAT32DirectoryTable searchdirtable;
    struct FAT32DriverRequest request = {
        .buf = &searchdirtable,
        .parent_cluster_number = clusterAddress,
        .buffer_size = sizeof(struct FAT32DirectoryTable)
    };
    memcpy(request.name, folderName, strlen(folderName));
    request.name[strlen(folderName)] = '\0'; 
    syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);


    if (retcode == 0) {
        if (memcmp(searchdirtable.table[0].name, "root\0\0\0", 8) != 0) {  // if the current folder is not root
            uint32_t parentCluster = (uint32_t)(searchdirtable.table[1].cluster_high << 16) | (uint32_t)searchdirtable.table[1].cluster_low;
            getFullPath(parentCluster, searchdirtable.table[1].name, result);
            concat(result, "/", result);
            concat(result, searchdirtable.table[0].name, result);
        } else {
            concat(result, "root", result);
        }
    }
}

void DFSfind(uint32_t clusterNumber, char* parentName, char* parentExt, char* searchName, int v, bool visited[63], char* result) {
    visited[v-1] = true;
    bool visitedNew[63];
    clear(visitedNew, 63);

    struct FAT32DirectoryTable dirtable;
    struct FAT32DriverRequest request = {
        .buf = &dirtable,
        .parent_cluster_number = clusterNumber,
        .buffer_size = sizeof(struct FAT32DirectoryTable)
    };
    memcpy(request.name, parentName, strlen(parentName));
    memcpy(request.ext, parentExt, strlen(parentExt));


    int32_t retcode;
    syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);


    if (retcode == 0) { 
        for (size_t i = 2; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry); i++) {
            if (dirtable.table[i].user_attribute == UATTR_NOT_EMPTY) {
                uint32_t subClusterNumber = (dirtable.table[i].cluster_high << 16) | dirtable.table[i].cluster_low;

                if (dirtable.table[i].attribute == ATTR_SUBDIRECTORY) { 
                    if (memcmp(dirtable.table[i].name, searchName, (uint32_t)strlen(searchName)) == 0) {
                        getFullPath(clusterNumber, parentName, result);
                        concat(result, "/", result);
                        concat(result, dirtable.table[i].name, result);
                        syscall(6, (uint32_t)result, (uint32_t)strlen(result), 0x07);
                        syscall(6, (uint32_t)"\n", 1, 0x07);
                        return;
                    }
                    if (!visitedNew[i - 1]) {
                        DFSfind(subClusterNumber, dirtable.table[i].name, dirtable.table[i].ext, searchName, i, visitedNew, result);
                    }
                } else { 
                    if (memcmp(dirtable.table[i].name, searchName, 8) == 0) {
                        getFullPath(clusterNumber, parentName, result);
                        concat(result, "/", result);
                        concat(result, searchName, result);
                        if (strlen(dirtable.table[i].ext) != 0) {
                            concat(result, ".", result);
                            concat(result, dirtable.table[i].ext, result);
                        }
                        syscall(6, (uint32_t)result, (uint32_t)strlen(result), 0x07);
                        syscall(6, (uint32_t)"\n", 1, 0x07);
                        return;
                    }
                    visited[i - 1] = true;
                }
            }
        }
    } else {
        char* message;
        switch (retcode) {
                case 2:
                    message = "Folder not found.\n";
                    break;
                default:
                    message = "Unknown error.\n";
                    break;
            }
        syscall(6, (uint32_t) message, (uint32_t) strlen(message), 0x4);
        syscall(6, (uint32_t) "\n", (uint32_t) 1, 0x4);
        return;
    }
}


void find(char* args2) {
    if (strlen(args2) == 0) {
        syscall(6, (uint32_t)"Missing destination file or folder operand", strlen("Missing destination file or folder operand"), 0x4);
        syscall(6, (uint32_t)"\n", 1, 0x4);
        return;
    }
    uint32_t clusterNumber = ROOT_CLUSTER_NUMBER;
    char* parentName = "root\0\0\0";
    char result[2048] = "";  
    char name[8];
    char ext[3];
    parseNameAndExt(args2, name, ext);
    bool visited[63];
    clear(visited, 63);


    struct FAT32DirectoryTable dirtable;
    struct FAT32DriverRequest request = {
        .buf = &dirtable,
        .parent_cluster_number = clusterNumber,
        .buffer_size = sizeof(struct FAT32DirectoryTable)
    };
    memcpy(request.name, parentName, strlen(parentName));

    int32_t retcode;
    syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);

    if (retcode == 0) {
        for (int i = 2; i < 64; i++) {
            if (dirtable.table[i].user_attribute == UATTR_NOT_EMPTY) {
                if (dirtable.table[i].attribute == ATTR_SUBDIRECTORY) {
                    if (memcmp(dirtable.table[i].name, name, 8) == 0) {
                        if (memcmp(dirtable.table[i].name, args2, strlen(args2)) == 0) {
                            syscall(6, (uint32_t)"root/", strlen("root/"), 0x07);
                            syscall(6, (uint32_t)dirtable.table[i].name, (uint32_t)strlen(dirtable.table[i].name), 0x07);
                            syscall(6, (uint32_t)"\n", 1, 0x07);
                            return;
                        }
                    }
                    if (!visited[i - 1]) {
                        DFSfind(((uint32_t)(dirtable.table[i].cluster_high << 16) | (uint32_t)dirtable.table[i].cluster_low), dirtable.table[i].name, dirtable.table[i].ext, args2, i, visited, result);
                    }
                } else {
                    if (memcmp(dirtable.table[i].name, name, 8) == 0) {
                        getFullPath(clusterNumber, parentName, result);
                        concat(result, "/", result);
                        concat(result, name, result);
                        if (strlen(ext) != 0) {
                            concat(result, ".", result);
                            concat(result, ext, result);
                        }
                        syscall(6, (uint32_t)result, (uint32_t)strlen(result), 0x07);
                        syscall(6, (uint32_t)"\n", 1, 0x07);
                        return;
                    }
                    visited[i - 1] = true;
                }
            }
        }
    }
    syscall(6, (uint32_t)"\n", 1, 0x07);
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
    } else if (strings_equal(args[0], "rm")) {
        rm(args);
    } else if (strings_equal(args[0], "mv"))  {
        mv(args);
    } else if (strings_equal(args[0], "find"))  {
        find(args[1]);
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

    for (int i = 0; i < 80 ;i++){
        memset(args[i], 0, 80);
    }

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