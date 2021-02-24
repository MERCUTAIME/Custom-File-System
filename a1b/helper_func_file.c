#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

#include <fuse.h>
#include <errno.h>
#include "a1fs.h"
#include "fs_ctx.h"
#include "options.h"
#include "map.h"
//Helper for mkfs.c
a1fs_inode *get_node(fs_ctx *fs, int pos)
{
    return &(fs->tbl[pos]);
}
unsigned int mkfs_helper(unsigned int a, unsigned int b)
{
    unsigned int result = 1 + b / a;
    if (b % a == 0)
    {
        result = result - 1;
    }
    return result;
}

int check_blk_err(int blk)
{

    if (blk >= 3)
    {
        blk += 1;
    }
    else
    {
        blk = -1;
    }
    return blk;
}

void dr_arr(unsigned int arr_bitmap[])
{
    for (int i = 0; i < 2; i++)
    {
        unsigned int ratio = 1 + arr_bitmap[2] / arr_bitmap[i];
        if ((arr_bitmap[2] % arr_bitmap[i]) == 0)
            arr_bitmap[i] = ratio - 1;
        else
        {
            arr_bitmap[i] = ratio;
        }
    }
}
void arr_construct(a1fs_blk_t data, void *image, unsigned int bmp, bool is_imap)
{
    unsigned char *arr = data * A1FS_BLOCK_SIZE + image;
    memset(arr, 0, bmp * A1FS_BLOCK_SIZE);
    if (is_imap)
    {
        arr[0] = 1 << 7;
    }
}

void init_root_dir(const struct a1fs_superblock *bblk, void *image, unsigned int third, unsigned int second)
{

    a1fs_inode *head_node = image + bblk->hz_inode_table * A1FS_BLOCK_SIZE;
    arr_construct(bblk->hz_bitmap_data, image, third, false);
    arr_construct(bblk->hz_bitmap_inode, image, second, true);

    head_node->mode = S_IFDIR | 0777;
    clock_gettime(CLOCK_REALTIME, &(head_node->mtime));
    head_node->hz_inode_pos = 0;
    head_node->hz_extent_size = 0;
    head_node->size = 0;
    head_node->links = 2;
    head_node->hz_extent_p = -1;
}

a1fs_dentry *get_ext_info(bool is_blk, fs_ctx *fs, int node)
{
    a1fs_dentry *result = fs->image + (node + fs->bblk->hz_datablk_head) * A1FS_BLOCK_SIZE;
    if (!is_blk)
    {
        fs->ext = (void *)result;
    }
    return result;
}

/* Loop over db in extent*/
a1fs_dentry *find_entry(a1fs_dentry *ent, size_t ent_blk_count, fs_ctx *fs, a1fs_dentry *head_blk, char *name)
{
    for (unsigned int d = 0; d < ent_blk_count / sizeof(a1fs_dentry); d++)
    {
        if (!strcmp(head_blk[d].name, name))
        {
            fs->err_code = 0;
            ent = &head_blk[d];
            break;
        }
    }
    return ent;
}

a1fs_dentry *blk_allocation(unsigned int a, unsigned int m, uint16_t ext_size, a1fs_dentry *ent, a1fs_inode *dir, fs_ctx *fs, char *name, bool allocate)
{
    size_t length = fs->ext[m].start + fs->ext[m].count;
    size_t ent_blk_count = A1FS_BLOCK_SIZE;
    a1fs_dentry *head_blk = get_ext_info(true, fs, a);
    if (a == (length - 1))
    {
        if (m + 1 == (ext_size))
            ent_blk_count = dir->size % ent_blk_count;
    }
    if (allocate)
    {
        memcpy((void *)ent, (void *)head_blk, ent_blk_count);
        ent += A1FS_BLOCK_SIZE;
    }
    else
    {
        ent = find_entry(ent, ent_blk_count, fs, head_blk, name);
    }
    return ent;
}
a1fs_dentry *loop_db(a1fs_dentry *ent, a1fs_blk_t length, int start, fs_ctx *fs, unsigned int m, a1fs_inode *dir, bool allocate, char *name)
{
    unsigned int a = start;

    while (a < length && fs->err_code == PROCESS)
    {
        size_t ent_blk_count = A1FS_BLOCK_SIZE;
        a1fs_dentry *head_blk = get_ext_info(true, fs, a);
        if (m + 1 == (dir->hz_extent_size) && a == (length - 1))
        {
            ent_blk_count = dir->size % ent_blk_count;
        }
        if (allocate)
        {
            memcpy((void *)ent, (const void *)head_blk, ent_blk_count);
            ent += A1FS_BLOCK_SIZE;
        }
        else
        {
            ent = find_entry(ent, ent_blk_count, fs, head_blk, name);
        }
        a++;
    }
    return ent;
}

