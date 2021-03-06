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
#include <kern/stat.h>
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
        off_t np;
        off_t os;
        int whence;
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

            case SYS_write:
			err = sys_write(tf->tf_a0,
			                (const void *)tf->tf_a1,
					(size_t)tf->tf_a2,
					(int *)(&retval));
			break;
            case SYS_read:
			err = sys_read(tf->tf_a0,
				       (void *)tf->tf_a1,
 				       (size_t)tf->tf_a2,
                                       (int * )(& retval));
			break;

	    case SYS_open:
			err = sys_open((const char *)tf->tf_a0,
				       tf->tf_a1,
                                       (int * )(& retval) );
			break;

	    case SYS_lseek:
                        /*help from Office hours : need to make proper conversions of the writebuf size for the fileonlytest (pos in general) handle 64 bits here too*/
                        os = (((off_t)tf->tf_a2 << 32) | tf->tf_a3);

                        /*help from office hours : need to copy in the whence and assign mem or else lseek is not reading it, also was return 512 != 1024 the error was being caused from this line */
			copyin((const userptr_t)(tf->tf_sp+16), &whence, sizeof(whence));
			err = sys_lseek(tf->tf_a0,
					os,
				      	whence,
				      	&np);

                        // err = sys_lseek (tf->tf_a1 , (off_t) tf->tf_a1 , tf->tf_a2 , (off_t *)(& retval)) ; /*Help from office hours : this is wrong not handling 64 bits anywhere*/

                        /*help from office hours : need to return 2 registers handling 64 bits */
                        if(err == 0){
                        	retval = (int32_t)((np & 0xFFFFFFFF00000000) >> 32);
				tf->tf_v1 = (int32_t)(np & 0xFFFFFFFF);
			}
			break;

            case SYS_close:
			err = sys_close(tf->tf_a0);
			break;

            case SYS__exit:
                        err = 0;
                        thread_exit();
                        break;
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


/*------------------------------------------------------*/
/*-------------------SYS CALL WRITE----------------------*/
/*------------------------------------------------------*/
ssize_t sys_write(int fd, const void *buf, size_t buflen, int *retval) {
	/*make sure the con file is open and does exists*/
        KASSERT (curproc->f_table[1]->vn != NULL);
 	KASSERT (curproc->f_table[2]->vn != NULL);

        lock_acquire(curproc->f_table[fd]->lk);
//        kprintf("write buf size in kernel : %d \n" , (int)buflen);
        if(curproc->f_table[fd] == NULL){
                return EBADF;
        }
        if ( fd < 0 ){
		return EBADF;
	}
        if ( fd > 63 ){
		return EBADF;
	}
	if(buf == NULL){
                return EFAULT;
        }
        /*inits for the uio_userinit function*/
	struct  iovec iov;
	struct uio u;      				/* what should we initilize it to?*/
	off_t pos = curproc->f_table[fd]->seek;         /*getting the offset of the file*/ /* This will come from the lseek*/
	enum uio_rw rw;    				/* this will the UIO_VALUE*/
	rw = UIO_WRITE;
	struct addrspace *as;
	as = proc_getas();				/*this will get the address space of the current process*/
	uio_Userinit(&iov , &u , (void *)buf, buflen, pos, rw, as);

        /*getting the current vnode*/
	struct vnode *v = curproc->f_table[fd]->vn;
        int result = VOP_WRITE(v,&u);
        if (result){
        /*we will reach here if the VOP was not successfull*/
               lock_release(curproc->f_table[fd]->lk);
	       return result;
        }
        /*VOP was successful*/
        curproc->f_table[fd]->seek = u.uio_offset;
        lock_release(curproc->f_table[fd]->lk);
        *retval = buflen - u.uio_resid;
//        kprintf("write buf size in kernel after write was completed is : %d \n" , (int)buflen);
	return 0; 					// return 0 means nothing could be written
}

/*------------------------------------------------------*/
/*-------------------SYS CALL READ----------------------*/
/*------------------------------------------------------*/
// rest of file system calls
ssize_t sys_read(int fd, void *buf, size_t buflen , int *retval) {
	/*make sure the con file is open and does exists*/
        KASSERT (curproc->f_table[1]->vn != NULL);
 	KASSERT (curproc->f_table[2]->vn != NULL);

        lock_acquire(curproc->f_table[fd]->lk);
        if( fd < 0 ){
                return EBADF;
        }
        if( fd > 63 ){
                return EBADF;
        }
	if(buf == NULL){
		return EFAULT;
	}
        /*inits for the uio_userinit function*/
	struct  iovec iov;
	struct uio u;      				/* what should we initilize it to?*/
	off_t pos = curproc->f_table[fd]->seek;         /*getting the offset of the file*/ /* This will come from the lseek*/
	enum uio_rw rw;    				/* this will the UIO_VALUE*/
	rw = UIO_READ;
	struct addrspace *as;
	as = proc_getas();				/*this will get the address space of the current process*/
	uio_Userinit(&iov , &u , (void *)buf, buflen, pos, rw, as);

	struct vnode *v = curproc->f_table[fd]->vn;
        int result = VOP_READ(v,&u);
        if (result){
                /*we will reach here if the VOP was not successfull*/
      		lock_release(curproc->f_table[fd]->lk);
		return result;
        }
        /*VOP was successful*/
        curproc->f_table[fd]->seek = u.uio_offset;
        lock_release(curproc->f_table[fd]->lk);
        *retval = buflen - u.uio_resid;
        return 0;
}

