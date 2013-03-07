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
static struct hash cache_table;

unsigned cache_hash (const struct hash_elem *c_, void *aux UNUSED);
bool cache_less (const struct hash_elem *a_, const struct hash_elem *b_,
void *aux UNUSED);

/* Returns a hash value for cache c. */
unsigned
cache_hash (const struct hash_elem *c_, void *aux UNUSED)
{
  const struct cache_block *c = hash_entry (c_, struct cache_block, elem);
  return hash_bytes (&c->sector, sizeof c->sector);
}

/* Returns true if cache a precedes cache b. */
bool
cache_less (const struct hash_elem *a_, const struct hash_elem *b_,
void *aux UNUSED)
{
  const struct cache_block *a = hash_entry (a_, struct cache_block, elem);
  const struct cache_block *b = hash_entry (b_, struct cache_block, elem);
  return a->sector < b->sector;
}

void
cache_init (void)
{
  hash_init (&cache_table, cache_hash, cache_less, NULL);
  
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
  }
}

void
cache_read_block (block_sector_t sector, void *buffer)
{
  struct cache_block *block = cache_lookup_block(sector);
  if (block == NULL)
  {
    cache_get_block (sector);
    cache_read_block (sector, buffer);
  }
  else
    memcpy(buffer, block->data, BLOCK_SECTOR_SIZE);
}

void
cache_write_block (block_sector_t sector, const void *buffer)
{
  struct cache_block *block = cache_lookup_block(sector);
  if (block == NULL)
  {
    cache_get_block (sector);
    cache_write_block (sector, buffer);
  }
  else
  {
    memcpy(block->data, buffer, BLOCK_SECTOR_SIZE);
    block->dirty = true;
  }
}

struct cache_block *
cache_lookup_block (block_sector_t sector)
{
  struct cache_block block;
  block.sector = sector;
  struct hash_elem *e = hash_find (&cache_table, &block.elem);
  return e != NULL ? hash_entry (e, struct cache_block, elem) : NULL;
}

void
cache_update_block (struct cache_block *block)
{
  hash_replace (&cache_table, &block->elem);
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

void cache_get_block (block_sector_t sector)
{
  struct cache_block *block = next_available_block();
  block->io = true;
  if (block->dirty)
    cache_put_block (block);
  block_read (fs_device, sector, &block->data);
  
  block->sector = sector;
  block->dirty = false;
  block->time =  timer_ticks ();
  block->readers = 0;
  block->writers = 0;
  
  cache_update_block (block);
  block->io = false;
}

void cache_put_block (struct cache_block *block)
{
  block_write (fs_device, block->sector, &block->data);
  block->dirty = false;
}