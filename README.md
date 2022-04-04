# EXT4-File_System

Part A
1. Read:
I’ve implemented read function using two for loops. First for loop for getting
an offset block which means in which block my offset is located. Then as
per next blocks I modified that offset starting from 0. Next for loop I used to
read data from blocks. In which I first checked whether starting point i.e
offset and final size are within the same block or not. If yes I printed that
data using memcpy variable. Then I checked condition for whether offset is
starting from 0 or not. If yes then reading data from 0 offset and if not then I
applied a logic where it will first read data from block where offset is
located and then another condition will read data from consecutive blocks.
Then I concatenated those strings and gave final result. And then freed all
used variables. In read I also checked various conditions like is offset less
than 0, then size plus offset is greater than total size or not and so on.
2. Remove:
In remove I first collected a inode number of a file and blockcount. Then I
iterated one for loop over curDir.numEntry for and checked two conditions
such as inode number is same and name is same or not. If yes then I stored it
into a particular variable n. Then I iterated another for loop for making bits
zero. Next I checked whether link count is equal to 1 or not. If yes I
incremented free inode counts and free block counts and used setbit
function. Next I iterated another for loop for decrementing link counts if file
was hard linked to some other file. After that I’ve take last entry from
curDir.dentry and added it into free space. And then used memset for
adjusting and size of curDir.dentry[curDir.numEntry]. And finally I
decreased a numEntry. Which will accurately ensure that file is properly
deleted. In this function I’ve also checked various conditions such as
whether that is file or not and that file really exist or not.
3. Hard Link:
In hard link first I checked various conditions regarding source and
destination file. Then I created another file with same data and with same
inode. And then increased a link count and number of entry in current
directory


Part B
1. Mkdir
I implemented a make directory is kind of similar to fs_mount and file
create. Where I assigned type, owner , group, size , blockcount and so on.
Then I also added two extra entries which are . and .. . I.e single .
represents a same directory and double .. represents a parent directory.
2. Change directory
For cd <directory_name> Firstly I am collecting an inode for the
directory where I want to go. Then collecting a block from the inode.
For Cd.. First I am navigating to a root directory which I get using inode
0. Starting from the root directory I check if directory exist there and I
also check whether it exist in the subdirectories using the name and inode
number and search_cur_dir function. Once I find the name of the
directory I compared the inode number with that file’s inode number
having the same name. If it matches my search is complete and I am
currently in the parent directory of the directory being searched.
3. Remove directory
I implemented remove in following manner:
Check if the directory is empty using the current directory numenrty.
If the numentry is 2 then the directory is empty then delete the directory.
If th directory is not empty, then check the directory count and consecutively
delete any files and directories that exists in that directory using recursion.
