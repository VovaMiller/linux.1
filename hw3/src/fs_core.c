#include <fs_core.h>

void directory_block_init(char *bytes, inode_pointer_t *inode_current, inode_pointer_t *inode_parent) {
    // Overwrite all with empty records.
    unsigned int i;
    struct BlockDirectoryRecord record_empty = {0, {'\0'}};
    for (i = 0; i < RECORDS_PER_BLOCK; ++i) {
        memcpy(bytes + i * sizeof(struct BlockDirectoryRecord), &record_empty, sizeof(struct BlockDirectoryRecord));
    }

    // Add two special records at the beginning.
    if ((inode_current != NULL) && (inode_parent != NULL)) {
        struct BlockDirectoryRecord record_current = {*inode_current, {'.', '\0'}};
        struct BlockDirectoryRecord record_parent = {*inode_parent, {'.', '.', '\0'}};
        memcpy(bytes, &record_current, sizeof(struct BlockDirectoryRecord));
        memcpy(bytes + sizeof(struct BlockDirectoryRecord), &record_parent, sizeof(struct BlockDirectoryRecord));
    }
}

FILE* open_fs_file(const char *fname) {
    // Opening file.
    FILE *file = fopen(fname, "r+");
    if (file == NULL) {
        fprintf(stderr, "Error while opening FS file.\n");
        exit(1);
    }

    // Sanity check.
    struct SuperBlock superblock;
    fread(&superblock, sizeof(struct SuperBlock), 1, file);
    if (superblock.magic_number != FS_MAGIC_NUMBER) {
        fprintf(stderr, "Provided file is not FS file.\n");
        exit(1);
    }
    if (superblock.block_size != FS_BLOCK_SIZE) {
        fprintf(stderr, "Block size other than %d is not supported yet!\n", FS_BLOCK_SIZE);
        exit(1);
    }

    return file;
}

FILE* generate_fs_file(const char *fname) {
    unsigned int i;
    char bitmap_chunk = 0;

    // Reserve the first bit in all bitmaps for root directory.
    write_bit(&bitmap_chunk, 0, 1);

    FILE *file = fopen(fname, "w+");
    if (file == NULL) {
        fprintf(stderr, "Error while creating file for FS.\n");
        exit(1);
    }

    // Super Block
    struct SuperBlock superblock;
    superblock.magic_number = FS_MAGIC_NUMBER;
    superblock.block_size = FS_BLOCK_SIZE;
    fwrite(&superblock, sizeof(struct SuperBlock), 1, file);

    // Blocks Bitmap Area
    fputc(bitmap_chunk, file);
    for (i = 1; i < AREA_SIZE_BITMAP_BLOCKS; ++i) {
        fputc(0, file);
    }

    // inodes Bitmap Area
    fputc(bitmap_chunk, file);
    for (i = 1; i < AREA_SIZE_BITMAP_INODES; ++i) {
        fputc(0, file);
    }

    // inodes table
    struct INode inode = {TYPE_NONE, 0, {0}};
    struct INode inode_root = {TYPE_DIRECTORY, 1, {0}};
    unsigned int inodes_count = (1 << (8 * sizeof(inode_pointer_t)));
    fwrite(&inode_root, sizeof(struct INode), 1, file);
    for (i = 1; i < inodes_count; ++i) {
        fwrite(&inode, sizeof(struct INode), 1, file);
    }

    // Blocks
    // Generate the only block for root directory.
    // Space for other blocks will be allocated dynamically.
    char block[FS_BLOCK_SIZE] = {0};
    inode_pointer_t inode_root_p = 0;
    directory_block_init(block, &inode_root_p, &inode_root_p);
    fwrite(block, FS_BLOCK_SIZE, 1, file);

    return file;
}

