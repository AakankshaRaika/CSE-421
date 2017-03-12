/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/syscall.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <kern/unistd.h>
#include <uio.h>
#include <vfs.h>
#include <sfs.h>
#include <proc.h>
#include <synch.h>
#include <kern/seek.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <copyinout.h>
#define MAX_PATH 512

/*
 * System call dispatcher.
 *
 * A pointer to the trapframe created during exception entry (in
 * exception-*.S) is passed in.
 *
 * The calling conventions for syscalls are as follows: Like ordinary
 * function calls, the first 4 32-bit arguments are passed in the 4
 * argument registers a0-a3. 64-bit arguments are passed in *aligned*
 * pairs of registers, that is, either a0/a1 or a2/a3. This means that
 * if the first argument is 32-bit and the second is 64-bit, a1 is
 * unused.
 *
 * This much is the same as the calling conventions for ordinary
 * function calls. In addition, the system call number is passed in
 * the v0 register.
 *
 * On successful return, the return value is passed back in the v0
 * register, or v0 and v1 if 64-bit. This is also like an ordinary
 * function call, and additionally the a3 register is also set to 0 to
 * indicate success.
 *
 * On an error return, the error code is passed back in the v0
 * register, and the a3 register is set to 1 to indicate failure.
 * (Userlevel code takes care of storing the error code in errno and
 * returning the value -1 from the actual userlevel syscall function.
 * See src/user/lib/libc/arch/mips/syscalls-mips.S and related files.)
 *
 * Upon syscall return the program counter stored in the trapframe
 * must be incremented by one instruction; otherwise the exception
 * return code will restart the "syscall" instruction and the system
 * call will repeat forever.
 *
 * If you run out of registers (which happens quickly with 64-bit
 * values) further arguments must be fetched from the user-level
 * stack, starting at sp+16 to skip over the slots for the
 * registerized values, with copyin().
 */
void
syscall(struct trapframe *tf)
{
	int callno;
	int32_t retval;
	int err;

	KASSERT(curthread != NULL);
	KASSERT(curthread->t_curspl == 0);
	KASSERT(curthread->t_iplhigh_count == 0);

	callno = tf->tf_v0;

	/*
	 * Initialize retval to 0. Many of the system calls don't
	 * really return a value, just 0 for success and -1 on
	 * error. Since retval is the value returned on success,
	 * initialize it to 0 by default; thus it's not necessary to
	 * deal with it except for calls that return other values,
	 * like write.
	 */

	retval = 0;
//	struct lock *lock;

	switch (callno) {
	    case SYS_reboot:
			err = sys_reboot(tf->tf_a0);
			break;

	    case SYS___time:
			err = sys___time((userptr_t)tf->tf_a0,
			(userptr_t)tf->tf_a1);
			break;

	    /* Add stuff here */

        case SYS_open:
			err = sys_open((const char *)tf->tf_a0,tf->tf_a1);
			break;

        case SYS_write:
			err = sys_write(tf->tf_a0,(const void *)tf->tf_a1,(size_t)tf->tf_a2);
			break;

		case SYS_read:
			err=sys_read(tf->tf_a0,(void *)tf->tf_a1,(size_t)tf->tf_a2);
			break;
/*
		case SYS_lseek:
			err=sys_lseek(tf->tf_a0, (off_t)tf->tf_a1, tf->tf_a2);
			break;

		case SYS_close:
			err=sys_close(tf->tf_a0);
			break;

		case SYS_dup2:
			err=sys_dup2(tf->tf_a0, tf->tf_a1);
			break;

		case SYS_chdir:
			err=sys_chdir((const char *)tf->tf_a0);
			break;

		case SYS___getcwd:
			err=sys__getcwd((char *)tf->tf_a0, (size_t)tf->tf_a1);
			break;
*/
		default:
			kprintf("Unknown syscall %d\n", callno);
			err = ENOSYS;
			break;
	}

//	spinlock_init(&lock->lk_lock);

	if (err) {
		/*
		 * Return the error code. This gets converted at
		 * userlevel to a return value of -1 and the error
		 * code in errno.
		 */
		tf->tf_v0 = err;
		tf->tf_a3 = 1;      /* signal an error */
	}
	else {
		/* Success. */
		tf->tf_v0 = retval;
		tf->tf_a3 = 0;      /* signal no error */
	}

	/*
	 * Now, advance the program counter, to avoid restarting
	 * the syscall over and over again.
	 */

	tf->tf_epc += 4;

	/* Make sure the syscall code didn't forget to lower spl */
	KASSERT(curthread->t_curspl == 0);
	/* ...or leak any spinlocks */
	KASSERT(curthread->t_iplhigh_count == 0);
}

