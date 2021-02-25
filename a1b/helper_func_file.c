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
#include <libgen.h>
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

/**Init root dir Based on the given information. **/
void init_dir(a1fs_inode *head_node, mode_t mode, int pos)
{
    //If init root dir,mode default is S_IFDIR | 0777;
    head_node->mode = mode;
    head_node->size = 0;
    //inode pos should be 0 if it's root dir
    head_node->hz_inode_pos = pos;
    head_node->hz_extent_size = 0;
    clock_gettime(CLOCK_REALTIME, &(head_node->mtime));
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

/**Find correponding inode given path in fs
 * The inode will be stored in fs->path_node
 * 
*/
int find_path_inode(const char *path, fs_ctx *fs)
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
            fs->path_inode = get_node(fs, pos);
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

int update_ext(bool success, a1fs_extent *ext, int head, unsigned int sum, unsigned int total_l, int flag)
{

    if ((success && total_l == sum) || (!success && ext->count < sum))
    {
        ext->start = head;
        ext->count = sum;
        flag = 0;
    }

    return flag;
}
/**
 * Loop over each bit to find any free inode
 */
int find_free_inode(a1fs_extent *ext, int bit, unsigned char curr, int used_bit, int head, unsigned int count, unsigned int total_l)
{
    int k = 0;
    int flag = PROCESS;
    while (k < bit && flag == PROCESS)
    {
        int left_shift = 1 << (7 - k);
        if (curr & left_shift)
        {
            update_ext(false, ext, head, count, total_l, flag);
            flag = PROCESS;
            count = 0;
        }
        else
        {
            head = (count == 0) ? used_bit + k : head;
            count++;
            flag = update_ext(true, ext, head, count, total_l, flag);
        }
        k++;
    }
    return flag;
}

void update_bitmap(bool deallocate, int bit_num, unsigned char *bitmap)
{
    unsigned char temp = (1 << (7 - bit_num % 8));
    if (deallocate)
    {
        temp = ~temp;
        bitmap[bit_num / 8] = bitmap[bit_num / 8] & temp;
    }
    else
    {
        bitmap[bit_num / 8] = bitmap[bit_num / 8] | temp;
    }
}
/**
 * Update free bit in free blocks and free inodes
 *  
 */
a1fs_blk_t update_free_bit(bool deallocate, bool is_dir, fs_ctx *fs)
{
    int offset = -1;
    if (deallocate)
    {
        offset = 1;
    }
    if (!is_dir)
    {
        fs->bblk->num_free_inodes += offset;
        return fs->bblk->hz_bitmap_inode;
    }
    else
    {
        fs->bblk->num_free_blocks += offset;
        return fs->bblk->hz_bitmap_data;
    }
} /**
 * Return Inode based on the inode number and update fs->tbl
 *  
 */
a1fs_inode *cal_inode(fs_ctx *fs, int pos)
{
    a1fs_inode *result = pos * sizeof(a1fs_inode) + fs->image + fs->bblk->hz_inode_table * A1FS_BLOCK_SIZE;
    return result;
}
/**Check are there any free space available for allocation**/
void check_free_space(fs_ctx *fs, int blk_count)
{

    int free_blk = (int)fs->bblk->num_free_blocks;
    int temp = A1FS_BLOCK_SIZE / sizeof(a1fs_extent);
    if (fs->path_inode->hz_extent_size / temp)
        fs->err_code = -ENOSPC;
    if (blk_count > free_blk || free_blk == 0)
        fs->err_code = -ENOSPC;
}

/**
 * Given length,find extent in the bitmap
 * Store any error in fs->error code
 * **/
void find_ext_in_bitmap(fs_ctx *fs, bool blk, unsigned int total_l, a1fs_extent *extent)
{
    unsigned char *bitmap;
    int num;
    if (blk)
    {
        num = fs->bblk->num_free_blocks;
        bitmap = fs->bitmp_data;
    }
    else
    {
        num = fs->bblk->num_inodes;
        bitmap = fs->bitmp_inode;
    }

    unsigned int start = 0;
    unsigned int count = 0;

    int ub = 0;
    int bit = 0;

    for (int i = 0; i < num / 8 + 1; i++)
    {
        bit = num - ub;
        bit = (bit >= 8) ? 8 : bit;
        fs->err_code = find_free_inode(extent, bit, bitmap[i], ub, start, count, total_l);
        if (fs->err_code == 0)
            break;

        ub += bit;
    }
    if (extent->count == 0)
        fs->err_code = -ENOSPC;
}

/**Switching bit to allocate/deallocate bit*/
void switch_bit(fs_ctx *fs, bool is_dir, int bit_number, bool deallocate)
{
    unsigned char *bitmap = fs->image + update_free_bit(deallocate, is_dir, fs) * A1FS_BLOCK_SIZE;
    update_bitmap(deallocate, bit_number, bitmap);
}

/**Init extent when creating dir/file**/
int init_ext(fs_ctx *fs, a1fs_extent *ext, unsigned int length, int blk_count, int size, bool init_ext)
{
    int result = 0;
    //Init extent
    if (!init_ext || (init_ext && size == -1))
    {
        ext->count = 0;
        find_ext_in_bitmap(fs, true, blk_count, ext);
        unsigned int c = 0;
        while (c < length)
        {
            unsigned char *bitmap = fs->image + update_free_bit(false, true, fs) * A1FS_BLOCK_SIZE;
            update_bitmap(false, c + ext->start, bitmap);
            if (!init_ext)
                memset(get_ext_info(true, fs, c + ext->start), 0, A1FS_BLOCK_SIZE);

            c++;
        }
        if (!init_ext)
        {
            fs->ext[size] = *ext;
            result = size + 1;
        }
        else
        {
            result = ext->start;
        }
    }
    return result;
}
/**Data Block Allocation**/
int load_datablock(a1fs_inode *inode, int blk_count, fs_ctx *fs)
{
    int free_blk = (int)fs->bblk->num_free_blocks;
    if (blk_count > free_blk || free_blk == 0)
        return -ENOSPC;
    if (blk_count > (int)fs->bblk->num_free_blocks || inode->hz_extent_size == A1FS_BLOCK_SIZE / sizeof(a1fs_extent))
    {
        return -ENOSPC;
    }

    a1fs_extent ext;
    if (inode->hz_extent_p == -1)
    {
        (&ext)->count = 0;
        find_ext_in_bitmap(fs, true, 1, &ext);
        unsigned char *bitmap = fs->image + update_free_bit(false, true, fs) * A1FS_BLOCK_SIZE;
        update_bitmap(false, ext.start, bitmap);
        inode->hz_extent_p = ext.start;
    }
    get_ext_info(false, fs, inode->hz_extent_p);

    if (blk_count > 0)
    {
        inode->hz_extent_size = init_ext(fs, &ext, blk_count, ext.count, inode->hz_extent_size, false);
    }

    return 0;
}

// Get the head of the inode
void *point_to_head(a1fs_inode *inode, fs_ctx *fs)
{
    get_ext_info(false, fs, inode->hz_extent_p);
    a1fs_extent ext_last = fs->ext[inode->hz_extent_size - 1];
    return (void *)get_ext_info(true, fs, ext_last.start + ext_last.count - 1) + inode->size % A1FS_BLOCK_SIZE;
}

/**
 * Create a directory or a file
 * 
*/
int create_file_dir(fs_ctx *fs, const char *path, mode_t mode, bool is_file)
{ //Clear err_node
    fs->err_code = 0;

    a1fs_extent extent;
    (&extent)->count = 0;
    // Find extent
    find_ext_in_bitmap(fs, false, 1, &extent);

    if (fs->err_code == 0)
    {
        unsigned char *bitmap = fs->image + update_free_bit(false, false, fs) * A1FS_BLOCK_SIZE;
        update_bitmap(false, extent.start, bitmap);

        //Calculate inode
        a1fs_inode *node = extent.start * sizeof(a1fs_inode) + fs->image + fs->bblk->hz_inode_table * A1FS_BLOCK_SIZE;
        init_dir(node, mode, extent.start);

        //The only different between file and dir is the numeber of link.
        if (is_file)
            node->links = 1;

        //Extract file name and prefix path
        char file[A1FS_NAME_MAX];
        char prefix[A1FS_PATH_MAX];
        strcpy(prefix, path);
        strncpy(file, basename(prefix), A1FS_NAME_MAX);
        strncpy(prefix, dirname(prefix), A1FS_PATH_MAX);
        //Find corresponding inode
        find_path_inode((const char *)(prefix), fs);
        //Check whether dir is full
        int dir_size = fs->path_inode->size;
        if (dir_size % A1FS_BLOCK_SIZE == 0)
        {
            check_free_space(fs, 1);
            if (fs->err_code == -ENOSPC)
            {
                return fs->err_code;
            }
            load_datablock(fs->path_inode, 1, fs);
        }
        a1fs_dentry *ent = (a1fs_dentry *)(point_to_head(fs->path_inode, fs));
        ent->ino = node->hz_inode_pos;
        //Update entry info;
        fs->path_inode->links += ((node->mode & S_IFDIR) == S_IFDIR) ? 1 : 0;
        strncpy(ent->name, file, A1FS_NAME_MAX);
        fs->path_inode->size += sizeof(a1fs_dentry);
    }

    return fs->err_code;
}