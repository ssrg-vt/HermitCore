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
#include <hermit/fs.h>
#include <hermit/errno.h>
#include <hermit/spinlock.h>
#include <hermit/time.h>

size_t fs_root = NULL;

ssize_t read_fs(fildes_t* file, uint8_t* buffer, size_t size)
{

	vfs_node_t* node = file->node;
	ssize_t ret = -EINVAL;

	if (BUILTIN_EXPECT(!node, 0))
		return ret;
	if (BUILTIN_EXPECT(!buffer, 0))
		return ret;
        spinlock_lock(&node->lock);
        hermit_blk_write_sync(file->sector, node, size_vn);           	// lock the node on the blkd
	// Has the node got a read callback?
	if (node->read != 0)
		ret = node->read(file, buffer, size);;
        spinlock_unlock(&node->lock);
        hermit_blk_write_sync(file->sector, node, size_vn);           	// lock the node on the blkd

//        kfree(node);
        return ret;
}

ssize_t write_fs(fildes_t* file, uint8_t* buffer, size_t size)
{

	vfs_node_t* node = file->node;
	ssize_t ret = -EINVAL;

	if (BUILTIN_EXPECT(!node, 0))
		return ret;
	if (BUILTIN_EXPECT(!buffer, 0))
		return ret;
        spinlock_lock(&node->lock);
        hermit_blk_write_sync(file->sector, node, size_vn);		// lock the node on the blkd
	if (node->write != 0)
		ret = node->write(file, buffer, size);
        spinlock_unlock(&node->lock);

        hermit_blk_write_sync(file->sector, node, size_vn);           	// lock the node on the blkd

//        kfree(node);
        return ret;
}

int open_fs(fildes_t* file, const char* name)
{
	uint32_t ret = 0, i, j = 1;
	vfs_node_t* file_node = NULL;  /* file node */
	vfs_node_t* dir_node = NULL;   /* dir node */
	vfs_node_t* tmp_node = NULL;
	size_t dir_sector = 0, file_sector = 0;
        file_node = kmalloc(size_vn);
        if (BUILTIN_EXPECT(!file_node, 0))
                return ret;
        dir_node = kmalloc(size_vn);
        if (BUILTIN_EXPECT(!dir_node, 0))
                return ret;
	char fname[MAX_FNAME];

	if (BUILTIN_EXPECT(!name, 0))
		return ret;

	if (name[0] == '/') {		// else, what is than the file_node???
	        hermit_blk_read_sync(fs_root, file_node, &size_vn);
        	if (BUILTIN_EXPECT(!file_node, 0))
                return ret;
	}
	while((name[j] != '\0') || ((file_node != 0) && (file_node->type == FS_DIRECTORY))) {
		i = 0;
		while((name[j] != '/') && (name[j] != '\0')) {
			fname[i] = name[j];
			i++; j++;
		}
		fname[i] = '\0';
		tmp_node = dir_node;
		dir_node = file_node; /* file must be a directory */
		file_node = tmp_node;
		file_sector = finddir_fs(dir_node->sector, fname);
		if (file_sector) {
	        	hermit_blk_read_sync(file_sector, file_node, &size_vn);
		} else {
			kfree(file_node);
			file_node = NULL;
		}
		if (name[j] == '/')
			j++;
	}

	if(file_node) { 				/* file exists */
		spinlock_lock(&file_node->lock);
		hermit_blk_write_sync(file_node->sector, file_node, size_vn);
		file->sector = file_node->sector;
		file->node = file_node;
		file->flags |= O_EXCL;
		// Has the file_node got an open callback?
		if (file_node->open != 0)
			ret = file->node->open(file, NULL);
		spinlock_unlock(&file_node->lock);
		hermit_blk_write_sync(file_node->sector, file_node, size_vn);
	} else if (dir_node) { 				/* file doesn't exist or opendir was called */
		spinlock_lock(&dir_node->lock);
		hermit_blk_write_sync(dir_node->sector, dir_node, size_vn);
		file->sector = dir_node->sector;
		file->node = dir_node;
		// Has the dir_node got an open callback?
		if (dir_node->open != 0)
			ret = dir_node->open(file, fname);
		spinlock_unlock(&dir_node->lock);
		hermit_blk_write_sync(dir_node->sector, dir_node, size_vn);
	} else {
		ret = -ENOENT;
	}
	kfree(dir_node);
	return ret;
}

