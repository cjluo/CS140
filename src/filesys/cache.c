#include <hash.h>
#include <stdio.h>
#include <string.h>
#include "devices/block.h"
#include "devices/rtc.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include "filesys/cache.h"
#include "filesys/filesys.h"
#define CACHESIZE 64

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
    cond_init (&buffer_cache[i].cond_available);

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
cache_read_block (block_sector_t sector, void *buffer)
{
  struct cache_block *block = cache_lookup_block(sector);
  if (block == NULL)
    block = cache_get_block (sector);

  memcpy(buffer, block->data, BLOCK_SECTOR_SIZE);
}

void
cache_write_block (block_sector_t sector, const void *buffer)
{
  struct cache_block *block = cache_lookup_block(sector);
  if (block == NULL)
    block = cache_get_block (sector);

  memcpy(block->data, buffer, BLOCK_SECTOR_SIZE);
  block->dirty = true;
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
  block->io = true;
  if (block->dirty)
    cache_put_block (block);
  block_read (fs_device, sector, block->data);
  
  block->sector = sector;
  block->dirty = false;
  block->time =  timer_ticks ();
  block->readers = 0;
  block->writers = 0;
  block->valid = true;
  
  block->io = false;
  return block;
}

void cache_put_block (struct cache_block *block)
{
  block_write (fs_device, block->sector, block->data);
  block->dirty = false;
}