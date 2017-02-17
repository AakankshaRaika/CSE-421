
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

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL) {
		return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void
P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

//////////////////////////////////////////////////////////////
// Lock.
// implementing this similar to semaphore

struct lock * lock_create(const char *name) {
	struct lock *lock;

	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}

	HANGMAN_LOCKABLEINIT(&lock->lk_hangman, lock->lk_name);
	lock->lk_wchan = wchan_create(lock->lk_name); //AR : creating a wchan for the lock
	if (lock->lk_wchan == NULL) // this is null that means lock was not properly created
	{
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}

	spinlock_init(&lock->lk_lock);
	lock->lk_holder = NULL;//also no threads should be holding it when it is created
        // unlike the semaphore we will not have count but instead we will nullify holds.

	return lock; // return the lock that was created
}

void lock_destroy(struct lock *lock) {
       /*implementing this similar to the semaphore*/

	KASSERT(lock != NULL); // making sure lock exists
        KASSERT(lock_do_i_hold(lock) == false);
        spinlock_cleanup(&lock->lk_lock);
        if (lock -> lk_wchan != NULL && lock->lk_holder == curthread ){ // checking if the lock even exists
                                       // in the wait channel, if it does
                                       // it is detroying it.
	wchan_destroy(lock->lk_wchan);
        }
        lock->lk_holder = NULL; // no thread should be holding it when it is destroyed
        kfree(lock->lk_name); // after the lock is destroyed freeing the name and memory
        kfree(lock);
}

void lock_acquire(struct lock *lock) {       /* implementing this similar to semaphore*/
	/* Call this (atomically) before waiting for a lock */

	HANGMAN_WAIT(&curthread->t_hangman, &lock->lk_hangman);
        KASSERT(lock != NULL);
	KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&lock->lk_lock);         //acquire a spin lock
	while (lock->lk_holder != NULL) {         //if the holder is not empty keep sleeping
                wchan_sleep(lock->lk_wchan,&lock->lk_lock); //putting lk to sleep
	}
        //only one (curthread) can hold the lock at the same time

        lock->lk_holder = curthread;//ini. holder to the current thread this is imp for release function

        spinlock_release(&lock->lk_lock); //release slk

        /* Call this (atomically) once the lock is acquired */
	HANGMAN_ACQUIRE(&curthread->t_hangman, &lock->lk_hangman);
}

void lock_release(struct lock *lock) {
        /*implementing this similar to the V function for the semaphore*/
	KASSERT(lock_do_i_hold(lock));
        KASSERT(lock != NULL);

	spinlock_acquire(&lock->lk_lock);
        //releasing only if it is the current thread
        if (lock->lk_holder == curthread){
                lock->lk_holder = NULL; //putting the holder to null as will release this
         	wchan_wakeone(lock->lk_wchan, &lock->lk_lock); //wake one of the locks
        }

	spinlock_release(&lock->lk_lock);
	/* Call this (atomically) when the lock is released */
	HANGMAN_RELEASE(&curthread->t_hangman, &lock->lk_hangman);
}

bool lock_do_i_hold(struct lock *lock) {
//all we need to do is check the curthread
//its a bool value
//curthread definded in current.h
//	KASSERT(lock != NULL); // making sure the lock exsits
	bool check; // new var for check
//        spinlock_acquire(&lock ->lk_lock);
	check = (lock->lk_holder == curthread); //checking if the holder = current thread 
//	spinlock_release(&lock->lk_lock);
        return check;
}

////////////////////////////////////////////////////////////
// CV
struct cv * cv_create(const char *name) {
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}
        /*similar implemention to the semaphone and lock for create*/
	cv->cv_wchan = wchan_create(cv->cv_name);
	if(cv->cv_wchan == NULL)
	{
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}
        spinlock_init(&cv->cv_lock);
	return cv;
}

void cv_destroy(struct cv *cv) {
	//destroy wait channel, cleanup spinlock, free cv
	wchan_destroy(cv->cv_wchan);		
	spinlock_cleanup(&cv->cv_lock);
	kfree(cv->cv_name);
	kfree(cv);
}

void cv_wait(struct cv *cv, struct lock *lock) {
	//assert lock/lock hold exists
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock));
	//acquire spinlock, release lock, sleep wait channel, release spinlock, then acquire lock
	spinlock_acquire(&cv->cv_lock);
	lock_release(lock);
	wchan_sleep(cv->cv_wchan, &cv->cv_lock);
	spinlock_release(&cv->cv_lock);
	lock_acquire(lock);
}

