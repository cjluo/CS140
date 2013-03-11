#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <hash.h>
#include "devices/rtc.h"
#include "threads/synch.h"

#define CACHESIZE 64

bool filesys_finished;

struct cache_block
{
  block_sector_t sector;
  
  struct lock cache_lock;
  struct condition cache_available;
  
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
struct cache_block *next_available_block (void);
struct cache_block *cache_get_block (block_sector_t);
void cache_put_block (struct cache_block *);
void cache_put_block_all (void);
void cache_read_block (block_sector_t, void *, int, int);
void cache_write_block (block_sector_t, const void *, int, int);

void cache_put_block_all_background (void);

void cache_readahead (block_sector_t);
void cache_get_block_all_background (void);
#endif
