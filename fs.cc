#include "fs.h"
#include <cmath>
#include <algorithm>

int INE5412_FS::fs_format()
{
	// Check if the file system is already mounted
    if (mounted) {
        cout << "Error: File system is already mounted. Format operation aborted.\n";
        return 0; // Return failure
    }

	// Calculate block distribution 
	int total_blocks = disk->size();
	int inode_blocks = static_cast<int>(ceil(total_blocks * 0.1));

	// Reserving ten percent of blocks to inodes
	superblock.ninodeblocks = inode_blocks;
	superblock.ninodes = inode_blocks * INODES_PER_BLOCK;

	// Freeing the inode table
	for (int i = 1; i <= inode_blocks; ++i) {
		fs_block inode_block;
		for (int j = 0; j < INODES_PER_BLOCK; ++j) {
			inode_block.inode[j].isvalid = false;
		}
		disk->write(i, inode_block.data);
	}

	// Writing the superblock
	superblock.magic = FS_MAGIC;
	superblock.nblocks = total_blocks;
	fs_block new_superblock;
	new_superblock.super = superblock;
	disk->write(0, new_superblock.data);

	return 1; // Return success
}

void INE5412_FS::fs_debug()
{
	union fs_block block;

	disk->read(0, block.data);

	const string spaces = "    ";
	cout << "superblock:\n";
	cout << spaces << (block.super.magic == FS_MAGIC ? "magic number is valid\n" : "magic number is invalid!\n");
 	cout << spaces << block.super.nblocks << " blocks\n";
	cout << spaces << block.super.ninodeblocks << " inode blocks\n";
	cout << spaces << block.super.ninodes << " inodes\n";

	for (int i = 1; i <= this->superblock.ninodeblocks; ++i) {
		fs_block block = this->read_block(i);

		for (int j = 0; j < this->INODES_PER_BLOCK; ++j) {
			fs_inode inode = block.inode[j];
			
			if (!inode.isvalid) continue;

			cout << "inode " << (i-1)*INODES_PER_BLOCK + j + 1 << ":\n" << spaces << "size: " << inode.size << " bytes\n" << spaces << "direct blocks: ";

			for (int k = 0; k < this->POINTERS_PER_INODE; ++k)
				if (inode.direct[k])
					cout << inode.direct[k] << ' ';
			
			cout << "\n" << spaces << "indirect block: " << ((inode.indirect) ? to_string(inode.indirect) : "-") << "\n" << spaces << "indirect data blocks: ";

			if (!inode.indirect) {
				cout << "-\n";
				continue;
			}

			fs_block indirect_block = this->read_block(inode.indirect);

			for (int k = 0; k < this->POINTERS_PER_BLOCK; ++k)
				if (indirect_block.pointers[k])
					cout << indirect_block.pointers[k] << ' ';
			cout << '\n';
		}
	}
}

int INE5412_FS::fs_mount()
{
	if (mounted) {
        cout << "Error: File system is already mounted.\n";
        return 0; // Return failure
    }

	// Read the superblock from disk
	fs_block superblock_block;
	disk->read(0, superblock_block.data);
	superblock = superblock_block.super;

	// Check if the magic number is valid
	if (superblock.magic != FS_MAGIC) {
		cout << " Error: Invalid magic number. Not a valid file system.\n";
		return 0; // Return failure
	}

	// Build a bitmap of free blocks
	free_blocks.assign(superblock.nblocks, true); // Assume all blocks are ionitially free

	// Mark inode blocks as used
	for (int i = 1; i <= superblock.ninodeblocks; ++i) {
		free_blocks[i] = false;
	}

	// Mark data blocks used by valid inodes
	for (int i = 1; i <= superblock.ninodeblocks; ++i) {
		fs_block inode_block = read_block(i);
		for (int j = 0; j < INODES_PER_BLOCK; ++j) {
			fs_inode inode = inode_block.inode[j];
			if (inode.isvalid) {
				for (int k = 0; k < POINTERS_PER_INODE; ++k) {
					if (inode.direct[k]) {
						free_blocks[inode.direct[k]] = false;
					}
				}
				if (inode.indirect) {
					fs_block indirect_block = read_block(inode.indirect);
					for (int k = 0; k < POINTERS_PER_BLOCK; ++k) {
						if (indirect_block.pointers[k]) {
							free_blocks[indirect_block.pointers[k]] = false;
						}
					}
				}
			}
		} 
	}
	mounted = true;
	return 1; // Return success
}

int INE5412_FS::fs_create() {
    if (!mounted) {
        cout << "Error: File system is not mounted.\n";
        return 0; // Return failure
    }

    // Find a free inode
    int freeInode = find_free_inode();

    if (freeInode == 0) {
        cout << "Error: No free inodes available.\n";
        return 0; // Return failure
    }

    // Mark the inode as used
    fs_block inodeBlock = read_block(1 + (freeInode - 1) / INODES_PER_BLOCK);
    fs_inode *inode = &inodeBlock.inode[(freeInode - 1) % INODES_PER_BLOCK];
    inode->isvalid = 1; // Mark the inode as valid
    inode->size = 0;    // New inode with zero length

    // Write the updated inode block back to disk
    disk->write(1 + (freeInode - 1) / INODES_PER_BLOCK, inodeBlock.data);

    // Step 3: Return the inode number (positive)
    return freeInode;
}

