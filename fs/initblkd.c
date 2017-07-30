/*
 * Copyright (c) 2010, Stefan Lankes, RWTH Aachen University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the University nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hermit/stdlib.h>
#include <hermit/stdio.h>
#include <hermit/string.h>
#include <hermit/errno.h>
#include <hermit/spinlock.h>
#include <hermit/logging.h>
#include <asm/processor.h>
#include <hermit/fs.h>

static vfs_node_t initblkd_root;
int uhyve_blk_init_ok;
typedef struct {
	uint32_t length;
	uint32_t offset;
	char fname[MAX_FNAME];
} initblkd_file_desc_t;

static size_t initblkd_read(fildes_t* file, uint8_t* buffer, size_t size)
{
	vfs_node_t* node = file->node;

	uint32_t i, pos = 0, found = 0, data_read = 0;
	off_t offset = 0;
	size_t tmp_sector = 0, new_sector = 0;
	int ns = 0;
	uint8_t* sector_buffer;
	sector_buffer = (uint8_t*) kmalloc(sector_size);
	block_list_t* blist = &node->block_list;
	uint8_t data[sector_size]; 
	memset(data, 0x00, sector_size);

	if (file->flags & O_WRONLY)
		return -EACCES;

	/* init the tmp offset */
	offset = file->offset;
	/* searching for the valid data block */
	if (offset) {
		pos = offset / sector_size;
		offset = offset % sector_size;
	}
	do {
		int i = 0;
		for(i=0; i<MAX_DATABLOCKS && !data_read; i++) {
			if ((size + offset) >= sector_size)
				size = sector_size - offset;
			if (blist->data[i]) {
				found++;
				if (found > pos) {
					hermit_blk_read_sync(blist->data[i], data, &sector_size);
					data_read = 1;
				}
			}
		}
		if (!blist->end) {
                hermit_blk_read_sync(blist->next_sector, sector_buffer, &sector_size);
                blist = sector_buffer + sizeof(block_list_t) * blist->next;
		} else {
			break;
		}
	} while(!data_read);

	/*
	 * If the data block is not large engough,
	 * we copy only the rest of the current block.
	 * The user has to restart the read operation
	 * for the next block.
	 */
	if ((offset + size) >= node->block_size)
		size = node->block_size - offset;

	memcpy(buffer, data + offset, size);

	file->offset += size;
	return size;
}

static size_t initblkd_emu_readdir(fildes_t* file, uint8_t* buffer, size_t size)
{
	vfs_node_t* node;
        node = kmalloc(size_vn);
        if (BUILTIN_EXPECT(!node, 0))
                return NULL;
        hermit_blk_read_sync(file->sector, node, &size_vn);

        if (BUILTIN_EXPECT(!node, 0))
                return NULL;

	uint32_t i, j, k, count;
	uint32_t index = file->offset;
        dir_block_t* tmp_db;
        tmp_db = (dir_block_t*) kmalloc(sizeof(dir_block_t));
        dirent_t dirent;
	size_t tmp_sector = 0, new_sector = 0;
	int ns = 0;
	uint8_t* sector_buffer;
	sector_buffer = (uint8_t*) kmalloc(sector_size);
	block_list_t* blist = &node->block_list;

	do {
		for(i=0,count=0; i<MAX_DATABLOCKS; i++) {
                        dirent.vfs_node = 0;
                        if(blist->data[i]) {
                                hermit_blk_read_sync(blist->data[i], tmp_db, &size_db);
				for(j=0; j<max_direntries; j++) {
					dirent = tmp_db->entries[j];
					if (dirent.vfs_node) {
						count++;
						if (count > index) {
							k=0;
							do {
								buffer[k] = dirent.name[k];
								k++;
							} while(dirent.name[k] != '\0');
							file->offset++;
							return k;
						}
					}
				}
			}
		}

		if (!blist->end) {
                hermit_blk_read_sync(blist->next_sector, sector_buffer, &sector_size);
                blist = sector_buffer + sizeof(block_list_t) * blist->next;
		} else {
			break;
		}
	} while(1);

	return -EINVAL;
}

