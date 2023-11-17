#ifndef FS_H
#define FS_H

#include "disk.h"
#include <vector>

class INE5412_FS
{
public:
    static const unsigned int FS_MAGIC = 0xf0f03410;
    static const unsigned short int INODES_PER_BLOCK = 128;
    static const unsigned short int POINTERS_PER_INODE = 5;
    static const unsigned short int POINTERS_PER_BLOCK = 1024;

    class fs_superblock {
        public:
            unsigned int magic;
            int nblocks;
            int ninodeblocks;
            int ninodes;
    }; 

    class fs_inode {
        public:
            int isvalid;
            int size;
            int direct[POINTERS_PER_INODE];
            int indirect;
    };

    union fs_block {
        public:
            fs_superblock super;
            fs_inode inode[INODES_PER_BLOCK];
            int pointers[POINTERS_PER_BLOCK];
            char data[Disk::DISK_BLOCK_SIZE];
    };

public:

    INE5412_FS(Disk *d) {
        disk = d;

        fs_block block;
        this->disk->read(0, block.data);
        this->superblock = block.super;
    } 

    void fs_debug();
    int  fs_format();
    int  fs_mount();

    int  fs_create();
    int  find_free_inode();
    int  fs_delete(int inumber);
    int  fs_getsize(int inumber);

    int  fs_read(int inumber, char *data, int length, int offset);
    int  fs_write(int inumber, const char *data, int length, int offset);

private:
    Disk *disk;
    fs_superblock superblock;
    bool mounted = false;
    vector<bool> free_blocks;

    fs_block read_block(int blocknum) {
        fs_block block;
        this->disk->read(blocknum, block.data);
        return block;
    }
};

#endif