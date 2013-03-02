#include <stdio.h>
#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/frame.h"

/* less function */
bool
sup_less (const struct hash_elem *hash_ea, 
          const struct hash_elem *hash_eb,
          void *aux UNUSED)
{
  const struct page_table_entry *sup_pte_a;
  const struct page_table_entry *sup_pte_b;
 
  sup_pte_a = hash_entry (hash_ea, struct page_table_entry, elem);
  sup_pte_b = hash_entry (hash_eb, struct page_table_entry, elem);

  return (sup_pte_a->upage - sup_pte_b->upage) < 0;
}


/* hash function */
unsigned
sup_hash (const struct hash_elem *hash_e, void *aux UNUSED)
{
  const struct page_table_entry *sup_pte;
  sup_pte = hash_entry (hash_e, struct page_table_entry, elem);
  return hash_bytes (&sup_pte->upage, sizeof sup_pte->upage);
}

bool 
sup_insert (struct file *file, off_t ofs, uint8_t *upage, 
            uint32_t read_bytes, uint32_t zero_bytes, bool writable,
            enum hash_type type)
{
  if(get_sup_page (upage) != NULL)
    return false;
  
  struct page_table_entry *pte; 
  pte = malloc (sizeof *pte);
  
  if (pte == NULL)
    PANIC ("Supplimental page table not allocated !!!");
  
  pte->file = file;
  pte->ofs = ofs;
  pte->upage = upage;
  pte->read_bytes = read_bytes;
  pte->zero_bytes = zero_bytes;
  pte->writable = writable;
  pte->type = type;
  
  struct hash_elem *result = hash_find (&thread_current ()->sup_page_table, 
                                        &pte->elem);
  if (result != NULL) 
    sys_exit (-1);
  
  hash_insert (&thread_current ()->sup_page_table, &pte->elem);

  if (result != NULL)
  {
    free (pte);
    return false;
  }

  return true;
}

struct page_table_entry *
get_sup_page (uint8_t *upage)
{
  struct hash_elem *e;
  struct page_table_entry pte;
  pte.upage = upage;

  e = hash_find (&thread_current ()->sup_page_table, &pte.elem);
  return e != NULL ? hash_entry (e, struct page_table_entry, elem) : NULL;
}

void
delete_sup_page (uint8_t *upage)
{
  struct hash_elem *e;
  struct page_table_entry pte;
  struct page_table_entry *delete;
  pte.upage = upage;

  e = hash_delete (&thread_current ()->sup_page_table, &pte.elem);
  if (e != NULL)
  {
    delete = hash_entry(e, struct page_table_entry, elem);
    ASSERT (delete);
    if (pagedir_is_dirty (thread_current ()->pagedir, delete->upage)
        && delete->writable)
    {
      file_seek (delete->file, delete->ofs);
      int return_value = pinned_file_op (delete->file, 
                                         delete->upage, 
                                         delete->read_bytes,
                                         true);
      if (return_value != (int)delete->read_bytes)
      {
        free (delete);
        sys_exit (-1);
      }
    }
    free (delete);
  }
  
  /* This page is clean now, it will be evicted at any time */
  pagedir_set_dirty (thread_current ()->pagedir, upage, false);
  
  return;
}

