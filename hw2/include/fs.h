#include <fs_core.h>

void cmd_pwd(FILE *fs, inode_pointer_t inode_p);

void cmd_mkdir(FILE *fs, inode_pointer_t inode_p, const char *name);

void cmd_rmdir(FILE *fs, inode_pointer_t inode_p, const char *name);

inode_pointer_t cmd_cd(FILE *fs, inode_pointer_t inode_p, const char *target);

void cmd_touch(FILE *fs, inode_pointer_t inode_p, const char *name);

void cmd_rm(FILE *fs, inode_pointer_t inode_p, const char *name);

void cmd_ls(FILE *fs, inode_pointer_t inode_p);

void cmd_cat(FILE *fs, inode_pointer_t inode_p, const char *name);

void cmd_download(FILE *fs, inode_pointer_t inode_p, const char *name_fs, const char *name_local);

void cmd_upload(FILE *fs, inode_pointer_t inode_p, const char *name_local, const char *name_fs);

void cmd_help();

int get_cmd(FILE *fs, inode_pointer_t *inode_p);
