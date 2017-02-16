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
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

struct lock *w_lk;
int m_cnt;
int f_cnt;
int mm_cnt;
struct cv *m_cv;
struct cv *f_cv;
struct cv *mm_cv;

/*
 * Called by the driver during initialization.
 */

void whalemating_init() {
	m_cnt = 0;					//init counts
	f_cnt = 0;
	mm_cnt = 0;
	m_cv = cv_create("m");				//create cv's
	f_cv = cv_create("f");
	mm_cv = cv_create("mm");
	w_lk = lock_create("lk");
	KASSERT(m_cv != NULL);				//assert not null
	KASSERT(f_cv != NULL);
	KASSERT(mm_cv != NULL);
	KASSERT(w_lk != NULL);
	return;
}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
	lock_destroy(w_lk);				//destroy lock/cv's
	cv_destroy(mm_cv);
	cv_destroy(f_cv);
	cv_destroy(m_cv);
	return;
}

void
male(uint32_t index)
{
	male_start(index);				//start male thread
	m_cnt = m_cnt + 1;
	lock_acquire(w_lk);				//acquire lock
	if(mm_cnt > 0 && f_cnt > 0)			//logic
	{
		f_cnt = f_cnt - 1;
		cv_signal(f_cv, w_lk);
		mm_cnt = mm_cnt - 1;
		cv_signal(mm_cv, w_lk);
		m_cnt = m_cnt - 1;
	}
	else if(mm_cnt <= 0 || f_cnt <= 0)
	{
		cv_wait(m_cv, w_lk);
	}
	male_end(index);				//end male thread
	lock_release(w_lk);				//release lock
	return;
}

void
female(uint32_t index)
{
	female_start(index);				//start female thread
	f_cnt = f_cnt + 1;
	lock_acquire(w_lk);				//acquire lock
	if(mm_cnt > 0 && m_cnt > 0)			//logic
	{
		m_cnt = m_cnt - 1;
		cv_signal(m_cv, w_lk);
		mm_cnt = mm_cnt - 1;
		cv_signal(mm_cv, w_lk);
		f_cnt = f_cnt - 1;
	}
	else if(mm_cnt <= 0 || m_cnt <= 0)
	{
		cv_wait(f_cv, w_lk);
	}
	female_end(index);				//end female thread
	lock_release(w_lk);				//release lock
	return;
}

void
matchmaker(uint32_t index)
{
	matchmaker_start(index);			//start mm thread
	mm_cnt = mm_cnt + 1;
	lock_acquire(w_lk);				//acquire lock
	if(m_cnt > 0 && f_cnt > 0)			//logic
	{
		m_cnt = m_cnt - 1;
		cv_signal(m_cv, w_lk);
		f_cnt = f_cnt - 1;
		cv_signal(f_cv, w_lk);
		mm_cnt = mm_cnt - 1;
	}
	else if(m_cnt <=0 || f_cnt <=0)
	{
		cv_wait(mm_cv, w_lk);
	}
	matchmaker_end(index);				//end mm thread
	lock_release(w_lk);				//release lock
	return;
}
