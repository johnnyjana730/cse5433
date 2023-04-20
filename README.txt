First, we designed the layout of our file system. And create the lab5fs moudule with the write_inode, read_inode, get_sb, fill_super functions so that we can register the lab5fs and mount out file system correctly. After that we create the mklab5fs that writes the initial meta-data to the disk. Beside writing the superblock to the image, we also writeh the root directory to the disk image. However, the parent directory of our file system is not actually stored on the disk. They are generated as needed during runtime in our implementation of readdir(). Our file system actually takes a different approach to directory entries by put the filename inside the inode structure. So we did not use a dentry structure to store the filename. When the ls command is called, the file system will loop through the inodes to find all the filename. The result comes out to be same as other file systems that use an extra dentry structure. 

After we could successfully displaying the files, we implement the touch, ln, and rm for our file system. Because we didn't use the dentrey structure, our implementation has a little difference with other file system like BFS. In the lookup() and create(), the opreration is based on inodes. So lookup will look through all the inode and compare the filename we stored in inode. And create will directly create the inode in the first open place according to the inode bitmap. For ln, since we don't use dentry struct, we uses a is_hard_link flag to mark the hard linked inode and a data block address variable to store the data block address. 

For file read and write, to simplify the implementation, we decided to allocate 15 data blocks for each file at once. Since we have a fixed blocks number for each inode, we could use the inode number to find the corresponding block address and get the block. By this way, we achieved the read and write of files.


Limitations and bugs:

File has fixed max size
File blocks allocation is not fragmented


Layout:

|| sb || Inode bitmap || Data bitmap || Inodes || Data blocks ||

block size: 512 byte
inode size: 64 byte
inode count: 1000
inode blocks: 125 blocks
inode bitmap: 2 blocks
data blocks: 2048 blocks
data bitmap: 4 blocks
file count: 1000
file max size: 512*15 byte
