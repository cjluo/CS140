#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>

struct lock file_lock;
void syscall_init (void);
int sys_exit (int);

struct fd_frame
  {
    int fd;
    struct file *file;
    struct list_elem elem;
    int pos;
  };

#endif /* userprog/syscall.h */
