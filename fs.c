#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fs.h"
#include "fs_util.h"
#include "disk.h"


char inodeMap[MAX_INODE / 8];
char blockMap[MAX_BLOCK / 8];
Inode inode[MAX_INODE];
SuperBlock superBlock;
Dentry curDir;
int curDirBlock;

int fs_mount(char *name)
{
		int numInodeBlock =  (sizeof(Inode)*MAX_INODE)/ BLOCK_SIZE;
		int i, index, inode_index = 0;

		// load superblock, inodeMap, blockMap and inodes into the memory
		if(disk_mount(name) == 1) {
				disk_read(0, (char*) &superBlock);
				if(superBlock.magicNumber != MAGIC_NUMBER) {
						printf("Invalid disk!\n");
						exit(0);
				}
				disk_read(1, inodeMap);
				disk_read(2, blockMap);
				for(i = 0; i < numInodeBlock; i++)
				{
						index = i+3;
						disk_read(index, (char*) (inode+inode_index));
						inode_index += (BLOCK_SIZE / sizeof(Inode));
				}
				// root directory
				curDirBlock = inode[0].directBlock[0];
				disk_read(curDirBlock, (char*)&curDir);

		} else {
				// Init file system superblock, inodeMap and blockMap
				superBlock.magicNumber = MAGIC_NUMBER;
				superBlock.freeBlockCount = MAX_BLOCK - (1+1+1+numInodeBlock);
				superBlock.freeInodeCount = MAX_INODE;

				//Init inodeMap
				for(i = 0; i < MAX_INODE / 8; i++)
				{
						set_bit(inodeMap, i, 0);
				}
				//Init blockMap
				for(i = 0; i < MAX_BLOCK / 8; i++)
				{
						if(i < (1+1+1+numInodeBlock)) set_bit(blockMap, i, 1);
						else set_bit(blockMap, i, 0);
				}
				//Init root dir
				int rootInode = get_free_inode();
				curDirBlock = get_free_block();

				inode[rootInode].type = directory;
				inode[rootInode].owner = 0;
				inode[rootInode].group = 0;
				gettimeofday(&(inode[rootInode].created), NULL);
				gettimeofday(&(inode[rootInode].lastAccess), NULL);
				inode[rootInode].size = 1;
				inode[rootInode].blockCount = 1;
				inode[rootInode].directBlock[0] = curDirBlock;

				curDir.numEntry = 1;
				strncpy(curDir.dentry[0].name, ".", 1);
				curDir.dentry[0].name[1] = '\0';
				curDir.dentry[0].inode = rootInode;
				disk_write(curDirBlock, (char*)&curDir);
		}
		return 0;
}

int fs_umount(char *name)
{
		int numInodeBlock =  (sizeof(Inode)*MAX_INODE )/ BLOCK_SIZE;
		int i, index, inode_index = 0;
		disk_write(0, (char*) &superBlock);
		disk_write(1, inodeMap);
		disk_write(2, blockMap);
		for(i = 0; i < numInodeBlock; i++)
		{
				index = i+3;
				disk_write(index, (char*) (inode+inode_index));
				inode_index += (BLOCK_SIZE / sizeof(Inode));
		}
		// current directory
		disk_write(curDirBlock, (char*)&curDir);

		disk_umount(name);
		return 0;	
}

int search_cur_dir(char *name)
{
		// return inode. If not exist, return -1
		int i;

		for(i = 0; i < curDir.numEntry; i++)
		{
				if(command(name, curDir.dentry[i].name)) return curDir.dentry[i].inode;
		}
		return -1;
}

