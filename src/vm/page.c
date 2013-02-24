#include <stdio.h>
#include "vm/page.h"
#include "threads/malloc.h"


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
            uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  struct page_table_entry *pte; 
  pte = malloc (sizeof *pte);

  pte->file = file;
  pte->ofs = ofs;
  pte->upage = upage;
  pte->read_bytes = read_bytes;
  pte->zero_bytes = zero_bytes;
  pte->writable = writable;
  
  printf("## sup_insert 1 %x \n", (uint32_t)pte->upage);
  ASSERT( &thread_current ()->sup_page_table);
  struct hash_elem *result = hash_insert (&thread_current ()->sup_page_table, &pte->elem);
  printf("## sup_insert 2 \n");

  if (result != NULL)
    return false;

  return true;
}

