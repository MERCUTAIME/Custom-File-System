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
 * CSC369 Assignment 1 - File system runtime context implementation.
 */

#include "fs_ctx.h"
#include "a1fs.h"

bool fs_ctx_init(fs_ctx *fs, void *image, size_t size)
{
	fs->image = image;
	fs->size = size;
	fs->bblk = (a1fs_superblock *)image;
	fs->ext = fs->image + (fs->bblk->hz_datablk_head) * A1FS_BLOCK_SIZE;
	fs->tbl = fs->image + fs->bblk->hz_inode_table * A1FS_BLOCK_SIZE;
	fs->err_code = 0;
	fs->path_inode = 0;
	return true;
}

void fs_ctx_destroy(fs_ctx *fs)
{
	//Cleanup any resources allocated in fs_ctx_init()
	fs->image = NULL;
	fs->size = -1;
	fs->bblk = NULL;
	fs->ext = NULL;
	fs->tbl = NULL;
	fs->path_inode = NULL;
	fs->err_code = -1;
	(void)fs;
}
