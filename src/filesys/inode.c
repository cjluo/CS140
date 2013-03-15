#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_SIZE 123
#define INDIRECT_SIZE 128
#define SECTOR_INDEX_SIZE 4


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long.
   We have direct sector, indirect sector, 
   and double indirect sector */
  struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    enum inode_type type;
    block_sector_t sector_indirect;     
    block_sector_t sector_double_indirect;
    block_sector_t sector_direct[DIRECT_SIZE];   /* First data sector. */
  };

static int inode_create_indirect (block_sector_t *, size_t);
static int inode_create_double_indirect (block_sector_t *, size_t);
static char zeros[BLOCK_SECTOR_SIZE];
static int inode_extend (struct inode *, off_t );


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}


/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    int extending_sector;
    struct inode_disk data;             /* Inode content. */
    struct lock extend_lock;
    struct lock eof_lock;
    struct lock length_lock;
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static int
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  int sector_pos = pos / BLOCK_SECTOR_SIZE;
  if (sector_pos < DIRECT_SIZE) {
    return inode->data.sector_direct[sector_pos];

  }
  else if (sector_pos < DIRECT_SIZE + INDIRECT_SIZE)
  {
    if ((int)inode->data.sector_indirect == -1)
    {
      return -1;
    }
    
    block_sector_t level;    
    cache_read_block (inode->data.sector_indirect, &level, 
                      SECTOR_INDEX_SIZE,
                      (sector_pos - DIRECT_SIZE) * SECTOR_INDEX_SIZE);
    return level;
  }
  else if (sector_pos < DIRECT_SIZE + INDIRECT_SIZE 
           + INDIRECT_SIZE * INDIRECT_SIZE)
  {  
    if ((int)inode->data.sector_double_indirect == -1)
    {
      return -1;
    }        
    block_sector_t level1;
    int level1_index = (sector_pos - DIRECT_SIZE - INDIRECT_SIZE) 
                       / INDIRECT_SIZE;
               
    cache_read_block (inode->data.sector_double_indirect, &level1, 
                      SECTOR_INDEX_SIZE, level1_index * SECTOR_INDEX_SIZE);
    if ((int)level1 == -1)
    {
      return -1;
    }     

    block_sector_t level2;
    int level2_index = (sector_pos - DIRECT_SIZE - INDIRECT_SIZE) 
                       % INDIRECT_SIZE;
    cache_read_block (level1, &level2, SECTOR_INDEX_SIZE, 
                      level2_index * SECTOR_INDEX_SIZE);
    return level2;
  }
  else{
    return -1;

  }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, enum inode_type type)
{
  struct inode_disk *disk_inode = NULL;
  bool success = true;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
    memset (disk_inode, -1, sizeof *disk_inode);    
    int sectors = bytes_to_sectors (length);
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    disk_inode->type = type;

    int i;
    for (i = 0; i <= sectors && i < DIRECT_SIZE; i++)
    {
      if(free_map_allocate (1, &disk_inode->sector_direct[i]))
         cache_write_block (disk_inode->sector_direct[i], zeros, 
                            BLOCK_SECTOR_SIZE, 0);
      else
      {
        free (disk_inode);
        return false;
      }
    }

    cache_write_block (sector, disk_inode, BLOCK_SECTOR_SIZE, 0);
    free (disk_inode);
  }
  else
    return false;
  return success;
}

void
inode_finish (void)
{
  struct list_elem *e;
  struct inode *inode;

  while (!list_empty (&open_inodes))
  {
    e = list_pop_front (&open_inodes);
    inode = list_entry (e, struct inode, elem);
    inode_close (inode);
  }
}