char get_block_k(FILE* fs, struct INode *inode, block_pointer_t k, block_pointer_t* block_p_holder) {
    block_pointer_t block_p;
    unsigned short level;
    unsigned int p;
    char block_temp[FS_BLOCK_SIZE];

    // Sanity check.
    if (k >= inode->file_size) {
        return 1;
    }

    // Determine required level.
    if (k < (INODE_BLOCKS_COUNT - 3)) {
        // Direct addressing.
        block_p = inode->block_p[k];
        level = 0;
    } else {
        // Inirect addressing.
        k -= (INODE_BLOCKS_COUNT - 3);
        if (k < BLOCKS_P_PER_BLOCK) {
            block_p = inode->block_p[INODE_BLOCKS_COUNT - 3];
            level = 1;
        } else {
            k -= BLOCKS_P_PER_BLOCK;
            if (k < int_pow(BLOCKS_P_PER_BLOCK, 2)) {
                block_p = inode->block_p[INODE_BLOCKS_COUNT - 2];
                level = 2;
            } else {
                k -= int_pow(BLOCKS_P_PER_BLOCK, 2);
                if (k < int_pow(BLOCKS_P_PER_BLOCK, 3)) {
                    block_p = inode->block_p[INODE_BLOCKS_COUNT - 1];
                    level = 3;
                } else {
                    return 2;
                }
            }
        }
    }

    // Iteratively descend to level 0.
    while (level > 0) {
        p = k / int_pow(BLOCKS_P_PER_BLOCK, level - 1);
        k = k % int_pow(BLOCKS_P_PER_BLOCK, level - 1);
        fseek(fs, AREA_POS_BLOCKS + FS_BLOCK_SIZE * block_p, SEEK_SET);
        fread(block_temp, FS_BLOCK_SIZE, 1, fs);
        memcpy(&block_p, block_temp + sizeof(block_pointer_t) * p, sizeof(block_pointer_t));
        --level;
    }

    *block_p_holder = block_p;
    return 0;
}

char is_block_allocated(FILE *fs, block_pointer_t block_p) {
    fseek(fs, 0L, SEEK_END);
    long sz = ftell(fs);
    if ((AREA_POS_BLOCKS + FS_BLOCK_SIZE * block_p) < sz) {
        return 1;
    } else {
        return 0;
    }
}

char occupy_block(FILE *fs, block_pointer_t *block_p_holder) {
    unsigned int i, j;
    block_pointer_t block_p;
    char page[PAGE_SIZE_BITMAP_BLOCKS];
    char block[FS_BLOCK_SIZE] = {0};
    fseek(fs, AREA_POS_BITMAP_BLOCKS, SEEK_SET);

    for (i = 0; i < PAGES_COUNT; ++i) {
        fread(page, PAGE_SIZE_BITMAP_BLOCKS, 1, fs);
        for (j = 0; j < PAGE_SIZE_BITMAP_BLOCKS; ++j) {
            if (!(read_bit(page, j))) {
                block_p = i * PAGE_SIZE_BITMAP_BLOCKS + j;
                write_bit(page, j, 1);

                // Update blocks bitmap.
                fseek(fs, AREA_POS_BITMAP_BLOCKS + i * PAGE_SIZE_BITMAP_BLOCKS, SEEK_SET);
                fwrite(page, PAGE_SIZE_BITMAP_BLOCKS, 1, fs);

                // Initialize (and allocate) occupied block.
                fseek(fs, AREA_POS_BLOCKS + FS_BLOCK_SIZE * block_p, SEEK_SET);
                fwrite(block, FS_BLOCK_SIZE, 1, fs);

                *block_p_holder = block_p;
                return 0;
            }
        }
    }

    return 1;
}

void free_block(FILE *fs, block_pointer_t block_p) {
    char byte;
    fseek(fs, AREA_POS_BITMAP_BLOCKS + (block_p / 8), SEEK_SET);
    fread(&byte, sizeof(byte), 1, fs);
    write_bit(&byte, block_p % 8, 0);
    fseek(fs, -sizeof(byte), SEEK_CUR);
    fwrite(&byte, sizeof(byte), 1, fs);
}

