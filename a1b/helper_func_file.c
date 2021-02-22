#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

#include <errno.h>
#include "a1fs.h"
#include "fs_ctx.h"
#include "options.h"
#include "map.h"
//Helper for mkfs.c

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

a1fs_inode *get_node(fs_ctx *fs, int pos)
{
    return &(fs->tbl[pos]);
}
a1fs_dentry *get_ext_info(bool is_blk, fs_ctx *fs, int node)
{
    a1fs_dentry *result = fs->image;
    size_t s = (node + fs->bblk->hz_datablk_head) * A1FS_BLOCK_SIZE;
    if (is_blk)
    {
        result += s;
    }
    else
    {
        fs->ext = (void *)result + s;
    }
    return result;
}

/* Loop over db in extent*/
void loop_datablock_ext(a1fs_blk_t length, int start, fs_ctx *fs, unsigned int index, a1fs_inode *dir, bool readdir, char *name)
{
    unsigned int a = start;
    while (a != length - 1)
    {
        a1fs_dentry *curr = get_ext_info(true, fs, a);
        size_t data_size = A1FS_BLOCK_SIZE;
        if (a == length)
        {
            if (index + 1 == dir->hz_extent_size)
            {
                data_size = dir->size % A1FS_BLOCK_SIZE;
            }
        }
        if (readdir)
        {
            memcpy((void *)fs->ent, (void *)curr, data_size);
            fs->ent += A1FS_BLOCK_SIZE;
        }
        else
        {
            curr = (a1fs_dentry *)curr;
            int temp = data_size / sizeof(a1fs_dentry);
            for (int c = 0; c < temp; c++)
            {
                if (strcmp(curr[c].name, name) == 0)
                {
                    fs->ent = &curr[c];
                }
            }
        }

        a++;
    }
    if (readdir)
    {
        fs->err_code = 0;
    }
    else
    {
        fs->err_code = -ENODATA;
    }
}

void cal_ent(a1fs_inode *dir, fs_ctx *fs, bool is_readdir, char *name)
{
    fs->ent = malloc(dir->size);
    if (fs->ent != NULL || !is_readdir)
    {

        get_ext_info(false, fs, dir->hz_extent_p);
        unsigned int m = 0;
        uint16_t ext_size = dir->hz_extent_size;
        while (m < ext_size)
        {
            a1fs_extent extent = fs->ext[m];
            loop_datablock_ext(extent.start + extent.count + 1, extent.start, fs, m, dir, is_readdir, name);
            m++;
        }
    }
}
void *get_block(int block_number, fs_ctx *fs)
{
    return fs->image + (fs->bblk->hz_datablk_head + block_number) * A1FS_BLOCK_SIZE;
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
void load_inode(int *pos, fs_ctx *fs)
{
    if (fs->err_code != 0 && fs->ent != NULL)
    {
        *pos = fs->ent->ino;
        fs->err_code = 0;
    }
    else
    {
        fs->err_code = -ENOTDIR;
    }
}
void dig_dir(fs_ctx *fs, char *temp_path, int pos, const char *path)
{

    while (temp_path != NULL && fs->err_code == 0)
    {
        fs->err_code = (get_node(fs, pos)->mode & S_IFDIR) != S_IFDIR;
        cal_ent(get_node(fs, pos), fs, false, temp_path);
        load_inode(&pos, fs);
        temp_path = init_path(path, false);
    }
}

int find_path(fs_ctx *fs, a1fs_inode **target, const char *p)
{
    if (p[0] == '/')
    {
        int pos = 0;
        char *temp_path = init_path(p, true);
        dig_dir(fs, temp_path, pos, p);
        if (fs->err_code != 0)
            fs->err_code = -ENOTDIR;
        else
        {
            *target = get_node(fs, pos);
        }
    }
    else
    {
        fprintf(stderr, "Not an absolute path\n");
        fs->err_code = -ENASDIR;
    }
    return fs->err_code;
}

