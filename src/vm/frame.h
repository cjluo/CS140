#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "threads/thread.h"
#include <stdint.h>

struct frame_table_entry
{
  struct thread *t;
  void *upage;
};

void frame_table_init (void);
void frame_set_upage (void * , void *);
void *get_next_frame (void);

#endif