static size_t initblkd_write(fildes_t* file, uint8_t* buffer, size_t size)
{
	uint32_t i, pos = 0, found = 0, data_writen = 0, tmp_next = 0;
	off_t offset = 0;
	vfs_node_t* node = file->node;
	size_t tmp_sector = 0, new_sector = 0;
	int ns = 0;
	uint8_t* sector_buffer;
	sector_buffer = (uint8_t*) kmalloc(sector_size);
	block_list_t* blist = &node->block_list;

	if (file->flags & O_RDONLY)
		return -EACCES;

	if (file->flags & O_APPEND)
		file->offset = node->block_size;

	/* init the tmp offset */
	offset = file->offset;
	/* searching for the valid data block */
	if (offset) {
		pos = offset / sector_size;
		offset = offset % sector_size;
	}
        char* tmp_data_block;
        tmp_data_block = (char*) kmalloc(sector_size);
	if (BUILTIN_EXPECT(!tmp_data_block, 0)) {
        	return -EFAULT;
	}
	do {
		for (i = 0; i < MAX_DATABLOCKS && !data_writen; i++) {
			if ((size + offset) >= sector_size)
				size = sector_size - offset;
			if(!blist->data[i]) {
				memcpy(tmp_data_block + offset, buffer, size);
                                blist->data[i] = initblkd_find_sector();
                                initblkd_set_sector(blist->data[i], 1);
				if (blist->sector) {
					if (i == MAX_DATABLOCKS -1) {
						blist->next++;
						if (sector_size == (blist->next + 1) * sizeof(block_list_t) ||
						    sector_size < (blist->next + 1) * sizeof(block_list_t)) {
							blist->next_sector = 0;
							blist->next = 0;
						}
					}
					hermit_blk_write_sync(blist->sector, sector_buffer, sector_size);
				}
                                hermit_blk_write_sync(blist->data[i], tmp_data_block, sector_size);
				data_writen = 1;
				goto out;
			}
			else if (blist->data[i]) {
				found++;
				if (found > pos) {
					hermit_blk_read_sync(blist->data[i], tmp_data_block, &sector_size);
					memcpy(tmp_data_block + offset, buffer, size);
					hermit_blk_write_sync(blist->data[i], tmp_data_block, sector_size);
					data_writen = 1;
					goto out;
				}
			}
		}
		/* if all blocks are reserved, we have  to allocate a new one */
		if (!blist->next_sector) {
			new_sector = initblkd_find_sector();
		        initblkd_set_sector(new_sector, 1);
			blist->next_sector = new_sector;
			if (tmp_sector) {
				hermit_blk_write_sync(tmp_sector, sector_buffer, sector_size);
			}
			memset(sector_buffer, 0x00, sector_size);
		} else {
			hermit_blk_read_sync(blist->next_sector, sector_buffer, &sector_size);
			tmp_sector = blist->next_sector;
			tmp_next = blist->next;

		}
		block_list_t* old_bl = blist;
		blist = sector_buffer + sizeof(block_list_t) * blist->next;
		if (blist->data[0] && !new_sector) {
			continue;
		}
		old_bl->end = 0;
		block_list_t* new_bl;
		new_bl = (block_list_t*) kmalloc(sizeof(block_list_t));
		memset(new_bl, 0x00, sizeof(block_list_t));
		if (new_sector) {
			new_bl->sector = new_sector;
			new_bl->next_sector = new_sector;
			new_bl->next = 0;
			new_bl->end = 1;
		} else { 
			new_bl->sector = tmp_sector;
			new_bl->next_sector = tmp_sector;
			new_bl->next = tmp_next;
			new_bl->end = 1;
		}
		memcpy(blist, new_bl, sizeof(block_list_t));
		kfree(new_bl);
		tmp_sector = blist->next_sector;

	} while(blist && !data_writen);
out:
	kfree(tmp_data_block);

	/* you may have to increase nodesize */
	if (node->block_size < (file->offset + size))
		node->block_size = file->offset + size;
	/*
	 * If the data block is not large engough,
	 * we copy only the rest of the current block.
	 * The user has to restart the write operation
	 * for the next block.
         */
	file->offset += size;
	return size;
}

static int initblkd_close(fildes_t* file) {
	file->flags = 0;
	file->offset = 0;
}


