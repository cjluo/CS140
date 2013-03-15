#include <list.h>
#include <stdio.h>
#include <string.h>
#include "devices/block.h"
#include "devices/rtc.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
#include "filesys/filesys.h"

static struct cache_block buffer_cache[CACHESIZE]; /* The cache block header */
static char blocks[CACHESIZE * BLOCK_SECTOR_SIZE];  /* The cache block space */
static struct list readahead_list;             /* A queue for read ahead task*/
static struct lock readahead_lock;   /* Read ahead lock to protect the queue */
static struct lock buffer_cache_lock;        /* buffer cache lock to protect */
                                             /* the block header */
static struct condition readahead_list_not_empty;    /* read ahead not empty */
static struct condition buffer_available;            /* read ahead not empty */
static struct cache_block *cache_lookup_helper (block_sector_t);

struct readahead_block             /* read ahead block frame */
{
  block_sector_t sector;
  struct list_elem elem;
};

void
cache_init (void)
{
  int i;
  for (i = 0; i < CACHESIZE; i++)
  {
    buffer_cache[i].sector = -1;
    buffer_cache[i].next_sector = -1;
    
    lock_init (&buffer_cache[i].cache_lock);
    cond_init (&buffer_cache[i].cache_available);

    buffer_cache[i].dirty = false;
    buffer_cache[i].io = false;

    buffer_cache[i].readers = 0;
    buffer_cache[i].writers = 0;
    buffer_cache[i].time = timer_ticks ();
    
    buffer_cache[i].data = &blocks[i * BLOCK_SECTOR_SIZE];
  }
  
  list_init (&readahead_list);
  lock_init (&readahead_lock);
  lock_init (&buffer_cache_lock);  
  cond_init (&readahead_list_not_empty);
  cond_init (&buffer_available);
}

/* cache_read_block, which is layer between inode read and block read */
void
cache_read_block (block_sector_t sector, void *buffer,
int chunk_size, int sector_ofs)
{
  /* look up for the block */
  struct cache_block *block = cache_get_block(sector);
  if (block == NULL)
    return;

  /* lock the block sector lock and wait for io if any */
  lock_acquire (&block->cache_lock);
  while (block->io)
    cond_wait(&block->cache_available, &block->cache_lock);

  block->readers++;
  lock_release (&block->cache_lock);
  
  /* memcpy operation is not blocked */
  memcpy(buffer, block->data + sector_ofs, chunk_size);
  
  lock_acquire (&block->cache_lock);
  block->readers--;

  /* If the current block is idle, signal the other threads */
  if (block->readers == 0 && block->writers == 0)
    cond_broadcast (&block->cache_available, &block->cache_lock);

  lock_release (&block->cache_lock);
}

/* cache_write_block, which is layer between inode write and block write */
void
cache_write_block (block_sector_t sector, const void *buffer,
int chunk_size, int sector_ofs)
{
  /* look up for the block */
  struct cache_block *block = cache_get_block(sector);
  if (block == NULL)
    return;

  /* lock the block sector lock and wait for io if any */
  lock_acquire (&block->cache_lock);  
  while (block->io)
  {
    cond_wait(&block->cache_available, &block->cache_lock);
  }

  block->writers++;
  lock_release (&block->cache_lock);

  /* memcpy operation is not blocked */  
  memcpy(block->data + sector_ofs, buffer, chunk_size);
  block->dirty = true;
  
  lock_acquire (&block->cache_lock);
  block->writers--;
  
  /* If the current block is idle, signal the other threads */
  if (block->readers == 0 && block->writers == 0)
    cond_broadcast (&block->cache_available, &block->cache_lock);
  
  lock_release (&block->cache_lock);
}

/* The helper just returns the block matching the sector number,
 * it is called helding buffer_cache_lock.  
 */
struct cache_block *
cache_lookup_helper (block_sector_t sector)
{
  ASSERT ((int)sector != -1);
  int i;

  /* scan the buffer cache and CACHESIZE */
  for (i = 0; i < CACHESIZE; i++)
  {
    if (buffer_cache[i].sector == sector)
      return &buffer_cache[i];
  }
  return NULL;
}

/* */
struct cache_block *
cache_lookup_block (block_sector_t sector)
{
  ASSERT ((int)sector != -1);
  
  /* buffer_cache_lock is the cache lock for the whole buffer cache */
  lock_acquire (&buffer_cache_lock);
  struct cache_block *return_value = cache_lookup_helper (sector);
  
  /* cache found */
  if (return_value != NULL)
  {
    return_value->next_sector = -1; 
    lock_release (&buffer_cache_lock);
    return return_value;
  }

  /* cache not found
   *
   * call next available block()
   * if it returns valid sector, return the sector
   * if it returns NULL, it means currently cannot find block,
   * conditional wait.
   */ 
  while((return_value = next_available_block ()) == NULL)
    cond_wait (&buffer_available, &buffer_cache_lock);
  return_value->next_sector = sector; 

  lock_release (&buffer_cache_lock);
  return return_value;

}

