/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Karen Reid
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019 Karen Reid
 */

/**
 * CSC369 Assignment 1 - a1fs driver implementation.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <math.h>
#define max(a, b) (((a) > (b)) ? (a) : (b))
// Using 2.9.x FUSE API
#define FUSE_USE_VERSION 29
#include <fuse.h>
#include "helper_func_file.c"

#include <time.h>
#include "a1fs.h"
#include "fs_ctx.h"
#include "options.h"
#include "map.h"

//NOTE: All path arguments are absolute paths within the a1fs file system and
// start with a '/' that corresponds to the a1fs root directory.
//
// For example, if a1fs is mounted at "~/my_csc369_repo/a1b/mnt/", the path to a
// file at "~/my_csc369_repo/a1b/mnt/dir/file" (as seen by the OS) will be
// passed to FUSE callbacks as "/dir/file".
//
// Paths to directories (except for the root directory - "/") do not end in a
// trailing '/'. For example, "~/my_csc369_repo/a1b/mnt/dir/" will be passed to
// FUSE callbacks as "/dir".

/**
 * Initialize the file system.
 *
 * Called when the file system is mounted. NOTE: we are not using the FUSE
 * init() callback since it doesn't support returning errors. This function must
 * be called explicitly before fuse_main().
 *
 * @param fs    file system context to initialize.
 * @param opts  command line options.
 * @return      true on success; false on failure.
 */
static bool a1fs_init(fs_ctx *fs, a1fs_opts *opts)
{
	// Nothing to initialize if only printing help
    if (opts->help) return true;

	size_t size;
	void *image = map_file(opts->img_path, A1FS_BLOCK_SIZE, &size);
    if (!image) return false;

	return fs_ctx_init(fs, image, size);
}

/**
 * Cleanup the file system.
 *
 * Called when the file system is unmounted. Must cleanup all the resources
 * created in a1fs_init().
 */
static void a1fs_destroy(void *ctx)
{
    fs_ctx *fs = (fs_ctx*)ctx;
    if (fs->image) {
		munmap(fs->image, fs->size);
		fs_ctx_destroy(fs);
	}
}

/** Get file system context. */
static fs_ctx *get_fs(void)
{
    return (fs_ctx*)fuse_get_context()->private_data;
}

/**
 * Get file system statistics.
 *
 * Implements the statvfs() system call. See "man 2 statvfs" for details.
 * The f_bfree and f_bavail fields should be set to the same value.
 * The f_ffree and f_favail fields should be set to the same value.
 * The following fields can be ignored: f_fsid, f_flag.
 * All remaining fields are required.
 *
 * Errors: none
 *
 * @param path  path to any file in the file system. Can be ignored.
 * @param st    pointer to the struct statvfs that receives the result.
 * @return      0 on success; -errno on error.
 */
static int a1fs_statfs(const char *path, struct statvfs *st)
{
    (void)path;// unused
	fs_ctx *fs = get_fs();

	memset(st, 0, sizeof(*st));
    st->f_bsize = A1FS_BLOCK_SIZE;
    st->f_frsize = A1FS_BLOCK_SIZE;
	st->f_ffree = fs->bblk->num_free_inodes;
	st->f_favail = fs->bblk->num_free_inodes;
	st->f_blocks = fs->size / A1FS_BLOCK_SIZE;
	st->f_bfree = fs->bblk->num_free_blocks;
	st->f_bavail = fs->bblk->num_free_blocks;
	st->f_files = fs->bblk->num_inodes;
	st->f_namemax = A1FS_NAME_MAX;

	return 0;
}

/**
 * Get file or directory attributes.
 *
 * Implements the lstat() system call. See "man 2 lstat" for details.
 * The following fields can be ignored: st_dev, st_ino, st_uid, st_gid, st_rdev,
 *                                      st_blksize, st_atim, st_ctim.
 * All remaining fields are required.
 *
 * NOTE: the st_blocks field is measured in 512-byte units (disk sectors);
 *       it should include any metadata blocks that are allocated to the 
 *       inode.
 *
 * NOTE2: the st_mode field must be set correctly for files and directories.
 *
 * Errors:
 *   ENAMETOOLONG  the path or one of its components is too long.
 *   ENOENT        a component of the path does not exist.
 *   ENOTDIR       a component of the path prefix is not a directory.
 *
 * @param path  path to a file or directory.
 * @param st    pointer to the struct stat that receives the result.
 * @return      0 on success; -errno on error;
 */
