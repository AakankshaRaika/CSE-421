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
#include <proc.h>


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
/*
		case SYS_read:
			err=sys_read(tf->tf_a0,(void *)tf->tf_a1,(size_t)tf->tf_a2);
			break;

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


ssize_t sys_write(int fd, const void *buf, size_t buflen) {
KASSERT(fd != 0);
KASSERT(buflen > 0);	//should probably get rid of this
KASSERT(buf != NULL);
if ( fd == 0 ) return -1;         // if the fd is null return fd is not valid
if ( buflen == 0 ) return -1;     // if buflen is not >0 return -1
if ( buf == NULL ) return -1;     //if buf is pointing to null return invalid address space
/*
struct  iovec iov;
struct uio u;
off_t pos;
enum uio_rw rw;
struct addrspace *as;
as = proc_getas();*/
//uio_Userinit(&iov ,&u , (void)buf, buflen, pos, rw, as);
return 0; // reutrn 0 means nothing could be written
}


int sys_open(const char *filename, int flags){
KASSERT(filename != NULL);
KASSERT(flags >= 0);
//KASSERT(flags == O_RDONLY || flags == O_WRONLY);
//struct addrspace *as;
struct vnode *v;
int result;

result =vfs_open((char *) filename, flags, 0, &v);

set_file_vnode(result, v);

if (result) return result;

//KASSERT(file handle >= 0);

return result; // returns file handle.
}

/*
// rest of file system calls
ssize_t sys_read(int fd, void *buf, size_t buflen) {
struct  iovec iov;
struct uio u;
off_t pos;
enum uio_rw rw;
struct addrspace *as;
as = proc_getas();
KASSERT(file is open);
KASSERT(fd > 0);
KASSERT(buf is valid); // what makes buf valid?

return 0;		// return byte count
}


int sys_close(int fd) {

KASSERT(fd > 0);
				// return -1 on error
return 0;		// return 0 on success
}



off_t sys_lseek(int fd, off_t pos, int whence) {

KASSERT(seek position > 0);
KASSERT(fd > 0);


				// returns -1 on error
return NULL;	// returns new position on success
}


int sys_dup2(int oldfd, int newfd) {

KASSERT(oldfd > 0);
KASSERT(newfd > 0);

// on error return -1;

return newfd;
}


// process system calls

pid_t sys_fork(void) {

return 0;
}

int sys_exec(const char *program, char **args) {

return 0;
}

pid_t sys_wait(pid_t pid, int *status, int options) {

return 0;
}

void sys_exit(int exitcode) {

}


// "easy" system calls

int sys_chdir(const char *pathname) {



return 0;
}

pid_t sys_getpid(void) {

return 0;
}
*/
