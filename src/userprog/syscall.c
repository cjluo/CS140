#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

static int get_user (const uint8_t *);
static bool put_user (uint8_t *, uint8_t);

static int sys_exit (int);
static int sys_halt (void);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

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
      printf ("SYS_OPEN Not Implemented\n");
      break;
    case SYS_FILESIZE:
      printf ("SYS_FILESIZE Not Implemented\n");
      break;
    case SYS_READ:
      printf ("SYS_READ Not Implemented\n");
      break;
    case SYS_WRITE:
      printf ("SYS_WRITE Not Implemented\n");
      break;
    case SYS_SEEK:
      printf ("SYS_SEEK Not Implemented\n");
      break;
    case SYS_TELL:
      printf ("SYS_TELL Not Implemented\n");
      break;
    case SYS_CLOSE:
      printf ("SYS_CLOSE Not Implemented\n");
      break;
    default:
      ASSERT (false);
  }

  f->eax = return_value;

  return;

}

static int sys_exit (int status)
{
  thread_exit();
  return -1;
}

static int sys_halt (void)
{
  shutdown_power_off ();
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