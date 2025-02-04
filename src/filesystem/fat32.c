#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../header/stdlib/string.h"
#include "../header/filesystem/fat32.h"

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

void insert_directory(struct FAT32DirectoryTable* dir_table, int i, char* name, char* ext, uint8_t attribute, uint8_t user_attribute, uint8_t cluster_number, uint32_t filesize) {
    memset(dir_table->table[i].name, 0, 8);
    memset(dir_table->table[i].ext, 0, 8);

    uint16_t high_bits = (cluster_number >> 16) & 0xFFFF;
    uint16_t low_bits = cluster_number & 0xFFFF;

    memcpy(dir_table->table[i].name, name, strlen(name));
    memcpy(dir_table->table[i].ext, ext, strlen(ext));
    dir_table->table[i].attribute = attribute;
    dir_table->table[i].user_attribute = user_attribute;
    dir_table->table[i].cluster_high = high_bits;
    dir_table->table[i].cluster_low = low_bits;
    dir_table->table[i].filesize = filesize;
}

void create_fat32(void) {
    char* empty_cluster[CLUSTER_SIZE];
    memset(empty_cluster, 0, CLUSTER_SIZE);
    for (int i = 0; i < CLUSTER_MAP_SIZE; i++) {
        write_clusters(empty_cluster, i, 1);
    }

    char* boot_sector_ptr[BLOCK_SIZE];
    memcpy(boot_sector_ptr, fs_signature, BLOCK_SIZE);

    fat32_driver_state.fat_table.cluster_map[0] = CLUSTER_0_VALUE;
    fat32_driver_state.fat_table.cluster_map[1] = CLUSTER_1_VALUE;
    fat32_driver_state.fat_table.cluster_map[2] = FAT32_FAT_END_OF_FILE;
    for (int i = 3; i < CLUSTER_MAP_SIZE; i++) {
        fat32_driver_state.fat_table.cluster_map[i] = FAT32_FAT_EMPTY_ENTRY;
    }

    write_blocks(boot_sector_ptr, 0, 1);
    write_clusters(&fat32_driver_state.fat_table, 1, 1);

    struct FAT32DirectoryTable root;
    insert_directory(&root, 0, "root", "", ATTR_SUBDIRECTORY, UATTR_NOT_EMPTY, 2, 0);
    insert_directory(&root, 1, "..", "", ATTR_SUBDIRECTORY, UATTR_NOT_EMPTY, 2, 0);

    write_clusters(&root, 2, 1);
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

bool folder_not_empty(uint32_t cluster_number) {
    struct FAT32DirectoryTable folder;
    read_clusters(&folder, cluster_number, 1);

    for (int i = 2; i < 64; i++) {
        if (folder.table[i].user_attribute == UATTR_NOT_EMPTY) {
            return true;
        }
    }
    return false;
}

bool is_valid_parent_cluster(uint32_t cluster_number) {
    if (cluster_number >= CLUSTER_MAP_SIZE || cluster_number < ROOT_CLUSTER_NUMBER) {
        return false;
    }
    if (fat32_driver_state.fat_table.cluster_map[cluster_number] != FAT32_FAT_END_OF_FILE) {
        return false;
    }
    struct FAT32DirectoryTable test_folder;
    read_clusters(&test_folder, cluster_number, 1);

    return (test_folder.table[0].attribute == ATTR_SUBDIRECTORY && test_folder.table[1].attribute == ATTR_SUBDIRECTORY);
}

uint32_t bytes_to_cluster(uint32_t filesize) {
    if (filesize % CLUSTER_SIZE == 0) {
        return filesize / CLUSTER_SIZE;
    } else {
        return (filesize / CLUSTER_SIZE) + 1;
    }
}

int first_fit(uint32_t clusters_needed) {
    uint32_t start_idx = 3;
    while (start_idx < CLUSTER_MAP_SIZE) {
        while (start_idx < CLUSTER_MAP_SIZE && fat32_driver_state.fat_table.cluster_map[start_idx] != FAT32_FAT_EMPTY_ENTRY) {
            start_idx++;
        }
        if (start_idx >= CLUSTER_MAP_SIZE) {
            return -1;
        }

        uint32_t empty_count = 0;
        uint32_t search_idx = start_idx;
        while (search_idx < CLUSTER_MAP_SIZE && fat32_driver_state.fat_table.cluster_map[search_idx] == FAT32_FAT_EMPTY_ENTRY) {
            empty_count++;
            search_idx++;
        }
        if (empty_count >= clusters_needed) {
            return start_idx;
        } else {
            start_idx = search_idx;
        }
    }
    return -1;
}

int8_t read(struct FAT32DriverRequest request) {
    if (!is_valid_parent_cluster(request.parent_cluster_number)) {
        return -1;
    }
    read_clusters(fat32_driver_state.dir_table_buf.table, request.parent_cluster_number, 1);

    for (int i = 0; i < 64; i++) {
        if ((memcmp(request.name, fat32_driver_state.dir_table_buf.table[i].name, 8) == 0) && (memcmp(request.ext, fat32_driver_state.dir_table_buf.table[i].ext, 3) == 0)) {
            if (fat32_driver_state.dir_table_buf.table[i].attribute == ATTR_SUBDIRECTORY) {
                return 1;
            }
            if (request.buffer_size < fat32_driver_state.dir_table_buf.table[i].filesize) {
                return 2;
            }
            uint32_t low = (uint32_t)(fat32_driver_state.dir_table_buf.table[i].cluster_low);
            uint32_t high = ((uint32_t) fat32_driver_state.dir_table_buf.table[i].cluster_high) << 16;
            uint32_t j = (low | high);

            uint32_t k = 0;
            do {
                read_clusters(((char*) request.buf) + (CLUSTER_SIZE * k), j, 1);
                j = fat32_driver_state.fat_table.cluster_map[j];
                k++;
            } while (j != FAT32_FAT_END_OF_FILE);

            return 0;
        }
    }
    return 3;
}

int8_t read_directory(struct FAT32DriverRequest request) {
    if (!is_valid_parent_cluster(request.parent_cluster_number)) {
        return -1;
    }
    read_clusters(fat32_driver_state.dir_table_buf.table, request.parent_cluster_number, 1);

    for (int i = 0; i < 64; i++) {
        if ((memcmp(request.name, fat32_driver_state.dir_table_buf.table[i].name, 8) == 0) && (memcmp(request.ext, fat32_driver_state.dir_table_buf.table[i].ext, 3) == 0)) {
            if (fat32_driver_state.dir_table_buf.table[i].attribute == ATTR_SUBDIRECTORY) {
                uint32_t low = (uint32_t)(fat32_driver_state.dir_table_buf.table[i].cluster_low);
                uint32_t high = ((uint32_t) fat32_driver_state.dir_table_buf.table[i].cluster_high) << 16;
                uint32_t j = (low | high);

                read_clusters(request.buf, j, bytes_to_cluster(request.buffer_size));
                return 0;
            } else {
                return 1;
            }
        }
    }

    return 2;
}

int8_t write(struct FAT32DriverRequest request) {
    if (!is_valid_parent_cluster(request.parent_cluster_number)) {
        return 2;
    }
    struct FAT32DirectoryTable parent_folder;
    read_clusters(parent_folder.table, request.parent_cluster_number, 1);

    for (int i = 1; i < 64; i++) {
        if ((memcmp(request.name, parent_folder.table[i].name, 8) == 0) && (memcmp(request.ext, parent_folder.table[i].ext, 3) == 0)) {
            return 1;
        }
    }

    if (request.buffer_size == 0) {
        int target_cluster = 3;
        while (fat32_driver_state.fat_table.cluster_map[target_cluster] != FAT32_FAT_EMPTY_ENTRY) {
            target_cluster++;
        }

        struct FAT32DirectoryTable new_folder;
        memset(new_folder.table, 0, sizeof(struct FAT32DirectoryTable));
        insert_directory(&new_folder, 0, request.name, "", ATTR_SUBDIRECTORY, UATTR_NOT_EMPTY, target_cluster, 0);
        insert_directory(&new_folder, 1, "..", "", ATTR_SUBDIRECTORY, UATTR_NOT_EMPTY, request.parent_cluster_number, 0);

        int i = 2;
        while (parent_folder.table[i].user_attribute == UATTR_NOT_EMPTY) {
            i++;
        }
        insert_directory(&parent_folder, i, request.name, "", ATTR_SUBDIRECTORY, UATTR_NOT_EMPTY, target_cluster, 0);

        fat32_driver_state.fat_table.cluster_map[target_cluster] = FAT32_FAT_END_OF_FILE;

        write_clusters(new_folder.table, target_cluster, 1);
        write_clusters(parent_folder.table, request.parent_cluster_number, 1);
        write_clusters(fat32_driver_state.fat_table.cluster_map, FAT_CLUSTER_NUMBER, 1);

        return 0;
    } else {
        if (request.buffer_size > 0) {
                int clusters_needed = bytes_to_cluster(request.buffer_size);
                int cluster_idx = first_fit(clusters_needed);

                if (cluster_idx > 0) {    
                    int i = 2;
                    while (parent_folder.table[i].user_attribute == UATTR_NOT_EMPTY) {
                        i++;
                    }
                    insert_directory(&parent_folder, i, request.name, request.ext, 0, UATTR_NOT_EMPTY, cluster_idx, request.buffer_size);
                    write_clusters(parent_folder.table, request.parent_cluster_number, 1);

                    for (int i = cluster_idx; i < cluster_idx + clusters_needed; i++) {
                        if (i != cluster_idx + clusters_needed - 1) {
                            fat32_driver_state.fat_table.cluster_map[i] = i + 1;
                        } else {
                            fat32_driver_state.fat_table.cluster_map[i] = FAT32_FAT_END_OF_FILE;
                        }
                        write_clusters(((char*) request.buf) + (CLUSTER_SIZE * (i - cluster_idx)), i, 1);
                    }
                    write_clusters(fat32_driver_state.fat_table.cluster_map, FAT_CLUSTER_NUMBER, 1);

                    return 0;
                } else {
                    return -1;
                }
        } else {
            return -1;
        }
    }
}

int8_t delete(struct FAT32DriverRequest request) {
    if (!is_valid_parent_cluster(request.parent_cluster_number)) {
        return -1;
    }
    struct FAT32DirectoryTable parent_folder;
    read_clusters(parent_folder.table, request.parent_cluster_number, 1);

    for (int i = 0; i < 64; i++) {
        if (memcmp(request.name, parent_folder.table[i].name, 8) == 0) {
            if (memcmp(request.ext, parent_folder.table[i].ext, 3) == 0) {
                uint32_t low = (uint32_t)(parent_folder.table[i].cluster_low);
                uint32_t high = ((uint32_t) parent_folder.table[i].cluster_high) << 16;
                uint32_t j = (low | high);

                if ((parent_folder.table[i].attribute == ATTR_SUBDIRECTORY) && (folder_not_empty(j))) {
                    return 2;
                }

                char* empty_cluster[CLUSTER_SIZE];
                memset(empty_cluster, 0, CLUSTER_SIZE);
                
                uint32_t next;
                do { 
                    write_clusters(empty_cluster, j, 1);
                    next = fat32_driver_state.fat_table.cluster_map[j];
                    fat32_driver_state.fat_table.cluster_map[j] = FAT32_FAT_EMPTY_ENTRY;
                    j = next;
                } while (j != FAT32_FAT_END_OF_FILE);
                memset(&(parent_folder.table[i]), 0, 32);

                write_clusters(fat32_driver_state.fat_table.cluster_map, 1, 1);
                write_clusters(parent_folder.table, request.parent_cluster_number, 1);
                return 0;
            }
        }
    }
    return 1;
}

int8_t update_directory_table(struct FAT32DriverRequest request) {
    if (!is_valid_parent_cluster(request.parent_cluster_number)) {
        return -1;
    }

    struct FAT32DirectoryTable parent_folder;
    read_clusters(parent_folder.table, request.parent_cluster_number, 1);
    if (parent_folder.table[0].attribute != ATTR_SUBDIRECTORY) {
        return 1;
    }


    write_clusters(request.buf, request.parent_cluster_number, 1);
    return 0;
}