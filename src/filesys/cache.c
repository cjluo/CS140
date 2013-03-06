#include "devices/block.h"
#include "devices/rtc.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include "filesys/cache.h"
#define CACHESIZE 64

struct cache_block
{
  struct lock cache_lock;
  bool dirty;
  time_t time;
  char sector[BLOCK_SECTOR_SIZE];
};

static struct cache_block buffer_cache[CACHESIZE];

void
cache_init (void)
{
  int i;
  for (i = 0; i < CACHESIZE; i++)
  {
    lock_init (&buffer_cache[i].cache_lock);
    buffer_cache[i].dirty = false;
    buffer_cache[i].time = timer_ticks ();
  }
}