char inode_block_append(FILE *fs, struct INode *inode, block_pointer_t *block_p_holder) {
    unsigned int p;
    unsigned int k = inode->file_size;
    unsigned int level = 0;
    block_pointer_t block_p, new_block_p;
    if (k < (INODE_BLOCKS_COUNT - 3)) {
        // Direct addressing.
        if (!(occupy_block(fs, &new_block_p))) {
            inode->block_p[k] = new_block_p;
            inode->file_size += 1;
            if (block_p_holder != NULL) {
                *block_p_holder = new_block_p;
            }
            return 0;
        } else {
            return 1;
        }
    } else {
        // Inirect addressing.
        k -= (INODE_BLOCKS_COUNT - 3);
        if (k < BLOCKS_P_PER_BLOCK) {
            block_p = inode->block_p[INODE_BLOCKS_COUNT - 3];
            level = 1;
        } else {
            k -= BLOCKS_P_PER_BLOCK;
            if (k < int_pow(BLOCKS_P_PER_BLOCK, 2)) {
                block_p = inode->block_p[INODE_BLOCKS_COUNT - 2];
                level = 2;
            } else {
                k -= int_pow(BLOCKS_P_PER_BLOCK, 2);
                if (k < int_pow(BLOCKS_P_PER_BLOCK, 3)) {
                    block_p = inode->block_p[INODE_BLOCKS_COUNT - 1];
                    level = 3;
                } else {
                    return 2;
                }
            }
        }
    }

    // level is from {1, 2, 3}.
    if (k == 0) {
        if (!(occupy_block(fs, &new_block_p))) {
            inode->block_p[INODE_BLOCKS_COUNT - (4 - level)] = new_block_p;
            block_p = new_block_p;
        } else {
            return 1;
        }
    }

    while (level > 1) {
        p = k / int_pow(BLOCKS_P_PER_BLOCK, level - 1);
        k = k % int_pow(BLOCKS_P_PER_BLOCK, level - 1);
        if (k == 0) {
            if (!(occupy_block(fs, &new_block_p))) {
                fseek(fs, AREA_POS_BLOCKS + block_p * FS_BLOCK_SIZE + p * sizeof(block_pointer_t), SEEK_SET);
                fwrite(&new_block_p, sizeof(new_block_p), 1, fs);
                block_p = new_block_p;
            } else {
                return 1;
            }
        } else {
            fseek(fs, AREA_POS_BLOCKS + block_p * FS_BLOCK_SIZE + p * sizeof(block_pointer_t), SEEK_SET);
            fread(&block_p, sizeof(block_p), 1, fs);
        }
        --level;
    }

    if (!(occupy_block(fs, &new_block_p))) {
        fseek(fs, AREA_POS_BLOCKS + block_p * FS_BLOCK_SIZE + k * sizeof(block_pointer_t), SEEK_SET);
        fwrite(&new_block_p, sizeof(new_block_p), 1, fs);
        inode->file_size += 1;
        if (block_p_holder != NULL) {
            *block_p_holder = new_block_p;
        }
        return 0;
    } else {
        return 1;
    }
}

char inode_block_pop(FILE *fs, struct INode *inode) {
    // Basically we can just substract inode.file_size.
    // But the most difficult part is to find out whether there will completely
    //  unused block left. If so, we should call free_block.
    unsigned int p;
    unsigned int k = inode->file_size - 1;
    unsigned int level = 0;
    block_pointer_t block_p, block_p_victim;

    // Is there anything to pop?
    if (inode->file_size == 0) {
        return INODE_BLOCK_POP_NOTHING;
    }

    if (k < (INODE_BLOCKS_COUNT - 3)) {
        // Direct addressing.
        block_p = inode->block_p[k];
        level = 0;
    } else {
        // Inirect addressing.
        k -= (INODE_BLOCKS_COUNT - 3);
        if (k < BLOCKS_P_PER_BLOCK) {
            block_p = inode->block_p[INODE_BLOCKS_COUNT - 3];
            level = 1;
        } else {
            k -= BLOCKS_P_PER_BLOCK;
            if (k < int_pow(BLOCKS_P_PER_BLOCK, 2)) {
                block_p = inode->block_p[INODE_BLOCKS_COUNT - 2];
                level = 2;
            } else {
                k -= int_pow(BLOCKS_P_PER_BLOCK, 2);
                if (k < int_pow(BLOCKS_P_PER_BLOCK, 3)) {
                    block_p = inode->block_p[INODE_BLOCKS_COUNT - 1];
                    level = 3;
                } else {
                    return INODE_BLOCK_POP_OVERSIZE;
                }
            }
        }
    }

    // level is from {1, 2, 3}.
    while (level > 0) {
        p = k / int_pow(BLOCKS_P_PER_BLOCK, level - 1);
        k = k % int_pow(BLOCKS_P_PER_BLOCK, level - 1);
        fseek(fs, AREA_POS_BLOCKS + block_p * FS_BLOCK_SIZE + p * sizeof(block_pointer_t), SEEK_SET);
        fread(&block_p_victim, sizeof(block_pointer_t), 1, fs);
        if ((p == 0) && (k == 0)) {
            free_block(fs, block_p);
        }
        block_p = block_p_victim;
        --level;
    }

    free_block(fs, block_p);
    inode->file_size -= 1;
    return INODE_BLOCK_POP_SUCCESS;
}

