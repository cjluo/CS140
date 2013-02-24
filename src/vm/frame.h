#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "threads/thread.h"
#include <stdint.h>

struct frame_table_entry
{
  uint32_t *pd;
  void *upage;
  tid_t tid;
};

void frame_table_init (void);
void frame_set_upage (void * , void *);
void *get_next_frame (void);

#endif
