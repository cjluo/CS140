#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "devices/input.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <string.h>

static void syscall_handler (struct intr_frame *);

static inline void check_valid_address (const void *);

static int sys_halt (void);
static int sys_exec (const char *);
static int sys_wait (int);
static int sys_create (const char *, unsigned);
static int sys_remove (const char *);
static int sys_open (const char *);
static int sys_filesize (int);
static int sys_read (int, void *, unsigned);
static int sys_write (int, const void *, unsigned);
static int sys_seek (int, unsigned);
static int sys_tell (int);
static int sys_close (int);
static int fd_gen (void);
static struct fd_frame * fd_to_fd_frame (int);

static int sys_chdir (const char *);
static int sys_mkdir (const char *);
static int sys_readdir (int, char *);
static int sys_isdir (int);
static int sys_inumber (int);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}
/*
* syscall_handler deal with system calls according to syscal_num,
* validates address and calls corresponding function 
*/

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t *esp = (uint32_t *) f->esp;
  check_valid_address (esp);

  int syscall_num = *esp;

  if (syscall_num < SYS_HALT || SYS_HALT > SYS_INUMBER)
    sys_exit (-1);
  
  int return_value = -1;
  switch(syscall_num)
  {
    case SYS_HALT:
      return_value = sys_halt ();
      break;
    case SYS_EXIT:
      check_valid_address (esp+1);
      return_value = sys_exit ((int)*(esp+1));
      break;
    case SYS_EXEC:
      check_valid_address (esp+1);
      return_value = sys_exec ((const char *)*(esp+1));
      break;
    case SYS_WAIT:
      check_valid_address (esp+1);
      return_value = sys_wait ((int)*(esp+1));
      break;
    case SYS_CREATE:
      check_valid_address (esp+1);
      check_valid_address (esp+2);
      return_value = sys_create ((const char *)*(esp+1), (unsigned)*(esp+2));
      break;
    case SYS_REMOVE:
      check_valid_address (esp+1);
      return_value = sys_remove ((const char *)*(esp+1));
      break;
    case SYS_OPEN:
      check_valid_address (esp+1);
      return_value = sys_open ((char *)*(esp+1));
      break;
    case SYS_FILESIZE:
      check_valid_address (esp+1);
      return_value = sys_filesize ((int)*(esp+1));
      break;
    case SYS_READ:
      check_valid_address (esp+1);
      check_valid_address (esp+2);
      check_valid_address (esp+3);
      return_value = sys_read ((int)*(esp+1),
                               (void *)*(esp+2),
                               (unsigned)*(esp+3));
      break;
    case SYS_WRITE:
      check_valid_address (esp+1);
      check_valid_address (esp+2);
      check_valid_address (esp+3);
      return_value = sys_write ((int)*(esp+1),
                                (void *)*(esp+2),
                                (unsigned)*(esp+3));    
      break;
    case SYS_SEEK:
      check_valid_address (esp+1);
      check_valid_address (esp+2);
      return_value = sys_seek ((int)*(esp+1), (unsigned)*(esp+2));
      break;
    case SYS_TELL:
      check_valid_address (esp+1);
      return_value = sys_tell ((int)*(esp+1));
      break;
    case SYS_CLOSE:
      check_valid_address (esp+1);
      return_value = sys_close ((int)*(esp+1));
      break;
    case SYS_CHDIR:
      check_valid_address (esp+1);
      return_value = sys_chdir ((const char *)*(esp+1));
      break;
    case SYS_MKDIR:
      check_valid_address (esp+1);
      return_value = sys_mkdir ((const char *)*(esp+1));
      break;
    case SYS_READDIR:
      check_valid_address (esp+1);
      check_valid_address (esp+2);
      return_value = sys_readdir ((int)*(esp+1), (char *)*(esp+2));
      break;
    case SYS_ISDIR:
      check_valid_address (esp+1);
      return_value = sys_isdir ((int)*(esp+1));
      break;
    case SYS_INUMBER:
      check_valid_address (esp+1);
      return_value = sys_inumber ((int)*(esp+1));
      break;
    default:
      ASSERT (false);
  }

  f->eax = (uint32_t) return_value;

  return;

}


/* terminate current user thread and close all files it opened*/
int
sys_exit (int status)
{
  struct thread *t = thread_current ();
  printf("%s: exit(%d)\n", t->name, status);
  t->exit_status = status;
  thread_exit();
  return -1;
}

/* Simply call shutdown_power_off */
static int
sys_halt (void)
{
  shutdown_power_off ();
}

/* validate address and call process_execute */
static int
sys_exec (const char *cmd_line)
{
  check_valid_address (cmd_line);
  /* avoid argument overflow */
  if (strlen (cmd_line) + 1 > 4096)
    sys_exit (-1);
  int tid = process_execute (cmd_line);
  return tid;
}

static int
sys_wait (int tid)
{
  return process_wait(tid);
}

/* aquire the lock for file system and call filesys_create*/
static int
sys_create (const char *file, unsigned initial_size)
{ 
  /* test address */
  check_valid_address (file);
  // lock_acquire (&file_lock);
  bool return_value = filesys_create (file, initial_size, FILE);
  // lock_release (&file_lock);
  return (int) return_value;
}

/* validate address and call filesys_remove */
static int
sys_remove (const char *file)
{
  /* test address */
  check_valid_address (file);
  // lock_acquire (&file_lock);
  int return_value = (int) filesys_remove (file);
  // lock_release (&file_lock);
  return return_value;
}