void get_block(FILE* fs, block_pointer_t block_p, char* block_holder) {
    fseek(fs, AREA_POS_BLOCKS + block_p * FS_BLOCK_SIZE, SEEK_SET);
    fread(block_holder, FS_BLOCK_SIZE, 1, fs);
}

void update_block(FILE* fs, block_pointer_t block_p, char* block) {
    fseek(fs, AREA_POS_BLOCKS + block_p * FS_BLOCK_SIZE, SEEK_SET);
    fwrite(block, FS_BLOCK_SIZE, 1, fs);
}

char get_inode_by_name_in_block(FILE* fs, block_pointer_t block_p, const char* name, inode_pointer_t* inode_p_holder) {
    // Reading block.
    char block[FS_BLOCK_SIZE];
    fseek(fs, AREA_POS_BLOCKS + FS_BLOCK_SIZE * block_p, SEEK_SET);
    fread(block, FS_BLOCK_SIZE, 1, fs);

    // Searching.
    struct BlockDirectoryRecord record;
    int i;
    for (i = 0; i < RECORDS_PER_BLOCK; ++i) {
        memcpy(&record, block + i * RECORD_SIZE, RECORD_SIZE);
        if (strcmp(name, record.name) == 0) {
            if (inode_p_holder != NULL) *inode_p_holder = record.inode_p;
            return 0;
        }
    }

    return 1;
}

char get_inode_by_name_in_inode(FILE* fs, struct INode *inode, const char* name, inode_pointer_t* inode_p_holder) {
    block_pointer_t k = 0;
    block_pointer_t block_p = 0;
    while (!(get_block_k(fs, inode, k, &block_p))) {
        if (get_inode_by_name_in_block(fs, block_p, name, inode_p_holder) == 0) {
            return 0;
        }
        ++k;
    }
    return 1;
}

char get_name_by_inode_in_block(FILE* fs, block_pointer_t block_p, inode_pointer_t inode_p, char* name_holder) {
    // Reading block.
    char block[FS_BLOCK_SIZE];
    fseek(fs, AREA_POS_BLOCKS + FS_BLOCK_SIZE * block_p, SEEK_SET);
    fread(block, FS_BLOCK_SIZE, 1, fs);

    // Searching.
    struct BlockDirectoryRecord record;
    int i;
    for (i = 0; i < RECORDS_PER_BLOCK; ++i) {
        memcpy(&record, block + i * RECORD_SIZE, RECORD_SIZE);
        if (inode_p == record.inode_p) {
            memcpy(name_holder, record.name, sizeof(record.name));
            return 0;
        }
    }

    return 1;
}

char get_name_by_inode_in_inode(FILE* fs, struct INode *inode, inode_pointer_t inode_p, char* name_holder) {
    block_pointer_t k = 0;
    block_pointer_t block_p = 0;
    while (!(get_block_k(fs, inode, k, &block_p))) {
        if (get_name_by_inode_in_block(fs, block_p, inode_p, name_holder) == 0) {
            return 0;
        }
        ++k;
    }
    return 1;
}