static int initblkd_open(fildes_t* file, const char* name)
{
	if (file->node->type == FS_FILE) {
		if ((file->flags & O_CREAT) && (file->flags & O_EXCL))  /// wo wird das flag O_EXCL gesetzt?
			return -EEXIST;

		/* in the case of O_TRUNC kfree all the nodes */
		if (file->flags & O_TRUNC) {
			uint32_t i;
			block_list_t* blist = &file->node->block_list;
			block_list_t* lastblist = NULL;
			uint8_t* tmp_sector;
			tmp_sector = (uint8_t*) kmalloc(sector_size);

			/* the first blist pointer have do remain valid. */
			for(i=0; i<MAX_DATABLOCKS; i++) {
				if (blist->data[i]) {
					initblkd_set_sector(blist->data[i], 0);
					kprintf("%i, ", blist->data[i]);
					blist->data[i] = 0;
				}
			}

			if (!blist->end) {
				lastblist = blist;
				hermit_blk_read_sync(blist->next_sector, tmp_sector, &sector_size);
				blist =  tmp_sector + sizeof(block_list_t) * blist->next;
				kprintf("blist = %i\n", blist);
				lastblist->next = 0;
				lastblist->next_sector = 0;
				lastblist->sector = 0,
				lastblist->end = 1;
				do {
					for(i=0; i<MAX_DATABLOCKS; i++) {
						if (blist->data[i]) {
							initblkd_set_sector(blist->data[i], 0);
							kprintf("%i, ", blist->data[i]);
							blist->data[i] = 0;
						}
					}
					if(blist->end) {
						break;
					}
					hermit_blk_read_sync(blist->next_sector, tmp_sector, &sector_size);
					blist = tmp_sector + sizeof(block_list_t) * blist->next;
				} while(1);
			}
			/* reset the block_size */
			hermit_blk_write_sync(file->node->sector, file->node, size_vn);
			file->node->block_size = 0;
		}
	}
	if (file->node->type == FS_DIRECTORY) {
		/* opendir was called: */
		if (name[0] == '\0') 
			return 0;

		/* open file was called: */
		if (!(file->flags & O_CREAT)) 
			return -ENOENT;
		file->flags &= O_EXCL;
		uint32_t i, j, tmp_next = 0;
		block_list_t* blist = NULL;
		/* CREATE FILE */
		vfs_node_t* new_node = kmalloc(sizeof(vfs_node_t));
		if (BUILTIN_EXPECT(!new_node, 0))
			return -EINVAL;
		size_t tmp_sector = 0, new_sector = 0;
		int ns = 0;
		uint8_t* sector_buffer;
		sector_buffer = (uint8_t*) kmalloc(sector_size);

		blist = &file->node->block_list;
	        dir_block_t* tmp_db;
        	tmp_db = (dir_block_t*) kmalloc(sizeof(dir_block_t));
		dirent_t dirent;
	        size_t node_sector = initblkd_find_sector();
        	initblkd_set_sector(node_sector, 1);

		memset(new_node, 0x00, sizeof(vfs_node_t));
		new_node->type = FS_FILE;
		new_node->sector = node_sector;
		new_node->mask = 0x0000;
		new_node->read = initblkd_read;
		new_node->write = initblkd_write;
		new_node->open = initblkd_open;
		new_node->close = initblkd_close;
		spinlock_init(&new_node->lock);
		/* create a entry for the new node in the directory block of current node */
		do {
			for(i=0; i<MAX_DATABLOCKS; i++) {
	                        dirent.vfs_node = 0;
	                        if (!blist->data[i]) {
                	                /* create tmp directory entry */
                        	        dir_block_t* next_dir_block;
                             		next_dir_block = (dir_block_t*) kmalloc(sizeof(dir_block_t));
	                                if (BUILTIN_EXPECT(!next_dir_block, 0)) {
						kfree(new_node);
						kfree(tmp_db);
       	                                 	return -EFAULT;
					}
       		                        memset(next_dir_block, 0x00, size_db);
	       	                        blist->data[i] = initblkd_find_sector();
        	                        initblkd_set_sector(blist->data[i], 1);
                	                hermit_blk_write_sync(blist->data[i], next_dir_block, size_db);
					hermit_blk_write_sync(file->node->sector, file->node, size_vn);
                        	        kfree(next_dir_block);
	                        }
				hermit_blk_read_sync(blist->data[i], tmp_db, &size_db);
				for(j=0; j<max_direntries; j++) {
					if (!tmp_db->entries[j].vfs_node) {
						tmp_db->entries[j].vfs_node = new_node->sector;
						int wr = strncpy(tmp_db->entries[j].name, (char*) name, MAX_FNAME);
						if (blist->sector) {
							if (j == max_direntries -1 && i == MAX_DATABLOCKS -1) {
								blist->next++;
								if (sector_size == (blist->next + 1) * sizeof(block_list_t) ||
								    sector_size < (blist->next + 1) * sizeof(block_list_t)) {
									blist->next_sector = 0;
									blist->next = 0;
								}
							}
							hermit_blk_write_sync(blist->sector, sector_buffer, sector_size);
						}
						hermit_blk_write_sync(blist->data[i], tmp_db, size_db);
						kfree(tmp_db);

						goto exit_create_file; // there might be a better Solution ***************
					}
				}
 			}
		/* if all blocks are reserved, we have  to allocate a new one */
		if (!blist->next_sector) {
			blist->end = 0;
			new_sector = initblkd_find_sector();
		        initblkd_set_sector(new_sector, 1);
			blist->next_sector = new_sector;
			if (tmp_sector) {
				hermit_blk_write_sync(tmp_sector, sector_buffer, sector_size);
			}
			memset(sector_buffer, 0x00, sector_size);
		} else {
			hermit_blk_read_sync(blist->next_sector, sector_buffer, &sector_size);
			tmp_sector = blist->next_sector;
			tmp_next = blist->next;

		}
		block_list_t* old_bl = blist;
		blist = sector_buffer + sizeof(block_list_t) * blist->next;
		if (blist->data[0] && !new_sector) {
			continue;
		}
		old_bl->end = 0;
		block_list_t* new_bl;
		new_bl = (block_list_t*) kmalloc(sizeof(block_list_t));
		memset(new_bl, 0x00, sizeof(block_list_t));
		if (new_sector) {
			new_bl->sector = new_sector;
			new_bl->next_sector = new_sector;
			new_bl->next = 0;
			new_bl->end = 1;
		} else { 
			new_bl->sector = tmp_sector;
			new_bl->next_sector = tmp_sector;
			new_bl->next = tmp_next;
			new_bl->end = 1;
		}
		memcpy(blist, new_bl, sizeof(block_list_t));
		kfree(new_bl);
		tmp_sector = blist->next_sector;
		} while(blist);

exit_create_file:
		new_node->block_size = 0;
		hermit_blk_write_sync(new_node->sector, new_node, size_vn);
		file->node = new_node;
		file->sector = new_node->sector;
	}
	return 0;
}