int file_create(char *name, int size)
{
		int i;

		if(size > SMALL_FILE) {
				printf("Do not support files larger than %d bytes.\n", SMALL_FILE);
				return -1;
		}

		if(size < 0){
				printf("File create failed: cannot have negative size\n");
				return -1;
		}

		int inodeNum = search_cur_dir(name); 
		if(inodeNum >= 0) {
				printf("File create failed:  %s exist.\n", name);
				return -1;
		}

		if(curDir.numEntry + 1 > MAX_DIR_ENTRY) {
				printf("File create failed: directory is full!\n");
				return -1;
		}

		int numBlock = size / BLOCK_SIZE;
		if(size % BLOCK_SIZE > 0) numBlock++;

		if(numBlock > superBlock.freeBlockCount) {
				printf("File create failed: data block is full!\n");
				return -1;
		}

		if(superBlock.freeInodeCount < 1) {
				printf("File create failed: inode is full!\n");
				return -1;
		}

		char *tmp = (char*) malloc(sizeof(int) * size + 1);

		rand_string(tmp, size);
		printf("New File: %s\n", tmp);

		// get inode and fill it
		inodeNum = get_free_inode();
		if(inodeNum < 0) {
				printf("File_create error: not enough inode.\n");
				return -1;
		}

		inode[inodeNum].type = file;
		inode[inodeNum].owner = 1;  // pre-defined
		inode[inodeNum].group = 2;  // pre-defined
		gettimeofday(&(inode[inodeNum].created), NULL);
		gettimeofday(&(inode[inodeNum].lastAccess), NULL);
		inode[inodeNum].size = size;
		inode[inodeNum].blockCount = numBlock;
		inode[inodeNum].link_count = 1;

		// add a new file into the current directory entry
		strncpy(curDir.dentry[curDir.numEntry].name, name, strlen(name));
		curDir.dentry[curDir.numEntry].name[strlen(name)] = '\0';
		curDir.dentry[curDir.numEntry].inode = inodeNum;
		printf("curdir %s, name %s\n", curDir.dentry[curDir.numEntry].name, name);
		curDir.numEntry++;

		// get data blocks
		for(i = 0; i < numBlock; i++)
		{
				int block = get_free_block();
				if(block == -1) {
						printf("File_create error: get_free_block failed\n");
						return -1;
				}
				//set direct block
				inode[inodeNum].directBlock[i] = block;

				disk_write(block, tmp+(i*BLOCK_SIZE));
		}

		//update last access of current directory
		gettimeofday(&(inode[curDir.dentry[0].inode].lastAccess), NULL);		

		printf("file created: %s, inode %d, size %d\n", name, inodeNum, size);

		free(tmp);
		return 0;
}

int file_cat(char *name)
{
		int inodeNum, i, size;
		char str_buffer[512];
		char * str;

		//get inode
		inodeNum = search_cur_dir(name);
		size = inode[inodeNum].size;

		//check if valid input
		if(inodeNum < 0)
		{
				printf("cat error: file not found\n");
				return -1;
		}
		if(inode[inodeNum].type == directory)
		{
				printf("cat error: cannot read directory\n");
				return -1;
		}

		//allocate str
		str = (char *) malloc( sizeof(char) * (size+1) );
		str[ size ] = '\0';

		for( i = 0; i < inode[inodeNum].blockCount; i++ ){
				int block;
				block = inode[inodeNum].directBlock[i];

				disk_read( block, str_buffer );

				if( size >= BLOCK_SIZE )
				{
						memcpy( str+i*BLOCK_SIZE, str_buffer, BLOCK_SIZE );
						size -= BLOCK_SIZE;
				}
				else
				{
						memcpy( str+i*BLOCK_SIZE, str_buffer, size );
				}
		}
		printf("%s\n", str);

		//update lastAccess
		gettimeofday( &(inode[inodeNum].lastAccess), NULL );

		free(str);

		//return success
		return 0;
}

