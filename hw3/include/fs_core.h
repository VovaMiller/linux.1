#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utils.h>
#include <bitmap.h>

typedef unsigned int block_pointer_t;
typedef unsigned short inode_pointer_t;

struct SuperBlock {
    unsigned int magic_number;
    unsigned int block_size;
};

#define INODE_BLOCKS_COUNT 14

struct INode {
    int file_type;
    unsigned int file_size;  // number of blocks with file data
    block_pointer_t block_p[INODE_BLOCKS_COUNT];
};

#define MAX_NAME_LENGTH 14

struct BlockDirectoryRecord {
    inode_pointer_t inode_p;
    char name[MAX_NAME_LENGTH];
};

#define FS_MAGIC_NUMBER 0x53EF53EF

#define FS_BLOCK_SIZE 1024
#define INODE_SIZE 64

#define TYPE_NONE      -1
#define TYPE_DIRECTORY  0
#define TYPE_REGULAR    1

#define AREA_SIZE_SUPERBLOCK    (sizeof(struct SuperBlock))
#define AREA_SIZE_BITMAP_BLOCKS (1 << (8 * sizeof(block_pointer_t) - 3))
#define AREA_SIZE_BITMAP_INODES (1 << (8 * sizeof(inode_pointer_t) - 3))
#define AREA_SIZE_INODES        (sizeof(struct INode) * (1 << (8 * sizeof(inode_pointer_t))))

#define AREA_POS_SUPERBLOCK     0
#define AREA_POS_BITMAP_BLOCKS  (AREA_POS_SUPERBLOCK + AREA_SIZE_SUPERBLOCK)
#define AREA_POS_BITMAP_INODES  (AREA_POS_BITMAP_BLOCKS + AREA_SIZE_BITMAP_BLOCKS)
#define AREA_POS_INODES         (AREA_POS_BITMAP_INODES + AREA_SIZE_BITMAP_INODES)
#define AREA_POS_BLOCKS         (AREA_POS_INODES + AREA_SIZE_INODES)

#define RECORD_SIZE         (sizeof(struct BlockDirectoryRecord))
#define BLOCKS_P_PER_BLOCK  (FS_BLOCK_SIZE / sizeof(block_pointer_t))
#define RECORDS_PER_BLOCK   (FS_BLOCK_SIZE / sizeof(struct BlockDirectoryRecord))

#define PAGE_SIZE_BITMAP_BLOCKS (1 << 13)
#define PAGES_COUNT (AREA_SIZE_BITMAP_BLOCKS / PAGE_SIZE_BITMAP_BLOCKS)

#define INODE_BLOCK_POP_SUCCESS  0
#define INODE_BLOCK_POP_NOTHING  1
#define INODE_BLOCK_POP_OVERSIZE 2


/*
 * Function: directory_block_init
 * --------------------
 * Writes all possible records with zero length name to provided pointer.
 * Also adds to special records when both provided:
 *  - one for current directory;
 *  - one for parent directory.
 * Required to initialize any directory block.
 *
 * bytes:           pointer to the beginning of the block
 * inode_current:   inode number of current directory
 * inode_parent:    inode number of parent directory
 */
void directory_block_init(char *bytes, inode_pointer_t *inode_current, inode_pointer_t *inode_parent);

/*
 * Function: open_fs_file
 * --------------------
 * Opens existing FS file by its name.
 */
FILE* open_fs_file(const char *fname);

/*
 * Function: generate_fs_file
 * --------------------
 * Creates FS file with specified name and constructs the FS architecture.
 */
FILE* generate_fs_file(const char *fname);

/*
 * Function: get_block_k
 * --------------------
 * Gets k-th block's number of file.
 *
 * fs:              filesystem file
 * inode:           inode of the file
 * k:               index number of a block to read within the file.
 * block_p_holder:  holder for output - block number
 *
 *  returns: 0 <=> block number was obtained successfully.
 */
char get_block_k(FILE* fs, struct INode *inode, block_pointer_t k, block_pointer_t* block_p_holder);

/*
 * Function: is_block_allocated
 * --------------------
 * Finds out whether space for a block with provided number
 * has already been allocated on a FS file.
 *
 * fs:          filesystem file
 * block_p:     block number
 *
 *  returns: 1 <=> space for the block is already allocated.
 */
char is_block_allocated(FILE *fs, block_pointer_t block_p);

/*
 * Function: occupy_block
 * --------------------
 * Finds the first free block and occupies it, allocating space on FS file
 * if necessary. Also initializes the whole block to zeros.
 *
 * fs:              filesystem file
 * block_p_holder:  holder for output - occupied block number
 *
 *  returns: 0 <=> some block has been occupied;
 *           otherwise, there are no free blocks left.
 */
char occupy_block(FILE *fs, block_pointer_t *block_p_holder);

/*
 * Function: free_block
 * --------------------
 * Marks specified block as free in blocks bitmap.
 *
 * fs:      filesystem file
 * block_p: the number of block to free
 */
