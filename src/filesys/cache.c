#include <hash.h>
#include <stdio.h>
#include <string.h>
#include "devices/block.h"
#include "devices/rtc.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include "filesys/cache.h"
#include "filesys/filesys.h"

static struct cache_block buffer_cache[CACHESIZE];
static char blocks[CACHESIZE * BLOCK_SECTOR_SIZE];

void
cache_init (void)
{

  int i;
  for (i = 0; i < CACHESIZE; i++)
  {
    buffer_cache[i].sector = 0;
    
    lock_init (&buffer_cache[i].cache_lock);
    cond_init (&buffer_cache[i].cache_available);

    buffer_cache[i].dirty = false;
    buffer_cache[i].valid = false;
    buffer_cache[i].io = false;

    buffer_cache[i].readers = 0;
    buffer_cache[i].writers = 0;
    buffer_cache[i].time = timer_ticks ();
    
    buffer_cache[i].data = &blocks[i * BLOCK_SECTOR_SIZE];
  }
}

void
cache_read_block (block_sector_t sector, void *buffer,
int chunk_size, int sector_ofs)
{
  struct cache_block *block = cache_lookup_block(sector);
  if (block == NULL)
    block = cache_get_block (sector);

  // ȥ��һ��
  /* When the old block is in IO phase */
  while (block->io)
  {
    lock_acquire (&block->cache_lock);
    cond_wait(&block->cache_available, &block->cache_lock);
    lock_release (&block->cache_lock);
    block = cache_get_block (sector);
  }
  
  lock_acquire (&block->cache_lock);
  block->readers++;
  lock_release (&block->cache_lock);
  
  memcpy(buffer, block->data + sector_ofs, chunk_size);
  
  lock_acquire (&block->cache_lock);
  block->readers--;
  lock_release (&block->cache_lock);
  
  if (block->readers == 0 && block->writers == 0)
  {
    lock_acquire (&block->cache_lock);
    cond_broadcast (&block->cache_available, &block->cache_lock);
    lock_release (&block->cache_lock);
  }
}

void
cache_write_block (block_sector_t sector, const void *buffer,
int chunk_size, int sector_ofs)
{
  struct cache_block *block = cache_lookup_block(sector);
  if (block == NULL)
    block = cache_get_block (sector);

  /* When the old block is in IO phase */
  while (block->io)
  {
    lock_acquire (&block->cache_lock);
    cond_wait(&block->cache_available, &block->cache_lock);
    lock_release (&block->cache_lock);
    block = cache_get_block (sector);
  }
  
  lock_acquire (&block->cache_lock);
  block->writers++;
  lock_release (&block->cache_lock);
  
  memcpy(block->data + sector_ofs, buffer, chunk_size);
  block->dirty = true;
  
  lock_acquire (&block->cache_lock);
  block->writers--;
  lock_release (&block->cache_lock);

  if (block->readers == 0 && block->writers == 0)
  {
    lock_acquire (&block->cache_lock);
    cond_broadcast (&block->cache_available, &block->cache_lock);
    lock_release (&block->cache_lock);
  }
}

struct cache_block *
cache_lookup_block (block_sector_t sector)
{
  int i;
  for (i = 0; i < CACHESIZE; i++)
  {
    if (buffer_cache[i].valid && buffer_cache[i].sector == sector)
      return &buffer_cache[i];
  }

  return NULL;
}

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
    if (buffer_cache[i].io == true)
      continue;

    if (buffer_cache[i].valid == false)
      return &buffer_cache[i];
    else
    {
      if (next_least_recent == -1 
       || buffer_cache[i].time < buffer_cache[next_least_recent].time)
        next_least_recent = i;
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
  
  if (next_clean_no_request != -1)
    return &buffer_cache[next_clean_no_request];
  if (next_no_request != -1)
    return &buffer_cache[next_no_request];
  if (next_least_recent != -1)
    return &buffer_cache[next_least_recent];

  PANIC ("All are doing IO");
}

struct cache_block *
cache_get_block (block_sector_t sector)
{
  struct cache_block *block = next_available_block();
  lock_acquire (&block->cache_lock);
  
  while (block->writers != 0 || block->readers != 0)
    cond_wait (&block->cache_available, &block->cache_lock);
  
  block->io = true;
  if (block->dirty)
  {
    block_write (fs_device, block->sector, block->data);
    block->dirty = false;
  }
  block_read (fs_device, sector, block->data);
  cond_broadcast (&block->cache_available, &block->cache_lock);
  
  
  block->sector = sector;
  block->dirty = false;
  block->time =  timer_ticks ();
  block->readers = 0;
  block->writers = 0;
  block->valid = true;
  
  block->io = false;
  lock_release (&block->cache_lock);
  return block;
}

void cache_put_block (struct cache_block *block)
{
  // lock_acquire (&block->cache_lock);
  // while (block->writers != 0 || block->readers != 0)
  //   cond_wait (&block->cache_available, &block->cache_lock);
  if (!block->dirty)
    return;
  block_write (fs_device, block->sector, block->data);
  block->dirty = false;  
  // lock_release (&block->cache_lock);
}

void cache_put_block_all (void)
{
  int i;
  for (i = 0; i < CACHESIZE; i++)
    cache_put_block (&buffer_cache[i]);
}

void cache_put_block_all_background (void)
{
  while(true)
  {
    if (filesys_finished)
      break;
    
    int i;
    for (i = 0; i < CACHESIZE; i++)
      cache_put_block (&buffer_cache[i]);
    timer_msleep (30 * 1000);
  }
}