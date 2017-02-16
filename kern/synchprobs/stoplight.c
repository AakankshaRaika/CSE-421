/*
 * Copyright (c) 2001, 2002, 2009
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
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

struct lock *lk1;
struct lock *lk2;
struct lock *lk3;
struct lock *lk4;
struct semaphore *buf_sem;

/*
 * Called by the driver during initialization.
 */

void
stoplight_init() {
	lk1 = lock_create("lk1");				//create locks/sem
	lk2 = lock_create("lk2");
	lk3 = lock_create("lk3");
	lk4 = lock_create("lk4");
	buf_sem = sem_create("buf_sem", 3);
	KASSERT(lk1 != NULL);					//assert not null
	KASSERT(lk2 != NULL);
	KASSERT(lk3 != NULL);
	KASSERT(lk4 != NULL);
	KASSERT(buf_sem != NULL);
	return;
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
	lock_destroy(lk1);					//destroy locks/sem
	lock_destroy(lk2);
	lock_destroy(lk3);
	lock_destroy(lk4);
	sem_destroy(buf_sem);
	return;
}

//Helper Functions

void left_helper(struct lock *, uint32_t direction, uint32_t index);
void left_helper(struct lock *lk_help1, struct lock *lk_help2, struct lock *lk_help3, uint32_t direction, uint32_t index)
{
	int help2_temp = ((direction + 3) % 4);	//left logic
	int help3_temp = ((direction + 2) % 4);
	P(buf_sem);
	lock_acquire(lk_help1);
	inQuadrant(direction, index);
	lock_acquire(lk_help2);
	inQuadrant(help2_temp, index);
	lock_release(lk_help1);
	lock_acquire(lk_help3);
	inQuadrant(help3_temp, index);
	lock_release(lk_help2);
	leaveIntersection(index);
	lock_release(lk_help3)
	V(buf_sem);
	return;
}

void right_helper(struct lock *, uint32_t direction, uint32_t index);
void right_helper(struct lock * lk_help1, uint32_t direction, uint32_t index)
{
	P(buf_sem);						//right logic
	lock_acquire(lk_help1);
	inQuadrant(direction, index);
	leaveIntersection(index);
	lock_release(lk_help1);
	V(buf_sem);
	return;
}

void straight_helper(stuct lock *, uint32_t direction, uint32_t index);
void straight_helper(stuct lock *lk_help1, struct lock *lk_help2, uint32_t direction, uint32_t index)
{
	int help2_temp = ((direction + 3) % 4);	//straight logic
	P(buf_sem);
	lock_acquire(lk_help1);
	inQuadrant(direction, index);
	lock_acquire(lk_help2);
	inQuadrant(help2_temp, index);
	lock_release(lk_help1);
	leaveIntersection(index);
	lock_release(lk_help2);
	V(buf_sem);
	return;
}

//Default functions

void
turnright(uint32_t direction, uint32_t index)
{
	if (direction == 0)					//implement right help
	{
		right_helper(lk1, direction, index);
	}
	else if (direction == 1)
	{
		right_helper(lk2, direction, index);
	}
	else if (direction == 2)
	{
		right_helper(lk3, direction, index);
	}
	else if (direction == 3)
	{
		right_helper(lk4, direction, index);
	}
	return;
}
void
gostraight(uint32_t direction, uint32_t index)
{
	if (direction == 0)					//implement straight help
	{
		straight_helper(lk1, lk4, direction, index);
	}
	else if (direction == 1)
	{
		straight_helper(lk2, lk1, direction, index);
	}
	else if (direction == 2)
	{
		straight_helper(lk3, lk2, direction, index);
	}
	else if (direction == 3)
	{
		straight_helper(lk4, lk3, direction, index);
	}
	return;
}
void
turnleft(uint32_t direction, uint32_t index)
{
	if (direction == 0)					//implement left help
	{
		left_helper(lk1, lk4, lk3, direction, index);
	}
	else if (direction == 1)
	{
		left_helper(lk2, lk1, lk4, direction, index);
	}
	else if (direction == 2)
	{
		left_helper(lk3, lk2, lk1, direction, index);
	}
	else if (direction == 3)
	{
		left_helper(lk4, lk3, lk2, direction, index);
	}
	return;
}