/* LRU to get next block */
struct cache_block *
next_available_block (void)
{
  /* there are several kinds of blocks */
  /* 1. kick out clean without request */
  /* 2. kick out without request */
  /* 3. kick out least recent */

  /* least recent without request */
  int next_clean_no_request = -1;
  /* least recent without current request */
  int next_no_request = -1;
  /* least recent*/
  int next_least_recent = -1;
  int i;
  for (i = 0; i < CACHESIZE; i++)
  {
    /* cannot kick the IO buffer cache,
     * if all buffer cache are marked as IO, we will return NULL
     * which will be a condition wait. 
     */
    if (buffer_cache[i].io == true)
      continue;

    if ((int)buffer_cache[i].sector == -1)
      return &buffer_cache[i];
    else
    {
      /* record the next least recent used */
      if (next_least_recent == -1 
       || buffer_cache[i].time < buffer_cache[next_least_recent].time)
        next_least_recent = i;

      /* check if there are no reading and writing on the buffer cache */
      if (buffer_cache[i].writers == 0 && buffer_cache[i].readers == 0)
      {
        if (next_no_request == -1 
         || buffer_cache[i].time < buffer_cache[next_no_request].time)
          next_no_request = i;
        if (buffer_cache[i].dirty == false)
        {
          if (next_clean_no_request == -1 
           || buffer_cache[i].time < buffer_cache[next_clean_no_request].time)
            next_clean_no_request = i;
        }
      }
    }
  }
  
  /*
   * (1) if we found a block that is clean and no request,
            return that block
     (2) else if there is a block with no request, but it is dirty
            return that block
     (3) else if there is the least recent used block
            return that block
     (4) else 
            return NULL. it means that all the buffer cache are marked as IO
            and the caller will be on conditional wait.

   */

  if (next_clean_no_request != -1)
    return &buffer_cache[next_clean_no_request];
  if (next_no_request != -1)
    return &buffer_cache[next_no_request];
  if (next_least_recent != -1)
    return &buffer_cache[next_least_recent];

  return NULL;
}

struct cache_block *
cache_get_block (block_sector_t sector)
{

  struct cache_block *block = cache_lookup_block (sector);
  if ((int)block->next_sector == -1)
    return block;

  lock_acquire (&block->cache_lock);
  
  /* conditional wait if there are read/write on the buffer cache*/
  while (block->writers != 0 || block->readers != 0)
    cond_wait (&block->cache_available, &block->cache_lock);
  
  /* write to disk if the data is dirty */
  block->io = true;
  if (block->dirty)
  {
    block_write (fs_device, block->sector, block->data);
    block->dirty = false;
  }

  /* read block from disk, 
     and then update the block information*/
  block_read (fs_device, sector, block->data);
  block->sector = sector;
  block->next_sector = -1;  
  block->dirty = false;
  block->time = timer_ticks ();
  block->readers = 0;
  block->writers = 0;
  
  block->io = false;
  cond_broadcast (&block->cache_available, &block->cache_lock);
  lock_release (&block->cache_lock);
  
  /* signal the IO finished for next_available block */
  lock_acquire (&buffer_cache_lock);
  cond_broadcast (&buffer_available, &buffer_cache_lock);
  lock_release (&buffer_cache_lock);

  return block;
}

void cache_put_block (struct cache_block *block)
{
  if (!block || !block->dirty)
    return;

  block->io = true;
  block_write (fs_device, block->sector, block->data);
  block->dirty = false;  
  block->io = false;

  /* signal the IO finished for next_available block */
  lock_acquire (&buffer_cache_lock);
  cond_broadcast (&buffer_available, &buffer_cache_lock);
  lock_release (&buffer_cache_lock);
}

/* helper function */
void
cache_put_block_all (void)
{
  int i;
  for (i = 0; i < CACHESIZE; i++)
    cache_put_block (&buffer_cache[i]);
}

/* put all block in background */
void
cache_put_block_all_background (void)
{
  while(true)
  {
    if (filesys_finished)
      break;
    
    cache_put_block_all();
    timer_msleep (30 * 1000);
  }
}


/* read ahead, and push */
void
cache_readahead (block_sector_t sector)
{
  struct readahead_block *b = malloc (sizeof(struct readahead_block));
  if (b == NULL)
    return;
  b->sector = sector;
  lock_acquire (&readahead_lock);
  list_push_front (&readahead_list, &b->elem);
  cond_signal (&readahead_list_not_empty, &readahead_lock);
  lock_release (&readahead_lock);
  return;
}


void
cache_get_block_all_background (void)
{
  while (true)
  {
    if (filesys_finished)
      break;

    /* read ahead the block */
    lock_acquire (&readahead_lock);

    while (list_empty (&readahead_list))
      cond_wait (&readahead_list_not_empty, &readahead_lock);
    
    struct list_elem *e = list_pop_front (&readahead_list);
    struct readahead_block *b = list_entry (e, struct readahead_block, elem);
    
    cache_get_block (b->sector);
    
    free (b);
    lock_release (&readahead_lock);
  }
  
  /* free the read ahead list*/
  lock_acquire (&readahead_lock);
  while (!list_empty (&readahead_list))
  {
    struct list_elem *e = list_pop_front (&readahead_list);
    struct readahead_block *b = list_entry (e, struct readahead_block, elem);
    free (b);
  }
  lock_release (&readahead_lock);
}
