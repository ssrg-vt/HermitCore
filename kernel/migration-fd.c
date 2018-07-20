#include <hermit/migration-fd.h>
#include <hermit/string.h>
#include <hermit/malloc.h>
#include <hermit/logging.h>
#include <hermit/spinlock.h>
#include <hermit/migration.h>

#define MIG_FD_ARRAY_SIZE	128
#define MAX_FD_PATH_SIZE	128

/* Moved this here from fcntl.h */
#define O_RDONLY	0x000
#define O_WRONLY	0x001
#define O_RDWR		0x002
#define O_CREAT		0x040
#define O_TRUNC		0x200
#define S_IRUSR		00400
#define S_IWUSR		00200
#define SEEK_SET	0x000
#define SEEK_CUR	0x001
#define SEEK_END	0x002

typedef struct {
	int app_fd;
	int real_fd;
	uint64_t offset;
	char path[MAX_FD_PATH_SIZE];
} migration_fd_t;

migration_fd_t fd_array[MIG_FD_ARRAY_SIZE];
spinlock_irqsave_t fd_array_lock = SPINLOCK_IRQSAVE_INIT;

static void print_migration_array(void);

void migration_fd_init() {
	int i;

	MIGLOG("Init fd migration subsystem for up to %d fds, with path size up to "
			"%d bytes\n", MIG_FD_ARRAY_SIZE, MAX_FD_PATH_SIZE);

	for(i=0; i<MIG_FD_ARRAY_SIZE; i++)
		fd_array[i].app_fd = fd_array[i].real_fd = -1;
}

int migrate_restore_fds(const char *filename) {
	int fd, i, filesize, nelems;
	migration_fd_t *local_fd_array;

	fd = sys_open(filename, O_RDONLY, 0x0);

	if(fd == -1) {
		MIGERR("Cannot open file for fd restore\n");
		return -1;
	}

	filesize = sys_lseek(fd, 0x0, SEEK_END);
	if(filesize == -1) {
		MIGERR("Cannot determine fd restore file size\n");
		sys_close(fd);
		return -1;
	}
	sys_lseek(fd, 0x0, SEEK_SET);

	if(filesize % sizeof(migration_fd_t) != 0) {
		MIGERR("Size of fd restore file is not a multiple of migration_fd_t "
				"size\n");
		return -1;
	}

	nelems = filesize/sizeof(migration_fd_t);
	if(!nelems) {
		sys_close(fd);
		return 0;
	}

	local_fd_array = kmalloc(nelems * sizeof(migration_fd_t));
	if(!local_fd_array) {
		MIGERR("FD migration: cannot allocate memory\n");
		return -ENOMEM;
	}

	if(sys_read(fd, (char *)local_fd_array, nelems*sizeof(migration_fd_t)) !=
			nelems*sizeof(migration_fd_t)) {
		MIGERR("read error on restoring fds\n");
		sys_close(fd);
		return -1;
	}

	sys_close(fd);

	for(i=0; i<nelems; i++) {
		int newfd, j, app_fd_set;
		migration_fd_t *elem = &(local_fd_array[i]);

		MIGLOG("Fd migration: extracted fd %d (%s), offset:0x%x\n",
				elem->app_fd, elem->path, elem->offset);

		/* re open the file */
		newfd = sys_open(elem->path, O_RDWR, 0x0); /* FIXME: migrate flags? */
		if(newfd == -1) {
			MIGERR("Cannot reopen %s after migration\n", elem->path);
			return -1;
		}

		/* set the app fd */
		app_fd_set = 0;
		for(j=0; j<MIG_FD_ARRAY_SIZE; j++)
			if(fd_array[j].app_fd == newfd && fd_array[j].real_fd == newfd) {
				fd_array[j].app_fd = elem->app_fd;
				app_fd_set = 1;
			}

		if(!app_fd_set) {
			MIGERR("Cannot reset app_fd for %s\n", elem->path);
			sys_close(fd);
			return -1;
		}

		/* set the proper offset */
		if(sys_lseek(elem->app_fd, elem->offset, SEEK_SET) != elem->offset) {
			MIGERR("Cannot reset offset for %s\n", elem->path);
			return -1;
		}
	}

	return 0;
}