int INE5412_FS::find_free_inode() {
    // Iterate through inodes to find the first free one
    for (int i = 1; i <= superblock.ninodes; ++i) {
        fs_block inodeBlock = read_block(1 + (i - 1) / INODES_PER_BLOCK);
        fs_inode *inode = &inodeBlock.inode[(i - 1) % INODES_PER_BLOCK];

        if (!inode->isvalid) {
            // Mark the inode as used
            inode->isvalid = 1;
            disk->write(1 + (i - 1) / INODES_PER_BLOCK, inodeBlock.data);

            return i; // Return the inode number
        }
    }

    return 0; // No free inode found
}

int INE5412_FS::fs_delete(int inumber)
{
	if (!mounted) {
        cout << "Error: File system is not mounted.\n";
        return 0;
	}

	// Check if the given inode number is valid
	if (inumber <= 0 || inumber > superblock.ninodes) {
		cout << "Error: Invalid inode number.\n";
		return 0;
	}

	// Read the inode block containing the target inode
	fs_block inodeBlock = read_block(1 + (inumber - 1) / INODES_PER_BLOCK);
	fs_inode *inode = &inodeBlock.inode[(inumber - 1) % INODES_PER_BLOCK];

	// Check if the inode is valid
	if (!inode->isvalid) {
		cout << "Error: Inode is not valid.\n";
		return 0;
	}

	// Free data blocks and indirect blocks associated with the inode
	for (int i = 0; i < POINTERS_PER_INODE; i++) {
		if (inode->direct[i]) {
			// Free the direct block
			free_blocks[inode->direct[i]] = true;
		}
	}
	if (inode->indirect) {
		// Free the indirect block
		free_blocks[inode->indirect] = true;

		// Free data blocks pointed by the indirect block
		fs_block indirectBlock = read_block(inode->indirect);
		for (int i = 0; i < POINTERS_PER_BLOCK; ++i) {
			if (indirectBlock.pointers[i]) {
				free_blocks[indirectBlock.pointers[i]] = true;
			}
		}
	}
	// Mark the inode as invalid
	inode->isvalid = 0;

	// Write the update inode block back to the disk
	disk->write(1 + (inumber - 1) / INODES_PER_BLOCK, inodeBlock.data);
	return 1;

}

int INE5412_FS::fs_getsize(int inumber)
{
	if (!mounted) {
        cout << "Error: File system is not mounted.\n";
        return -1; 
    }

	// Check if the given inode number is valid
	if (inumber <= 0 || inumber > superblock.ninodes) {
		cout << "Error: Invalid inode number.\n";
		return 0;
	}

	// Read the inode block containing the target inode
	fs_block inodeBlock = read_block(1 + (inumber - 1) / INODES_PER_BLOCK);
	fs_inode *inode = &inodeBlock.inode[(inumber - 1) % INODES_PER_BLOCK];

	// Check if the inode is valid
	if (!inode->isvalid) {
		cout << "Error: Inode is not valid.\n";
		return 0;
	}

	// Return the size of the inode
	return inode->size;
}

int INE5412_FS::fs_read(int inumber, char *data, int length, int offset)
{
	if (!mounted) {
        cout << "Error: File system is not mounted.\n";
        return 0; 
    }

	// Check if the given inode number is valid
	if (inumber <= 0 || inumber > superblock.ninodes) {
		cout << "Error: Invalid inode number.\n";
		return 0;
	}

	// Read the inode block containing the target inode
	fs_block inodeBlock = read_block(1 + (inumber - 1) / INODES_PER_BLOCK);
	fs_inode *inode = &inodeBlock.inode[(inumber - 1) % INODES_PER_BLOCK];

	// Check if the inode is valid
	if (!inode->isvalid) {
		cout << "Error: Inode is not valid.\n";
		return 0;
	}

	// Check if the offset is within the valid range
	if (offset < 0 || offset >= inode->size) {
		return 0;
	}

	// Calculate the effective lenght to read (considering the end of the inode)
	int effectiveLength = min(length, inode->size - offset);
	
	// Read data from the inode starting at the offset
	int bytesRead = 0;

	while (bytesRead < effectiveLength) {
		// Calculate the block index and position within the block
		int blockIndex = (offset + bytesRead) / Disk::DISK_BLOCK_SIZE;
		int blockOffset = (offset + bytesRead) % Disk::DISK_BLOCK_SIZE;

		// Read the block containing the data
		fs_block dataBlock;

		if (blockIndex < POINTERS_PER_INODE) {
			// Direct block
			disk->read(inode->direct[blockIndex], dataBlock.data);
		} else {
			// Indirect block
			int indirectIndex = blockIndex - POINTERS_PER_INODE;
			disk->read(inode->indirect, dataBlock.data);
			disk->read(dataBlock.pointers[indirectIndex], dataBlock.data);
		}

		// Copy data from the block to the provided data pointer
		int bytesToCopy = min(effectiveLength - bytesRead, Disk::DISK_BLOCK_SIZE - blockOffset);
		for (int i = 0; i < bytesToCopy; i++) {
			data[bytesRead++] = dataBlock.data[blockOffset++];
		}
	
	}
	return bytesRead;
}

