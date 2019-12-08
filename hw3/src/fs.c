#include <fs.h>

void cmd_pwd(FILE *fs, inode_pointer_t inode_p, char *buffer, int endline) {
    char path[BUFFER_SIZE];
    if (get_full_path(fs, inode_p, path) != 0) {
        sprintf(buffer, "[Error] pwd\n");
        return;
    }
    if (endline) {
        sprintf(buffer + strlen(buffer), "%s\n", path);
    } else {
        sprintf(buffer + strlen(buffer), "%s", path);
    }
}

void cmd_mkdir(FILE *fs, inode_pointer_t inode_p, const char *name, char *buffer) {
    char err;

    // Check name.
    if (!is_name_valid(name)) {
        sprintf(buffer, "name \"%s\" is invalid\n", name);
        return;
    }

    // Check for duplicates.
    if (is_name_taken(fs, inode_p, name)) {
        sprintf(buffer, "name \"%s\" is already taken\n", name);
        return;
    }

    if (err = create_file_in_dir(fs, inode_p, TYPE_DIRECTORY, name, NULL)) {
        sprintf(buffer, "[Error] mkdir (%d)\n", err);
        return;
    }
}

void cmd_rmdir(FILE *fs, inode_pointer_t inode_p, const char *name, char *buffer) {
    struct INode inode;
    struct INode inode_victim;
    inode_pointer_t inode_victim_p;
    char err;

    // Check name.
    if (!is_name_valid(name)) {
        sprintf(buffer, "name \"%s\" is invalid\n", name);
        return;
    }

    // Get file with provided name.
    get_inode(fs, inode_p, &inode);
    if (get_inode_by_name_in_inode(fs, &inode, name, &inode_victim_p)) {
        sprintf(buffer, "directory \"%s\" doesn't exist\n", name);
        return;
    }

    // Be sure it's a directory.
    get_inode(fs, inode_victim_p, &inode_victim);
    if (inode_victim.file_type != TYPE_DIRECTORY) {
        sprintf(buffer, "\"%s\" is not a directory\n", name);
        return;
    }

    if (err = remove_file_from_dir(fs, inode_p, inode_victim_p)) {
        sprintf(buffer, "[Error] rmdir (%d)\n", err);
        return;
    }
}

inode_pointer_t cmd_cd(FILE *fs, inode_pointer_t inode_p, const char *target, char *buffer) {
    inode_pointer_t inode_cur_p;
    size_t begin, end;
    char name[MAX_NAME_LENGTH];
    char zero = '\0';

    if (strlen(target) == 0) return inode_p;

    if (target[0] == '/') {
        inode_cur_p = 0;
        begin = 1;
    } else {
        inode_cur_p = inode_p;
        begin = 0;
    }

    while (begin < strlen(target)) {
        for (end = begin + 1; target[end] != '/' && target[end] != '\0'; ++end) {}
        if ((end - begin) >= MAX_NAME_LENGTH) {
            sprintf(buffer, "cd: invalid path\n");
            return inode_p;
        }
        memcpy(name, target + begin, end - begin);
        memcpy(name + (end - begin), &zero, sizeof(zero));
        if (get_dir(fs, inode_cur_p, name, &inode_cur_p)) {
            sprintf(buffer, "cd: invalid path\n");
            return inode_p;
        }
        begin = end + 1;
    }

    return inode_cur_p;
}

void cmd_touch(FILE *fs, inode_pointer_t inode_p, const char *name, char *buffer) {
    char err;

    // Check name.
    if (!is_name_valid(name)) {
        sprintf(buffer, "name \"%s\" is invalid\n", name);
        return;
    }

    // Check for duplicates.
    if (is_name_taken(fs, inode_p, name)) {
        sprintf(buffer, "name \"%s\" is already taken\n", name);
        return;
    }

    if (err = create_file_in_dir(fs, inode_p, TYPE_REGULAR, name, NULL)) {
        sprintf(buffer, "[Error] touch (%d)\n", err);
        return;
    }
}

