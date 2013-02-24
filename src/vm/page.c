#include <stdio.h>
#include "vm/page.h"


/* Functionality required by hash table*/
unsigned
suppl_pt_hash (const struct hash_elem *he, void *aux UNUSED)
{
  const struct page_table_entry *vspte;
  vspte = hash_entry (he, struct page_table_entry, elem);
  return 0; //hash_bytes (&vspte->uvaddr, sizeof vspte->uvaddr);
}

/* Functionality required by hash table*/
bool
suppl_pt_less (const struct hash_elem *hea,
               const struct hash_elem *heb,
         void *aux UNUSED)
{
  const struct page_table_entry *vsptea;
  const struct page_table_entry *vspteb;
 
  vsptea = hash_entry (hea, struct page_table_entry, elem);
  vspteb = hash_entry (heb, struct page_table_entry, elem);

  return 0; //(vsptea->uvaddr - vspteb->uvaddr) < 0;
}