/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  
  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
      {
        return inode_reopen (inode);
      }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  inode->extending_sector = -1;
  lock_init (&inode->extend_lock);
  lock_init (&inode->eof_lock);
  lock_init (&inode->length_lock);
  cache_read_block (inode->sector, &inode->data, BLOCK_SECTOR_SIZE, 0);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode == NULL || inode->removed)
    return NULL;

  inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          int i;
          if ((int)inode->data.sector_indirect != -1)
          {
            int indirect[INDIRECT_SIZE];
            cache_read_block (inode->data.sector_indirect, indirect,
                              BLOCK_SECTOR_SIZE, 0);
            for (i = 0; i < INDIRECT_SIZE; i++)
            {
              if(indirect[i] != -1)
                free_map_release (indirect[i], 1);
            }
            free_map_release (inode->data.sector_indirect, 1);
          }
          for (i = 0; i < DIRECT_SIZE; i++)
          {
            if((int)inode->data.sector_direct[i] != -1)
              free_map_release (inode->data.sector_direct[i], 1);
          }
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  // printf ("remove1 inode %x removed %s\n", inode, inode->removed ? "YES": "NO");
  inode->removed = true;
  // printf ("remove2 inode %x removed %s\n", inode, inode->removed ? "YES": "NO");
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      // printf ("## byte_to_sector1 %d \n", offset);
      int sector_idx = byte_to_sector (inode, offset);
      // printf ("## byte_to_sector2 %d\n", sector_idx);

      if (sector_idx == -1)
      {
        lock_acquire (&inode->length_lock);
        int inode_len = inode_length (inode);
        lock_release (&inode->length_lock);

        /* read unallocated sectors should fill with zeros*/
        if (offset <= inode_len)
        {
          lock_acquire (&inode->extend_lock);      
          sector_idx = inode_extend (inode, offset);
          lock_release (&inode->extend_lock);     
        }
        else /* read eof return 0 */
        {
          return 0;
        }
   
      }
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      cache_read_block (sector_idx, buffer + bytes_read,
                        chunk_size, sector_ofs);
      
      int next_sector_idx = byte_to_sector (inode, offset + BLOCK_SECTOR_SIZE);
      if (next_sector_idx != -1)
        cache_readahead (next_sector_idx);
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      int sector_idx = byte_to_sector (inode, offset);
      // printf ("## byte_to_sector3 %d\n", sector_idx);

      if (sector_idx == -1)
      {
        lock_acquire (&inode->length_lock);
        int inode_len = inode_length (inode);
        lock_release (&inode->length_lock);

        int return_value = -1;
        /* write unallocated sectors should fill with zeros*/
        if (offset <= inode_len)
        {
          lock_acquire (&inode->extend_lock);      
          return_value = inode_extend (inode, offset);
          lock_release (&inode->extend_lock);     
        }
        else /* write at EOF  */
        {
          lock_acquire (&inode->eof_lock);      
          int sectors = offset / BLOCK_SECTOR_SIZE;
          if (sectors > inode->extending_sector) {
            return_value = inode_extend (inode, offset);
            if (return_value >0)
              inode->extending_sector = sectors;
          }
          lock_release (&inode->eof_lock);     
        }

        if (return_value <=0)
          break;
        sector_idx = (block_sector_t)return_value;     
        cache_write_block (inode->sector, &inode->data, BLOCK_SECTOR_SIZE, 0);
      }

      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */

      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs; 
      ASSERT (sector_left >= 0);

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < sector_left ? size : sector_left;

      if (chunk_size == 0)
        break;

      cache_write_block (sector_idx, buffer + bytes_written,
                         chunk_size, sector_ofs);
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
    
    lock_acquire (&inode->length_lock);
    off_t inode_left = inode_length (inode) - offset;
    if (inode_left <= 0)
      inode->data.length = offset;
    lock_release (&inode->length_lock);

    if (inode_left <= 0)
      cache_write_block (inode->sector, &inode->data, BLOCK_SECTOR_SIZE, 0);

  return bytes_written;  
}

static int 
inode_extend (struct inode *inode, off_t offset)
{
  struct inode_disk *disk_inode = &inode->data;
  if (disk_inode == NULL)
    return -1;

  int sectors = offset / BLOCK_SECTOR_SIZE;

  if (sectors < DIRECT_SIZE)
  {
    if(free_map_allocate (1, &disk_inode->sector_direct[sectors]))
    {
      cache_write_block (disk_inode->sector_direct[sectors], zeros, 
                              BLOCK_SECTOR_SIZE, 0);
      return disk_inode->sector_direct[sectors];
    }  
    return -1;
  } 

  sectors -= DIRECT_SIZE;

  if (sectors >= 0 && sectors < INDIRECT_SIZE)
    return inode_create_indirect (&disk_inode->sector_indirect, sectors);

  sectors -= INDIRECT_SIZE;
  if (sectors >= 0)
    return inode_create_double_indirect (&disk_inode->sector_double_indirect,
                                         sectors);
  return -1;
}

static int 
inode_create_indirect (block_sector_t *sector_indirect, size_t sectors)
{
    block_sector_t indirect_idx; 

    if(free_map_allocate (1, &indirect_idx))
      cache_write_block (indirect_idx, zeros, BLOCK_SECTOR_SIZE, 0);
    else
      return -1;

  if ((int)*sector_indirect == -1)
  {
    if (free_map_allocate (1, sector_indirect))
    {
      block_sector_t indirect[INDIRECT_SIZE];
      memset (indirect, -1, INDIRECT_SIZE * SECTOR_INDEX_SIZE);    
      indirect[sectors] = indirect_idx;
      cache_write_block (*sector_indirect, indirect, BLOCK_SECTOR_SIZE, 0);
      return indirect_idx;
    }
    else
      return -1;
  }
  else
  {
    cache_write_block (*sector_indirect, &indirect_idx, SECTOR_INDEX_SIZE, 
                       sectors * SECTOR_INDEX_SIZE);
    return indirect_idx;

  }

  return -1;
}

static int 
inode_create_double_indirect (block_sector_t *sector_double_indirect, size_t sectors)
{
  if ((int)*sector_double_indirect == -1)
  {
    if (free_map_allocate (1, sector_double_indirect))
    {
      block_sector_t indirect[INDIRECT_SIZE];
      memset (indirect, -1, INDIRECT_SIZE * SECTOR_INDEX_SIZE);    
      cache_write_block (*sector_double_indirect, indirect, BLOCK_SECTOR_SIZE, 
                         0);
    }
    else
      return -1;
  }

  block_sector_t sector_indirect;
  int sectors_indirect = sectors / INDIRECT_SIZE;
  cache_read_block (*sector_double_indirect,
                    &sector_indirect,
                    SECTOR_INDEX_SIZE, 
                    sectors_indirect * SECTOR_INDEX_SIZE);
                      
  int return_value = inode_create_indirect (&sector_indirect,
                                            sectors % INDIRECT_SIZE);
  cache_write_block (*sector_double_indirect,
                     &sector_indirect,
                     SECTOR_INDEX_SIZE, 
                     sectors_indirect * SECTOR_INDEX_SIZE);
  return return_value;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

enum inode_type
inode_type (const struct inode *inode)
{
  return inode->data.type;
}

block_sector_t
inode_sector (const struct inode *inode)
{
  return inode->sector;
}

int
inode_open_cnt (const struct inode *inode)
{
  return inode->open_cnt;
}