void cmd_rm(FILE *fs, inode_pointer_t inode_p, const char *name, char *buffer) {
    struct INode inode;
    struct INode inode_victim;
    inode_pointer_t inode_victim_p;
    char err;

    // Check name.
    if (!is_name_valid(name)) {
        sprintf(buffer, "name \"%s\" is invalid\n", name);
        return;
    }

    // Get file with provided name.
    get_inode(fs, inode_p, &inode);
    if (get_inode_by_name_in_inode(fs, &inode, name, &inode_victim_p)) {
        sprintf(buffer, "file \"%s\" doesn't exist\n", name);
        return;
    }

    // Be sure it's a directory.
    get_inode(fs, inode_victim_p, &inode_victim);
    if (inode_victim.file_type != TYPE_REGULAR) {
        sprintf(buffer, "\"%s\" is not a regular file\n", name);
        return;
    }

    if (err = remove_file_from_dir(fs, inode_p, inode_victim_p)) {
        sprintf(buffer, "[Error] rm (%d)\n", err);
        return;
    }
}

void cmd_ls(FILE *fs, inode_pointer_t inode_p, char *buffer) {
    char err;
    struct INode inode, inode2;
    unsigned int k;
    int i, i_start;
    block_pointer_t block_p;
    char block[FS_BLOCK_SIZE];
    struct BlockDirectoryRecord record;
    unsigned long long sz;
    char file_type;

    get_inode(fs, inode_p, &inode);

    for (k = 0; k < inode.file_size; ++k) {
        if (err = get_block_k(fs, &inode, k, &block_p)) {
            sprintf(buffer, "[Error] ls, get_block_k (%d)\n", err);
            return;
        }
        get_block(fs, block_p, block);
        if (k == 0) {
            i_start = 2;
        } else {
            i_start = 0;
        }
        for (i = i_start; i < RECORDS_PER_BLOCK; ++i) {
            memcpy(&record, block + i * sizeof(record), sizeof(record));
            if (strlen(record.name) == 0) {
                return;
            }
            get_inode(fs, record.inode_p, &inode2);
            switch (inode2.file_type) {
                case TYPE_REGULAR:
                    file_type = 'F';
                    break;
                case TYPE_DIRECTORY:
                    file_type = 'D';
                    break;
                default:
                    file_type = '?';
            }
            if (inode2.file_type == TYPE_REGULAR) {
                sz = get_regular_file_size(fs, &inode2);
            } else {
                sz = get_size_on_disk(&inode2) * FS_BLOCK_SIZE;
            }
            sprintf(buffer + strlen(buffer), "%c %-13llu %s\n", file_type, sz, record.name);
        }
    }
}

void cmd_cat(FILE *fs, inode_pointer_t inode_p, const char *name, char *buffer) {
    char err;
    struct INode inode;
    struct INode inode_file;
    inode_pointer_t inode_file_p;
    block_pointer_t block_p;
    char block[FS_BLOCK_SIZE];
    size_t k, i;
    long eof_pos;

    // Check name.
    if (!is_name_valid(name)) {
        sprintf(buffer, "name \"%s\" is invalid\n", name);
        return;
    }

    get_inode(fs, inode_p, &inode);

    // Find file.
    if (get_inode_by_name_in_inode(fs, &inode, name, &inode_file_p)) {
        sprintf(buffer, "file \"%s\" doesn't exist\n", name);
        return;
    }

    get_inode(fs, inode_file_p, &inode_file);

    // Can't be directory.
    if (inode_file.file_type == TYPE_DIRECTORY) {
        sprintf(buffer, "\"%s\" is a directory\n", name);
        return;
    }

    // Do nothing with empty file.
    if (inode_file.file_size == 0) return;

    // Show full blocks one by one, except the last one.
    for (k = 0; k < inode_file.file_size - 1; ++k) {
        if (get_block_k(fs, &inode_file, k, &block_p)) {
            sprintf(buffer, "[Error] cat, get_block_k (%d)\n", err);
            return;
        }
        get_block(fs, block_p, block);
        for (i = 0; i < FS_BLOCK_SIZE; ++i) sprintf(buffer + strlen(buffer), "%c", block[i]);
    }

    // Show the last block properly (considering EOF).
    if (get_block_k(fs, &inode_file, inode_file.file_size - 1, &block_p)) {
        sprintf(buffer, "[Error] cat, get_block_k (%d)\n", err);
        return;
    }
    get_block(fs, block_p, block);
    for (eof_pos = FS_BLOCK_SIZE - 1; (eof_pos >= 0) && block[eof_pos] != EOF; --eof_pos) {}
    if (eof_pos < 0) {
        sprintf(buffer, "[Error] cat: no EOF in the last block (%d)\n", err);
        return;
    }
    for (i = 0; i < eof_pos; ++i) sprintf(buffer + strlen(buffer), "%c", block[i]);
}

