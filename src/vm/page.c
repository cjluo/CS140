#include <stdio.h>
#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

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
  struct page_table_entry *pte; 
  pte = malloc (sizeof *pte);

  pte->file = file;
  pte->ofs = ofs;
  pte->upage = upage;
  pte->read_bytes = read_bytes;
  pte->zero_bytes = zero_bytes;
  pte->writable = writable;
  pte->type = type;
  
  //printf("## sup_insert 1 %x \n", (uint32_t)pte->upage);
  struct hash_elem *result = hash_insert (&thread_current ()->sup_page_table, &pte->elem);

  if (result != NULL)
    return false;

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

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
bool
lazy_load_segment (struct file *file, off_t ofs, uint8_t *upage,
                   uint32_t read_bytes, uint32_t zero_bytes,
                   bool writable, enum hash_type type)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      if (!sup_insert (file, ofs, upage, page_read_bytes,
                       page_zero_bytes, writable, type))
        return false;

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;

      // update offset of the file seeker.
      ofs += page_read_bytes;

    }
  return true;
}