int close_fs(fildes_t* file)
{
	int ret = -EINVAL;

	if (BUILTIN_EXPECT(!(file->node), 0))
		return ret;

	spinlock_lock(&file->node->lock);
	// Has the node got a close callback?
	if (file->node->close != 0)
		ret = file->node->close(file);
	spinlock_unlock(&file->node->lock);

	return ret;
}

struct dirent readdir_fs(size_t sector, uint32_t index)
{
	struct dirent ret;
        vfs_node_t* node;
        node = kmalloc(size_vn);
        if (BUILTIN_EXPECT(!node, 0))
                return;
        hermit_blk_read_sync(sector, node, &size_vn);

	if (BUILTIN_EXPECT(!node, 0))
		return;

	spinlock_lock(&node->lock);
	hermit_blk_write_sync(sector, node, size_vn);		// lock the node on the blkd
	// Is the node a directory, and does it have a callback?
	if ((node->type == FS_DIRECTORY) && node->readdir != 0)
		ret = node->readdir(node, index);
	spinlock_unlock(&node->lock);
	hermit_blk_write_sync(sector, node, size_vn);		// lock the node on the blkd

	kfree(node);
	return ret;
}

size_t finddir_fs(size_t sector, const char *name)
{
	size_t ret = NULL;
	vfs_node_t* node;
        node = kmalloc(size_vn);
        if (BUILTIN_EXPECT(!node, 0))
                return NULL;
        hermit_blk_read_sync(sector, node, &size_vn);

	if (BUILTIN_EXPECT(!node, 0))
		return ret;

	spinlock_lock(&node->lock);
	hermit_blk_write_sync(sector, node, size_vn);		// lock the node on the blkd
	// Is the node a directory, and does it have a callback?
	if ((node->type == FS_DIRECTORY) && node->finddir != 0)
		ret = node->finddir(node, name);
	spinlock_unlock(&node->lock);
	hermit_blk_write_sync(sector, node, size_vn);		// lock the node on the blkd

	kfree(node);
	return ret;
}

size_t mkdir_fs(size_t sector, const char *name)
{

	size_t ret = NULL;
	vfs_node_t* node;
	node = kmalloc(size_vn);
        if (BUILTIN_EXPECT(!node, 0))
                return NULL;
	hermit_blk_read_sync(sector, node, &size_vn);

	if (BUILTIN_EXPECT(!node, 0))
		return ret;

	spinlock_lock(&node->lock);
	hermit_blk_write_sync(sector, node, size_vn);		// lock the node on the blkd
	if (node->mkdir != 0)
		ret = node->mkdir(node, name);
	spinlock_unlock(&node->lock);
	hermit_blk_write_sync(sector, node, size_vn);		// unlock the node ohne the blkd

	kfree(node);
	return ret;
}

vfs_node_t* findnode_fs(const char* name)
{
	uint32_t i, j = 1;
	vfs_node_t* ret = NULL;
	char fname[MAX_FNAME];

	if (BUILTIN_EXPECT(!name, 0))
		return ret;

	if (name[0] == '/')
		ret = fs_root;

	while((name[j] != '\0') && ret) {
		i = 0;
		while((name[j] != '/') && (name[j] != '\0')) {
			fname[i] = name[j];
			i++; j++;
		}
		fname[i] = '\0';
		ret = finddir_fs(ret, fname);
		if (name[j] == '/') 
			j++;
	}

	return ret;
}

void list_fs(size_t sector, uint32_t depth)
{
	int j, i = 0;
	dirent_t dirent;
	vfs_node_t* new_node;

        new_node = (vfs_node_t*) kmalloc(size_vn);
        if (BUILTIN_EXPECT(!new_node, 0))
                return;

	fildes_t* file = kmalloc(sizeof(fildes_t));
	file->offset = 0;
	file->flags = 0;


	while ((dirent = readdir_fs(sector, i)).vfs_node != 0 ) {
		for(j=0; j<depth; j++)
			kputs("  ");
		kprintf("%s", dirent.name); //, dirent.vfs_node);

		if (strcmp(dirent.name, ".") && strcmp(dirent.name, "..")) {
			size_t new_node_sector = finddir_fs(sector, dirent.name); // hier gehts falsch oder?
			if (new_node_sector) {
			        hermit_blk_read_sync(new_node_sector, new_node, &size_vn);
			        if (BUILTIN_EXPECT(!new_node, 0))
			                return;
				if (new_node->type == FS_FILE) {
					char buff[16] = {[0 ... 15] = 0x00};

					file->node = new_node;
					file->offset = 0;
					file->flags = 0;

					read_fs(file, (uint8_t*)buff, 15);
					for(j=0; j<depth+1; j++)
						kputs("  ");
					kprintf("\t\tcontent: %s\n", buff);
				} else {
					kprintf("\n");
					list_fs(new_node_sector, depth+1);
				}
			}
		} else {
			kprintf("\n");
		}
		i++;
	};
	kfree(new_node);
	kfree(file);
}

