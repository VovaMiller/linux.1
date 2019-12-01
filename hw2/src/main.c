#include <fs.h>

// Arguments:
//  1) Filesystem file name

// pwd
// ls
// mkdir DIRECTORY
// rmdir DIRECTORY
// cd DIRECTORY
// touch FILE
// rm FILE
// cat FILE
// upload FILE_LOCAL FILE_FS
// download FILE_FS FILE_LOCAL
// unmount
// help

// SuperBlock
// Blocks Bitmap Area
// inodes Bitmap Area
// inodes table
// blocks

// # SuperBlock Size = 8 Bytes
//      4 Bytes (unsigned int) - Magic Number
//      4 Bytes (unsigned int) - Block Size
// # Block Size = 1 KB
// # Block Pointer = 4 Bytes (unsigned int (2^32))
// # inode Size = 64 Bytes:
//      4*1  Bytes - file type
//      4*1  Bytes - file size
//      4*11 Bytes - blocks pointers
//      4*3  Bytes - indirect addressing
// # inode Pointer = 2 Bytes (unsigned short (2^16))

// Blocks Bitmap Area Size = 2^32 Bits = 512 MB
// inodes Bitmap Area Size = 2^16 Bits = 8 KB
// inode Area Size = 2^16 * 64 Bytes = 4 MB
// max FS size = 2^32 KB = 4096 GB
// max Blocks/File = 12+(1+256)+(1+256+256^2)+(1+256+256^2+256^3) = 16 909 071
// max File Size = 16 843 020 Blocks = 16 843 020 KB ~ 16 GB

int main(int argc, char *argv[]) {
    inode_pointer_t inode_cur_dir = 0;

    // Checking arguments.
    if (argc != 2) {
        fprintf(stderr, "Expected single argument, got %d.\n", argc - 1);
        return 1;
    }

    // Opening/Generating filesystem file.
    FILE *file;
    if (access(argv[1], F_OK) != -1) {
        // Filesystem file already exists.
        printf("Loading filesystem...");
        fflush(stdout);
        file = open_fs_file(argv[1]);
        printf(" OK!\n");
    } else {
        // Filesystem file doesn't exist, create a new one.
        printf("Generating a new filesystem...");
        fflush(stdout);
        file = generate_fs_file(argv[1]);
        printf(" OK!\n");
        cmd_help();
    }

    // Listening for commands.
    while (!get_cmd(file, &inode_cur_dir)) {}

    fclose(file);
    return 0;
}
