#ifndef MIGRATION_FD_H
#define MIGRATION_FD_H

#include <hermit/stddef.h>

void migration_fd_init();
int migration_fd_add(int app_fd, const char *path);
int migration_fd_del(int app_fd);
int get_real_fd(int app_fd);
int migrate_chkpt_fds(const char *filename);
int migrate_restore_fds(const char *filename);

#endif /* MIGRATION_FD_H */