static dirent_t initblkd_readdir(vfs_node_t* node, uint32_t index)
{
	uint32_t i, j, count = 0;
        int size_db = sizeof(dir_block_t);
        dir_block_t* tmp_db;
        tmp_db = (dir_block_t*) kmalloc(sizeof(dir_block_t));
	uint8_t* sector_buffer;
        sector_buffer = (uint8_t*) kmalloc(sector_size);
	dirent_t dirent;
	block_list_t* blist = &node->block_list;
	int counter = 0;
	do {
		for(i=0; i<MAX_DATABLOCKS; i++) {
			dirent.vfs_node = 0;
			if(blist->data[i]) {
				hermit_blk_read_sync(blist->data[i], tmp_db, &size_db);
				for(j=0; j<max_direntries; j++) {
					dirent = tmp_db->entries[j];
					if (dirent.vfs_node) {
						count++;
						if (count > index) {
							kfree(tmp_db);
							return dirent;
						}
					}
				}
			}
		}
		if (!blist->end) {
                hermit_blk_read_sync(blist->next_sector, sector_buffer, &sector_size);
                blist = sector_buffer + sizeof(block_list_t) * blist->next;
		} else {
			break;
		}
		counter++;
	} while(1);
	kfree(tmp_db);
	return dirent;
}

static size_t initblkd_finddir(vfs_node_t* node, const char *name)
{
	uint32_t i, j, tmp_next = 0;
	int size_db = sizeof(dir_block_t);
	dir_block_t* tmp_db;
	tmp_db = (dir_block_t*) kmalloc(sizeof(dir_block_t));
	uint8_t* sector_buffer;
        sector_buffer = (uint8_t*) kmalloc(sector_size);
	dirent_t dirent;
	dirent.vfs_node = 0;
	block_list_t* blist = &node->block_list;
	int count = 0;
	do {
		for(i=0; i<MAX_DATABLOCKS; i++) {
			if(blist->data[i]) {
				hermit_blk_read_sync(blist->data[i], tmp_db, &size_db);

				for(j=0; j<max_direntries; j++) {
					if (!strncmp(tmp_db->entries[j].name, name, MAX_FNAME)) {
						dirent = tmp_db->entries[j];
						kfree(tmp_db);
						return dirent.vfs_node;
					}
				}
			}
		}
		if (!blist->end) {
                hermit_blk_read_sync(blist->next_sector, sector_buffer, &sector_size);
                blist = sector_buffer + sizeof(block_list_t) * blist->next;
		} else {
			break;
		}
		count++;
	} while(1);
	kfree(tmp_db);
	return 0;
}

