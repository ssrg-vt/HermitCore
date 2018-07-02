#include <hermit/migration-chkpt.h>
#include <hermit/stddef.h>
#include <hermit/syscall.h>
#include <hermit/logging.h>
#include <asm/page.h>
#include <asm/uhyve.h>
#include <hermit/migration.h>

/* Moved this here from fcntl.h */
#define O_RDONLY	0x000
#define O_WRONLY	0x001
#define O_CREAT		0x040
#define O_TRUNC		0x200
#define S_IRUSR		00400
#define S_IWUSR		00200

/* Saves guest virtual memory from addr to (addr + size) into host file
 * filename. Area must be contiguous in physical memory. */
int migrate_chkpt_area(uint64_t addr, uint64_t size, const char *filename) {
	int bytes_written, fd;

	fd = sys_open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if(fd == -1) {
		MIGERR("checkpointing area 0x%x -> 0x%x into %s: cannot open "
				"target file\n", addr, addr+size, filename);
		return -1;
	}

	bytes_written = sys_write(fd, (const char *)addr, size);
	if(bytes_written != size) {
		MIGERR("checkpointing area 0x%x -> 0x%x into %s: cannot write "
				"in file\n", addr, addr+size, filename);
		return -1;
	}

	sys_close(fd);
	return 0;
}

/* Saves guest virtual memory from addr to (addr + size) into host file
 * filename. Area does not need to be contiguous in physical memory, but this
 * is slower than migrate_chkpt_area. The area needs to start and end on page
 * boundaries. If the area we are checkpointing is mapped on demand (i.e. part
 * of it might not be mapped), set the last flag to 1, otherwise 0. */
int migrate_chkpt_area_not_contiguous(uint64_t addr, uint64_t size,
		const char *filename, int mapped_on_demand) {
	uint64_t ptr;
	int fd;

	if((addr % PAGE_SIZE) != 0 || ((addr + size) % PAGE_SIZE) != 0) {
		MIGERR("migrate_chkpt_area_not_contiguous called on non-page-aligned"
				" area (0x%x -> 0x%x)\n", addr, addr+size);
		return -1;
	}

	fd = sys_open(filename,	O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if(fd == -1) {
		MIGERR("Checkpointing non-contiguous area 0x%x -> 0x%x into %s: "
				"cannot open file\n", addr, addr+size, filename);
		return -1;
	}

	/* Save the memory area page by page */
	for(ptr = addr; ptr < (addr+size); ptr+=PAGE_SIZE) {

		if(mapped_on_demand && !check_pagetables(ptr))
			*(int *)ptr = 0x0; /* force mapping */

		if(sys_write(fd, (char *)ptr, PAGE_SIZE) != PAGE_SIZE) {
		MIGERR("Checkpointing non-contiguous area 0x%x -> 0x%x into %s: "
				"short write\n", addr, addr+size, filename);
			return -1;
		}
	}

	sys_close(fd);
	return 0;
}

/* Restores  guest virtual memory from addr to (addr + size) from host file
* filename. Area does not need to be contiguous in physical memory, but
* this is slower than migrate_chkpt_area. The area needs to start and
* end on page boundaries. */
int migrate_restore_area_not_contiguous(const char *filename, uint64_t addr,
		uint64_t size) {
	uint64_t ptr;
	int fd;

	if((addr % PAGE_SIZE) != 0 || ((addr + size) % PAGE_SIZE) != 0) {
		MIGERR("restore_chkpt_area_not_contiguous called on non-page-aligned"
				" area (0x%x -> 0x%x)\n", addr, addr+size);
		return -1;
	}

	fd = sys_open(filename, O_RDONLY, 0x0);
	if(fd == -1) {
		MIGERR("Restoring non-contiguous area 0x%x -> 0x%x from %s: "
				"cannot open file\n", addr, addr+size, filename);
		return -1;
	}

	/* Restore the memory area page by page */
	for(ptr = addr; ptr < (addr+size); ptr+=PAGE_SIZE)
		if(sys_read(fd, (char *)ptr, PAGE_SIZE) != PAGE_SIZE) {
			MIGERR("Restoring non-contiguous area 0x%x -> 0x%x from %s: "
				"short read\n", addr, addr+size, filename);
			return -1;
		}

	sys_close(fd);
	return 0;
}

/* Restore a previously saved area of memory from filename into memory at
 * addr. Area must be contiguous in physical memory */
int migrate_restore_area(const char *filename, uint64_t addr, uint64_t size) {
	int bytes_read, fd;

	fd = sys_open(filename, O_RDONLY, 0x0);
	if(fd == -1) {
		MIGERR("Restoring area 0x%x -> 0x%x from %s: cannot open file\n",
				addr, addr+size, filename);
		return -1;
	}

	bytes_read = sys_read(fd, (char *)addr, size);
	if(bytes_read != size) {
		MIGERR("Restoring area 0x%x -> 0x%x from %s: cannot read from "
				"file\n", addr, addr+size, filename);
		return -1;
	}

	sys_close(fd);
	return 0;

}

