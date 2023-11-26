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
	
	if (mounted) {
		cout << '\n' << "free blocks: ";
		for (size_t i = 0; i < free_blocks.size(); ++i)
			if (free_blocks[i])
				cout << i << ' ';
		cout << '\n';
	}


	for (int i = 1; i <= this->superblock.ninodeblocks; ++i) {
		fs_block block = this->read_block(i);

		for (int j = 0; j < this->INODES_PER_BLOCK; ++j) {
			fs_inode inode = block.inode[j];
			
			if (!inode.isvalid) continue;

			cout << "inode " << (i-1)*INODES_PER_BLOCK + j + 1 << ":\n" << spaces << "size: " << inode.size << " bytes\n" << spaces << "direct blocks: ";

			bool has_direct_block = false;
			for (int k = 0; k < this->POINTERS_PER_INODE; ++k) {
				if (inode.direct[k]) {
					cout << inode.direct[k] << ' ';
					has_direct_block = true;
				}
			}

			if (!has_direct_block) cout << '-';
			
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

	// Mark superblock and inode blocks as used
	for (int i = 0; i <= superblock.ninodeblocks; ++i) {
		free_blocks[i] = false;
	}

	// Mark data blocks used by valid inodes
	for (int i = 1; i <= superblock.ninodeblocks; ++i) {
		fs_block inode_block = read_block(i);
		for (int j = 0; j < INODES_PER_BLOCK; ++j) {
			fs_inode inode = inode_block.inode[j];
			if (inode.isvalid) {
				for (int k = 0; k < POINTERS_PER_INODE; ++k)
					if (inode.direct[k])
						free_blocks[inode.direct[k]] = false;
				
				if (inode.indirect) {
					free_blocks[inode.indirect] = false;
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

int INE5412_FS::fs_umount() {
	if (!mounted) {
		cout << "Error: filesystem is already umounted.\n";
		return 0;
	}

	mounted = false;
	free_blocks.clear();
	return 1;
}

bool INE5412_FS::is_usable() {
	return mounted;
}
bool INE5412_FS::is_usable(int inumber) {
	return mounted && inumber_is_valid(inumber);
}

int INE5412_FS::fs_create() {
    if (!is_usable())
        return 0; // Return failure

    // Find a free inode
    auto result = find_free_inode();

    if (!result.has_value()) {
        cout << "Error: No free inodes available.\n";
        return 0; // Return failure
    }

	int inumber = result.value().first;
	fs_block inodeBlock = result.value().second;
	fs_inode *inode = &inodeBlock.inode[find_inode_offset(inumber)];
	
	inode->isvalid = 1; // Mark the inode as valid
	inode->size = 0;    // New inode with zero length

	for (int i = 0; i < POINTERS_PER_INODE; ++i)
		inode->direct[i] = 0;
	inode->indirect = 0;

	// Write the updated inode block back to disk
	disk->write(find_inode_block(inumber), inodeBlock.data);

	// Step 3: Return the inode number (positive)
	return inumber;
}

optional<pair<int,INE5412_FS::fs_block>> INE5412_FS::find_free_inode() {
    // Iterate through inodes to find the first free one
    for (int i = 1; i <= superblock.ninodeblocks; ++i) {
        fs_block inodeBlock = read_block(i);

		for (int j = 0; j < INODES_PER_BLOCK; ++j) {
			fs_inode *inode = &inodeBlock.inode[j];

			if (!inode->isvalid) {
				// Mark the inode as used
				inode->isvalid = 1;
				return {{(i-1)*INODES_PER_BLOCK + j + 1, inodeBlock}}; // Return the inode number
			}
		}
    }

    return {}; // No free inode found
}

int INE5412_FS::fs_delete(int inumber)
{
	if (!is_usable(inumber))
		return 0;
	
	// Read the inode block containing the target inode
	fs_block inodeBlock = read_block( find_inode_block(inumber) );
	fs_inode *inode = &inodeBlock.inode[find_inode_offset(inumber)];
	
	
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
	disk->write(find_inode_block(inumber), inodeBlock.data);
	return 1;

}

int INE5412_FS::fs_getsize(int inumber)
{
	if (!is_usable(inumber))
		return 0;

	// Read the inode block containing the target inode
	fs_block inodeBlock = read_block( find_inode_block(inumber) );
	fs_inode *inode = &inodeBlock.inode[find_inode_offset(inumber)];

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
	if (!is_usable(inumber))
		return 0;

	// Read the inode block containing the target inode
	fs_block inodeBlock = read_block( find_inode_block(inumber) );
	fs_inode *inode = &inodeBlock.inode[find_inode_offset(inumber)];

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
  int *indirect_pointers = nullptr;
	while (bytesRead < effectiveLength) {
		// Calculate the block index and position within the block
		int blockOffset = (offset + bytesRead) % Disk::DISK_BLOCK_SIZE;
    fs_block dataBlock = read_block(inode, offset+bytesRead, &indirect_pointers);

		// Copy data from the block to the provided data pointer
		int bytesToCopy = min(effectiveLength - bytesRead, Disk::DISK_BLOCK_SIZE - blockOffset);
		for (int i = 0; i < bytesToCopy; i++) {
			data[bytesRead++] = dataBlock.data[blockOffset++];
		}
	}

  if (indirect_pointers)
    delete[] indirect_pointers;
	return bytesRead;
}

int INE5412_FS::fs_write(int inumber, const char *data, int length, int offset)
{
	if (!is_usable(inumber))
		return 0;

	// Read the inode block containing the target inode
	fs_block inodeBlock = read_block( find_inode_block(inumber) );
	fs_inode *inode = &inodeBlock.inode[find_inode_offset(inumber)];

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
	
	bool willNeedIndirectBlock = (offset + effectiveLength)/Disk::DISK_BLOCK_SIZE > POINTERS_PER_INODE;
	
	fs_block *indirect_block;
	
	if (!willNeedIndirectBlock) {
		indirect_block = nullptr;
	} else if (inode->indirect) {
		indirect_block = new fs_block;
		disk->read(inode->indirect, indirect_block->data);
	} else {
		if (!allocate_indirect_block(inode)) {
			cout << "Error: Disk Full!!\n";
			return bytesWritten;
		}
		indirect_block = new fs_block;
		disk->read(inode->indirect, indirect_block->data);
	}
	
	while (bytesWritten < effectiveLength) {
		// Calculate the block index and position within the block
		int blockOffset = (offset + bytesWritten) % Disk::DISK_BLOCK_SIZE;
		int blockIndex = (offset + bytesWritten) / Disk::DISK_BLOCK_SIZE;

		// number of the block that will be written to.
		// we only allocate a new block if theres no block already allocated  here.
		int allocated_block_index = (blockIndex < POINTERS_PER_INODE)
							? inode->direct[blockIndex]
							: indirect_block->pointers[blockIndex - POINTERS_PER_INODE];
		
		int newBlock = (allocated_block_index == 0)
							? allocate_data_block(inode, blockIndex, &indirect_block)
							: allocated_block_index;
		
		if (!newBlock) {
			cout << "Error: Disk Full!!\n";
			break;
		}
    

		int bytesToCopy = min(effectiveLength - bytesWritten, Disk::DISK_BLOCK_SIZE - blockOffset);
		// create the block containing the data
		fs_block dataBlock;
		// Copy data from the provided data pointer to the block
		for (int i = 0; i < bytesToCopy; ++i) {
			dataBlock.data[blockOffset++] = data[bytesWritten++];
		}

		// write the allocated block to disk.
		disk->write(newBlock, dataBlock.data);

	}

	// update inode size if necessary
	if (offset + bytesWritten >  inode->size)
		inode->size = offset + bytesWritten;

	// Write the updated inode back to the disk
	disk->write(find_inode_block(inumber), inodeBlock.data);

	// if an indirect block was used, write it to disk and delete the pointer
	if (indirect_block) {
		disk->write(inode->indirect, indirect_block->data);
		delete indirect_block;
	}


	// Return the total number of bytes written
	return bytesWritten;
}


int INE5412_FS::allocate_indirect_block(INE5412_FS::fs_inode *inode) {
	int num_indirect_block;
	if (! (num_indirect_block = find_free_iblock() ) )
		return 0;
	
	free_blocks[num_indirect_block] = false;

	inode->indirect = num_indirect_block;
	fs_block indirect;

	for (int i = 0; i < POINTERS_PER_BLOCK; ++i) 
		indirect.pointers[i] = 0;
	
	disk->write(num_indirect_block, indirect.data);
	return num_indirect_block;
}

int INE5412_FS::allocate_data_block(INE5412_FS::fs_inode *inode, int block_index, INE5412_FS::fs_block **indirect_block) {
	
	// find free block that will have data written to it.
	int new_block;
	if (! (new_block = find_free_iblock() ) )
		return 0;
	
	free_blocks[new_block] = false;


	if (block_index < POINTERS_PER_INODE) {
		inode->direct[block_index] = new_block;
	} else {
		// if inode has no indirect block, allocate one.
		if (!inode->indirect) {
			allocate_indirect_block(inode);
			if (!inode->indirect) 
				return 0;
			
			if (!(*indirect_block))
				*indirect_block = new fs_block;
			
			// set the provided pointer to the newly created block.
			// this keeps us from having to read the indirect block from disk every time.
			disk->read(inode->indirect, (*indirect_block)->data);
		}
		(*indirect_block)->pointers[block_index - POINTERS_PER_INODE] = new_block;
	}

	return new_block;
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

INE5412_FS::fs_block INE5412_FS::read_block(INE5412_FS::fs_inode* inode, int offset, int** indirect_block_ptr) {
        int blockIndex = offset / Disk::DISK_BLOCK_SIZE;
        int blockNum = (blockIndex < POINTERS_PER_INODE)
                        ? inode->direct[blockIndex]
                        : inode->indirect;
        
        if (blockIndex >= POINTERS_PER_INODE) {
            if (!(*indirect_block_ptr)) {
                fs_block indirect = read_block(inode->indirect);
                *indirect_block_ptr = new int[POINTERS_PER_BLOCK];
                for (int i = 0; i < POINTERS_PER_BLOCK; ++i)
                  (*indirect_block_ptr)[i] = indirect.pointers[i];
            }
            return read_block((*indirect_block_ptr)[blockIndex - POINTERS_PER_INODE]);
        }
        
        return read_block(blockNum);
    }