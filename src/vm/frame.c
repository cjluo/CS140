#include <stdio.h>
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "vm/frame.h"

static struct frame_table_entry *frame_table;
static uint32_t frame_clock_point;    /* clock point to trace each algorithm */
static uint32_t user_pool_base;
static uint32_t user_pool_page_cnt;

void
frame_table_init (void)
{
  /* pull user pool information out to frame table */
  set_user_pool_info(&user_pool_base, &user_pool_page_cnt, &frame_lock);
  
  /* init the frame table */
  frame_table = malloc(sizeof (struct frame_table_entry) * user_pool_page_cnt);
  
  frame_clock_point = 0;
  
  lock_init(&user_address_lock);
  
  /* clear the frame table */
  uint32_t i;
  for (i = 0; i < user_pool_page_cnt; i++)
  {
    frame_table[i].t = NULL;
    frame_table[i].upage = NULL;
  }
}

void
frame_set_upage (void* kpage, void* upage)
{
  lock_acquire (frame_lock);
  /* calculate the index based on kpage for the frame table */
  uint32_t index = ((uint32_t)kpage - user_pool_base)/PGSIZE;
  
  ASSERT(index < user_pool_page_cnt);
  /* set the frame tabel with thread and virtual address */
  frame_table[index].upage = upage;
  frame_table[index].t = thread_current ();
  
  lock_release (frame_lock);
}


/* In get_next_frame we walk through the page table three times */
/* The first walk will return the frame not accessed and        */
/* clear the accessed bits                                      */
/* The second walk will return the frame that is not used in the*/
/* current threads                                              */
/* The third walk will return whatever frame available          */

void *
get_next_frame (void)
{
  lock_acquire (frame_lock);
  
  uint32_t i;
  for (i = 0; i < user_pool_page_cnt*3; i++)
  {
    /* frame_clock_point increment */
    frame_clock_point++;
    frame_clock_point %= user_pool_page_cnt;
    
    struct frame_table_entry *f = &frame_table[frame_clock_point];
    
    /* frame_clock_point increment */
    void *next_frame = (void *)(frame_clock_point * PGSIZE + user_pool_base);
    
    ASSERT (is_thread(f->t) && f->t->pagedir != NULL)
    
    bool accessed = pagedir_is_accessed (f->t->pagedir, f->upage);
    
    /* if accessed, clear the access bit */
    if (accessed)
      pagedir_set_accessed (f->t->pagedir, f->upage, false);
    /* otherwize page out if satifies the following condition */
    else if (f->t != thread_current () || i >= 2 * user_pool_page_cnt)
    {
      if (pagedir_is_dirty (f->t->pagedir, f->upage))
      { 
        uint32_t *pte = lookup_page (f->t->pagedir, f->upage, false);
        ASSERT (pte != NULL && (*pte & PTE_P) != 0)
        
        if ((*pte & PTE_AVL2) != 0)
          continue;
        
        /* Swap to disk: notice, at this time, f->could still use this page*/
        uint32_t index = write_to_swap(next_frame);
        /* Clear the kernal page mapping */
        *pte &= ~PTE_P;
        *pte &= PTE_FLAGS;
        /* Use the AVL bits */
        *pte |= PTE_AVL1;
        /* Set it to be accessed to prevent from immediate pageout */
        pagedir_set_accessed (f->t->pagedir, f->upage, true);
          
        /* Record index into page table */
        *pte |= index << 12;
      }
      
      pagedir_clear_page (f->t->pagedir, f->upage);
      lock_release (frame_lock);
      return next_frame;
    }
  }
  
  lock_release (frame_lock);
  PANIC ("No available frame!!!"); 
}