void free_block(FILE *fs, block_pointer_t block_p);

/*
 * Function: inode_block_append
 * --------------------
 * Appends new block with zeros to inode.
 * Doesn't update the inode in FS file.
 *
 * fs:              FS file
 * inode:           inode with blocks
 * block_p_holder:  holder for a new block number (optional)
 *
 *  returns: 0 <=> a new block was appended successfully.
 */
char inode_block_append(FILE *fs, struct INode *inode, block_pointer_t *block_p_holder);

/*
 * Function: inode_block_pop
 * --------------------
 * Dettaches and frees the last block in inode.
 * Doesn't update the inode in FS file.
 *
 * fs:              FS file
 * inode:           inode with blocks
 *
 *  returns: 0 <=> the last block was popped successfully.
 */
char inode_block_pop(FILE *fs, struct INode *inode);

/*
 * Function: get_block
 * --------------------
 * Gets block content by its number.
 *
 * fs:              filesystem file
 * block_p:         the number of block to obtain
 * block_holder:    holder for output - block content
 */
void get_block(FILE* fs, block_pointer_t block_p, char* block_holder);

/*
 * Function: update_block
 * --------------------
 * Updates block content by on FS file.
 *
 * fs:          FS file
 * block_p:     the number of block to update
 * block:       block content
 */
void update_block(FILE* fs, block_pointer_t block_p, char* block);

/*
 * Function: get_inode_by_name_in_block
 * --------------------
 * Gets inode number of a file with specified name in provided block of directory.
 *
 * fs:              filesystem file
 * block_p:         block number of directory to search in
 * name:            name of file/directory to search
 * inode_p_holder:  holder for output - inode number of file/directory (optional)
 *
 *  returns: 0 <=> inode number was determined successfully;
 *           1 <=> record with provided name doesn't exist.
 */
char get_inode_by_name_in_block(FILE* fs, block_pointer_t block_p, const char* name, inode_pointer_t* inode_p_holder);

/*
 * Function: get_inode_by_name_in_inode
 * --------------------
 * Gets inode number of a file with specified name in provided directory inode.
 *
 * fs:              filesystem file
 * inode:           directory inode to search in
 * name:            name of file/directory to search
 * inode_p_holder:  holder for output - inode number of file/directory (optional)
 *
 *  returns: 0 <=> inode number was determined successfully;
 *           1 <=> record with provided name doesn't exist.
 */
char get_inode_by_name_in_inode(FILE* fs, struct INode *inode, const char* name, inode_pointer_t* inode_p_holder);

/*
 * Function: get_name_by_inode_in_block
 * --------------------
 * Gets file/directory name by its inode number in provided block of directory.
 *
 * fs:          filesystem file
 * block_p:     block number of directory to search in
 * inode_p:     inode number of the file/directory to search
 * name_holder: holder for output - name of the file/directory
 *
 *  returns: 0 <=> name was determined successfully;
 *           1 <=> record with provided inode number doesn't exist.
 */
char get_name_by_inode_in_block(FILE* fs, block_pointer_t block_p, inode_pointer_t inode_p, char* name_holder);

/*
 * Function: get_name_by_inode_in_inode
 * --------------------
 * Gets file/directory name by its inode number in provided directory inode.
 *
 * fs:          filesystem file
 * inode:       directory inode to search in
 * inode_p:     inode number of the file/directory to search
 * name_holder: holder for output - name of the file/directory
 *
 *  returns: 0 <=> name was determined successfully;
 *           1 <=> record with provided inode number doesn't exist.
 */
char get_name_by_inode_in_inode(FILE* fs, struct INode *inode, inode_pointer_t inode_p, char* name_holder);

/*
 * Function: get_parent_directory
 * --------------------
 * fs:              filesystem file
 * inode_p:         inode number of the directory
 * inode_p_parent:  holder for output - inode number of parent directory
 *
 *  returns: 0 <=> parent was found successfully.
 */
char get_parent_directory(FILE* fs, inode_pointer_t inode_p, inode_pointer_t* inode_p_parent);

/*
 * Function: get_directory_name
 * --------------------
 * Determines the name of a directory by its inode number.
 *
 * fs:          filesystem file
 * inode_p:     inode number of the directory
 * name_holder: holder for output name
 *
 *  returns: 0 <=> name was determined successfully.
 */
char get_directory_name(FILE* fs, inode_pointer_t inode_p, char* name_holder);

/*
 * Function: get_full_path
 * --------------------
 * Gets full path of a directory.
 *
 * fs:      filesystem file
 * inode_p: inode number of the directory
 * holder:  holder for output
 *
 *  returns: 0 <=> path was obtained successfully;
 *           1 <=> error occurred.
 */
int get_full_path(FILE *fs, inode_pointer_t inode_p, char *holder);

/*
 * Function: occupy_inode
 * --------------------
 * Finds the first free inode and occupies it, initializing it with zeros.
 *
 * fs:              filesystem file
 * inode_p_holder:  holder for output - occupied inode number
 *
 *  returns: 0 <=> some inode has been occupied;
 *           otherwise, there are no free inodes left.
 */
