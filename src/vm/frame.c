#include <stdio.h>
#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "threads/pte.h"
#define USER_FRAME_NUMBER 1024

static struct frame_table_entry frame_table[USER_FRAME_NUMBER];
static uint32_t frame_clock_point;
static struct lock frame_lock;
static uint32_t user_pool_base;
static uint32_t user_pool_page_cnt;

void
frame_table_init (void)
{
  set_user_pool_info(&user_pool_base, &user_pool_page_cnt);
  if (user_pool_page_cnt > USER_FRAME_NUMBER)
    user_pool_page_cnt = USER_FRAME_NUMBER;
  frame_clock_point = 0;
  lock_init (&frame_lock);
  uint32_t i;
  for (i = 0; i < user_pool_page_cnt; i++)
  {
    frame_table[i].pd = NULL;
    frame_table[i].upage = NULL;
    frame_table[i].tid = -1;
  }
}

void
frame_set_upage (void* kpage, void* upage)
{
  lock_acquire (&frame_lock);
  uint32_t index = ((uint32_t)kpage - user_pool_base)/PGSIZE;
  ASSERT(index < user_pool_page_cnt);
  frame_table[index].upage = upage;
  frame_table[index].tid = thread_current ()->tid;
  frame_table[index].pd = thread_current ()->pagedir;
  // printf("index: %u tid %d\n", index, (int)frame_table[index].tid);
  lock_release (&frame_lock);
}

void *
get_next_frame (void)
{
  lock_acquire (&frame_lock);
  struct thread *t = thread_current ();
  
  uint32_t i;
  for (i = 0; i < user_pool_page_cnt*3; i++)
  {
    frame_clock_point++;
    frame_clock_point %= user_pool_page_cnt;
    
    struct frame_table_entry *f = &frame_table[frame_clock_point];
    bool accessed = pagedir_is_accessed (t->pagedir, f->upage);
    if (accessed)
      pagedir_set_accessed (t->pagedir, f->upage, false);
    else if (f->tid != t->tid || i >= 2 * user_pool_page_cnt)
    {
      void *next_frame = (void *)(frame_clock_point * PGSIZE + user_pool_base);

      if (get_swap_enable() 
          && pagedir_is_dirty (thread_current ()->pagedir, f->upage))
      {
        uint32_t index = write_to_swap(next_frame);
        uint32_t *pte = lookup_page (f->pd, f->upage, false);
        if (pte != NULL && (*pte & PTE_P) != 0)
        {
          /* Recorde the index in SWAP */
          *pte &= PTE_FLAGS;
          *pte |= index << 12;
          /* Use the AVL bits */
          *pte |= 1 << 9;
          *pte &= ~PTE_P;
          // printf("write: index: %u upage: %x\n", index, (uint32_t)f->upage);
          // if(f->upage == 0x804b000)
            // hex_dump(0, next_frame, PGSIZE, true);
        }
        else
          PANIC ("Frame not mapped!!!");
      }
      pagedir_clear_page (f->pd, f->upage);
      lock_release (&frame_lock);
      return next_frame;
    }
  }
  
  lock_release (&frame_lock);
  PANIC ("Kernel bug - No available frame!"); 
}
