#ifndef VM_PAGE_H
#define VM_PAGE_H
#include "threads/thread.h"
#include "filesys/file.h"
#include <stdint.h>
#include <hash.h>

struct page_table_entry
{
  uint8_t *upage; // Key

  size_t read_bytes;
  size_t zero_bytes;
  struct file *file;
  off_t ofs;
  bool writable;

  struct hash_elem elem;
};

/* hash function and less function */
bool sup_less (const struct hash_elem *, const struct hash_elem *, 
                void * UNUSED);
unsigned sup_hash (const struct hash_elem *, void * UNUSED);
bool sup_insert ( struct file *, off_t, uint8_t *, uint32_t, uint32_t, bool);
struct page_table_entry *get_sup_page (uint8_t *);

#endif
