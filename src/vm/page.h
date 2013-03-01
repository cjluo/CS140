#ifndef VM_PAGE_H
#define VM_PAGE_H
#include "threads/thread.h"
#include "filesys/file.h"
#include <stdint.h>
#include <hash.h>

enum hash_type 
{
  SUP_MAP,      /* supplemental page table mapping */
  M_MAP         /* mmapping */
};

struct page_table_entry
{
  uint8_t *upage;        /* user virtual page address */

  size_t read_bytes;     /* bytes which are written */
  size_t zero_bytes;     /* bytes which are zero */
  struct file *file;     /* file pointer */
  off_t ofs;             /* file offset */
  bool writable;         /* whether it is writable */
  enum hash_type type;   /* whether it is excutable or mmap */

  struct hash_elem elem;
};

/* hash function and less function */
bool sup_less (const struct hash_elem *, const struct hash_elem *,
               void * UNUSED);
unsigned sup_hash (const struct hash_elem *, void * UNUSED);
bool sup_insert (struct file *, off_t, uint8_t *,
                 uint32_t, uint32_t, bool, enum hash_type);

struct page_table_entry *get_sup_page (uint8_t *);
void delete_sup_page (uint8_t *);
#endif