char get_parent_directory(FILE* fs, inode_pointer_t inode_p, inode_pointer_t* inode_p_parent) {
    // Reading directory inode.
    struct INode inode;
    fseek(fs, AREA_POS_INODES + inode_p * sizeof(struct INode), SEEK_SET);
    fread(&inode, sizeof(struct INode), 1, fs);

    // Sanity check.
    if (inode.file_type != TYPE_DIRECTORY) {
        return 1;
    }

    block_pointer_t k = 0;
    block_pointer_t block_p = 0;
    char name_parent[3] = {'.', '.', '\0'};
    while (!(get_block_k(fs, &inode, k, &block_p))) {
        if (get_inode_by_name_in_block(fs, block_p, name_parent, inode_p_parent) == 0) {
            return 0;
        }
        ++k;
    }

    return 2;
}

char get_directory_name(FILE* fs, inode_pointer_t inode_p, char* name_holder) {
    // Name of root is zero string.
    if (inode_p == 0) {
        char zero = '\0';
        memcpy(name_holder, &zero, sizeof(zero));
        return 0;
    }

    // Getting parent directory.
    inode_pointer_t inode_p_parent;
    if (get_parent_directory(fs, inode_p, &inode_p_parent)) {
        return 1;
    }

    // Reading parent directory inode.
    struct INode inode_parent;
    fseek(fs, AREA_POS_INODES + inode_p_parent * sizeof(struct INode), SEEK_SET);
    fread(&inode_parent, sizeof(struct INode), 1, fs);

    // Getting out directory name from parent directory.
    block_pointer_t block_p;
    block_pointer_t k = 0;
    while (!(get_block_k(fs, &inode_parent, k, &block_p))) {
        if (get_name_by_inode_in_block(fs, block_p, inode_p, name_holder) == 0) {
            return 0;
        }
        ++k;
    }

    return 2;
}

int get_full_path(FILE *fs, inode_pointer_t inode_p, char *holder) {
    unsigned int level = 0;
    inode_pointer_t inode_p2;
    int i;
    size_t path_len = 0;
    inode_pointer_t *inode_fp;
    char zero = '\0';

    // Trivial case with root directory.
    if (inode_p == 0) {
        strcpy(holder + path_len, "/");
        path_len += 1;
        return 0;
    }

    // Determine a level current directory is in.
    inode_p2 = inode_p;
    while (inode_p2 != 0) {
        if (get_parent_directory(fs, inode_p2, &inode_p2)) {
            return 1;
        }
        ++level;
    }
    inode_fp = malloc(level * sizeof(inode_pointer_t));

    // Build fullpath chain of inodes numbers.
    inode_p2 = inode_p;
    for (i = 0; i < level; ++i) {
        inode_fp[i] = inode_p2;
        if (get_parent_directory(fs, inode_p2, &inode_p2)) {
            free(inode_fp);
            return 1;
        }
    }

    // Getting full path.
    char name[MAX_NAME_LENGTH];
    for (i = level - 1; i >= 0; --i) {
        if (get_directory_name(fs, inode_fp[i], name) == 0) {
            strcpy(holder + path_len, "/");
            path_len += 1;
            strcpy(holder + path_len, name);
            path_len += strlen(name);
        } else {
            memcpy(holder + path_len, "/...", 4);
            path_len += 4;
        }
    }

    free(inode_fp);
    memcpy(holder + path_len, &zero, sizeof(zero));
    path_len += sizeof(zero);
    return 0;
}

char occupy_inode(FILE *fs, inode_pointer_t *inode_p_holder) {
    unsigned int j;
    inode_pointer_t inode_p;
    struct INode inode = {TYPE_NONE, 0, {0}};
    char bitmap_inodes[AREA_SIZE_BITMAP_INODES];

    fseek(fs, AREA_POS_BITMAP_INODES, SEEK_SET);
    fread(bitmap_inodes, AREA_SIZE_BITMAP_INODES, 1, fs);
    for (j = 0; j < AREA_SIZE_BITMAP_INODES; ++j) {
        if (!(read_bit(bitmap_inodes, j))) {
            inode_p = j;
            write_bit(bitmap_inodes, j, 1);

            // Update inodes bitmap.
            fseek(fs, AREA_POS_BITMAP_INODES, SEEK_SET);
            fwrite(bitmap_inodes, AREA_SIZE_BITMAP_INODES, 1, fs);

            // Initialize occupied inode.
            fseek(fs, AREA_POS_INODES + sizeof(struct INode) * inode_p, SEEK_SET);
            fwrite(&inode, sizeof(struct INode), 1, fs);

            *inode_p_holder = inode_p;
            return 0;
        }
    }

    return 1;
}