int file_read(char *name, int offset, int size)
{

		int inodeNum = search_cur_dir(name);
		int read_block, new_size;
		int i,j,else_block,mod_offset;
		char str_buffer[1000] = {0};

		
		char * str,* str_1,* str_2;
		int m,b;
		b=0;
	
		if(inodeNum < 0)
		{
			printf("Error! File does not exist! \n");
			return -1;
		}
		if(size < 0)
		{
			printf("Error! Negative size! \n");
			return -1;
		}
		if(inode[inodeNum].type == file)
		{
			
			if(size <= inode[inodeNum].size)
			{
				
				read_block = inode[inodeNum].blockCount;
				
				str = (char *) malloc( sizeof(char) * (size+1) );
				str[ size ] = '\0';
				memset(str,0,size+1);
				int s1;
				
				int remaining_chunk;

				if(offset < 0)
				{
					printf("Offset is negative!! \n");
					return -1;
				}
				
				int k, loop_var, store_var = 0, new_block_1;	

				for(k=0; k<read_block; k++)
				{
					loop_var = (k+1) * BLOCK_SIZE;
					loop_var --;
					
					
					if(offset >= store_var && offset<=loop_var)
					{
						
						new_block_1 = inode[inodeNum].directBlock[k];
						m = k;
						if(i==0)
						{
							mod_offset = offset;
						}
						else
						{
							mod_offset = offset - store_var;
						}

					}
					store_var = loop_var;
					store_var ++;
					
				}

				int n1, total_size;
				n1 = size;
			
				total_size = inode[inodeNum].size;
				

				str_1 = (char *) malloc( sizeof(char) * (6144 + 1 ));
				str_1[ 6144] = '\0';


				str_2 = (char *) malloc( sizeof(char) * ( 6144 + 1 ));
				str_2[ 6144] = '\0';



				memset(str_1,0,6144 + 1);
				memset(str_2,0,6144 + 1);
				


				for( i = 0; i < read_block; i++ )
				{

				int block;
				block = inode[inodeNum].directBlock[i];

				if(offset != 0)
				{
				if(size + mod_offset <= 512 && i==m)
				{
								disk_read(block,str_buffer);
								memcpy(str_2, str_buffer + mod_offset,size);
								printf("%s\n", str_2);
								free(str_2);
								break;
				}
				}
				

				if(offset == 0)
				{

					disk_read(block, str_buffer);
					if(size >= BLOCK_SIZE)
					{
							memcpy( str+i*BLOCK_SIZE, str_buffer, BLOCK_SIZE );
							size -= BLOCK_SIZE;
							
							
					}
					else
					{
							memcpy(str+i*BLOCK_SIZE, str_buffer, size);
					}
				}
				else
				{
					if(size+offset <= inode[inodeNum].size)
					{

						

						if(i==m)
						{	
							disk_read(block,str_buffer);
							memcpy(str_1, str_buffer + mod_offset,BLOCK_SIZE - mod_offset);
							s1 = strlen(str_1);
							size = size - s1;	
						}	
						if (i>m)
						{
							disk_read(block,str_buffer);
							if( size >= BLOCK_SIZE )
							{	
								
									memcpy( str+b*BLOCK_SIZE, str_buffer, BLOCK_SIZE);
									size -= BLOCK_SIZE;
									b++;
									
							}
							else
							{		
									memcpy( str+b*BLOCK_SIZE, str_buffer, size);	
							}
						}
							
					} 
					else
					{
						printf("Invalid size!! \n");
						return -1;
					}
				}		
		}
		

		strcat(str_1,str);
		printf("%s \n",str_1);
		free(str_1);
		gettimeofday( &(inode[inodeNum].lastAccess), NULL );
		free(str);
		}
			else
			{
					printf("Invalid request size! \n");
					return -1;
			}

		}
		else
		{
			printf("It is a directory! \n");
			return -1;
		}	
		return 0;
}

int file_stat(char *name)
{
		char timebuf[28];
		int inodeNum = search_cur_dir(name);
		if(inodeNum < 0) {
				printf("file cat error: file is not exist.\n");
				return -1;
		}

		printf("Inode\t\t= %d\n", inodeNum);
		if(inode[inodeNum].type == file) printf("type\t\t= File\n");
		else printf("type\t\t= Directory\n");
		printf("owner\t\t= %d\n", inode[inodeNum].owner);
		printf("group\t\t= %d\n", inode[inodeNum].group);
		printf("size\t\t= %d\n", inode[inodeNum].size);
		printf("link_count\t= %d\n", inode[inodeNum].link_count);
		printf("num of block\t= %d\n", inode[inodeNum].blockCount);
		format_timeval(&(inode[inodeNum].created), timebuf, 28);
		printf("Created time\t= %s\n", timebuf);
		format_timeval(&(inode[inodeNum].lastAccess), timebuf, 28);
		printf("Last acc. time\t= %s\n", timebuf);

		return 0;
}

