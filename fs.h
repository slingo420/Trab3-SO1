#ifndef FS_H
#define FS_H

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "disk.h"
class INE5412_FS {
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
  int fs_format();
  int fs_mount();
  int fs_umount();
  int fs_create();
  int fs_delete(int inumber);
  int fs_getsize(int inumber);

  int fs_read(int inumber, char *data, int length, int offset);
  int fs_write(int inumber, const char *data, int length, int offset);

 private:
  Disk *disk;
  fs_superblock superblock;
  bool mounted = false;
  vector<bool> free_blocks;

  /**
   * Find if inumber is valid
   */
  bool inumber_is_valid(int inumber) {
    return inumber > 0 && inumber <= superblock.ninodes;
  }

  /**
   * Find block in which inode is stored
   */
  int find_inode_block(int inumber) {
    return 1 + (inumber - 1) / INODES_PER_BLOCK;
  }

  /**
   * Find inode position inside a block.
   */
  int find_inode_offset(int inumber) {
    return (inumber - 1) % INODES_PER_BLOCK;
  }

  /**
   * Given a block number, read it from the disk.
   */
  fs_block read_block(int blocknum) {
    fs_block block;
    this->disk->read(blocknum, block.data);
    return block;
  }

  /**
   * Given an inode and an offset inside it, read the block defined by the
   * offset. If an indirect block is necessary and indirect_block_ptr is null,
   * read it from disk and write to indirect_block_ptr, else read the indirect
   * block from the pointer.
   */
  fs_block read_block(fs_inode *inode, int offset, int **indirect_block_ptr);

  optional<pair<int, fs_block>> find_free_inode();

  /**
   * Find if disk can be used by other functions besides debug, mount and
   * format.
   */
  bool is_usable();
  bool is_usable(int inumber);

  /**
   * Find free block on the disk and return it's number.
   */
  int find_free_iblock();

  /**
   * Allocate a block used to store data.
   * If an indirect block is necessary and indirect_block is null, read it from
   * disk and write to indirect_block, else read the indirect block from the
   * pointer.
   */
  int allocate_data_block(fs_inode *inode, int block_index,
                          fs_block **indirect_block);

  /**
   * Allocate indirect block to inode.
   */
  int allocate_indirect_block(fs_inode *inode);
};

#endif
