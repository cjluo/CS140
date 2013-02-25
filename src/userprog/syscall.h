#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
#include "lib/user/syscall.h"

struct fd_frame
  {
    int fd;
    struct file *file;
    struct list_elem elem;
  };

struct mmap_frame
{
    mapid_t mmap_id;
    uint32_t page_cnt;
    void *upage;
    struct file* mfile;
    struct list_elem elem;
};

struct lock file_lock;
void syscall_init (void);
int sys_exit (int);
void mmap_remove (struct mmap_frame *);

#endif /* userprog/syscall.h */