void cv_signal(struct cv *cv, struct lock *lock) {
	//assert lock/lock hold exists
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock));
	//acquire spinlock, wake one, release spinlock
	spinlock_acquire(&cv->cv_lock);
	wchan_wakeone(cv->cv_wchan, &cv->cv_lock);//wake one thing
	spinlock_release(&cv->cv_lock);
}

void cv_broadcast(struct cv *cv, struct lock *lock) {
	//assert lock/lock hold exists
        KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock));
	//acquire spinlock, wake all, release spinlock
	spinlock_acquire(&cv->cv_lock);
	wchan_wakeall(cv->cv_wchan, &cv->cv_lock);//wake everything
	spinlock_release(&cv->cv_lock);
}

struct rwlock *rwlock_create(const char *name)
{
	struct rwlock *rwlock;				//same as sem lock structure
	rwlock = kmalloc(sizeof(*rwlock));
	if(rwlock == NULL)
	{
		return NULL;
	}
	rwlock->rwlock_name = kstrdup(name);
	if(rwlock->rwlock_name == NULL)
	{
		kfree(rwlock);
		return NULL;
	}
	rwlock->rwlock_wchan = wchan_create(rwlock->rwlock_name);
	if(rwlock->rwlock_wchan == NULL)
	{
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}
	spinlock_init(&rwlock->rwlock_lock);		//init booleans and counts
	rwlock->rwlock_wc = rwlock->rwlock_rc = 0;
	rwlock->rwlock_wr = rwlock->rwlock_wa = false;
	return rwlock;
}

void rwlock_destroy(struct rwlock *rwlock)
{
	KASSERT(rwlock != NULL);			//assert count is 0, writing is false
	KASSERT(rwlock->rwlock_rc == 0);
	KASSERT(rwlock->rwlock_wr == false);

	wchan_destroy(rwlock->rwlock_wchan);
	spinlock_cleanup(&rwlock->rwlock_lock);		//cleanup spinlock
	kfree(rwlock->rwlock_name);
	kfree(rwlock);

}

void rwlock_acquire_read(struct rwlock *rwlock)
{
	KASSERT(rwlock != NULL);
	spinlock_acquire(&rwlock->rwlock_lock);
	while(rwlock->rwlock_wr || rwlock->rwlock_wa)
	{
		wchan_sleep(rwlock->rwlock_wchan, &rwlock->rwlock_lock);		//sleep wait channel while writing or waiting
	}
	rwlock->rwlock_rc = rwlock->rwlock_rc + 1;					//increment reader count
	KASSERT(rwlock->rwlock_wr == false);
	spinlock_release(&rwlock->rwlock_lock);
	return;
}

void rwlock_release_read(struct rwlock *rwlock)
{
	int temp = random() % 2;						//random int for reader count logic
	KASSERT(rwlock != NULL);
	KASSERT(rwlock->rwlock_rc > 0);
	spinlock_acquire(&rwlock->rwlock_lock);
	rwlock->rwlock_rc = rwlock->rwlock_rc - 1;
	if(temp == 0 && rwlock->rwlock_rc > 0)
	{
		rwlock->rwlock_wa = true;
	}
	wchan_wakeall(rwlock->rwlock_wchan, &rwlock->rwlock_lock);		//wake all
	spinlock_release(&rwlock->rwlock_lock);
	return;
}

void rwlock_acquire_write(struct rwlock *rwlock)
{
	KASSERT(rwlock != NULL);
	spinlock_acquire(&rwlock->rwlock_lock);
	rwlock->rwlock_wc = rwlock->rwlock_wc + 1;			//increment writer count
	while(rwlock->rwlock_wr || rwlock->rwlock_rc > 0)
	{
		wchan_sleep(rwlock->rwlock_wchan, &rwlock->rwlock_lock);	//sleep wait channel while writing or reader count > 0
	}
	spinlock_release(&rwlock->rwlock_lock);				//release spinlock, set writing to true
	rwlock->rwlock_wr = true;
	return;
}

void rwlock_release_write(struct rwlock *rwlock)
{
	KASSERT(rwlock != NULL);
	spinlock_acquire(&rwlock->rwlock_lock);
	rwlock->rwlock_wc = rwlock->rwlock_wc - 1;		//decrement writer count
	rwlock->rwlock_wa = rwlock->rwlock_wr = false;		//set waiting boolean to false
	wchan_wakeall(rwlock->rwlock_wchan, &rwlock->rwlock_lock);	//wake all
	spinlock_release(&rwlock->rwlock_lock);			//release spinlock
	return;
}