static int a1fs_getattr(const char *path, struct stat *st)
{
    if (strlen(path) >= A1FS_PATH_MAX) return -ENAMETOOLONG;
	fs_ctx *fs = get_fs();

	memset(st, 0, sizeof(*st));

	//Clear err_node
	fs->err_code = 0;
	//TODO: lookup the inode for given path and, if it exists, fill in the
	// required fields based on the information stored in the inode

	// a1fs_inode *inode;
	// find_path(path, &inode, fs);
	find_path_inode(path, fs);
	if (fs->err_code == 0)
	{

		//NOTE: all the fields set below are required and must be set according
		// to the information stored in the corresponding inode

		st->st_nlink = fs->path_inode->links;
		st->st_size = fs->path_inode->size;
		st->st_mtim = fs->path_inode->mtime;
		st->st_ino = fs->path_inode->hz_inode_pos;
		st->st_mode = fs->path_inode->mode;
		unsigned int result = 1 + st->st_size / A1FS_BLOCK_SIZE;
		if (st->st_size % A1FS_BLOCK_SIZE == 0)
		{
			result = result - 1;
		}
		result *= A1FS_BLOCK_SIZE;
		st->st_blocks = result / 512;
	}
	return fs->err_code;
}

/**
 * Read a directory.
 *
 * Implements the readdir() system call. Should call filler(buf, name, NULL, 0)
 * for each directory entry. See fuse.h in libfuse source code for details.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a filler() call failed).
 *
 * @param path    path to the directory.
 * @param buf     buffer that receives the result.
 * @param filler  function that needs to be called for each directory entry.
 *                Pass 0 as offset (4th argument). 3rd argument can be NULL.
 * @param offset  unused.
 * @param fi      unused.
 * @return        0 on success; -errno on error.
 */
static int a1fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi)
{
	(void)offset; // unused
	(void)fi;	  // unused
	fs_ctx *fs = get_fs();

	fs->err_code = 0;
	//Lookup the directory inode for given path and iterate through its
	// directory entries

	find_path_inode(path, fs);
	fs->ent = malloc(fs->path_inode->size);
	if (fs->ent == NULL || filler(buf, ".", NULL, 0) || filler(buf, "..", NULL, 0))
		return -ENOMEM;
	find_ent_in_ext(fs->path_inode, fs, "", true);
	//Check whether calling filler generates error
	//Check if error occurs and Update fs->err_code
	check_filler_err(fs, fs->path_inode->size, buf, filler, fs->ent);
	free(fs->ent);
	return fs->err_code;
}

/**
 * Create a directory.
 *
 * Implements the mkdir() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the directory to create.
 * @param mode  file mode bits.
 * @return      0 on success; -errno on error.
 */
static int a1fs_mkdir(const char *path, mode_t mode)
{
	mode = mode | S_IFDIR;
	fs_ctx *fs = get_fs();
	//Clear error_code
	fs->err_code = 0;

	return create_file_dir(fs, path, mode, false);
}

/**
 * Remove a directory.
 *
 * Implements the rmdir() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a directory.
 *
 * Errors:
 *   ENOTEMPTY  the directory is not empty.
 *
 * @param path  path to the directory to remove.
 * @return      0 on success; -errno on error.
 */
static int a1fs_rmdir(const char *path)
{
	fs_ctx *fs = get_fs();
	//TODO: remove the directory at given path (only if it's empty)
	return rm_dir_file(fs, path, true);
}

/**
 * Create a file.
 *
 * Implements the open()/creat() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" doesn't exist.
 *   The parent directory of "path" exists and is a directory.
 *   "path" and its components are not too long.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to create.
 * @param mode  file mode bits.
 * @param fi    unused.
 * @return      0 on success; -errno on error.
 */
static int a1fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void)fi;// unused
	assert(S_ISREG(mode));
	fs_ctx *fs = get_fs();

	//Create a file at given path with given mode
	return create_file_dir(fs, path, mode, true);
}

/**
 * Remove a file.
 *
 * Implements the unlink() system call.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors: none
 *
 * @param path  path to the file to remove.
 * @return      0 on success; -errno on error.
 */
static int a1fs_unlink(const char *path)
{
	fs_ctx *fs = get_fs();
	//remove the file at given path
	return rm_dir_file(fs, path, false);
}

/**
 * Change the modification time of a file or directory.
 *
 * Implements the utimensat() system call. See "man 2 utimensat" for details.
 *
 * NOTE: You only need to implement the setting of modification time (mtime).
 *       Timestamp modifications are not recursive. 
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists.
 *
 * Errors: none
 *
 * @param path   path to the file or directory.
 * @param times  timestamps array. See "man 2 utimensat" for details.
 * @return       0 on success; -errno on failure.
 */
static int a1fs_utimens(const char *path, const struct timespec times[2])
{
	fs_ctx *fs = get_fs();

	//TODO: update the modification timestamp (mtime) in the inode for given
	// path with either the time passed as argument or the current time,
	// according to the utimensat man page

	//Clear error code
	fs->err_code = 0;

	find_path_inode(path, fs);
	const struct timespec last_time = times[1];
	if (last_time.tv_nsec != UTIME_NOW)
		fs->path_inode->mtime = last_time;
	else
	{
		clock_gettime(CLOCK_REALTIME, &(fs->path_inode->mtime));
	}

	return 0;
}

