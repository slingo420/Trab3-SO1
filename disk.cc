#include "disk.h"
#include <unistd.h>

Disk::Disk(const char *filename, int n)
{
	diskfile = fopen(filename, "r+");

	if(!diskfile) 
		diskfile = fopen(filename, "w+");

	if(!diskfile) { 
		cout << "Error when opening the file " << filename << "\n";
		return;
	}

	ftruncate(fileno(diskfile), n * DISK_BLOCK_SIZE);

    nblocks = n;
    nreads = 0;
    nwrites = 0;
}

int Disk::size()
{
	return nblocks;
}

void Disk::sanity_check( int blocknum, const void *data )
{
	if(blocknum < 0) {
		cout << "ERROR: blocknum (" << blocknum << ") is negative!\n";
		abort();
	}

	if(blocknum >= nblocks) {
		cout << "ERROR: blocknum (" << blocknum << ") is too big!\n";
		abort();
	}

	if(!data) {
		cout << "ERROR: null data pointer!\n";
		abort();
	}
}

void Disk::read(int blocknum, char *data )
{
	sanity_check(blocknum, data);

    fseek(diskfile, blocknum * DISK_BLOCK_SIZE, SEEK_SET);

	if(fread(data,DISK_BLOCK_SIZE,1,diskfile)==1) {
		nreads++;
	} else {
		cout << "ERROR: couldn't access simulated disk\n";
		abort();
	}
}

void Disk::write(int blocknum, const char *data)
{
	sanity_check(blocknum, data);

    fseek(diskfile,blocknum*DISK_BLOCK_SIZE,SEEK_SET);

	if(fwrite(data,DISK_BLOCK_SIZE,1,diskfile)==1) {
		nwrites++;
	} else {
		cout << "ERROR: couldn't access simulated disk\n";
		abort();
	}
	
}

void Disk::close()
{
	if(diskfile) {
		cout << nreads << " disk block reads\n";
		cout << nwrites << " disk block writes\n";
		fclose(diskfile);
		diskfile = 0;
	}
}

