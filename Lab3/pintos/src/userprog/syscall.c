/* 
 * This file is derived from source code for the Pintos
 * instructional operating system which is itself derived
 * from the Nachos instructional operating system. The 
 * Nachos copyright notice is reproduced in full below. 
 *
 * Copyright (C) 1992-1996 The Regents of the University of California.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose, without fee, and
 * without written agreement is hereby granted, provided that the
 * above copyright notice and the following two paragraphs appear
 * in all copies of this software.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
 * AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
 * HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
 * BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 *
 * Modifications Copyright (C) 2017-2018 David C. Harrison. 
 * All rights reserved.
 */

#include <syscall-nr.h>
#include <list.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/umem.h"
#include "threads/lock.h"

typedef int pid_t;

static void syscall_handler(struct intr_frame *);
static void write_handler(struct intr_frame *);
static void create_handler(struct intr_frame *);
static void exit_handler(struct intr_frame *);
static void open_handler(struct intr_frame *);
static bool sys_create(const char *fname, unsigned isize);
static int sys_open(const char *file);
static void read_handler(struct intr_frame *);
static int sys_read( int fd, void *buffer, unsigned size); 
static void size_handler(struct intr_frame *f);
static int sys_size(int fd);
static void close_handler(struct intr_frame *f);
static void sys_close(int fd);
static void exec_handler(struct intr_frame *f);
pid_t sys_exec(const char * cmd_line);
static void wait_handler(struct intr_frame *f);
int sys_wait(pid_t);


static struct semaphore sema_esp;
static struct lock sys_lock;
void
syscall_init (void)
{

    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    semaphore_init(&sema_esp, 0);
    lock_init(&sys_lock);
}

static void
syscall_handler(struct intr_frame *f)
{
  int syscall;
  ASSERT( sizeof(syscall) == 4 ); // assuming x86

  // The system call number is in the 32-bit word at the caller's stack pointer.
  umem_read(f->esp, &syscall, sizeof(syscall));

  // Store the stack pointer esp, which is needed in the page fault handler.
  // Do NOT remove this line


  thread_current()->current_esp = f->esp;

  switch (syscall) {
  case SYS_HALT: 
    shutdown_power_off();
    break;

  case SYS_EXIT: 
    exit_handler(f);
    break;
      
  case SYS_WRITE: 
    write_handler(f);
    break;

  case SYS_CREATE:
    create_handler(f);
    break;

  case SYS_OPEN:
    open_handler(f);
    break;

  case SYS_READ:
    read_handler(f);
    break;

  case SYS_FILESIZE:
    size_handler(f);
    break;     

  case SYS_CLOSE:
    close_handler(f);
    break;

  case SYS_EXEC:
    exec_handler(f);
    break;

  case SYS_WAIT:
   wait_handler(f);
   break;

  default:
    printf("[ERROR] system call %d is unimplemented!\n", syscall);
    thread_exit();
    break;
  }
}

/****************** System Call Implementations ********************/

void sys_exit(int status) 
{
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();

}

static void exit_handler(struct intr_frame *f) 
{
  int exitcode;
  umem_read(f->esp + 4, &exitcode, sizeof(exitcode));

  sys_exit(exitcode);
}

/*
 * BUFFER+0 and BUFFER+size should be valid user adresses
 */

static uint32_t sys_write(int fd, const void *buffer, unsigned size)
{
  umem_check((const uint8_t*) buffer);
  umem_check((const uint8_t*) buffer + size - 1);

  int ret = -1;

  if (fd == 1) { // write to stdout
    putbuf(buffer, size);
    ret = size;
  }

    if (size == 0 || thread_current()->filesys[fd]==NULL) return 0;
    lock_acquire(&sys_lock); 
    ret = file_write(thread_current()->filesys[fd], buffer, size);
    lock_release(&sys_lock); 
    return (uint32_t) ret;
}
static void write_handler(struct intr_frame *f)
{
    int fd;
    const void *buffer;
    unsigned size;

    umem_read(f->esp + 4, &fd, sizeof(fd));
    umem_read(f->esp + 8, &buffer, sizeof(buffer));
    umem_read(f->esp + 12, &size, sizeof(size));

    f->eax = sys_write(fd, buffer, size);
}
static bool sys_create(const char *fname, unsigned isize)
{
    if (strlen(fname) == 0 || strlen(fname) >= 255)
        return false;
    return filesys_create(fname, isize, true);

}
static void create_handler(struct intr_frame *f)
{


    const char* fname;
    int isize;
    umem_read(f->esp + 4, &fname, sizeof(fname));
    umem_read(f->esp + 8, &isize, sizeof(isize));
    f->eax = sys_create(fname, isize);

}
static int sys_open(const char* file){
    if(strlen(file) ==0 || filesys_open(file) == NULL){ 
        return -1; 
    }
    int index = thread_current()->fileindex + 1;
    struct file* files = filesys_open(file);
    thread_current()->filesys[index] = files; 
    thread_current()->fileindex = index;
    return index;
}
static void open_handler(struct intr_frame *f)
{
    const char* fname;
    umem_read(f->esp + 4, &fname, sizeof(fname));
    f->eax = sys_open(fname);
}
static int sys_read( int fd, void *buffer, unsigned size){ 

    
    umem_check((const void*) buffer);
    umem_check((const void*) buffer + size - 1);
    if (size == 0 || thread_current()->filesys[fd]==NULL) return 0;

    lock_acquire(&sys_lock); 
    int bytes = file_read(thread_current()->filesys[fd], buffer, size);
    lock_release(&sys_lock);
    return bytes; 
}
static void read_handler(struct intr_frame *f){
    int fd;
    void *buffer;
    unsigned size;

    umem_read(f->esp + 4, &fd, sizeof(fd));
    umem_read(f->esp + 8, &buffer, sizeof(buffer));
    umem_read(f->esp + 12, &size, sizeof(size));

    f->eax = sys_read(fd, buffer, size);
}

static int sys_size(int fd){
    lock_acquire(&sys_lock); 
    if (thread_current()->filesys[fd]==NULL) return -1;

    int length = file_length(thread_current()->filesys[fd]);
    lock_release(&sys_lock);
    return length; 
}
static void size_handler(struct intr_frame *f){
    int fd; 
    umem_read(f->esp + 4, &fd, sizeof(fd));
    f->eax = sys_size(fd);
}
static void sys_close(int fd){

    file_close(thread_current()->filesys[fd]);
}
static void close_handler(struct intr_frame *f){
    int fd; 
    umem_read(f->esp + 4, &fd, sizeof(fd));
    sys_close(fd);
}
pid_t sys_exec (const char * cmd_line){

    lock_acquire (&sys_lock); 
    pid_t pid = process_execute(cmd_line);
    lock_release (&sys_lock);
    return pid;
}
static void exec_handler(struct intr_frame *f){

    const char* fname;
    umem_read(f->esp + 4, &fname, sizeof(fname));
    f->eax = sys_exec(fname);
}

int sys_wait(pid_t pid ){
    lock_acquire (&sys_lock); 
    int id = process_wait(pid);
    lock_release (&sys_lock);
    return id; 
}
static void wait_handler(struct intr_frame *f){
    pid_t pid;
    umem_read(f->esp + 4, &pid, sizeof(&pid));
    f->eax = sys_wait(pid);
}