/*
 * Enter user mode for a newly forked process.
 *
 * This function is provided as a reminder. You need to write
 * both it and the code that calls it.
 *
 * Thus, you can trash it and do things another way if you prefer.
 */
void
enter_forked_process(struct trapframe *tf)
{
	(void)tf;
}


/*------------------------------------------------------*/
/*-------------------SYS CALL WRITE----------------------*/
/*------------------------------------------------------*/
ssize_t sys_write(int fd, const void *buf, size_t buflen) {
	KASSERT(fd != 0);
	KASSERT(buflen > 0);	//should probably get rid of this
	KASSERT(buf != NULL);
	if ( fd == 0 ) return EBADF;      // if the fd is null return fd is not valid
	if ( buflen == 0 ) return -1;     // if buflen is not >0 return -1
	if ( buf == NULL ) return -1;     //if buf is pointing to null return invalid address space

	struct  iovec iov;
	struct uio u;      		/* what should we initilize it to?*/
	off_t pos = 0;     	/* This will come from the lseek*/
	enum uio_rw rw;    		/* this will the UIO_VALUE*/
	rw = UIO_WRITE;
	struct addrspace *as;
	as = proc_getas();

	uio_Userinit(&iov , &u , (void *)buf, buflen, pos, rw, as);
        struct vnode *v;
 	if (sizeof(buf) < buflen){ //the error is that i am not setting the vnode before accessing the vnode
                v = curproc->f_table[fd]->vn;
                VOP_WRITE(v , &u);
                return (size_t)sizeof(buf);
 	}
	return 0; // return 0 means nothing could be written
}

/*------------------------------------------------------*/
/*-------------------SYS CALL OPEN----------------------*/
/*------------------------------------------------------*/
int sys_open(const char *filename, int flags){
//KASSERT(filename != NULL);
//KASSERT(flags >= 0);
//KASSERT(flags == O_RDONLY || flags == O_WRONLY);
//struct addrspace *as;
struct vnode *v;
int result; // this is the file handle
//do kmalloc to allocate memory on the heap for the file

struct addrspace *as;
as = kmalloc(sizeof(filename));

size_t actual;
copyinstr((const_userptr_t) filename, (char *)as, (size_t )MAX_PATH, &actual);

result =vfs_open((char *) filename, flags, 0, &v);
set_file_vnode(result, v);
set_file_name (result , filename);
if (result) return result;


return -1; // returns -1 on an error
}

/*------------------------------------------------------*/
/*-------------------SYS CALL READ----------------------*/
/*------------------------------------------------------*/
// rest of file system calls
ssize_t sys_read(int fd, void *buf, size_t buflen) {
        KASSERT(fd != 0);
        KASSERT(buflen > 0);    //should probably get rid of this
        KASSERT(buf != NULL);
        if ( fd == 0 ) return EBADF;      // if the fd is null return fd is not valid
        if ( buflen == 0 ) return -1;     // if buflen is not >0 return -1
        if ( buf == NULL ) return -1;     //if buf is pointing to null return invalid address space

        struct  iovec iov;
        struct uio u;                   /* what should we initilize it to?*/
        off_t pos = 0;          /* This will come from the lseek*/
        enum uio_rw rw;                 /* this will the UIO_VALUE*/
        rw = UIO_READ;
        struct addrspace *as;
        as = proc_getas();

        uio_Userinit(&iov , &u , (void *)buf, buflen, pos, rw, as);
        struct vnode *v;
        if (sizeof(buf) < buflen){
                v = curproc->f_table[fd]->vn;
                VOP_READ(v , &u);
                return (size_t)sizeof(buf);
        }
        return 0; // return 0 means nothing could be written
}


/*------------------------------------------------------*/
/*-------------------SYS CALL CLOSE---------------------*/
/*------------------------------------------------------*/

