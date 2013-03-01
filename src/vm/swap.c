#include <bitmap.h>
#include <stdio.h>
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
  lock_init (&swap_lock);
  swap_block = block_get_role (BLOCK_SWAP);
  if (swap_block == NULL)
    PANIC("No SWAP device allocated !!!");
  /* create bitmap as swap table data structure */
  swap_map = bitmap_create (block_size(swap_block) / SECTORS_PER_PAGE);
  if (swap_map == NULL)
    PANIC("Create bitmap failed !!!");
  bitmap_set_all (swap_map, false);
}

uint32_t
write_to_swap (void *frame)
{
  /* allocate from swap table */
  uint32_t index = get_next_block();
  if (index == BITMAP_ERROR)
    PANIC ("No enough SWAP space!!");
  
  /* write the page to the swap */
  uint32_t i;
  for(i = 0; i < SECTORS_PER_PAGE; i++)
  {
    block_write (swap_block, index * SECTORS_PER_PAGE + i,
                 frame + i * BLOCK_SECTOR_SIZE);
  }
  return index;
}

bool
read_from_swap (uint32_t index, void *frame)
{
  /* test if swap exist */
  lock_acquire (&swap_lock);
  if (bitmap_test (swap_map, index) == false)
  {
    lock_release (&swap_lock);
    return false;
  }
  lock_release (&swap_lock);
  
  /* read from swap slot */
  uint32_t i;
  for(i = 0; i < SECTORS_PER_PAGE; i++)
  {
    block_read (swap_block, index * SECTORS_PER_PAGE + i,
                frame + i * BLOCK_SECTOR_SIZE);
  }
  
  /* clear the swap table bit */
  lock_acquire (&swap_lock);
  bitmap_set (swap_map, index, false);
  lock_release (&swap_lock);
  return true;
}

/* free the swap table just by setting the bit to be false */
void
free_swap (uint32_t index)
{
  lock_acquire (&swap_lock);
  bitmap_set (swap_map, index, false);
  lock_release (&swap_lock);
}

/* get the next available swap slot */
static uint32_t
get_next_block(void)
{
  lock_acquire (&swap_lock);
  uint32_t page_idx = bitmap_scan_and_flip (swap_map, 0, 1, false);
  lock_release (&swap_lock);
  return page_idx;
}
