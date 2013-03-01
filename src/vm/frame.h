#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "threads/thread.h"
#include "threads/synch.h"
#include <stdint.h>


struct frame_table_entry
{
  struct thread *t;
  void *upage;
};

void frame_table_init (void);

/* this is to link the frame table entry with user virtual address */
void frame_set_upage (void * , void *);
void *get_next_frame (void);
void kill_frame_table (void);

void pin_upage (void *);
void unpin_upage (void *);

/* this lock is just user pool lock */
struct lock *frame_lock;

/* this lock is designed to synchronize palloc_get_page and install_page*/
struct lock user_address_lock;

#endif