/**
 * Change the size of a file.
 *
 * Implements the truncate() system call. Supports both extending and shrinking.
 * If the file is extended, the new uninitialized range at the end must be
 * filled with zeros.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *
 * @param path  path to the file to set the size.
 * @param size  new file size in bytes.
 * @return      0 on success; -errno on error.
 */
static int a1fs_truncate(const char *path, off_t size)
{
	fs_ctx *fs = get_fs();

	//Clear err_node
	fs->err_code = 0;
	find_path_inode(path, fs);
	unsigned int unsign_size = (unsigned int)size;
	if (unsign_size == fs->path_inode->size)
	{
		return 0;
	}
	check_byte(fs, size, unsign_size - fs->path_inode->size);
	if (fs->err_code == 0 && unsign_size < fs->path_inode->size)
	{

		blk_deallocation(fs, size);
	}

	return fs->err_code;
}

/**
 * Read data from a file.
 *
 * Implements the pread() system call. Must return exactly the number of bytes
 * requested except on EOF (end of file). Reads from file ranges that have not
 * been written to must return ranges filled with zeros. You can assume that the
 * byte range from offset to offset + size is contained within a single block.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors: none
 *
 * @param path    path to the file to read from.
 * @param buf     pointer to the buffer that receives the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to read from.
 * @param fi      unused.
 * @return        number of bytes read on success; 0 if offset is beyond EOF;
 *                -errno on error.
 */
static int a1fs_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
	(void)fi;// unused
	fs_ctx *fs = get_fs();

	//Clear Error code
	fs->err_code = 0;

	find_path_inode(path, fs);
	unsigned int inode_size = fs->path_inode->size;
	if (inode_size != 0 && inode_size <= offset)
	{
		return read_write_IO(true, fs, buf, size, offset);
	}

	return 0;
}

/**
 * Write data to a file.
 *
 * Implements the pwrite() system call. Must return exactly the number of bytes
 * requested except on error. If the offset is beyond EOF (end of file), the
 * file must be extended. If the write creates a "hole" of uninitialized data,
 * the new uninitialized range must filled with zeros. You can assume that the
 * byte range from offset to offset + size is contained within a single block.
 *
 * Assumptions (already verified by FUSE using getattr() calls):
 *   "path" exists and is a file.
 *
 * Errors:
 *   ENOMEM  not enough memory (e.g. a malloc() call failed).
 *   ENOSPC  not enough free space in the file system.
 *   ENOSPC  too many extents (a1fs only needs to support 512 extents per file)
 *
 * @param path    path to the file to write to.
 * @param buf     pointer to the buffer containing the data.
 * @param size    buffer size (number of bytes requested).
 * @param offset  offset from the beginning of the file to write to.
 * @param fi      unused.
 * @return        number of bytes written on success; -errno on error.
 */
static int a1fs_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
	(void)fi;// unused
	fs_ctx *fs = get_fs();
	fs->err_code = 0;
	//Check if size is empty
	if (size == 0)
		return 0;
	//Find correponding path inode
	find_path_inode(path, fs);

	if (fs->path_inode->hz_extent_p == -1)
	{
		load_datablock(fs->path_inode, 0, fs);
	}
	if (fs->err_code == 0)
	{

		check_byte(fs, get_num_byte(fs, offset), offset - fs->path_inode->size);
		if (fs->err_code == 0)
		{
			if (get_num_byte(fs, offset + size) > 0)
			{
				byte_addition(fs, fs->path_inode, (int)size);
			}
		}
		if (fs->err_code == 0)
		{
			read_write_IO(false, fs, (char *)buf, size, offset);
			return size;
		}
	}
	return fs->err_code;
}

static struct fuse_operations a1fs_ops = {
	.destroy  = a1fs_destroy,
	.statfs   = a1fs_statfs,
	.getattr  = a1fs_getattr,
	.readdir  = a1fs_readdir,
	.mkdir    = a1fs_mkdir,
	.rmdir    = a1fs_rmdir,
	.create   = a1fs_create,
	.unlink   = a1fs_unlink,
	.utimens  = a1fs_utimens,
	.truncate = a1fs_truncate,
	.read     = a1fs_read,
	.write    = a1fs_write,
};

int main(int argc, char *argv[])
{
	a1fs_opts opts = {0};// defaults are all 0
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (!a1fs_opt_parse(&args, &opts)) return 1;

	fs_ctx fs = {0};
	if (!a1fs_init(&fs, &opts)) {
		fprintf(stderr, "Failed to mount the file system\n");
		return 1;
	}

	return fuse_main(args.argc, args.argv, &a1fs_ops, &fs);
}
