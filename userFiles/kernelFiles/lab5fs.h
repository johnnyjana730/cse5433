#ifndef _LAB5FS_H_
#define _LAB5FS_H_

#include <linux/types.h>
#include <linux/fs.h>

/* Definitions */
#define LAB5FS_SB_MAGIC_NUM  0xCAFECAFE
#define MAX_FILE_NAME_LEN    10
#define BLOCK_SIZE 512
#define NUM_INODES 1000
#define INODE_SIZE 64
#define DATABLOCK_SIZE 8000
#define NUM_DATA_BLOCKS 16384

/* Macros */
#define BYTES_TO_BLOCKS(loc, blocksize) (loc/blocksize)
#define BLOCK_N(n, blocksize)           (n*blocksize)
#define INODE_N(n, bs)                  (BLOCK_N(33,BLOCK_SIZE)+(n*bs))
#define DATA_N(n, bs)                   (BLOCK_N(158,BLOCK_SIZE)+(n*bs))

/* Types */
typedef struct lab5fs_sb {
    /* Misc. */
    unsigned long magic_num;

    /* Sizes */
    unsigned long blocksize;
    unsigned long blocksize_bits;
    unsigned long max_bytes;
    unsigned long inode_size;

    /* Block locations */
    unsigned long inode_bitmap_loc;
    unsigned long data_bitmap_loc;
    unsigned long inode_loc;
    unsigned long root_inode_loc;
    unsigned long data_loc;

    unsigned long last_data_block;

    /* Free/etc. counts */
    unsigned long data_blocks_free;
    unsigned long inode_blocks_free;
    unsigned long data_blocks_total;
    unsigned long inode_blocks_total;

    __u16 padding[226];
} lab5fs_sb;

typedef struct lab5fs_ino {
    /* Inode data */
    char name[MAX_FILE_NAME_LEN];

    __u32 i_uid;
    __u32 i_gid;
    __u32 i_mode;
    __u16 blocks;
    __u32 size;

    struct timespec i_atime;
    struct timespec i_mtime;
    struct timespec i_ctime;

    /* Link data */
    __u8 is_hard_link;
    __u32 block_to_link_to;
} lab5fs_ino;

#endif
