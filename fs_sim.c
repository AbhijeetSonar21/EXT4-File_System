#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "fs.h"
#include "disk.h"
#include <stdlib.h>

bool command(char *comm, char *comm2)
{
	if(strlen(comm) == strlen(comm2) && strncmp(comm, comm2, strlen(comm)) == 0) return true;
	return false;
}

int main(int argc, char **argv)
{
	char input[64+16+16+16+SMALL_FILE];
	char comm[64], arg1[16], arg2[16], arg3[16], arg4[SMALL_FILE];

	srand(0);

	if(argc < 2) {
		fprintf(stderr, "usage: ./fs disk_name\n");
		return -1;
	}
	srand(0);
		
	printf("sizeof inode: %d, sizeof superblock: %d, sizeof Dentry: %d\n", sizeof(Inode), sizeof(SuperBlock), sizeof(Dentry));
	fs_mount(argv[1]);
	printf("%% ");
	while(fgets(input, 256, stdin))
	{
		bzero(comm,64); bzero(arg1,16); bzero(arg2,16); bzero(arg3,16); bzero(arg4, SMALL_FILE);
		int numArg = sscanf(input, "%s %s %s %s %s", comm, arg1, arg2, arg3, arg4);
		if(command(comm, "quit")) break;
		else if(command(comm, "exit")) break;
		else execute_command(comm, arg1, arg2, arg3, arg4, numArg - 1);

		printf("%% ");
	}

	fs_umount(argv[1]);
}