void free_inode(FILE *fs, inode_pointer_t inode_p) {
    char byte;
    fseek(fs, AREA_POS_BITMAP_INODES + (inode_p / 8), SEEK_SET);
    fread(&byte, sizeof(byte), 1, fs);
    write_bit(&byte, inode_p % 8, 0);
    fseek(fs, -sizeof(byte), SEEK_CUR);
    fwrite(&byte, sizeof(byte), 1, fs);
}

void get_inode(FILE* fs, inode_pointer_t inode_p, struct INode *inode_holder) {
    fseek(fs, AREA_POS_INODES + inode_p * sizeof(struct INode), SEEK_SET);
    fread(inode_holder, sizeof(struct INode), 1, fs);
}

void update_inode(FILE* fs, inode_pointer_t inode_p, struct INode *inode) {
    fseek(fs, AREA_POS_INODES + inode_p * sizeof(struct INode), SEEK_SET);
    fwrite(inode, sizeof(struct INode), 1, fs);
}

char is_directory_block_full(char *block) {
    struct BlockDirectoryRecord record;
    unsigned int i;
    for (i = 0; i < RECORDS_PER_BLOCK; ++i) {
        memcpy(&record, block + i * RECORD_SIZE, RECORD_SIZE);
        if (strlen(record.name) == 0) {
            return 0;
        }
    }
    return 1;
}

char is_directory_block_empty(char *block) {
    struct BlockDirectoryRecord record;
    unsigned int i;
    for (i = 0; i < RECORDS_PER_BLOCK; ++i) {
        memcpy(&record, block + i * RECORD_SIZE, RECORD_SIZE);
        if (strlen(record.name) != 0) {
            return 0;
        }
    }
    return 1;
}

char create_file_in_dir(FILE *fs, inode_pointer_t inode_p, int file_type, const char *name, inode_pointer_t *inode_p_holder) {
    block_pointer_t block_p, block_new_p;
    char block[FS_BLOCK_SIZE];
    inode_pointer_t inode_new_p;
    struct INode inode;
    struct INode inode_new;
    struct BlockDirectoryRecord record;
    unsigned int i;

    // Read the inode.
    get_inode(fs, inode_p, &inode);

    // Sanity check: inode represents a directory.
    if (inode.file_type != TYPE_DIRECTORY) {
        return 1;
    }

    // Types other than regular file and directory are not supported yet.
    if ((file_type != TYPE_DIRECTORY) && (file_type != TYPE_REGULAR)) {
        return 2;
    }

    // Access the last block.
    if (get_block_k(fs, &inode, inode.file_size - 1, &block_p)) {
        return 3;
    }

    // If the block is full, then we need a new one.
    get_block(fs, block_p, block);
    if (is_directory_block_full(block)) {
        if (inode_block_append(fs, &inode, &block_p)) {
            return 4;
        }
        update_inode(fs, inode_p, &inode);
    }

    // Initialize inode for a new file.
    if (occupy_inode(fs, &inode_new_p)) {
        return 5;
    }
    get_inode(fs, inode_new_p, &inode_new);
    if (file_type == TYPE_DIRECTORY) {
        // It's a directory.
        inode_new.file_type = TYPE_DIRECTORY;
        if (inode_block_append(fs, &inode_new, &block_new_p)) {
            return 6;
        }
        get_block(fs, block_new_p, block);
        directory_block_init(block, &inode_new_p, &inode_p);
        update_block(fs, block_new_p, block);
    } else {
        // It's a regular file.
        inode_new.file_type = TYPE_REGULAR;
    }
    update_inode(fs, inode_new_p, &inode_new);

    // Attach the inode (inode_new_p) to the block (block_p).
    get_block(fs, block_p, block);
    for (i = 0; i < RECORDS_PER_BLOCK; ++i) {
        memcpy(&record, block + i * sizeof(record), sizeof(record));
        if (strlen(record.name) == 0) {
            record.inode_p = inode_new_p;
            memcpy(record.name, name, MAX_NAME_LENGTH);
            memcpy(block + i * sizeof(record), &record, sizeof(record));
            update_block(fs, block_p, block);
            if (inode_p_holder != NULL) *inode_p_holder = inode_new_p;
            return 0;
        }
    }

    return 7;
}