void cmd_download(FILE *fs, inode_pointer_t inode_p, const char *name_fs, const char *name_local, char *buffer) {
    char err;
    struct INode inode;
    struct INode inode_file;
    inode_pointer_t inode_file_p;
    block_pointer_t block_p;
    char block[FS_BLOCK_SIZE];
    size_t k;
    FILE *file;
    long eof_pos;

    // Check name.
    if (!is_name_valid(name_fs)) {
        sprintf(buffer, "name \"%s\" is invalid\n", name_fs);
        return;
    }

    get_inode(fs, inode_p, &inode);

    // Find file.
    if (get_inode_by_name_in_inode(fs, &inode, name_fs, &inode_file_p)) {
        sprintf(buffer, "file \"%s\" doesn't exist\n", name_fs);
        return;
    }

    get_inode(fs, inode_file_p, &inode_file);

    // Can't be directory.
    if (inode_file.file_type == TYPE_DIRECTORY) {
        sprintf(buffer, "\"%s\" is a directory\n", name_fs);
        return;
    }

    // Open local file.
    file = fopen(name_local, "w");
    if (file == NULL) {
        sprintf(buffer, "Can't access local file \"%s\".\n", name_local);
        return;
    }

    // Do nothing with empty file.
    if (inode_file.file_size == 0) {
        fclose(file);
        return;
    }

    // Write full blocks to file one by one, except the last one.
    for (k = 0; k < inode_file.file_size - 1; ++k) {
        if (get_block_k(fs, &inode_file, k, &block_p)) {
            sprintf(buffer, "[Error] download, get_block_k (%d)\n", err);
            fclose(file);
            return;
        }
        get_block(fs, block_p, block);
        fwrite(block, FS_BLOCK_SIZE, 1, file);
    }

    // Write the last block to file properly (considering EOF).
    if (get_block_k(fs, &inode_file, inode_file.file_size - 1, &block_p)) {
        sprintf(buffer, "[Error] download, get_block_k (%d)\n", err);
        fclose(file);
        return;
    }
    get_block(fs, block_p, block);
    for (eof_pos = FS_BLOCK_SIZE - 1; (eof_pos >= 0) && block[eof_pos] != EOF; --eof_pos) {}
    if (eof_pos < 0) {
        sprintf(buffer, "[Error] download: no EOF in the last block (%d)\n", err);
        fclose(file);
        return;
    }
    fwrite(block, eof_pos, 1, file);

    fclose(file);
}

void cmd_upload(FILE *fs, inode_pointer_t inode_p, const char *name_local, const char *name_fs, char *buffer) {
    char err;
    struct INode inode_file;
    inode_pointer_t inode_file_p;
    block_pointer_t block_p;
    char block[FS_BLOCK_SIZE];
    size_t k;
    FILE *file;
    long sz, pos;

    // Open local file.
    file = fopen(name_local, "r");
    if (file == NULL) {
        sprintf(buffer, "Can't access local file \"%s\".\n", name_local);
        return;
    }

    // Check name.
    if (!is_name_valid(name_fs)) {
        sprintf(buffer, "name \"%s\" is invalid\n", name_fs);
        fclose(file);
        return;
    }
    if (is_name_taken(fs, inode_p, name_fs)) {
        sprintf(buffer, "name \"%s\" is already taken\n", name_fs);
        fclose(file);
        return;
    }

    // Create file in our FS.
    if (err = create_file_in_dir(fs, inode_p, TYPE_REGULAR, name_fs, &inode_file_p)) {
        sprintf(buffer, "[Error] upload, create_file_in_dir (%d)\n", err);
        fclose(file);
        return;
    }

    // Copy content from local file to file in FS.
    get_inode(fs, inode_file_p, &inode_file);
    fseek(file, 0L, SEEK_END);
    sz = ftell(file);
    fseek(file, 0L, SEEK_SET);
    for (pos = 0; (pos + FS_BLOCK_SIZE) <= sz; pos += FS_BLOCK_SIZE) {
        // Reading full blocks.
        if (err = inode_block_append(fs, &inode_file, &block_p)) {
            sprintf(buffer, "[Error] upload, inode_block_append (%d)\n", err);
            fclose(file);
            return;
        }
        fread(block, FS_BLOCK_SIZE, 1, file);
        update_block(fs, block_p, block);
    }
    // Final block.
    if (err = inode_block_append(fs, &inode_file, &block_p)) {
        sprintf(buffer, "[Error] upload, inode_block_append (%d)\n", err);
        fclose(file);
        return;
    }
    for (k = 0; k < FS_BLOCK_SIZE; ++k) block[k] = '\0';
    block[sz - pos] = EOF;
    if (pos < sz) fread(block, sz - pos, 1, file);
    update_block(fs, block_p, block);
    update_inode(fs, inode_file_p, &inode_file);

    fclose(file);
}

