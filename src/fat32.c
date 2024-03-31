#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/stdlib/string.h"
#include "header/filesystem/fat32.h"

const uint8_t fs_signature[BLOCK_SIZE] = {
    'C', 'o', 'u', 'r', 's', 'e', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'D', 'e', 's', 'i', 'g', 'n', 'e', 'd', ' ', 'b', 'y', ' ', ' ', ' ', ' ',  ' ',
    'L', 'a', 'b', ' ', 'S', 'i', 's', 't', 'e', 'r', ' ', 'I', 'T', 'B', ' ',  ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', '<', '3', ' ', ' ', ' ',  ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '4', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

static struct FAT32DriverState fat32_driver_state;

uint32_t cluster_to_lba(uint32_t cluster) {
    return cluster * 4;
}

// void init_directory_table(struct FAT32DirectoryTable *dir_table, char *name, uint32_t parent_dir_cluster) {
//     memcpy(&(dir_table->table[0].name), name, 8);
//     dir_table->table[0].
// }

void create_fat32(void) {
    void* boot_sector_ptr = (void*) BOOT_SECTOR;
    memcpy(boot_sector_ptr, fs_signature, BLOCK_SIZE);
    fat32_driver_state.fat_table.cluster_map[0] = CLUSTER_0_VALUE;
    fat32_driver_state.fat_table.cluster_map[1] = CLUSTER_1_VALUE;
    fat32_driver_state.fat_table.cluster_map[2] = FAT32_FAT_END_OF_FILE;
    for (int i = 3; i < CLUSTER_MAP_SIZE; i++) {
        fat32_driver_state.fat_table.cluster_map[i] = FAT32_FAT_EMPTY_ENTRY;
    }
    write_clusters(boot_sector_ptr, 0, 1);
    write_clusters(&fat32_driver_state.fat_table, 1, 1);
}

bool is_empty_storage(void) {
    struct ClusterBuffer boot_sector;
    read_clusters(&boot_sector, BOOT_SECTOR, 1);

    return (memcmp(&boot_sector, fs_signature, BLOCK_SIZE) != 0);
}

void initialize_filesystem_fat32(void) {
    if (is_empty_storage()) {
        create_fat32();
    } else {
        read_clusters(&fat32_driver_state.fat_table, 1, 1);
    }
}

void write_clusters(const void *ptr, uint32_t cluster_number, uint8_t cluster_count) {
    write_blocks(ptr, cluster_to_lba(cluster_number), cluster_to_lba(cluster_count));
}

void read_clusters(void *ptr, uint32_t cluster_number, uint8_t cluster_count) {
    read_blocks(ptr, cluster_to_lba(cluster_number), cluster_to_lba(cluster_count));
}

uint32_t bytes_to_cluster(uint32_t filesize) {
    if (filesize % CLUSTER_SIZE == 0) {
        return filesize / CLUSTER_SIZE;
    } else {
        return (filesize / CLUSTER_SIZE) + 1;
    }
}

int8_t read(struct FAT32DriverRequest request) {
    bool not_file_flag = false;

    read_clusters(fat32_driver_state.dir_table_buf.table, request.parent_cluster_number, 1);

    for (int i = 0; i < 64; i++) {
        if (memcmp(request.name, fat32_driver_state.dir_table_buf.table[i].name, 8) == 0) {
            if (memcmp(request.ext, fat32_driver_state.dir_table_buf.table[i].ext, 3) == 0) {
                uint32_t filesize_in_clusters = bytes_to_cluster(fat32_driver_state.dir_table_buf.table[i].filesize);

                if (request.buffer_size >= filesize_in_clusters) {
                    uint32_t low = (uint32_t)(fat32_driver_state.dir_table_buf.table[i].cluster_low);
                    uint32_t high = ((uint32_t) fat32_driver_state.dir_table_buf.table[i].cluster_high) << 16;
                    uint32_t j = (low | high);

                    struct ClusterBuffer temp[request.buffer_size];
                    uint32_t k = 0;
                    do {
                        read_clusters(&temp[k], j, 1);
                        j = fat32_driver_state.fat_table.cluster_map[j];
                        k++;
                    } while (j != FAT32_FAT_END_OF_FILE);

                    memcpy(request.buf, temp, CLUSTER_SIZE * request.buffer_size);
                    return 0;
                } else {
                    return 2;
                }
            } else {
                not_file_flag = true;
            }
        }
    }

    if (not_file_flag) {
        return 1;
    } 
    return 3;
}

int8_t read_directory(struct FAT32DriverRequest request) {
    bool not_folder_flag = false;
    read_clusters(fat32_driver_state.dir_table_buf.table, request.parent_cluster_number, 1);

    for (int i = 0; i < 64; i++) {
        if (memcmp(request.name, fat32_driver_state.dir_table_buf.table[i].name, 8) == 0) {
            if (fat32_driver_state.dir_table_buf.table[i].attribute == ATTR_SUBDIRECTORY) {
                uint32_t low = (uint32_t)(fat32_driver_state.dir_table_buf.table[i].cluster_low);
                uint32_t high = ((uint32_t) fat32_driver_state.dir_table_buf.table[i].cluster_high) << 16;
                uint32_t j = (low | high);

                read_clusters(request.buf, j, request.buffer_size);
                return 0;
            } else {
                not_folder_flag = true;
            }
        }
    }

    if (not_folder_flag) {
        write_clusters("not folder", 32, 1);
        return 1;
    }

    return -1;
}

// int8_t write(struct FAT32DriverRequest request) {

// }