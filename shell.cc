#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class File_Ops
{
public:
    static int do_copyin(const char *filename, int inumber, INE5412_FS *fs);

    static int do_copyout(int inumber, const char *filename, INE5412_FS *fs);

};

using namespace std;

int main( int argc, char *argv[] )
{
	char line[1024];
	char cmd[1024];
	char arg1[1024];
	char arg2[1024];
	int inumber, result, args;

	if(argc != 3) {
		cout << "use: " << argv[0] << " <diskfile> <nblocks>\n";
		return 1;
	}


    Disk disk(argv[1], atoi(argv[2]));

    INE5412_FS fs(&disk);

	cout << "opened emulated disk image " << argv[1] << " with " << disk.size() << " blocks\n";

	while(1) {
		cout << " simplefs> ";
		fflush(stdout);

		if(!fgets(line,sizeof(line),stdin)) 
            break;

		if(line[0] == '\n') 
            continue;

		line[strlen(line)-1] = 0;

		args = sscanf(line,"%s %s %s", cmd, arg1, arg2);

		if(args == 0) 
            continue;

		if(!strcmp(cmd, "format")) {
			if(args == 1) {
				if(fs.fs_format()) {
					cout << "disk formatted.\n";
				} else {
					cout << "format failed!\n";
				}
			} else {
				cout << "use: format\n";
			}
		} else if(!strcmp(cmd, "mount")) {
			if(args == 1) {
				if(fs.fs_mount()) {
					cout << "disk mounted.\n";
				} else {
					cout << "mount failed!\n";
				}
			} else {
				cout << "use: mount\n";
			}
		} else if(!strcmp(cmd, "debug")) {
			if(args == 1) {
				fs.fs_debug();
			} else {
				cout << "use: debug\n";
			}
		} else if(!strcmp(cmd, "getsize")) {
			if(args == 2) {
				inumber = atoi(arg1);
				result = fs.fs_getsize(inumber);
				if(result >= 0) {
					cout << "inode " << inumber << " has size " << result << "\n";
				} else {
					cout << "getsize failed!\n";
				}
			} else {
				cout << "use: getsize <inumber>\n";
			}
			
		} else if(!strcmp(cmd, "create")) {
			if(args == 1) {
				inumber = fs.fs_create();
				if(inumber > 0) {
					cout << "created inode " << inumber << "\n";
				} else {
					cout << "create failed!\n";
				}
			} else {
				cout << "use: create\n";
			}
		} else if(!strcmp(cmd, "delete")) {
			if(args == 2) {
				inumber = atoi(arg1);
				if(fs.fs_delete(inumber)) {
					cout << "inode " << inumber << " deleted.\n";
				} else {
					cout << "delete failed!\n";	
				}
			} else {
				cout << "use: delete <inumber>\n";
			}
		} else if(!strcmp(cmd, "cat")) {
			if(args==2) {
				inumber = atoi(arg1);
				if(!File_Ops::do_copyout(inumber, "/dev/stdout", &fs)) {
					cout << "cat failed!\n";
				}
			} else {
				cout << "use: cat <inumber>\n";
			}

		} else if(!strcmp(cmd,"copyin")) {
			if(args==3) {
				inumber = atoi(arg2);
				if(File_Ops::do_copyin(arg1, inumber, &fs)) {
					cout << "copied file " << arg1 << " to inode " << inumber << "\n";
				} else {
					cout << "copy failed!\n";
				}
			} else {
				cout << "use: copyin <filename> <inumber>\n";
			}

		} else if(!strcmp(cmd, "copyout")) {
			if(args == 3) {
				inumber = atoi(arg1);
				if(File_Ops::do_copyout(inumber, arg2, &fs)) {
					cout << "copied inode " << inumber << " to file " << arg2 << "\n";
				} else {
					cout << "copy failed!\n";
				}
			} else {
				cout << "use: copyout <inumber> <filename>\n";
			}

		} else if(!strcmp(cmd, "help")) {
			cout << "Commands are:\n";
			cout << "    format\n";
			cout << "    mount\n";
			cout << "    debug\n";
			cout << "    create\n";
			cout << "    delete  <inode>\n";
			cout << "    cat     <inode>\n";
			cout << "    copyin  <file> <inode>\n";
			cout << "    copyout <inode> <file>\n";
			cout << "    help\n";
			cout << "    quit\n";
			cout << "    exit\n";
		} else if(!strcmp(cmd, "quit")) {
			break;
		} else if(!strcmp(cmd, "exit")) {
			break;
		} else {
			cout << "unknown command: " << cmd << "\n";
			cout << "type 'help' for a list of commands.\n";
			result = 1;
		}
	}

	cout << "closing emulated disk.\n";
	disk.close();

	return 0;
}

int File_Ops::do_copyin(const char *filename, int inumber, INE5412_FS *fs)
{
	FILE *file;
	int offset=0, result, actual;
	char buffer[16384];

	file = fopen(filename, "r");
	if(!file) {
		cout << "couldn't open " << filename << "\n";
		return 0;
	}

	while(1) {
		result = fread(buffer,1,sizeof(buffer),file);
		if(result <= 0) break;
		if(result > 0) {
			actual = fs->fs_write(inumber,buffer,result,offset);
			if(actual<0) {
				cout << "ERROR: fs_write return invalid result " << actual << "\n";
				break;
			}
			offset += actual;
			if(actual!=result) {
				cout << "WARNING: fs_write only wrote " << actual << " bytes, not " << result << " bytes\n";
				break;
			}
		}
	}

	cout << offset << " bytes copied\n";

    fclose(file);

	return 1;
}

int File_Ops::do_copyout(int inumber, const char *filename, INE5412_FS *fs)
{
	FILE *file;
	int offset = 0, result;
	char buffer[16384];

	file = fopen(filename,"w");
	if(!file) {
		cout << "couldn't open " << filename << "\n";
		return 0;
	}

	while(1) {
		result = fs->fs_read(inumber,buffer,sizeof(buffer),offset);
		if(result<=0) break;
		fwrite(buffer,1,result,file);
		offset += result;
	}

	cout << offset << " bytes copied\n";

	fclose(file);
	return 1;
}