int INE5412_FS::fs_write(int inumber, const char *data, int length, int offset)
{
	if (!mounted) {
        cout << "Error: File system is not mounted.\n";
        return 0; 
    }

	// Check if the given inode number is valid
	if (inumber <= 0 || inumber > superblock.ninodes) {
		cout << "Error: Invalid inode number.\n";
		return 0;
	}

	// Read the inode block containing the target inode
	fs_block inodeBlock = read_block(1 + (inumber - 1) / INODES_PER_BLOCK);
	fs_inode *inode = &inodeBlock.inode[(inumber - 1) % INODES_PER_BLOCK];

	// Check if the inode is valid
	if (!inode->isvalid) {
		cout << "Error: Inode is not valid.\n";
		return 0;
	}

	// Check if the offset is within the valid range
	if (offset < 0 || offset > inode->size) {
		cout << "Error: Invalid offset.\n";
		return 0;
	}

	// Calculate the effective lenght to read (considering the end of the inode)
	int effectiveLength = min(length, Disk::DISK_BLOCK_SIZE * (POINTERS_PER_INODE + POINTERS_PER_BLOCK) - offset);
	
	// Write data from the inode starting at the offset
	int bytesWritten = 0;

	while (bytesWritten < effectiveLength) {
		// Calculate the block index and position within the block
		int blockIndex = (offset + bytesWritten) / Disk::DISK_BLOCK_SIZE;
		int blockOffset = (offset + bytesWritten) % Disk::DISK_BLOCK_SIZE;

		// Check if a new block needs to be allocated
		if (blockIndex < POINTERS_PER_INODE && inode->direct[blockIndex] == 0) {
			// Allocate a new direct block
			int newBlock = find_free_iblock();
			if (newBlock == 0) {
				// Disk is full, return total bytes written so far
				return bytesWritten;
			}
			inode->direct[blockIndex] = newBlock;
			free_blocks[newBlock] = false;
		} else if (blockIndex >= POINTERS_PER_INODE && inode->indirect == 0) {
			// Allocate a new indirect block
			int newBlock = find_free_iblock();
			if (newBlock == 0) {
				// Disk is full, return total bytes written so far
				return bytesWritten;
			}
			inode->indirect = newBlock;
			free_blocks[newBlock] = false;

			// Initialize the indirect block
			fs_block indirectBlock;
			for (int i = 0; i < POINTERS_PER_BLOCK; ++i) {
				indirectBlock.pointers[i] = 0;
			}
			disk->write(inode->indirect, indirectBlock.data);
		}

		// Read the block containing the data
		fs_block dataBlock;

		if (blockIndex < POINTERS_PER_INODE) {
			// Direct block
			disk->read(inode->direct[blockIndex], dataBlock.data);
		} else {
			// Indirect block
			disk->read(inode->indirect, dataBlock.data);
			disk->read(dataBlock.pointers[blockIndex - POINTERS_PER_INODE], dataBlock.data);
		}

		// Copy data from the provided data pointer to the block
		int bytesToCopy = min(effectiveLength - bytesWritten, Disk::DISK_BLOCK_SIZE - blockOffset);

		for (int i = 0; i < bytesToCopy; ++i) {
			dataBlock.data[blockOffset++] = data[bytesWritten++];
		}

		// Write the block back to the disk
		if (blockIndex < POINTERS_PER_INODE) {
			// Direct block
			disk->write(inode->direct[blockIndex], dataBlock.data);
		} else {
			// Indirect block
			disk->write(dataBlock.pointers[blockIndex - POINTERS_PER_INODE], dataBlock.data);
		}
	}

	// Update the inode size if needed
	if (offset + bytesWritten > inode->size) {
		inode->size = offset + bytesWritten;
	}

	// Write the updated inode back to the disk
	disk->write(1 + (inumber - 1) / INODES_PER_BLOCK, inodeBlock.data);

	// Return the total number of bytes written
	return bytesWritten;
}

int INE5412_FS::find_free_iblock() {
	// Find a free block in the bitmap
	for (int i = superblock.ninodeblocks + 1; i < superblock.nblocks; ++i) {
		if (free_blocks[i]) {
			free_blocks[i] = false;
			return i;
		}
	}

	// No free block found
	return 0;
}