int sys_close(int fd) {
KASSERT(fd > 0);
//TODO handle a bad _ close with kasserts and if's 
//int result;
vfs_close(curproc->f_table[fd]->vn);
return 0;       	// return 0 on success
}

/*------------------------------------------------------*/
/*-------------------SYS CALL LSEEK---------------------*/
/*------------------------------------------------------*/
off_t sys_lseek(int fd, off_t pos, int whence) {
KASSERT(fd > 0);

if (whence == SEEK_SET) set_seek(fd, pos);

else if (whence == SEEK_CUR) set_seek(fd, get_seek(fd)+pos);

else if (whence == SEEK_END) set_seek(fd,sizeof(curproc->f_table[fd]->vn));

else return -1;

return get_seek(fd);	// returns new position on success
}


/*------------------------------------------------------*/
/*-------------------SYS CALL DUP2----------------------*/
/*------------------------------------------------------*/

int sys_dup2(int oldfd, int newfd) {

KASSERT(oldfd > 0);
KASSERT(newfd > 0);

// on error return -1;

//if (oldfd is not a valid file handle, or newfd is a value that cannot be a valid file handle)
//return EBADF;

//if ((the process's file table was full or a process specific limit on open files was reached) or (the system's file table was full, if such a thing is possible, or a global limit on open files was reached))
//return EMFILE;

struct vnode *vn = get_file_vnode(oldfd);
off_t seek = get_seek(oldfd);
const char *filename = get_file_name(oldfd);

set_file_vnode(newfd, vn);
set_seek(newfd, seek);
set_file_name(newfd, filename);


return newfd;
}

/*
int sys__getcwd(char *buf, size_t buflen){
KASSERT(buflen > 0);
KASSERT(buf != NULL);
struct vnode *v = curproc->p_cwd;

vfs_open

find index given fd from file table, given buf or vnode

get seek pos, seek end

check tag to make sure it is coming from write

return seek end - seek pos


   


return 0;
}
*/






/*------------------------------------------------------*/
/*-------------------PROCESS SYS CALL FORK--------------*/
/*------------------------------------------------------*/

/*
pid_t sys_fork(void) {

if (the current user already has too many processes or there are already too many processes on the system)
return EMPROC;

if (sufficient virtual memory for the new process was not available)
return ENOMEM;

return 0;
}
*/

/*------------------------------------------------------*/
/*-------------------PROCESS SYS CALL EXECV-------------*/
/*------------------------------------------------------*/
/*
int sys_execv(const char *program, char **args) {
if (the device prefix of program did not exist)
return ENODEV;

if (the non-final component of program was not a valid directory)
return ENOTDIR;

if (program did not exist)
return ENOENT;

if (program is a directory)
return EISDIR;

if (program is not in a recognizable executable file format, was for the wrong platform, or contained invalid fields)
return ENOEXEC;

if (insufficient virtual memory is avaliable)
return ENOMEM;

if (the total size of the argument strings exceeds ARG_MAX)
return E2BIG;

if (a hardware I/O error occurred)
return EIO;

if (one of the arguments is an invalid pointer)
return EFAULT;

return 0;
}
*/

/*------------------------------------------------------*/
/*-------------------PROCESS SYS CALL WAITPID-----------*/
/*------------------------------------------------------*/

/*
pid_t sys_waitpid(pid_t pid, int *status, int options) {

if (the options argument requested invalid or unsupported options)
return EINVAL;

if (the pid argument named a process that was not a valid child of the current process)
return ECHILD;

if (the pid arguent named a nonexistent process)
return ESRCH;

if (the status argument was an invalid pointer)
return EFAULT;


return 0;
}
*/

/*------------------------------------------------------*/
/*-------------------PROCESS SYS CALL EXIT--------------*/
/*------------------------------------------------------*/
/*
void sys_exit(int exitcode) {

}
*/

/*
// "easy" system calls

int sys_chdir(const char *pathname) {

if (the device prefix of pathname did not exist)
return ENODEV;

if (a non-final component of pathname was not a directory or pathname did not refer to a directory)
return ENOTDIR;

if (pathname did not exist)
return ENOENT;

if (a hardware I/O error occurred)
return EIO;

if (pathname was an invalid pointer)
return EFAULT;

return 0;
}

pid_t sys_getpid(void) {

return 0;
}
*/