static size_t* initblkd_mkdir(vfs_node_t* act_node, const char* name)
{
	uint32_t i, j, k = 0, tmp_next = 0;
	int ret = NULL;
	dir_block_t* dir_block;
	dir_block_t* tmp_dir_block;
	dirent_t* dirent;
	vfs_node_t* new_node;

	size_t tmp_sector = 0, new_sector = 0;
	int ns = 0;
	uint8_t* sector_buffer;
	sector_buffer = (uint8_t*) kmalloc(sector_size);
	block_list_t* blist = &act_node->block_list;

	if (BUILTIN_EXPECT(act_node->type != FS_DIRECTORY, 0))
		goto out1;

	/* exists already a entry with same name? */
	if (initblkd_finddir(act_node, name)) {
		goto out1;
	}

	new_node = kmalloc(sizeof(vfs_node_t));
	if (BUILTIN_EXPECT(!new_node, 0))
		goto out1;

	memset(new_node, 0x00, sizeof(vfs_node_t));
	new_node->type = FS_DIRECTORY;
	new_node->read = &initblkd_emu_readdir;
	new_node->readdir = &initblkd_readdir;
	new_node->finddir = &initblkd_finddir;
	new_node->mkdir = &initblkd_mkdir;
	new_node->open = &initblkd_open;
	new_node->close = &initblkd_close;
	spinlock_init(&new_node->lock);

	/* create default directory entry */
	dir_block = (dir_block_t*) kmalloc(sizeof(dir_block_t));
	if (BUILTIN_EXPECT(!dir_block, 0))
		goto out2;

	/* create tmp directory entry */
	tmp_dir_block = (dir_block_t*) kmalloc(sizeof(dir_block_t));
	if (BUILTIN_EXPECT(!tmp_dir_block, 0))
		goto out3;

	/* find and lock sectors for node and block_dir */
	size_t node_sector = initblkd_find_sector();
	initblkd_set_sector(node_sector, 1);
	size_t dir_block_sector = initblkd_find_sector();
	initblkd_set_sector(dir_block_sector, 1);

	new_node->sector = node_sector;
	memset(dir_block, 0x00, sizeof(dir_block_t));
	new_node->block_list.data[0] = dir_block_sector;
	new_node->block_list.end = 1;
	strncpy(dir_block->entries[0].name, ".", MAX_FNAME);
	dir_block->entries[0].vfs_node = node_sector;
	strncpy(dir_block->entries[1].name, "..", MAX_FNAME);
	dir_block->entries[1].vfs_node = act_node->sector;
	do {
		/* searching for a free directory block */
		int new_db = 0;
		for(i=0; i<MAX_DATABLOCKS; i++) {
			if (!blist->data[i]) {
				/* create tmp directory entry */
				dir_block_t* next_dir_block;
		        	next_dir_block = (dir_block_t*) kmalloc(sizeof(dir_block_t));
			        if (BUILTIN_EXPECT(!next_dir_block, 0))
		        	        goto out4;
				memset(next_dir_block, 0x00, size_db);
				blist->data[i] = initblkd_find_sector();
				initblkd_set_sector(blist->data[i], 1);
				hermit_blk_write_sync(blist->data[i], next_dir_block, size_db);
				kfree(next_dir_block);
				new_db = 1;
			}
			hermit_blk_read_sync(blist->data[i], tmp_dir_block, &size_db);
			for(j=0; j<max_direntries; j++) {
				dirent = &tmp_dir_block->entries[j];
				if (!dirent->vfs_node) {
					dirent->vfs_node = node_sector;
					strncpy(dirent->name, name, MAX_FNAME);
					hermit_blk_write_sync(blist->data[i], tmp_dir_block, size_db);
					hermit_blk_write_sync(node_sector, new_node, size_vn);
					hermit_blk_write_sync(dir_block_sector, dir_block, size_db);
					if (blist->sector) {
						if (j == max_direntries -1 && i == MAX_DATABLOCKS -1) {
							blist->next++;
							if (sector_size == (blist->next + 1) * sizeof(block_list_t) ||
							    sector_size < (blist->next + 1) * sizeof(block_list_t)) {
								blist->next_sector = 0;
								blist->next = 0;
							}
						}
						hermit_blk_write_sync(blist->sector, sector_buffer, sector_size);
					}
					ret = node_sector;
					goto out4;
				}
			}
		}
		/* if all blocks are reserved, we have  to allocate a new one */
		if (!blist->next_sector) {
			blist->end = 0;
			new_sector = initblkd_find_sector();
		        initblkd_set_sector(new_sector, 1);
			blist->next_sector = new_sector;
			if (tmp_sector) {
				hermit_blk_write_sync(tmp_sector, sector_buffer, sector_size);
			}
			memset(sector_buffer, 0x00, sector_size);
		} else {
			hermit_blk_read_sync(blist->next_sector, sector_buffer, &sector_size);
			tmp_sector = blist->next_sector;
			tmp_next = blist->next;

		}
		block_list_t* old_bl = blist;
		blist = sector_buffer + sizeof(block_list_t) * blist->next;
		if (blist->data[0] && !new_sector) {
			continue;
		}
		old_bl->end = 0;
		block_list_t* new_bl;
		new_bl = (block_list_t*) kmalloc(sizeof(block_list_t));
		memset(new_bl, 0x00, sizeof(block_list_t));
		if (new_sector) {
			new_bl->sector = new_sector;
			new_bl->next_sector = new_sector;
			new_bl->next = 0;
			new_bl->end = 1;
		} else { 
			new_bl->sector = tmp_sector;
			new_bl->next_sector = tmp_sector;
			new_bl->next = tmp_next;
			new_bl->end = 1;
		}
		memcpy(blist, new_bl, sizeof(block_list_t));
		kfree(new_bl);
		tmp_sector = blist->next_sector;


	} while(blist);
out4:
	kfree(sector_buffer);
	kfree(tmp_dir_block);
out3:
	kfree(dir_block);
out2:
	kfree(new_node);
out1:
	kfree(act_node);
	return ret;


}


