#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

//Helper for mkfs.c
// void arr_construct(a1fs_blk_t data, void *image, unsigned int bmp, bool is_imap)
// {
//     unsigned char *arr = data * A1FS_BLOCK_SIZE + image;
//     memset(arr, 0, bmp * A1FS_BLOCK_SIZE);
//     if (is_imap)
//     {
//         arr[0] = 1 << 7;
//     }
// }
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
void init_root_dir(const struct a1fs_superblock *bblk, void *image, unsigned int third, unsigned int second)
{
    a1fs_inode *head_node = image + bblk->hz_inode_table * A1FS_BLOCK_SIZE;

    head_node->mode = S_IFDIR;
    clock_gettime(CLOCK_REALTIME, &(head_node->mtime));
    head_node->hz_inode_pos = 0;
    head_node->hz_extent_size = 0;
    head_node->size = 0;
    head_node->links = 2;
    head_node->hz_extent_p = -1;
}
