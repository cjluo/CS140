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
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);

static int get_user (const uint8_t *);
static bool put_user (uint8_t *, uint8_t);

static int sys_exit (int);
static int sys_halt (void);
static int sys_open (const char *file);
static int sys_read (int, void *, unsigned);
static int sys_write (int, const void *, unsigned);
static int sys_close (int);

static int fd_gen (void);

static struct lock file_lock;
struct fd_frame
  {
    int fd;
    struct file *file;
    struct list_elem elem;
  };

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
  // syscall_table[SYS_EXIT] = (handle_ptr)sys_exit;
  // syscall_table[SYS_HALT] = (handle_ptr)sys_halt;
  // syscall_table[SYS_CREATE] = (handle_ptr)sys_create;
  // syscall_table[SYS_OPEN] = (handle_ptr)sys_open;
  // syscall_table[SYS_CLOSE] = (handle_ptr)sys_close;
  // syscall_table[SYS_READ] = (handle_ptr)sys_read;
  // syscall_table[SYS_WRITE] = (handle_ptr)sys_write;
  // syscall_table[SYS_EXEC] = (handle_ptr)sys_exec;
  // syscall_table[SYS_WAIT] = (handle_ptr)sys_wait;
  // syscall_table[SYS_FILESIZE] = (handle_ptr)sys_filesize;
  // syscall_table[SYS_SEEK] = (handle_ptr)sys_seek;
  // syscall_table[SYS_TELL] = (handle_ptr)sys_tell;
  // syscall_table[SYS_REMOVE] = (handle_ptr)sys_remove;  

}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t *esp = (uint32_t *)f->esp;
  // printf ("system call Num %d \n", *esp);
  
  if (!is_user_vaddr (esp))
    sys_exit (-1);
  
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
      return_value = sys_exit ((int)*(esp+1));
      break;
    case SYS_EXEC:
      printf ("SYS_EXEC Not Implemented\n");
      break;
    case SYS_WAIT:
      printf ("SYS_WAIT Not Implemented\n");
      break;
    case SYS_CREATE:
      printf ("SYS_CREATE Not Implemented\n");
      break;
    case SYS_REMOVE:
      printf ("SYS_REMOVE Not Implemented\n");
      break;
    case SYS_OPEN:
      // printf ("SYS_OPEN Not Implemented\n");
      return_value = sys_open ((char *)*(esp+1));
      break;
    case SYS_FILESIZE:
      printf ("SYS_FILESIZE Not Implemented\n");
      break;
    case SYS_READ:
      return_value = sys_read ((int)*(esp+1),
                               (void *)*(esp+2),
                               (unsigned)*(esp+3));
      break;
    case SYS_WRITE:
      return_value = sys_write ((int)*(esp+1),
                                (void *)*(esp+2),
                                (unsigned)*(esp+3));
      break;
    case SYS_SEEK:
      printf ("SYS_SEEK Not Implemented\n");
      break;
    case SYS_TELL:
      printf ("SYS_TELL Not Implemented\n");
      break;
    case SYS_CLOSE:
      return_value = sys_close ((int)*(esp+1));
      break;
    default:
      ASSERT (false);
  }

  f->eax = (uint32_t) return_value;

  return;

}

static int
sys_exit (int status)
{
  thread_exit();
  return -1;
}

static int
sys_halt (void)
{
  shutdown_power_off ();
}

static int
sys_open (const char *file)
{
  printf ("sys_open finished filename: %s\n", file);   
  //test address
  if (!file)
    return -1;
  if (!is_user_vaddr (file))
    sys_exit (-1);
  lock_acquire (&file_lock);
  struct file *f = filesys_open(file);
  lock_release (&file_lock);
  if (!f)
    return -1;

  
  struct fd_frame *fd_open_frame = (struct fd_frame *) malloc (
                                    sizeof(struct fd_frame));
  if(!fd_open_frame)
  {
    lock_acquire (&file_lock);
    file_close (f);
    lock_release (&file_lock);
    return -1;
  }
  fd_open_frame->file = f;
  fd_open_frame->fd = fd_gen ();
  list_push_back(&thread_current ()->file_list, &fd_open_frame->elem);
  printf ("sys_open finished fd: %d\n", fd_open_frame->fd);  
  
  return fd_open_frame->fd;
}

static int
sys_read (int fd, void *buffer, unsigned size)
{
  if (!is_user_vaddr (buffer) || !is_user_vaddr (buffer + size))
    sys_exit(-1);
  
  if (fd < 0 || fd == 1)
    return -1;

  if (fd == STDIN_FILENO)
  {
    unsigned i;
    /* what if we get EOF ??? */
    for (i = 0; i != size; ++i)
      *(char *)(buffer + i) = input_getc ();
    return size;
  }
  
  struct list *l = &thread_current() -> file_list;
  struct list_elem *e;
  for (e = list_begin (l); e != list_end (l); e = list_next (e))
  {
    struct fd_frame *f = list_entry (e, struct fd_frame, elem);
    if (f->fd == fd)
    {
      lock_acquire (&file_lock);
      off_t return_value = file_read (f->file, buffer, size);
      lock_release (&file_lock);
      return (int)return_value;
    }
  }
  
  return -1;
}

static int
sys_write (int fd, const void *buffer, unsigned size)
{
  // printf("FD: %d\n", fd);
  // printf("Buffer: %s\n", (char *) buffer);
  // printf("Size: %u\n", size);
  if (fd == STDOUT_FILENO)
    putbuf(buffer, size);
  else
  {
    lock_acquire (&file_lock);
    ASSERT (false);
    lock_release (&file_lock);
  }
  return -1;
}

static int
sys_close (int fd)
{
  if (fd < 2)
    return -1;
  
  struct list *l = &thread_current() -> file_list;
  struct list_elem *e;
  for (e = list_begin (l); e != list_end (l); e = list_next (e))
  {
    struct fd_frame *f = list_entry (e, struct fd_frame, elem);
    if (f->fd == fd)
    {
      lock_acquire (&file_lock);
      file_close (f->file);
      lock_release (&file_lock);
    }
  }
  return -1;
}

/* Reads a byte at user virtual address UADDR.
UADDR must be below PHYS_BASE.
Returns the byte value if successful, -1 if a segfault
occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
/* Writes BYTE to user address UDST.
UDST must be below PHYS_BASE.
Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

static int
fd_gen (void)
{
  static int fd = 1;
  return ++fd;
}