int file_remove(char *name)
{
		int inodeNum = search_cur_dir(name); 
		int numBlock = inode[inodeNum].blockCount;
		int n=0;
		int i;
		int last;
		int s;
		int link_count_1=0;
		if(inodeNum < 0)
		{
			printf(" File does not exist! \n");
			return -1;
		}
		if(inode[inodeNum].type == directory)
		{
			printf("Invalid! It is a directory! \n");
			return -1;
		}
		for(i = 0; i < curDir.numEntry; i++)
		{
			if(curDir.dentry[i].inode == inodeNum)
			{
				if(strcmp(curDir.dentry[i].name,name) == 0)
				{
					n = i;
					break;
				}
			}
		}
	
		for(i = 0; i < numBlock; i++)
		{
				inode[inodeNum].directBlock[i] = 0;
				set_bit(blockMap, i, 0);
		}
		
		if(inode[inodeNum].link_count == 1)
		{
			
			superBlock.freeInodeCount ++;
			set_bit(inodeMap, curDir.dentry[n].inode, 0);
			superBlock.freeBlockCount = superBlock.freeBlockCount + numBlock;
		}
		for(i=0; i < curDir.numEntry; i++)
		 {
		 if(curDir.dentry[i].inode == inodeNum)
		 {
		 		if(strcmp(curDir.dentry[i].name,name) == 0)
				{
		 			if(inode[inodeNum].link_count > 1)
		 			{
		 				inode[inodeNum].link_count --;
						break;
		 			}
				}
		 } 
		 }
		curDir.dentry[n] = curDir.dentry[curDir.numEntry - 1];
		memset(&curDir.dentry[curDir.numEntry - 1], 0, 0);
		curDir.numEntry --;
		return 0;
}


int dir_make(char *name)
{


    int inodeNumSrc = search_cur_dir(name);



    if (inodeNumSrc >= 0)
    {
        printf("Directory create Failed\n");
        return -1;
    }

	if (superBlock.freeInodeCount < 1)
    {
        printf("Directory create failed! Error!!\n");
        return -1;
    }

    if (curDir.numEntry + 1 > MAX_DIR_ENTRY)
    {
        printf("Error! Directory create failed!!\n");
        return -1;
    }
    
    inodeNumSrc = get_free_inode();


    int block = get_free_block();

    if (inodeNumSrc < 0)
    {
        printf("Directory create failed!!\n");
        return -1;
    }


    inode[inodeNumSrc].type = directory;
    inode[inodeNumSrc].owner = 0; 
    inode[inodeNumSrc].group = 0; 
    gettimeofday(&(inode[inodeNumSrc].created), NULL);
    gettimeofday(&(inode[inodeNumSrc].lastAccess), NULL);
    inode[inodeNumSrc].size = 1;
    inode[inodeNumSrc].blockCount = 1;
    inode[inodeNumSrc].directBlock[0] = block;
    strncpy(curDir.dentry[curDir.numEntry].name, name, strlen(name));
    curDir.dentry[curDir.numEntry].name[strlen(name)] = '\0';
    curDir.dentry[curDir.numEntry].inode = inodeNumSrc;
    curDir.numEntry++;
    disk_write(curDirBlock, (char *)&curDir);
    int inode_b=curDir.dentry[0].inode;
    int block_b=curDirBlock;
    block = inode[inodeNumSrc].directBlock[0];
    disk_write(curDirBlock, (char *)&curDir);
    curDirBlock = block;
    disk_read(block, (char *)&curDir);
	strncpy(curDir.dentry[0].name, ".", 1);
	curDir.dentry[0].name[1] = '\0';
	curDir.dentry[0].inode = inodeNumSrc;
	curDir.numEntry++;
	strncpy(curDir.dentry[1].name, "..", 2);
	curDir.dentry[1].name[2] = '\0';
	curDir.dentry[1].inode = inode_b;
	curDir.numEntry++;
	disk_write(curDirBlock, (char *)&curDir);
	block = block_b;
	disk_write(curDirBlock, (char *)&curDir);
	curDirBlock = block;
	disk_read(block, (char *)&curDir);
	
    return 0;


}

int deleteFiles()
{ 
	int k;
	for(k=0;k<curDir.numEntry;k++)
    {
        int inode_f=curDir.dentry[k].inode;
        if(inode[inode_f].type==file)
        {
				char *name=curDir.dentry[k].name;
				int inodeNum = search_cur_dir(name); 
				int numBlock = inode[inodeNum].blockCount;
				int n;
				int i;
				int last;
				int s;
				int link_count_1;
				for(i = 0; i < curDir.numEntry; i++)
				{
					if(curDir.dentry[i].inode == inodeNum)
					{
						if(strcmp(curDir.dentry[i].name,name) == 0)
						{
							n = i;
							break;
						}
					}
				}
				for(i=0; i < curDir.numEntry; i++)
				{
					if(curDir.dentry[i].inode == inodeNum)
					{
						if(strcmp(curDir.dentry[i].name,name) == 0)
						{
							if(inode[inodeNum].link_count > 1)
							{
								link_count_1 = inode[inodeNum].link_count;
								inode[inodeNum].link_count --;
							}
						}
					} 
				}
				for(i = 0; i < numBlock; i++)
				{
						inode[inodeNum].directBlock[i] = 0;
						set_bit(blockMap, i, 0);
				}
				if(link_count_1 == 1)
				{
					superBlock.freeInodeCount ++;
					set_bit(inodeMap, curDir.dentry[n].inode, 0);
					superBlock.freeBlockCount = superBlock.freeBlockCount + numBlock;
				}
				curDir.dentry[n] = curDir.dentry[curDir.numEntry - 1];
				memset(&curDir.dentry[curDir.numEntry - 1], 0, 0);
				curDir.numEntry --;
            	k--;
        }
    }
	return 0;
}