char occupy_inode(FILE *fs, inode_pointer_t *inode_p_holder);

/*
 * Function: free_inode
 * --------------------
 * Marks specified inode as free in inodes bitmap.
 *
 * fs:      filesystem file
 * inode_p: the number of inode to free
 */
void free_inode(FILE *fs, inode_pointer_t inode_p);

/*
 * Function: get_inode
 * --------------------
 * Gets inode by its number.
 *
 * fs:              filesystem file
 * inode_p:         the number of inode to obtain
 * inode_holder:    holder for output - struct INode
 */
void get_inode(FILE* fs, inode_pointer_t inode_p, struct INode *inode_holder);

/*
 * Function: update_inode
 * --------------------
 * Updates inode's content on FS file.
 *
 * fs:      filesystem file
 * inode_p: inode number
 * inode:   struct INode
 */
void update_inode(FILE* fs, inode_pointer_t inode_p, struct INode *inode);

/*
 * Function: is_directory_block_full
 * --------------------
 * Checks if provided directory block has no zero records.
 *
 * block: the content of directory block
 *
 *  returns: 1 <=> directory block has no zero records;
 *           0 <=> directory block has at least one zero record.
 */
char is_directory_block_full(char *block);

/*
 * Function: is_directory_block_empty
 * --------------------
 * Checks if provided directory block has zero records only.
 *
 * block: the content of directory block
 *
 *  returns: 1 <=> directory block has only zero records;
 *           0 <=> directory block has at least one non-zero record.
 */
char is_directory_block_empty(char *block);

/*
 * Function: create_file_in_dir
 * --------------------
 * Creates file (regular file or directory) inside provided directory.
 * Doesn't check if a file with provided name already exists.
 * Doesn't check the name itself.
 *
 * fs:              FS file
 * inode_p:         inode number of the directory
 * file_type:       whether to create regular file or directory
 * name:            file name
 * inode_p_holder:  holder for inode number of created file (optional)
 *
 *  returns: 0 <=> file was created successfully.
 */
char create_file_in_dir(FILE *fs, inode_pointer_t inode_p, int file_type, const char *name, inode_pointer_t *inode_p_holder);

/*
 * Function: remove_file
 * --------------------
 * Removes file (regular file or directory) by its inode number.
 * Applies itself to all subfiles if input file is a directory.
 * Doesn't check if file exists in any of directories.
 *
 * fs:              FS file
 * inode_p:         inode number of the file to remove
 *
 *  returns: 0 <=> file was removed successfully.
 */
char remove_file(FILE *fs, inode_pointer_t inode_p);

/*
 * Function: remove_file_from_dir
 * --------------------
 * Removes file (regular file or directory) from provided directory.
 *
 * fs:              FS file
 * inode_dir_p:     inode number of the directory
 * inode_victim_p:  inode number of file to remove
 *
 *  returns: 0 <=> file was removed successfully.
 */
char remove_file_from_dir(FILE *fs, inode_pointer_t inode_dir_p, inode_pointer_t inode_victim_p);

/*
 * Function: get_size_on_disk
 * --------------------
 * Calculates how much actual disk space the file occupies.
 * This is different from inode.file_size, as the latter only shows a number
 *  of blocks with exactly file data, not indirect addresses.
 *
 * inode:     inode representing the file
 *
 *  returns: total number of blocks occupied by the file
 */
block_pointer_t get_size_on_disk(struct INode *inode);

/*
 * Function: get_regular_file_size
 * --------------------
 * Gets size of a regular file's content.
 *
 * inode:     inode representing the file
 *
 *  returns: file's content size in bytes.
 */
size_t get_regular_file_size(FILE *fs, struct INode *inode);

/*
 * Function: get_dir
 * --------------------
 * Finds inode that represents directory with specified name
 *  inside specified directory.
 * Doesn't check if input inode represents directory.
 * Doesn't check name validity.
 *
 * fs:              FS file
 * inode_p:         inode number of the directory to search in
 * target:          name of the directory to search
 * inode_target_p:  holder for output - inode number of wanted directory
 *
 *  returns: 0 <=> inode was found successfully.
 */
char get_dir(FILE *fs, inode_pointer_t inode_p, const char *target, inode_pointer_t *inode_target_p);

/*
 * Function: is_name_valid
 * --------------------
 * Checks if name for file is valid.
 * Name must have restricted length, can't contain '/' and can't be "." or ".."
 *
 * name:     name for file to check
 *
 *  returns: 1 <=> name is valid
 */
int is_name_valid(const char *name);

/*
 * Function: is_name_taken
 * --------------------
 * Checks if name is already taken inside provided directory.
 *
 * fs:      FS file
 * inode_p: inode number of directory to search in
 * name:    name to check
 *
 *  returns: 1 <=> name is already taken
 */
int is_name_taken(FILE *fs, inode_pointer_t inode_p, const char *name);