char *init_path(const char *path, bool is_first)
{
    char new_p[A1FS_PATH_MAX];
    if (is_first)
    {
        strncpy(new_p, path, A1FS_PATH_MAX);
        new_p[A1FS_PATH_MAX - 1] = '\0';
        return strtok(new_p, "/");
    }
    else
    {
        return strtok(NULL, "/");
    }
}
void load_inode(a1fs_dentry *ent, int *pos, fs_ctx *fs)
{
    if (ent != NULL && fs->err_code == 0)
    {
        *pos = ent->ino;
        fs->err_code = 0;
    }
}
a1fs_dentry *find_ent_inext(a1fs_dentry *ent, a1fs_inode *dir, fs_ctx *fs, char *name, bool allocate)
{
    //Update fs->ext to point to the head of the extent array
    get_ext_info(false, fs, dir->hz_extent_p);
    //loop through extents
    unsigned int m = 0;
    uint16_t ext_size = dir->hz_extent_size;
    if (fs->err_code == 0)
    {
        fs->err_code = PROCESS;
    }
    while (m < ext_size && fs->err_code == PROCESS)
    {
        size_t length = fs->ext[m].start + fs->ext[m].count;
        unsigned int a = fs->ext[m].start;
        a1fs_dentry *k = loop_db(ent, length, a, fs, m, dir, allocate, name);
        if (!allocate)
        {
            ent = k;
        }
        m++;
    }

    if (!allocate && fs->err_code != 0)
    {
        fs->err_code = -ENOENT;
        ent = NULL;
    }
    else
    {
        fs->err_code = 0;
    }

    return ent;
}

void dig_dir(fs_ctx *fs, char *temp_path, int pos, const char *path)
{
    while (temp_path != NULL && fs->err_code == 0)
    {
        a1fs_inode *directory = get_node(fs, pos);
        if ((directory->mode & S_IFDIR) != S_IFDIR)
        {
            fs->err_code = -ENOTDIR;
            break;
        }

        a1fs_dentry *ent = 0;
        ent = find_ent_inext(ent, get_node(fs, pos), fs, temp_path, false);
        load_inode(ent, &pos, fs);
        temp_path = init_path(path, false);
    }
}

int find_path(const char *path, a1fs_inode **target, fs_ctx *fs)
{
    int pos = 0;
    if (path[0] == '/')
    {
        char *name = init_path(path, true);
        while (name != NULL)
        {
            // Check if the directory exists.
            if ((get_node(fs, pos)->mode & S_IFDIR) != S_IFDIR)
            {
                fs->err_code = -ENOTDIR;
                break;
            }
            a1fs_dentry *entry = 0;
            entry = find_ent_inext(entry, get_node(fs, pos), fs, name, false);
            load_inode(entry, &pos, fs);
            name = init_path(path, false);
        }
        if (fs->err_code == 0)
        {
            *target = get_node(fs, pos);
        }
    }
    else
    {
        //Not an absolute path
        fs->err_code = -ENASDIR;
    }
    return fs->err_code;
}
void check_filler_err(fs_ctx *fs, size_t size, void *buf, fuse_fill_dir_t filler, a1fs_dentry *entries)
{
    unsigned int b = 0;
    //Failed at calling filler
    while (b < (size / sizeof(a1fs_dentry)))
    {
        if (filler(buf, entries[b].name, NULL, 0) == 1)
        {
            fs->err_code = -ENOMEM;
            break;
        }

        b++;
    }
}