int sub_directory(depth,in_b,dblock_b , back)
{
    int inodeNumSrc,k,i,block,n,bk1;
	deleteFiles();
    depth++;
    
    for(k=2;k<curDir.numEntry;k++)
    {
        inodeNumSrc=curDir.dentry[k].inode;
        if(inode[inodeNumSrc].type==directory)
        {
            block=inode[inodeNumSrc].directBlock[0];
            disk_write(curDirBlock, (char *)&curDir);
            curDirBlock = block;
            disk_read(block, (char *)&curDir);
			char *name=curDir.dentry[k].name;
            if(curDir.numEntry<=2)
            {
					int pinode=curDir.dentry[1].inode;
					int block = inode[pinode].directBlock[0];
					disk_write(curDirBlock, (char *)&curDir);
					curDirBlock = block;
					disk_read(block, (char *)&curDir);
					for ( i = 0; i < curDir.numEntry; i++)
					{
						char *dirName=curDir.dentry[i].name;
						if (strcmp(dirName, name) == 0)
						{
							n = i;
							break;
						}
					}
					int numBlock = inode[inodeNumSrc].blockCount;
					for ( i = 0; i < numBlock; i++)
					{
						inode[inodeNumSrc].directBlock[i] = 0;
						set_bit(blockMap, i, 0);
					}
					superBlock.freeBlockCount++;
					superBlock.freeInodeCount++;
					set_bit(inodeMap, inodeNumSrc, 0);
					curDir.dentry[n] = curDir.dentry[curDir.numEntry - 1];
					memset(&curDir.dentry[curDir.numEntry - 1], 0, sizeof(DirectoryEntry));
					curDir.numEntry--;
                	k--;
            }
            else if(curDir.numEntry>2)
            {
                sub_directory(depth,in_b,dblock_b , back);
                k--;
            }
        }
    }
	// lastCheck()
	// int cur_block=curDirBlock;
	// lastCheck(bDir,name,depth,inodeNumSrc,curDirBlock,cur_block);
    // int dirBlocktoCheck=inode[inodeNumSrc].directBlock[0];
	// 	disk_write(curDirBlock, (char *)&curDir);
	// 	curDirBlock = block;
	// 	disk_read(block, (char *)&curDir);
	// 	if(curDir.numEntry>2)
	// 	{
	// 		sub_directory(depth,in_b,dblock_b , back);
	// 	}
	
	int j=0;
	while (j<depth)
    {
			j++;
            bk1=curDir.dentry[1].inode;
            block = inode[bk1].directBlock[0];
			depth--;
            if(block>dblock_b)
            {
                disk_write(curDirBlock, (char *)&curDir);
                curDirBlock = block;
                disk_read(block, (char *)&curDir);
                sub_directory(depth,in_b,dblock_b , back);
                // depth--;
            }
    }
}