/*------------------------------------------------------*/
/*-------------------SYS CALL OPEN----------------------*/
/*------------------------------------------------------*/
int sys_open(const char *filename, int flags , int *retval){
        //TODO : HOW DO I MAKE THIS ATOMIC??
        if (filename == NULL) return EFAULT;
        if (flags < 0){
		return EINVAL;
        }
        if (flags == O_EXCL){
                return EEXIST;
        }
	struct vnode *v;
	int result; 					// this is the file handle
	char *as = kmalloc( MAX_PATH * sizeof(char)); 	// TA said this should be char * and this is correctly allocating enough memory

	size_t actual;
	int err = copyinstr((const_userptr_t) filename, as, (size_t )MAX_PATH, &actual);  // puts filename into as
        if (err){
              return err;                               //the copy failed.
	}

        int fd = next_fd(curproc);                      //next proc fd found is invalid
        if (fd < 0 || fd >63){
		return ENOSPC;
        }
 	result = vfs_open(as, flags, 0, &v);  		// should open with as not filename

        /*initilize all the file components here for the file opened*/
        if(result == 0 ){
                curproc->f_table[fd]->lk = lock_create(filename);
                if (curproc->f_table[fd]->lk == NULL){
			        return ENOMEM;
		}
                curproc->f_table[fd]->vn = v;
 		curproc->f_table[fd]->file_name = filename;
		curproc->f_table[fd]->seek = 0;                                  	//when file initially opened the offset should be zero
                curproc->f_table[fd]->ref = 1;
		curproc->f_table[fd]->flag = flags;
                *retval = fd;
                return 0;
        }

	return -1; 					// returns -1 on an error
}

/*------------------------------------------------------*/
/*-------------------SYS CALL CLOSE---------------------*/
/*------------------------------------------------------*/

int sys_close(int fd ) {
        /*vfs_close doese all the work for close*/
	if (fd < 0 || fd >= 64) return EBADF;
        if(curproc->f_table[fd] == NULL) {
		return EBADF;
	}

        lock_acquire(curproc->f_table[fd]->lk);
        if(curproc->f_table[fd]->ref == 1){                     //Help from Office hours : if and only if there is only one refernce to this file opened then close it other wise -1 the ref from the filetable
	   vfs_close(curproc->f_table[fd]->vn);
           lock_release(curproc->f_table[fd]->lk);
           lock_destroy(curproc->f_table[fd]->lk);

           curproc->f_table[fd] = NULL;                         //Help office hours : need to do the nulling before kfree like in any other processs
           kfree(curproc->f_table[fd]);
	}
        else{
        /*help from office hours : this is imp for dup2*/
           curproc->f_table[fd]->ref -= 1;
           lock_release(curproc->f_table[fd]->lk);
        }
   return 0;       						// return 0 on success
}


/*------------------------------------------------------*/
/*-------------------SYS CALL LSEEK---------------------*/
/*------------------------------------------------------*/

off_t sys_lseek(int fd, off_t pos, int whence , off_t *retval) {
        /*Office hours : check for validation*/
        if(curproc->f_table[fd] == NULL) {                      //the file table was destroyed
                return EBADF;
        }

        if (!((whence == SEEK_CUR) || (whence == SEEK_END) || (whence == SEEK_SET) )) return EBADF; //the whence is not valid

        if (fd < 0 || fd >= 64) return EBADF;                   //the file doesnt exists

        /*if validation was ok start lseek*/
        lock_acquire(curproc->f_table[fd]->lk);
        struct stat f_stat;                                     //for getting the file size (kern/stat.h)
        off_t off = curproc->f_table[fd]->seek;
        off_t err;

        /*Feedback Office hours : double check locks they might be a little off*/
	switch(whence) {
		case SEEK_SET:
	           off = pos;
		   break;
		case SEEK_CUR:
	           off = curproc->f_table[fd]->seek +pos;
	           break;
		case SEEK_END:
		   err = (off_t)VOP_STAT(curproc->f_table[fd]->vn, &f_stat); //checks for the file info data
		   if (err) {
			lock_release(curproc->f_table[fd]->lk);
			return err;
	           }
	           off = f_stat.st_size + pos; 			//file size + pos
	           break;
        }

	if ( (whence == SEEK_CUR) || (whence == SEEK_END) || (whence == SEEK_SET) ){
           curproc->f_table[fd]->seek = off;
           *retval = off;                                       //Feedback office hours : this is giving be 64 bits
           lock_release(curproc->f_table[fd]->lk);
           return 0;
 	}

        // ask carl what to do with retval on error
        lock_release(curproc->f_table[fd]->lk);
	return -1;

}