int initblkd_set_sector(size_t sector, int value) {
        size_t bitmap_block = (size_t)((int)begin_bitmap + (int)(sector / (sector_size*8)));
        size_t sector_in_block = sector % (sector_size*8);
	int a = (int) sector_in_block / 64;
	int pos = (int) sector_in_block % 64;
	uint64_t bit = 1;
	bit <<= pos;
	uint64_t bitmap_sector[(sector_size/8)];
	hermit_blk_read_sync(bitmap_block, bitmap_sector, &sector_size);
	if (value == 1){
		bitmap_sector[a] |= bit;
	} else if (value == 0) {
		bitmap_sector[a] &= ~bit;
	} else {
		return -1;
	}
	hermit_blk_write_sync(bitmap_block, bitmap_sector, sector_size);
        return 1;
}

int initblkd_find_sector(void) {
        size_t last_bitmap = begin_bitmap + (size_t) (num_bitmap_blocks -1);
        size_t empty_sector = -1, tmp_sector = 0;
        size_t i = 0;
        int j,k;
        uint64_t bitmap_sector[(sector_size/8)];        // (sector_size / 64) * 8 , 8Bits per Byte
        memset(bitmap_sector, 0x00, sector_size);
        for (i = begin_bitmap; i <= last_bitmap; i++) {
                hermit_blk_read_sync(i, bitmap_sector, &sector_size);
                for (j = 0; j < (sector_size/8); j++) {
                        if (bitmap_sector[j] == 0xFFFFFFFFFFFFFFFF) {
                                tmp_sector += sector_size/8;
                                continue;
                        }
                        uint64_t running_bit = 1;
                        for(k = 0; k < 64; k++) {
                                if (!(running_bit & bitmap_sector[j])) {
                                        empty_sector = tmp_sector;
                                        goto found;
                                }
                                running_bit <<= 1;
                                tmp_sector++;
                        }
                }
        }
found:
        if (empty_sector <= last_bitmap) {
                empty_sector = -1;
        }
        return empty_sector;
}

int initblkd_bitmapinit(void) {
	num_bitmap_blocks = sectors/(sector_size*8);
	uint64_t bitmap_sector[(sector_size/8)];	// (sector_size / 64) * 8 , 8Bits per Byte
	memset(bitmap_sector, 0x00, sector_size);
	LOG_INFO("initblkd_init: we need %i blocks for the bit_map\n", num_bitmap_blocks);
	size_t tmp_sector = begin_bitmap;
	for(int i = 1; i <= num_bitmap_blocks; i++) {
		hermit_blk_write_sync(tmp_sector++, bitmap_sector, sector_size);
	}
	uint64_t bit = 1;
	for(int i = 0; i < (num_bitmap_blocks); i++) {
		bitmap_sector[0] |= bit;
		bit = bit << 1;
	}
	hermit_blk_write_sync(begin_bitmap, bitmap_sector, sector_size);
	LOG_INFO("initblkd_init: bitmap initialized\n");
	return num_bitmap_blocks;
}


