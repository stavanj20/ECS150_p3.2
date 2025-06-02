#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xffff
/* Copied from Phase 1 */

/* Superblock attributes */
struct __attribute__((packed)) superblock {
    char signature[8];
    uint16_t total_blocks;
    uint16_t root_index;
    uint16_t data_index;
    uint16_t data_blocks;
    uint8_t  fat_blocks;
    uint8_t  unused[4079];
};

/* Root directory attributes */
struct __attribute__((packed)) root {
    char filename[FS_FILENAME_LEN];
    uint32_t size;
    uint16_t data_index;
    uint8_t  unused[10];
};

static struct superblock sb; //state of mounted file
static struct root *root_dir = NULL; //
static uint16_t *FAT = NULL; //FAT table
static int mount = 0; //checks if file has been mounted

/* Mount file system (copied from 3.1) */
int fs_mount(const char *diskname) {
	/* Open virtual disk */
    if(block_disk_open(diskname) == -1)
        return -1;

    /* Read from superblock, return -1 if it cannot be located */
    block_read(0, &sb);
    if(strncmp(sb.signature, "ECS150FS", 8) != 0)
        return -1;

    /* Allocate memory to FAT and read to memory */
    FAT = malloc(sb.fat_blocks*BLOCK_SIZE);
    for(int i = 0; i < sb.fat_blocks; i++)
        block_read(i+1, ((uint8_t*)FAT) + (i*BLOCK_SIZE));

    /* Allocate memory and load root directory */
    root_dir = malloc(BLOCK_SIZE);
    block_read(sb.root_index, root_dir);

	/* File mount successful */
	mount = 1;
    return 0;
}

/* Unmount file system (copied from 3.1) */
int fs_umount(void) {
    /* File mount check */
    if(!mount)
        return -1;

    /* Reset FAT and Root directory */
    free(FAT);
	FAT = NULL;
    free(root_dir);
    root_dir = NULL;

	/* Reset mount and close disk */
    mount = 0;
    if(block_disk_close() == -1)
        return -1;

    return 0;
}

/* Get info on file system (Copied from 3.1) */
int fs_info(void) {
    /* Check if file is mounted */
	if(!mount)
        return -1;
    
    int fat_entries = (sb.fat_blocks*BLOCK_SIZE)/sizeof(uint16_t); //total entries in FAT
    int unused_root_files = 0; //unused files in root directory
    int unused_fat = 0; //unused entries in FAT

    /* count unused FAT entries */
    for(int i = 0; i < fat_entries; i++) {
        if(FAT[i] == 0)
            unused_fat++;
    }

    /* Count unused directory entries */
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if(root_dir[i].filename[0] == '\0')
            unused_root_files++;
    }

    /* Display info */
    printf("FS Info:\n");
    printf("total_blk_count=%u\n", sb.total_blocks);
    printf("fat_blk_count=%u\n", sb.fat_blocks);
    printf("rdir_blk=%u\n", sb.root_index);
    printf("data_blk=%u\n", sb.data_index);
    printf("data_blk_count=%u\n", sb.data_blocks);
    printf("fat_free_ratio=%d/%d\n", unused_fat, fat_entries);
    printf("rdir_free_ratio=%d/%d\n", unused_root_files, FS_FILE_MAX_COUNT);

    return 0;
}

/* Create a new file */
int fs_create(const char *filename)
{
	/* Check for invalid cases */
	if(!mount || !filename || strnlen(filename) >= FS_FILENAME_LEN)
		return -1;

	/* Find first free index and check for duplicate filenames */
	int free_ind = -1;
	int free_found = 0;
	for(int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		/* Check if index is used and has a duplicate filename*/
		if(root_dir[i].unused[0] != '\0') {
			if(strcmp(root_dir[i].filename, filename) == 0)
				return -1;
		}
		/* Find first free index */
		else if(root_dir[i].unused[0] == '\0' && free_found == 0) {
			free_ind = i;
			free_found = 1;
		}
	}

	/* If free index not found, root directory is full */
	if(free_found == 0)
		return -1;

	/* Create filename in directory */
	strncpy(root_dir[free_ind].filename, filename, FS_FILENAME_LEN);
	root_dir[free_ind].data_index = FAT_EOC;
	root_dir[free_ind].size = 0;
	root_dir[free_ind].unused[0] = 1;

	/* Write to disk */
	if(block_write(sb.root_index, root_dir) < 0)
		return -1;

	return 0;
}

int fs_delete(const char *filename)
{
	/* Check for invalid cases */
	if(!mount || !filename || strnlen(filename) >= FS_FILENAME_LEN)
		return -1;

	/* Find file to delete */
	int file_ind = -1;
	for(int i = 0; i < FS_FILENAME_LEN; i++) {
		if(root_dir[i].unused[0] != '\0') {
			if(strcmp(root_dir[i].filename, filename) == 0) {
				file_ind = i;
				break;
			}
		}
	}

	/* File not found */
	if(file_ind == -1)
		return -1;
	
	/* Check if file is open (TODO) */

	/* Free FAT and deallocate memory */
	uint16_t data_block = root_dir[file_ind].data_index;
	uint16_t next = FAT[data_block];
    while (data_block != FAT_EOC) {
        next = FAT[data_block];
        FAT[data_block] = 0;
        data_block = next;
    }
	memset(&root_dir[file_ind], 0, sizeof(struct root));

	/* Update disk */
	if (block_write(sb.root_index, root_dir) < 0)
        return -1;
    for (int i = 0; i < sb.fat_blocks; i++) {
        if (block_write(i + 1, ((uint8_t *)FAT) + (i * BLOCK_SIZE)) < 0)
            return -1;
    }

    return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}