int dir_remove(char *name)
{
    int inodeNumSrc = search_cur_dir(name);
    int i;
    int depth=0;
    int n = 0;
    if (inodeNumSrc < 0)
    {
        printf("\n Directory Not found\n");
        return 0;
    }
    if (inode[inodeNumSrc].type == file)
    {
        printf("\n Cannot Delete a file\n");
        return 0;
    }

    int bDir=curDirBlock;
    int block = inode[inodeNumSrc].directBlock[0];
    disk_write(curDirBlock, (char *)&curDir);
    curDirBlock = block;
    disk_read(block, (char *)&curDir);
    int back=curDirBlock;
	if(curDir.numEntry<=2)
    {
					int block = bDir;
					disk_write(curDirBlock, (char *)&curDir);
					curDirBlock = block;
					disk_read(block, (char *)&curDir);

					for ( i = 0; i < curDir.numEntry; i++)
					{
						char *dirName=curDir.dentry[i].name;
						if (strcmp(dirName, name) == 0)
						{
							n = i;
							break;
						}
					}
					int numBlock = inode[inodeNumSrc].blockCount;
					for ( i = 0; i < numBlock; i++)
					{
						inode[inodeNumSrc].directBlock[i] = 0;
						set_bit(blockMap, i, 0);
					}
					superBlock.freeInodeCount++;
					superBlock.freeBlockCount++;
					set_bit(inodeMap, curDir.dentry[n].inode, 0);
					curDir.dentry[n] = curDir.dentry[curDir.numEntry - 1];
					memset(&curDir.dentry[curDir.numEntry - 1], 0, sizeof(DirectoryEntry));
					curDir.numEntry--;
					return 0;
    }
    if(curDir.numEntry>2)
    {
		sub_directory(depth,inodeNumSrc,bDir,curDirBlock);
		// int dirBlocktoCheck=inode[inodeNumSrc].directBlock[0];
		// disk_write(curDirBlock, (char *)&curDir);
		// curDirBlock = block;
		// disk_read(block, (char *)&curDir);
		// if(curDir.numEntry>2)
		// {
		// 	sub_directory(depth,inodeNumSrc,bDir,curDirBlock);
		// }
			int block = bDir;
			disk_write(curDirBlock, (char *)&curDir);
			curDirBlock = block;
			disk_read(block, (char *)&curDir);for ( i = 0; i < curDir.numEntry; i++)
			{
				char *dirName=curDir.dentry[i].name;
				if (strcmp(dirName, name) == 0)
				{
					n = i;
					break;
				}
			}
			int numBlock = inode[inodeNumSrc].blockCount;
			for ( i = 0; i < numBlock; i++)
			{
				inode[inodeNumSrc].directBlock[i] = 0;
				set_bit(blockMap, i, 0);
			}
			superBlock.freeInodeCount++;
			superBlock.freeBlockCount++;
			set_bit(inodeMap, inodeNumSrc, 0);
			curDir.dentry[n] = curDir.dentry[curDir.numEntry - 1];
			memset(&curDir.dentry[curDir.numEntry - 1], 0, sizeof(DirectoryEntry));
			curDir.numEntry--;
		
		return 0;
    }
}


int dir_change(char* name)
{
	int n,i;
	int totalBlockCount=0;
	int inodeNumSrc = search_cur_dir(name);
	int lastInode=0;
	int inodeNum;
	if( strcmp("..",name)==0)
	{
		int block_to_search=curDirBlock;
		int block_b=0;
		int i;
		int traversed_i=0;
		int directory_inodes[500];
		directory_inodes[0]=0;
		int inode_to_search;
		int count=0;
		char *to_search_dir;
		
		for(i = 0; i < MAX_INODE; i++)
		{
			if(get_bit(inodeMap, i) == 1) 
			{
				if(inode[i].type==directory && i>0)
				{
					int block=inode[i].directBlock[0];
					if(block<=block_to_search)
					{
						count++;
						directory_inodes[count]=i;
						if(block_to_search==block)
						{
							inode_to_search=i;
						}
					}
				}
			}
		}
		for(i=0;i<count;i++)
		{
			int dir_inode=directory_inodes[i];
			int block=inode[dir_inode].directBlock[0];
			block_b=curDirBlock;
			disk_write(curDirBlock, (char *)&curDir);
			curDirBlock = block;
			disk_read(block, (char *)&curDir);
			int j;
			for(j=0;j<curDir.numEntry;j++)
			{
				int sub_inodeNum=curDir.dentry[j].inode;
				int sub_block=inode[sub_inodeNum].directBlock[0];
				if(inode_to_search==sub_inodeNum)
				{
					to_search_dir=curDir.dentry[j].name;
				}
			}
		}
		for(i=0;i<count;i++)
		{
			int dir_inode=directory_inodes[i];
			int block=inode[dir_inode].directBlock[0];
			block_b=curDirBlock;
			disk_write(curDirBlock, (char *)&curDir);
			curDirBlock = block;
			disk_read(block, (char *)&curDir);
			int j;
			int k=search_cur_dir(to_search_dir);
			if(k>=0 && k==inode_to_search)
			{
				return 0;
			}
		}
		
		return 0;
	}
	
	if(inodeNumSrc<0)
	{
		printf("No such directory\n");
		return -1;
	}

	if( strcmp(".",name)==0)
	{
		return 0;
	}
	
	if (inode[inodeNumSrc].type == file)
	{
		printf("\n It Is a File\n");
		return -1;
	}
	
	int block = inode[inodeNumSrc].directBlock[0];
	disk_write(curDirBlock, (char *)&curDir);
	curDirBlock = block;
	disk_read(block, (char *)&curDir);
	return 0;
}