void cmd_help(char *buffer) {
    sprintf(buffer + strlen(buffer), "%-30s%s\n", "pwd", "-- показать текущую директорию");
    sprintf(buffer + strlen(buffer), "%-30s%s\n", "ls", "-- аналог ls -l");
    sprintf(buffer + strlen(buffer), "%-30s%s\n", "mkdir DIRECTORY", "-- создать каталог");
    sprintf(buffer + strlen(buffer), "%-30s%s\n", "rmdir DIRECTORY", "-- удаляет каталог и его содержимое");
    sprintf(buffer + strlen(buffer), "%-30s%s\n", "cd DIRECTORY", "-- изменить текущую директорию");
    sprintf(buffer + strlen(buffer), "%-30s%s\n", "touch FILE", "-- создать пустой файл");
    sprintf(buffer + strlen(buffer), "%-30s%s\n", "rm FILE", "-- удалить файл");
    sprintf(buffer + strlen(buffer), "%-30s%s\n", "cat FILE", "-- вывести содержимое файла");
    sprintf(buffer + strlen(buffer), "%-30s%s\n", "upload FILE_LOCAL FILE_FS", "-- загрузка локального файла с абсолютным путём FILE_LOCAL в ФС");
    sprintf(buffer + strlen(buffer), "%-30s%s\n", "download FILE_FS FILE_LOCAL", "-- выгрузка файла FILE_FS в локальный файл по абсолютному пути FILE_LOCAL");
    sprintf(buffer + strlen(buffer), "%-30s%s\n", "unmount", "-- завершение работы сервера виртуальной ФС");
    sprintf(buffer + strlen(buffer), "%-30s%s\n", "help", "-- вывести список доступных комманд");
}