char remove_file(FILE *fs, inode_pointer_t inode_p) {
    struct INode inode;
    char err;
    unsigned int i, k, i_edge;
    block_pointer_t block_p;
    struct BlockDirectoryRecord record;
    char block[FS_BLOCK_SIZE];

    get_inode(fs, inode_p, &inode);

    // Apply removing to all subfiles.
    if (inode.file_type == TYPE_DIRECTORY) {
        for (k = 0; k < inode.file_size; ++k) {
            if (get_block_k(fs, &inode, k, &block_p)) {
                return 1;
            }
            get_block(fs, block_p, block);
            if (k == 0) {
                i_edge = 2;
            } else {
                i_edge = 0;
            }
            for (i = i_edge; i < RECORDS_PER_BLOCK; ++i) {
                memcpy(&record, block + i * sizeof(record), sizeof(record));
                if (strlen(record.name) > 0) {
                    if (remove_file(fs, record.inode_p)) {
                        return 2;
                    }
                }
            }
        }
    }

    // Pop all blocks one by one.
    while (1) {
        err = inode_block_pop(fs, &inode);
        if (err == INODE_BLOCK_POP_SUCCESS) {
            continue;
        } else if (err == INODE_BLOCK_POP_NOTHING) {
            break;
        } else {
            return 3;
        }
    }

    free_inode(fs, inode_p);
}

char remove_file_from_dir(FILE *fs, inode_pointer_t inode_dir_p, inode_pointer_t inode_victim_p) {
    // First, we extract the last record.
    // If it turns out to be the victim, then do nothing.
    // Otherwise, we search for the victim's record and replace it with extracted one.
    block_pointer_t block_p;
    char block[FS_BLOCK_SIZE];
    struct INode inode_dir;
    struct BlockDirectoryRecord record_last, record_victim;
    struct BlockDirectoryRecord record_null = {0, {'\0'}};
    int i, i_edge;
    unsigned int k;

    get_inode(fs, inode_dir_p, &inode_dir);

    // Sanity check: this is directory's inode.
    if (inode_dir.file_type != TYPE_DIRECTORY) {
        return 1;
    }

    // Extract the last record.
    if (get_block_k(fs, &inode_dir, inode_dir.file_size - 1, &block_p)) {
        return 2;
    }
    get_block(fs, block_p, block);
    if (inode_dir.file_size == 1) {
        i_edge = 2;
    } else {
        i_edge = 0;
    }
    for (i = RECORDS_PER_BLOCK - 1; i >= i_edge; --i) {
        memcpy(&record_last, block + i * sizeof(record_last), sizeof(record_last));
        if (strlen(record_last.name) > 0) {
            memcpy(block + i * sizeof(record_last), &record_null, sizeof(record_last));
            break;
        }
    }
    if (i == (i_edge - 1)) {
        return 3;
    }
    if (is_directory_block_empty(block)) {
        inode_dir.file_size -= 1;
        free_block(fs, block_p);
        update_inode(fs, inode_dir_p, &inode_dir);
    } else {
        update_block(fs, block_p, block);
    }

    // Is the last record a victim?
    if (record_last.inode_p == inode_victim_p) {
        if (remove_file(fs, inode_victim_p)) {
            return 4;
        }
        return 0;
    }

    // Search for the victim's record and overwrite it with extracted one.
    // Also remove file itself.
    for (k = 0; k < inode_dir.file_size; ++k) {
        if (get_block_k(fs, &inode_dir, k, &block_p)) {
            return 5;
        }
        get_block(fs, block_p, block);
        if (k == 0) {
            i_edge = 2;
        } else {
            i_edge = 0;
        }
        for (i = i_edge; i < RECORDS_PER_BLOCK; ++i) {
            memcpy(&record_victim, block + i * sizeof(record_victim), sizeof(record_victim));
            if (record_victim.inode_p == inode_victim_p) {
                memcpy(block + i * sizeof(record_victim), &record_last, sizeof(record_last));
                update_block(fs, block_p, block);
                if (remove_file(fs, inode_victim_p)) {
                    return 6;
                }
                return 0;
            }
        }
    }

    return 7;
}