int initblkd_init(void)
{
	// initblkd_root as global useful?????????????????????
	// if useful, than sotre results in it in first if clause

	LOG_INFO("initblkd_init: Initializing BLK device\n");

	if (!hermit_blk_stat()) {
		LOG_INFO("initblkd_init: no hermit_blk device \n");
		return -1;
	}

	dir_block_t* tmp_db;
	vfs_node_t* tmp_vn;
	sectors = hermit_blk_sectors();
	sector_size = hermit_blk_sector_size();

	int size_mb = (sector_size*sectors) / (1024*1024);
	LOG_INFO("initblkd_init: hermit_blk device hast %i MB\n", size_mb);
	LOG_INFO("initblkd_init: block_list_t: %i\n", sizeof(block_list_t));

	num_bitmap_blocks = sectors/(sector_size*8);
	max_direntries = sector_size/sizeof(dirent_t);

	first_sector += num_bitmap_blocks;
	second_sector += num_bitmap_blocks;
	size_t tmp_sector;



	tmp_vn = (vfs_node_t*) kmalloc(sizeof(vfs_node_t));
	tmp_db = (dir_block_t*) kmalloc(sizeof(dir_block_t));
	size_vn = sizeof(vfs_node_t);
	size_db = max_direntries*sizeof(dirent_t);
	hermit_blk_read_sync(first_sector, tmp_vn, &size_vn);
	hermit_blk_read_sync(second_sector, tmp_db, &size_db);

	if( ((!strncmp(tmp_db->entries[0].name, ".", MAX_FNAME)) && tmp_db->entries[0].vfs_node == first_sector) &&
	    ((!strncmp(tmp_db->entries[1].name, "..", MAX_FNAME)) && tmp_db->entries[1].vfs_node == first_sector) &&
	    (tmp_vn->type == FS_DIRECTORY) && (hermit_blk_stat() != 2)) {
		LOG_INFO("initblkd_init: hermit_blk device is initialized\n");
		LOG_INFO("initblkd_init: hermit_blk device has %i sectors and the sector size is %i Bytes\n",
			 sectors, sector_size);
	} else if (hermit_blk_stat() == 2){

		dir_block_t* dir_block;

		initblkd_bitmapinit();

		/* Initialize the root directory. */
		size_t root_sector = initblkd_find_sector();
		initblkd_set_sector(root_sector, 1);
		size_t dirblock_sector = initblkd_find_sector();
		initblkd_set_sector(dirblock_sector, 1);
		fs_root = root_sector;
		memset(&initblkd_root, 0x00, sizeof(vfs_node_t));
		initblkd_root.type = FS_DIRECTORY;
		initblkd_root.sector = root_sector;
		initblkd_root.read = &initblkd_emu_readdir;
		initblkd_root.readdir = &initblkd_readdir;
		initblkd_root.finddir = &initblkd_finddir;
		initblkd_root.mkdir = &initblkd_mkdir;
		initblkd_root.open = &initblkd_open;
		initblkd_root.close = &initblkd_close;
		spinlock_init(&initblkd_root.lock);

		/* create default directory block */
		dir_block = (dir_block_t*) kmalloc(sizeof(dir_block_t));
		if (BUILTIN_EXPECT(!dir_block, 0))
		return -ENOMEM;
		memset(dir_block, 0x00, sizeof(dir_block_t));
		initblkd_root.block_list.data[0] = dirblock_sector;
		initblkd_root.block_list.end = 1;
		strncpy(dir_block->entries[0].name, ".", MAX_FNAME);
		dir_block->entries[0].vfs_node = fs_root;
		strncpy(dir_block->entries[1].name, "..", MAX_FNAME);
		dir_block->entries[1].vfs_node = fs_root;

		LOG_INFO("initblkd_init: formatting hermit_blk device\n");

		hermit_blk_write_sync(dirblock_sector, dir_block, sizeof(dir_block_t));
		kfree(dir_block);
		hermit_blk_write_sync(root_sector, &initblkd_root, sizeof(vfs_node_t));

		/* create the directory bin and dev */

		mkdir_fs(fs_root, "bin");
		mkdir_fs(fs_root, "sbin");
		tmp_sector = mkdir_fs(fs_root, "dev");
		mkdir_fs(fs_root, "tmp");
		mkdir_fs(tmp_sector, "test");

		LOG_INFO("initblkd_init: root directory block and inode writen\n");


//		---- test functions ----
/*
		uint8_t test_blist[12];
		strncpy (test_blist, "/tf", 4);
		int v = 0;

/		kprintf("for-loop\n");

/*		---- create u directorys ----
		for (int u = 1; u <= 500; u++) { // -6, wegen . .. bin sbin tmp test 
			test_blist[3] = '0' + (u / 1000) % 10;
			test_blist[4] = '0' + (u / 100) % 10;
			test_blist[5] = '0' + (u / 10) % 10;
			test_blist[6] = '0' + u % 10;

			mkdir_fs(fs_root, (char*) test_blist);
		}

		---- create u files ----
/*		for (int u = 1; u <= 40; u++) { // -6, wegen . .. bin sbin tmp test 
			test_blist[3] = '0' + (u / 1000) % 10;
			test_blist[4] = '0' + (u / 100) % 10;
			test_blist[5] = '0' + (u / 10) % 10;
			test_blist[6] = '0' + u % 10;

			fildes_t test_files;
			fildes_t* tfs = &test_files;
			tfs->flags = O_CREAT;
			open_fs(tfs, test_blist);
			close_fs(tfs);
		}

/*		---- create file with i sectors ---
/*
		char testchars[sector_size];
		char output[sector_size];
		for (i = 0; i < sector_size; i++) {
			testchars[i] = '0' + i % 10;
			output[i] = 0;
		}


		fildes_t testfile2;
		fildes_t* tf2 = &testfile2;
		for (i = 0; i <600; i++){
		tf2->flags = O_CREAT;
		tf2->offset = i * sector_size;
		open_fs(tf2, "/bin/tf2.c");
		write_fs(tf2, testchars, sector_size);
		close_fs(tf2);
		}

		tf2->flags = O_TRUNC;
		open_fs(tf2, "/bin/tf2.c");
		close_fs(tf2);

//		---- create/truncate files with different filedescriptors and options ---
/*		open_fs(tf2, "/bin/tf2.c");
		write_fs(tf2, "jaratim", 7);
		close_fs(tf2);

		fildes_t testfile3;
		fildes_t* tf3 = &testfile3;
		tf3->flags = O_CREAT;
		open_fs(tf3, "/bin/tf3.c");
		write_fs(tf3, "abcdefg", 7);
		write_fs(tf3, "jaratim", 7);
		close_fs(tf3);

		fildes_t testfile4;
		fildes_t* tf4 = &testfile4;
		tf4->flags = O_CREAT;
		open_fs(tf4, "/dev/test/tf4.c");
		write_fs(tf4, testchars, sector_size);
		close_fs(tf4);

		list_fs(root_sector, 2);

		tf4->offset = 4;
		open_fs(tf4, "/dev/test/tf4.c");
		write_fs(tf4, " U ", 3);
		close_fs(tf4);

		mkdir_fs(fs_root, "var");
/**/
/*		tf2->flags = O_APPEND;
		open_fs(tf2, "/bin/tf2.c");
		write_fs(tf2, "mitaraj", 7);
		close_fs(tf2);

		tf3->flags = O_TRUNC;
		open_fs(tf3, "/bin/tf3.c");
		close_fs(tf3);

		open_fs(tf4, "/bin/tf3.c");
		read_fs(tf4, output, sector_size);
		close_fs(tf4);

		kprintf("output: %s", output);
*/
/*		int log_fd = fs_open("/bin/log.txt", O_CREAT);
		fs_write(log_fd, "logging", 7);
		fs_close(log_fd);

		char buf[10];
		int log_read = fs_open("/bin/tf2.c", O_RDONLY);
		fs_read(log_read, buf, 10);
		fs_close(log_read);
		kprintf("buf: %s\n", buf);

/**/
	} else {
		LOG_INFO("initblkd_init: wrong filesystem, set HERMIT_BLK_FORMAT=1 to format blk device\n");
		return 0;
	}
	fs_mkdir("/bin/testdir");
//	list_fs(root_sector, 2);
	kfree(tmp_vn);
	kfree(tmp_db);
	uhyve_blk_init_ok = 1; //ACHTUNG: momentan lokal und nicht als externe Variable aus der uhyve-blk.h!!!!

	return 0;
}
