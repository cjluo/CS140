#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_finished = false;
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  cache_init();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  /* write back dirty to disk */
  filesys_finished = true;
  cache_put_block_all ();
  free_map_close ();
  inode_finish ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, enum inode_type type)
{
  block_sector_t inode_sector = 0;
  char *file_name = NULL;
  struct dir *dir = dir_parse(name, &file_name);
  if (dir == NULL)
  {
    free (file_name);
    return false;
  }
  
  lock_acquire (&dir->dir_lock);
  bool success = (free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, type)
                  && dir_add (dir, file_name, inode_sector));
  lock_release (&dir->dir_lock);
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  
  dir_close (dir);

  free (file_name);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char *file_name = NULL;
  struct dir *dir = dir_parse(name, &file_name);
  if (dir == NULL)
  {
    free (file_name);
    return NULL;
  }
  
  if (strcmp(file_name, "..") == 0)
  {
    dir_close (dir);
    free (file_name);
    return NULL;
  }
  
  struct inode *inode = NULL;

  dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  free (file_name);
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char *file_name = NULL;
  struct dir *dir = dir_parse(name, &file_name);
  if (dir == NULL)
  {
    free (file_name);
    return false;
  }
  if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0)
  {
    free (file_name);
    dir_close (dir);
    return false;
  }

  lock_acquire (&dir->dir_lock);
  struct inode *inode = NULL;
  dir_lookup (dir, file_name, &inode);
  
  if (inode != NULL && inode_type (inode) == DIR)
  {
    struct dir *rm_dir = dir_open(inode);
    
    if (!dir_empty (rm_dir))
    {
      free (file_name);
      lock_release (&dir->dir_lock);
      dir_close (dir);
      dir_close (rm_dir);
      return false;
    }
    dir_close_free (rm_dir);
  }
  
  inode_close (inode);
  bool success = dir_remove (dir, file_name);
  lock_release (&dir->dir_lock);
  dir_close (dir); 
  free (file_name);
  
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  dir_create_dot ("//");
  printf ("done.\n");
}