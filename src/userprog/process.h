#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "vm/page.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool lazy_load_segment (struct file *, off_t, uint8_t *, uint32_t, 
                        uint32_t, bool, enum hash_type); 
bool load_segment (struct page_table_entry *);
bool install_page (void *upage, void *kpage, bool writable);

#endif /* userprog/process.h */