int fs_open(char* filename, int oflags) {
	fildes_t* fd;
	fd = (fildes_t*) kmalloc(sizeof(fildes_t));
	memset(fd, 0x00, sizeof(fildes_t));
	fd->flags = oflags;
	open_fs(fd, filename);
	return fd;
}

int fs_read(int fildes, void *buf, size_t nbytes) {
	int ret = 0;
	int tmp = 0;
	int i = 0;
	void* buffer;
	fildes_t* fd = (fildes_t*) fildes;
	size_t sectorsize = hermit_blk_sector_size();
	do {
		buffer = buf + sectorsize*i;
		tmp = read_fs(fd, buffer, nbytes);
		if (tmp < 0) {
			return tmp;
		}
		nbytes -= tmp;
		ret += tmp;
		i++;
	} while (nbytes > 0 && tmp > 0);
	return ret;
}

int fs_write(int fildes, void *buf, size_t nbytes) {
	int ret = 0;
	int tmp = 0;
	int i = 0;
	void* buffer;
	fildes_t* fd = (fildes_t*) fildes;
	size_t sectorsize = hermit_blk_sector_size();
	do {
		buffer = buf + sectorsize*i;
		tmp = write_fs(fd, buffer, nbytes);
		if (tmp < 0) {
			return tmp;
		}
		nbytes -= tmp;
		ret += tmp;
		i++;
	} while (nbytes > 0 && tmp > 0);
	return ret;
}

int fs_close(int fildes) {
	fildes_t* fd = (fildes_t*) fildes;
	close_fs(fd);
	kfree(fd->node);
	kfree(fd);
	return 0;
}

int fs_mkdir(const char* name) {
        uint32_t ret = -5, i, j = 1;
        vfs_node_t* file_node = NULL;  /* file node */
        vfs_node_t* dir_node = NULL;   /* dir node */
        vfs_node_t* tmp_node = NULL;
        size_t dir_sector = 0, file_sector = 0;
        file_node = kmalloc(size_vn);
        if (BUILTIN_EXPECT(!file_node, 0))
                return ret;
        dir_node = kmalloc(size_vn);
        if (BUILTIN_EXPECT(!dir_node, 0))
                return ret;
        char fname[MAX_FNAME];

        if (BUILTIN_EXPECT(!name, 0))
                return ret;
	if (name[0] == '/') {
		hermit_blk_read_sync(fs_root, file_node, &size_vn);
		if (BUILTIN_EXPECT(!file_node, 0))
			return ret;
	} else {
		return -1;
	}
	while((name[j] != '\0') || ((file_node != 0) && (file_node->type == FS_DIRECTORY))) {
		i = 0;
		while((name[j] != '/') && (name[j] != '\0')) {
			fname[i] = name[j];
			i++;
			j++;
		}
		fname[i] = '\0';
		tmp_node = dir_node;
		dir_node = file_node;
		file_node = tmp_node;
		file_sector = finddir_fs(dir_node->sector, fname);
		if (file_sector) {
			hermit_blk_read_sync(file_sector, file_node, &size_vn);
		} else {
			kfree(file_node);
			file_node = NULL;
			if (name[j] == '/')
				return -2;
		}
		if (name[j] == '/')
			j++;
	}
	if(file_node) {
		return -3;
	} else {
		if (!mkdir_fs(dir_node->sector, fname))
			return -4;
	}
	return 0;
}
//static inline uint64_t get_clock_tick(void);
uint64_t blk_get_time() {
  	int ret = (int) get_clock_tick();
	return ret;
}