int migrate_chkpt_fds(const char *filename) {
	int i, fd;
	
	fd = sys_open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if(fd == -1) {
		MIGERR("Cannot open file for fd migration\n");
		return -1;
	}

	/* Save each migration_fd_t one by one so that we can filter out the fd
	 * corresponding to the file we are currently writing!
	 */
	for(i=0; i<MIG_FD_ARRAY_SIZE; i++) {
		migration_fd_t *ptr = &(fd_array[i]);

		if(ptr->app_fd != -1 && ptr->app_fd != fd) {
			uint64_t offset = sys_lseek(ptr->app_fd, 0x0, SEEK_CUR);
			if(offset == -1) {
				MIGERR("Cannot get offset from chkpt fd (%s)\n", ptr->path);
				return -1;
			}
			ptr->offset = offset;

			if(sys_write(fd, (char *)ptr, sizeof(migration_fd_t)) != 
				sizeof(migration_fd_t)) {
				MIGERR("Saving fd array: short write\n");
				sys_close(fd);
				return -1;
			}

			MIGLOG("Saved fd %d (%s), offset 0x%x\n", ptr->app_fd, ptr->path,
					ptr->offset);
		}
	}

	sys_close(fd);
	return 0;
}

int migration_fd_add(int app_fd, const char *path) {
	int i;

	if(strlen(path) >= MAX_FD_PATH_SIZE) {
		MIGERR("Adding fd %d (%s) to migration subsystem: path too long\n",
				app_fd, path);
		return -1;
	}

	spinlock_irqsave_lock(&fd_array_lock);

	for(i=0; i<MIG_FD_ARRAY_SIZE; i++)
		if(fd_array[i].app_fd == -1) {
			fd_array[i].app_fd = app_fd;
			fd_array[i].real_fd = app_fd;
			strcpy(fd_array[i].path, path);
			spinlock_irqsave_unlock(&fd_array_lock);
			return 0;
		}

	spinlock_irqsave_unlock(&fd_array_lock);
	MIGERR("add fd due to open: migration fd array full\n");
	return -1;
}

int migration_fd_del(int app_fd) {
	int i;

	spinlock_irqsave_lock(&fd_array_lock);

	for(i=0; i<MIG_FD_ARRAY_SIZE; i++)
		if(fd_array[i].app_fd == app_fd) {
			fd_array[i].app_fd = -1;
			fd_array[i].real_fd = -1;
			spinlock_irqsave_unlock(&fd_array_lock);
			return 0;
		}

	spinlock_irqsave_unlock(&fd_array_lock);
	MIGERR("Migration fd del on non existing fd (%d)\n", app_fd);
	return -1;
}

int get_real_fd(int app_fd) {
	int i;

	spinlock_irqsave_lock(&fd_array_lock);
	for(i=0; i<MIG_FD_ARRAY_SIZE; i++)
		if(fd_array[i].app_fd == app_fd) {
			spinlock_irqsave_unlock(&fd_array_lock);
			return fd_array[i].real_fd;
		}

	spinlock_irqsave_unlock(&fd_array_lock);
	MIGERR("Trying to get real fd cannot find app_fd %d\n", app_fd);
	print_migration_array();
	return -1;
}

static void print_migration_array(void) {
	int i;

	LOG_INFO("FD migration array:\n");
	LOG_INFO("-------------------\n");

	for(i=0; i<MIG_FD_ARRAY_SIZE; i++) {
		migration_fd_t *ptr = fd_array + i;
		if(ptr->app_fd != -1) {
			LOG_INFO("- %s: app_fd: %d, real_fd: %d, offset: %d\n", ptr->path,
					ptr->app_fd, ptr->real_fd, ptr->offset);
		}
	}
}