int ls()
{
		int i;
		for(i = 0; i < curDir.numEntry; i++)
		{
				int n = curDir.dentry[i].inode;
				if(inode[n].type == file) printf("type: file, ");
				else printf("type: dir, ");
				printf("name \"%s\", inode %d, size %d byte \n", curDir.dentry[i].name, curDir.dentry[i].inode, inode[n].size);
		}

		return 0;
}

int fs_stat()
{
		printf("File System Status: \n");
		printf("# of free blocks: %d (%d bytes), # of free inodes: %d\n", superBlock.freeBlockCount, superBlock.freeBlockCount*512, superBlock.freeInodeCount);
		return 0;
}

int hard_link(char *src, char *dest)
{
		int inodeNum_Source = search_cur_dir(src); 
		printf("%d \n", inodeNum_Source);

		if(inode[inodeNum_Source].type == directory)
		{
			printf("Directories cannot be linked!!\n");
			return -1;
		}

		if(inodeNum_Source < 0)
		{
			printf("Failed! Source Not exist\n");
			return -1;
		}

		int inodeNum_Dest = search_cur_dir(dest);
		if(inodeNum_Dest > 0)
		{
			printf("Destination already exist \n");
			return -1;
		}

		if(inode[inodeNum_Dest].type == directory)
		{
			printf("File cannot be linked to Directory!! \n");
			return -1;
		}


		strncpy(curDir.dentry[curDir.numEntry].name, dest, strlen(dest));
		curDir.dentry[curDir.numEntry].name[strlen(dest)] = '\0';
		curDir.dentry[curDir.numEntry].inode = inodeNum_Source;
		printf("curdir %s, name %s\n", curDir.dentry[curDir.numEntry].name, dest);
		curDir.numEntry++;
		inode[inodeNum_Source].link_count ++;
		return 0;
}

int execute_command(char *comm, char *arg1, char *arg2, char *arg3, char *arg4, int numArg)
{

    printf ("\n");
	if(command(comm, "df")) {
				return fs_stat();

    // file command start    
    } else if(command(comm, "create")) {
        if(numArg < 2) {
            printf("error: create <filename> <size>\n");
            return -1;
        }
		return file_create(arg1, atoi(arg2)); // (filename, size)

	} else if(command(comm, "stat")) {
		if(numArg < 1) {
			printf("error: stat <filename>\n");
			return -1;
		}
		return file_stat(arg1); //(filename)

	} else if(command(comm, "cat")) {
		if(numArg < 1) {
			printf("error: cat <filename>\n");
			return -1;
		}
		return file_cat(arg1); // file_cat(filename)

	} else if(command(comm, "read")) {
		if(numArg < 3) {
			printf("error: read <filename> <offset> <size>\n");
			return -1;
		}
		return file_read(arg1, atoi(arg2), atoi(arg3)); // file_read(filename, offset, size);

	} else if(command(comm, "rm")) {
		if(numArg < 1) {
			printf("error: rm <filename>\n");
			return -1;
		}
		return file_remove(arg1); //(filename)

	} else if(command(comm, "ln")) {
		return hard_link(arg1, arg2); // hard link. arg1: src file or dir, arg2: destination file or dir

    // directory command start
	} else if(command(comm, "ls"))  {
		return ls();

	} else if(command(comm, "mkdir")) {
		if(numArg < 1) {
			printf("error: mkdir <dirname>\n");
			return -1;
		}
		return dir_make(arg1); // (dirname)

	} else if(command(comm, "rmdir")) {
		if(numArg < 1) {
			printf("error: rmdir <dirname>\n");
			return -1;
		}
		return dir_remove(arg1); // (dirname)

	} else if(command(comm, "cd")) {
		if(numArg < 1) {
			printf("error: cd <dirname>\n");
			return -1;
		}
		return dir_change(arg1); // (dirname)

	} else {
		fprintf(stderr, "%s: command not found.\n", comm);
		return -1;
	}
	return 0;
}

