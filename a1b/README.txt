Our runit.sh does not take any arguments. To run it, use
"./runit.sh" or "bash runit.sh". It will prompt you to enter the
mount point. Enter the path to a existing directory to mount
the image.

In runit.sh, we tested that
- mkfs.a1fs works on the sample list of configurations from the
 a1b handout

- mkfs.a1fs fails gracefully with a too-small disk image (e.g. 16 KB,
 with any number of inodes)

- creating a directory, displaying the contents of a directory, 
creating a file, adding data to a file all works

- Truncate to both extend and shrink a file works

- Using 'touch' to update the timestamp on a file works

- Creating a file that fills the file system by appending data 
to the file returns error.

- After unmounting and remounting the file system, data is 
still there and we can create files and write to them again. 

One thing that our code does not do well is that when a file 
is filling the file system, we get "write error: Transport endpoint
is not connected" as error message instead of ENOSPC. 