int get_cmd(FILE *fs, inode_pointer_t *inode_p, char *cmd, char *buffer) {
    char unit[BUFFER_SIZE];
    char unit2[BUFFER_SIZE];
    unsigned int i, units_count, *units_begins, *units_lens;
    int on_space, return_code;
    char zero = '\0';

    buffer[0] = zero;

    // Count units.
    on_space = 1;
    units_count = 0;
    for (i = 0; i < BUFFER_SIZE; ++i) {
        if (cmd[i] == '\0') {
            if (!on_space) ++units_count;
            break;
        }
        switch (cmd[i]) {
            case ' ':
                if (on_space) continue;
                on_space = 1;
                ++units_count;
                break;
            default:
                on_space = 0;
        }
    }
    if (i == BUFFER_SIZE) {
        sprintf(buffer, "[Error] Command line is too long.\n");
        return 0;
    }

    // Remember all units.
    units_begins = malloc(units_count * sizeof(unsigned int));
    units_lens = malloc(units_count * sizeof(unsigned int));
    on_space = 1;
    units_count = 0;
    for (i = 0; i < BUFFER_SIZE; ++i) {
        if (cmd[i] == '\0') {
            if (!on_space) {
                units_lens[units_count] = i - units_begins[units_count];
                ++units_count;
            }
            break;
        }
        switch (cmd[i]) {
            case ' ':
                if (on_space) continue;
                on_space = 1;
                units_lens[units_count] = i - units_begins[units_count];
                ++units_count;
                break;
            default:
                if (on_space) units_begins[units_count] = i;
                on_space = 0;

        }
    }

    // Processing units.
    return_code = 0;
    if (units_count > 0) {
        memcpy(unit, cmd + units_begins[0], units_lens[0]);
        memcpy(unit + units_lens[0], &zero, sizeof(zero));

        if (strcmp(unit, "pwd") == 0) {
            if (units_count == 1) {
                cmd_pwd(fs, *inode_p, buffer, 1);
            } else {
                sprintf(buffer, "%s: invalid number of parameters (expected %d, got %d)\n", "pwd", 0, units_count - 1);
            }
        } else if (strcmp(unit, "ls") == 0) {
            if (units_count == 1) {
                cmd_ls(fs, *inode_p, buffer);
            } else {
                sprintf(buffer, "%s: invalid number of parameters (expected %d, got %d)\n", "ls", 0, units_count - 1);
            }
        } else if (strcmp(unit, "mkdir") == 0) {
            if (units_count == 2) {
                memcpy(unit, cmd + units_begins[1], units_lens[1]);
                memcpy(unit + units_lens[1], &zero, sizeof(zero));
                cmd_mkdir(fs, *inode_p, unit, buffer);
            } else {
                sprintf(buffer, "%s: invalid number of parameters (expected %d, got %d)\n", "mkdir", 1, units_count - 1);
            }
        } else if (strcmp(unit, "rmdir") == 0) {
            if (units_count == 2) {
                memcpy(unit, cmd + units_begins[1], units_lens[1]);
                memcpy(unit + units_lens[1], &zero, sizeof(zero));
                cmd_rmdir(fs, *inode_p, unit, buffer);
            } else {
                sprintf(buffer, "%s: invalid number of parameters (expected %d, got %d)\n", "rmdir", 1, units_count - 1);
            }
        } else if (strcmp(unit, "cd") == 0) {
            if (units_count == 2) {
                memcpy(unit, cmd + units_begins[1], units_lens[1]);
                memcpy(unit + units_lens[1], &zero, sizeof(zero));
                *inode_p = cmd_cd(fs, *inode_p, unit, buffer);
            } else {
                sprintf(buffer, "%s: invalid number of parameters (expected %d, got %d)\n", "cd", 1, units_count - 1);
            }
        } else if (strcmp(unit, "touch") == 0) {
            if (units_count == 2) {
                memcpy(unit, cmd + units_begins[1], units_lens[1]);
                memcpy(unit + units_lens[1], &zero, sizeof(zero));
                cmd_touch(fs, *inode_p, unit, buffer);
            } else {
                sprintf(buffer, "%s: invalid number of parameters (expected %d, got %d)\n", "touch", 1, units_count - 1);
            }
        } else if (strcmp(unit, "rm") == 0) {
            if (units_count == 2) {
                memcpy(unit, cmd + units_begins[1], units_lens[1]);
                memcpy(unit + units_lens[1], &zero, sizeof(zero));
                cmd_rm(fs, *inode_p, unit, buffer);
            } else {
                sprintf(buffer, "%s: invalid number of parameters (expected %d, got %d)\n", "rm", 1, units_count - 1);
            }
        } else if (strcmp(unit, "cat") == 0) {
            if (units_count == 2) {
                memcpy(unit, cmd + units_begins[1], units_lens[1]);
                memcpy(unit + units_lens[1], &zero, sizeof(zero));
                cmd_cat(fs, *inode_p, unit, buffer);
            } else {
                sprintf(buffer, "%s: invalid number of parameters (expected %d, got %d)\n", "cmd", 1, units_count - 1);
            }
        } else if (strcmp(unit, "download") == 0) {
            if (units_count == 3) {
                memcpy(unit, cmd + units_begins[1], units_lens[1]);
                memcpy(unit + units_lens[1], &zero, sizeof(zero));
                memcpy(unit2, cmd + units_begins[2], units_lens[2]);
                memcpy(unit2 + units_lens[2], &zero, sizeof(zero));
                cmd_download(fs, *inode_p, unit, unit2, buffer);
            } else {
                sprintf(buffer, "%s: invalid number of parameters (expected %d, got %d)\n", "download", 2, units_count - 1);
            }
        } else if (strcmp(unit, "upload") == 0) {
            if (units_count == 3) {
                memcpy(unit, cmd + units_begins[1], units_lens[1]);
                memcpy(unit + units_lens[1], &zero, sizeof(zero));
                memcpy(unit2, cmd + units_begins[2], units_lens[2]);
                memcpy(unit2 + units_lens[2], &zero, sizeof(zero));
                cmd_upload(fs, *inode_p, unit, unit2, buffer);
            } else {
                sprintf(buffer, "%s: invalid number of parameters (expected %d, got %d)\n", "upload", 2, units_count - 1);
            }
        } else if (strcmp(unit, "unmount") == 0) {
            return_code = 1;
        } else if (strcmp(unit, "help") == 0) {
            cmd_help(buffer);
        } else {
            sprintf(buffer, "Command \"%s\" doesn't exist.\n", unit);
        }
    }

    fflush(fs);
    free(units_begins);
    free(units_lens);
    return return_code;
}
