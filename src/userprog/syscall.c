#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handle_ptr (struct intr_frame *);
typedef int (*handle_ptr) (uint32_t, uint32_t, uint32_t);
static handle_ptr syscall_table[128];



int sys_write (int a, int b, int c)
{
   
   //syscall1 (SYS_EXEC, "asdf");
	printf("#### SYS_EXEC: %d %s %d \n", a, b, c);

}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handle_ptr, "syscall");
  /*
  syscall_table[SYS_EXIT] = (handle_ptr)sys_exit;
  syscall_table[SYS_HALT] = (handle_ptr)sys_halt;
  syscall_table[SYS_CREATE] = (handle_ptr)sys_create;
  syscall_table[SYS_OPEN] = (handle_ptr)sys_open;
  syscall_table[SYS_CLOSE] = (handle_ptr)sys_close;
  syscall_table[SYS_READ] = (handle_ptr)sys_read;
  syscall_table[SYS_WRITE] = (handle_ptr)sys_write;
  syscall_table[SYS_EXEC] = (handle_ptr)sys_exec;
  syscall_table[SYS_WAIT] = (handle_ptr)sys_wait;
  syscall_table[SYS_FILESIZE] = (handle_ptr)sys_filesize;
  syscall_table[SYS_SEEK] = (handle_ptr)sys_seek;
  syscall_table[SYS_TELL] = (handle_ptr)sys_tell;
  syscall_table[SYS_REMOVE] = (handle_ptr)sys_remove;  
  */
}

static void
syscall_handle_ptr (struct intr_frame *f UNUSED) 
{
  printf ("system call \n");

  handle_ptr h;
  int *p;
  int ret;
  
  p = f->esp;
  printf ("system call Num %d \n", *p);
  
  switch(*p){
  	case SYS_WRITE: 
      sys_write( *(p+1), *(p+2), *(p+3));
      printf("### End SYS_WRITE\n");
  	  break;
  }

  //thread_exit();

  return;



  if (!is_user_vaddr (p))
    thread_exit();
  
  if (*p < SYS_HALT || *p > SYS_INUMBER)
    thread_exit();
  
  h = syscall_table[*p];
  
  if (!(is_user_vaddr (p + 1) && is_user_vaddr (p + 2) && is_user_vaddr (p + 3)))
    thread_exit();
  
  ret = h (*(p + 1), *(p + 2), *(p + 3));
  
  f->eax = ret;

  return;

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