block_pointer_t get_size_on_disk(struct INode *inode) {
    block_pointer_t sz = inode->file_size;
    size_t k = sz - 1;  // index of the last block with file data
    size_t p;
    int level = 0;

    if (sz == 0) return 0;

    // Determine the level of indirect addressing.
    if (k >= (INODE_BLOCKS_COUNT - 3)) {
        k -= (INODE_BLOCKS_COUNT - 3);
        if (k < BLOCKS_P_PER_BLOCK) {
            level = 1;
        } else {
            k -= BLOCKS_P_PER_BLOCK;
            if (k < int_pow(BLOCKS_P_PER_BLOCK, 2)) {
                level = 2;
            } else {
                k -= int_pow(BLOCKS_P_PER_BLOCK, 2);
                if (k < int_pow(BLOCKS_P_PER_BLOCK, 3)) {
                    level = 3;
                }
            }
        }
        sz += 1;
    }

    while (level > 1) {
        p = k / int_pow(BLOCKS_P_PER_BLOCK, level - 1);
        k = k % int_pow(BLOCKS_P_PER_BLOCK, level - 1);
        sz += p * int_pow(BLOCKS_P_PER_BLOCK, level - 2);
        sz += 1;
        --level;
    }

    return sz;
}

size_t get_regular_file_size(FILE *fs, struct INode *inode) {
    block_pointer_t block_p;
    char block[FS_BLOCK_SIZE];
    size_t eof_pos;
    size_t sz = 0;
    if (inode->file_type != TYPE_REGULAR) return FS_BLOCK_SIZE * get_size_on_disk(inode);
    if (inode->file_size == 0) return sz;
    sz += (inode->file_size - 1) * FS_BLOCK_SIZE;
    if (get_block_k(fs, inode, inode->file_size - 1, &block_p)) return sz;
    get_block(fs, block_p, block);
    for (eof_pos = 0; (eof_pos < FS_BLOCK_SIZE) && block[eof_pos] != EOF; ++eof_pos) {}
    sz += eof_pos;
    return sz;
}

char get_dir(FILE *fs, inode_pointer_t inode_p, const char *target, inode_pointer_t *inode_target_p) {
    struct INode inode;
    struct INode inode2;
    inode_pointer_t inode2_p;

    // Get directory with provided name.
    get_inode(fs, inode_p, &inode);
    if (get_inode_by_name_in_inode(fs, &inode, target, &inode2_p)) return 1;

    // Be sure it's actually a directory.
    get_inode(fs, inode2_p, &inode2);
    if (inode2.file_type != TYPE_DIRECTORY) return 1;

    *inode_target_p = inode2_p;
    return 0;
}

int is_name_valid(const char *name) {
    // Save space for zero-byte.
    if (strlen(name) >= MAX_NAME_LENGTH) return 0;
    if (strchr(name, '/') != NULL) return 0;
    if (strcmp(name, ".") == 0) return 0;
    if (strcmp(name, "..") == 0) return 0;
    return 1;
}

int is_name_taken(FILE *fs, inode_pointer_t inode_p, const char *name) {
    struct INode inode;
    get_inode(fs, inode_p, &inode);
    if (get_inode_by_name_in_inode(fs, &inode, name, NULL)) return 0;
    return 1;
}
