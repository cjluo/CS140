#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <hash.h>
#include "devices/rtc.h"
#include "threads/synch.h"

struct cache_block
{
  block_sector_t sector;
  
  struct lock cache_lock;
  struct condition cond_available;
  
  bool dirty;
  bool valid;
  bool io;
  time_t time;
  
  uint32_t readers;
  uint32_t writers;
  
  void *data;
  
  struct hash_elem elem;
};

void cache_init (void);

struct cache_block *cache_lookup_block (block_sector_t);
void cache_update_block (struct cache_block *);
void cache_delete_block (struct cache_block *);
struct cache_block *next_available_block (void);
void cache_get_block (block_sector_t);
void cache_put_block (struct cache_block *);
void cache_read_block (block_sector_t, void *);
void cache_write_block (block_sector_t, const void *);

void cache_readahead (void);
void cache_writebehind (void);

#endif
