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

/* Functionalities required by hash table, which is supplemental_pt */
unsigned suppl_pt_hash (const struct hash_elem *, void * UNUSED);
bool suppl_pt_less (const struct hash_elem *, 
		    const struct hash_elem *,
		    void * UNUSED);

#endif
