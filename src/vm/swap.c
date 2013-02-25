#include <bitmap.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "vm/swap.h"


static struct bitmap *swap_map;
static struct block *swap_block;
static struct lock swap_lock;

static uint32_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;
static uint32_t get_next_block(void);

void
swap_table_init (void)
{
  swap_block = block_get_role (BLOCK_SWAP);
  swap_map = bitmap_create (block_size(swap_block) / SECTORS_PER_PAGE);
  if (swap_map == NULL)
    PANIC("Not enough block space");
  lock_init (&swap_lock);
}

uint32_t
write_to_swap (void *frame)
{
  uint32_t index = get_next_block();
  uint32_t i;
  
  lock_acquire (&swap_lock);
  for(i = 0; i < SECTORS_PER_PAGE; i++)
  {
    block_write (swap_block, index * SECTORS_PER_PAGE + i,
                 frame + i * SECTORS_PER_PAGE);
  }
  lock_release (&swap_lock);
  
  return index;
}

bool
read_from_swap (uint32_t index, void *frame)
{
  lock_acquire (&swap_lock);

  if (bitmap_test (swap_map, index) == false)
  {
    lock_release (&swap_lock);
    return false;
  }
  
  uint32_t i;
  for(i = 0; i < SECTORS_PER_PAGE; i++)
  {
    block_read (swap_block, index * SECTORS_PER_PAGE + i,
                frame + i * SECTORS_PER_PAGE);
  }
  
  bitmap_set (swap_map, index, false);
  lock_release (&swap_lock);
  return true;
}

void
free_swap (uint32_t index)
{
  lock_acquire (&swap_lock);
  bitmap_set (swap_map, index, false);
  lock_release (&swap_lock);
}

static uint32_t
get_next_block(void)
{
  lock_acquire (&swap_lock);
  uint32_t page_idx = bitmap_scan_and_flip (swap_map, 0, 1, false);
  lock_release (&swap_lock);
  return page_idx;
}
