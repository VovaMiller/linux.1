#include <fs_core.h>

void cmd_pwd(FILE *fs, inode_pointer_t inode_p, char *buffer, int endline);

void cmd_mkdir(FILE *fs, inode_pointer_t inode_p, const char *name, char *buffer);

void cmd_rmdir(FILE *fs, inode_pointer_t inode_p, const char *name, char *buffer);

inode_pointer_t cmd_cd(FILE *fs, inode_pointer_t inode_p, const char *target, char *buffer);

void cmd_touch(FILE *fs, inode_pointer_t inode_p, const char *name, char *buffer);

void cmd_rm(FILE *fs, inode_pointer_t inode_p, const char *name, char *buffer);

void cmd_ls(FILE *fs, inode_pointer_t inode_p, char *buffer);

void cmd_cat(FILE *fs, inode_pointer_t inode_p, const char *name, char *buffer);

void cmd_download(FILE *fs, inode_pointer_t inode_p, const char *name_fs, const char *name_local, char *buffer);

void cmd_upload(FILE *fs, inode_pointer_t inode_p, const char *name_local, const char *name_fs, char *buffer);

void cmd_help(char *buffer);

int get_cmd(FILE *fs, inode_pointer_t *inode_p, char *cmd, char *buffer);