/* validate address;
 * aquire lock for file system;
 * add the opened file to the thread's file list
*/
static int
sys_open (const char *file)
{
  /* test address */
  check_valid_address (file);

  // lock_acquire (&file_lock);
  struct file *f = filesys_open (file);
  // lock_release (&file_lock);
  
  if (!f)
    return -1;

  
  struct fd_frame *fd_open_frame = (struct fd_frame *) malloc (
                                    sizeof (struct fd_frame));
  if (fd_open_frame == NULL)
    sys_exit (-1);
  if(!fd_open_frame)
  {
    // lock_acquire (&file_lock);
    file_close (f);
    // lock_release (&file_lock);
    return -1;
  }
  fd_open_frame->file = f;
  fd_open_frame->fd = fd_gen ();
  list_push_back (&thread_current ()->file_list, &fd_open_frame->elem);
  return fd_open_frame->fd;
}

static int
sys_filesize (int fd)
{
  struct fd_frame *f = fd_to_fd_frame (fd);
  if (f)
    return (int) file_length (f->file);
  return -1;
}

/* validate address;
 * if fd is STDIN_FILENO, read from console
 * else, search the file list with fd and read
 */
static int
sys_read (int fd, void *buffer, unsigned size)
{
  if (size == 0)
    return 0;  
  check_valid_address (buffer);
  check_valid_address (buffer + size - 1);
  
  if (fd < 0 || fd == 1)
    return -1;

  if (fd == STDIN_FILENO)
  {
    unsigned i;
    /* what if we get EOF */
    for (i = 0; i != size; ++i)
      *(char *)(buffer + i) = input_getc ();
    return size;
  }
  
  struct fd_frame *f = fd_to_fd_frame (fd);
  if (f)
  {
    // lock_acquire (&file_lock);
    off_t return_value = file_read (f->file, buffer, size);
    // lock_release (&file_lock);
    return (int) return_value;
  }
  
  return -1;
}

/* validate address;
 * if fd is STDOUT_FILENO, write to console
 * else, search the file list with fd and write
 */
static int
sys_write (int fd, const void *buffer, unsigned size)
{
  if (size == 0)
    return 0;  
  check_valid_address (buffer);
  check_valid_address (buffer + size - 1);

  if (fd <= 0)
    return -1;
  
  if (fd == STDOUT_FILENO)
  {
    putbuf (buffer, size);
    return size;
  }
  else
  {
    struct fd_frame *f = fd_to_fd_frame (fd);
    if(f)
    {
      // lock_acquire (&file_lock);    
      int return_value = file_write (f->file, buffer, size);
      // lock_release (&file_lock);
      return (int) return_value;
    }
  }
  return -1;
}

static int
sys_seek (int fd, unsigned position)
{
  struct fd_frame *f = fd_to_fd_frame (fd);
  if(f)
    file_seek (f->file, position);
  return -1;
}


static int
sys_tell (int fd)
{
  struct fd_frame *f = fd_to_fd_frame (fd);
  if(f)
    return file_tell (f->file);
  return -1;
}
/* close and remove file from this thread's file list */
static int
sys_close (int fd)
{
  if (fd < 2)
    return -1;
  
  struct fd_frame *f = fd_to_fd_frame (fd);
  if (f)
  {
    // lock_acquire (&file_lock);
    file_close (f->file);
    // lock_release (&file_lock);
    list_remove (&f->elem);
    free (f);
  }
  return -1;
}

/* called when we need the next fd */
static int
fd_gen (void)
{
  static int fd = 1;
  return ++fd;
}

/* fild fd_frame by fd */
static struct fd_frame *
fd_to_fd_frame (int fd)
{
  struct list *l = &thread_current () -> file_list;
  struct list_elem *e;
  for (e = list_begin (l); e != list_end (l); e = list_next (e))
  {
    struct fd_frame *f = list_entry (e, struct fd_frame, elem);
    if (f->fd == fd)
      return f;
  }
  return NULL;
}

/* validate address */
static inline void
check_valid_address (const void *address)
{
  if (!is_user_vaddr (address) || address == NULL)
    sys_exit (-1);
}

static int
sys_chdir (const char *dir)
{
  check_valid_address (dir);
  char *dir_name;
  struct dir *new_dir = dir_parse (dir, &dir_name);
  if (new_dir == NULL)
  {
    free (dir_name);
    return (int) false;
  }
  
  struct inode *inode = NULL;

  if (new_dir != NULL)
    dir_lookup (new_dir, dir_name, &inode);
  dir_close (new_dir);
  
  if (inode == NULL)
  {
    free (dir_name);
    return (int) false;
  }
  thread_current () ->current_dir = inode_sector (inode);
  free (dir_name);
  return (int) true;
}

static int
sys_mkdir (const char *dir)
{
  check_valid_address (dir);
  if(filesys_create (dir, 0, DIR))
  {
    return dir_create_dot (dir);
  }
  return (int) false;
}

static int
sys_readdir (int fd, char *name)
{
  if (fd < 2)
    return (int) false;
  
  struct fd_frame *f = fd_to_fd_frame (fd);
  if (f)
  {
    return (int) (dir_readdir (dir_open (file_get_inode(f->file)), name));
  }
  return (int) false;
}

static int
sys_isdir (int fd)
{
  if (fd < 2)
    return (int) false;
  
  struct fd_frame *f = fd_to_fd_frame (fd);
  if (f)
    return (int) (inode_type(file_get_inode(f->file)) == DIR);
  return (int) false;
}

static int
sys_inumber (int fd)
{
  if (fd < 2)
    return -1;
  
  struct fd_frame *f = fd_to_fd_frame (fd);
  if (f)
    return inode_sector(file_get_inode(f->file));
  return -1;
}

