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
sem_create(const char *name, int initial_count)
{
        struct semaphore *sem;

        KASSERT(initial_count >= 0);

        sem = kmalloc(sizeof(struct semaphore));
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

	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 * Bridge to the wchan lock, so if someone else comes
		 * along in V right this instant the wakeup can't go
		 * through on the wchan until we've finished going to
		 * sleep. Note that wchan_sleep unlocks the wchan.
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
		wchan_lock(sem->sem_wchan);
		spinlock_release(&sem->sem_lock);
                wchan_sleep(sem->sem_wchan);

		spinlock_acquire(&sem->sem_lock);
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
	wchan_wakeone(sem->sem_wchan);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(struct lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }
        
        // add stuff here as needed
        lock->lk_wchan = wchan_create(lock->lk_name); 
	if(lock->lk_wchan == NULL) {
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}

	spinlock_init(&lock->lk_spin);
	lock->lk_isLocked = false;
	lock->thread = NULL;

        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

        // add stuff here as needed
        
        kfree(lock->lk_name);
	kfree(lock->thread);
	spinlock_cleanup(&lock->lk_spin);
	wchan_destroy(lock->lk_wchan);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
        // Write this
	KASSERT(lock != NULL);

	KASSERT(curthread->t_in_interrupt == false);

	if(lock_do_i_hold(lock)){
		panic("Trying to acquire the lock for the second time\n");
		return;
	}

	spinlock_acquire(&lock->lk_spin);	

	while(lock->lk_isLocked == true) {
		wchan_lock(lock->lk_wchan);
		spinlock_release(&lock->lk_spin);
		wchan_sleep(lock->lk_wchan);

		spinlock_acquire(&lock->lk_spin);
	}

	lock->lk_isLocked = true;
	lock->thread = curthread;
	spinlock_release(&lock->lk_spin);
}

void
lock_release(struct lock *lock)
{
        // Write this
	KASSERT(lock != NULL);
	
	if(lock_do_i_hold(lock)){
		spinlock_acquire(&lock->lk_spin);
		lock->lk_isLocked = false;
		lock->thread = NULL;
	
		wchan_wakeone(lock->lk_wchan);
		spinlock_release(&lock->lk_spin);	
	} else {
		return;
	}
}

bool
lock_do_i_hold(struct lock *lock)
{
        // Write this
	if(lock->thread == curthread) {
		return true;
	} else {
		return false;
	}
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(struct cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }
        
        // add stuff here as needed
	cv->cv_wchan = wchan_create(cv->cv_name);
	if(cv->cv_wchan == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

	spinlock_init(&cv->cv_lock);
	//cv->cv_var = false;
        
        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

        // add stuff here as needed
        spinlock_cleanup(&cv->cv_lock);
	wchan_destroy(cv->cv_wchan);
        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
        // Write this
	if(lock_do_i_hold(lock)) {
		wchan_lock(cv->cv_wchan);
		lock_release(lock);
		wchan_sleep(cv->cv_wchan);
		lock_acquire(lock);	
	} else {
		return;
	}
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
        // Write this
	if(lock_do_i_hold(lock)){
		spinlock_acquire(&cv->cv_lock);
		wchan_wakeone(cv->cv_wchan);
		spinlock_release(&cv->cv_lock);
	} else {
		return;
	}
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
	if(lock_do_i_hold(lock)){
		spinlock_acquire(&cv->cv_lock);
		wchan_wakeall(cv->cv_wchan);
		spinlock_release(&cv->cv_lock);
	} else {
		return;
	}
}

struct rwlock*
rwlock_create(const char *name){
	struct rwlock *rwlock;

	rwlock = kmalloc(sizeof(struct rwlock));
	if(rwlock == NULL) {
		return NULL;
	}

	rwlock->rwlk_name = kstrdup(name);
	if(rwlock->rwlk_name == NULL){
		kfree(rwlock);
		return NULL;
	} 

	rwlock->rwlk_rwchan = wchan_create(rwlock->rwlk_name);
	if(rwlock->rwlk_rwchan == NULL){
		kfree(rwlock->rwlk_name);
		kfree(rwlock);
		return NULL;
	}

	rwlock->rwlk_wwchan = wchan_create(rwlock->rwlk_name);
	if(rwlock->rwlk_wwchan == NULL){
		kfree(rwlock->rwlk_name);
		wchan_destroy(rwlock->rwlk_rwchan);
		kfree(rwlock);
		return NULL;
	}

	rwlock->rwlk_lock = lock_create("lock");
	if(rwlock->rwlk_lock == NULL){
		kfree(rwlock->rwlk_name);
		wchan_destroy(rwlock->rwlk_rwchan);
		wchan_destroy(rwlock->rwlk_wwchan);
		kfree(rwlock);
		return NULL;
	}

	spinlock_init(&rwlock->rwlk_spin);
	rwlock->rwlk_rcount = 0;
	rwlock->rwlk_iswriting = false;
	//rwlock->rwlk_wcount = 0;
	rwlock->rwlk_prevrelease = false;

	return rwlock;
}

void
rwlock_destroy(struct rwlock *rwlock){
	KASSERT(rwlock != NULL);

	kfree(rwlock->rwlk_name);
	wchan_destroy(rwlock->rwlk_rwchan);
	wchan_destroy(rwlock->rwlk_wwchan);
	spinlock_cleanup(&rwlock->rwlk_spin);
	lock_destroy(rwlock->rwlk_lock);
	kfree(rwlock);
}

void
rwlock_acquire_read(struct rwlock *rwlock){
	KASSERT(rwlock != NULL);

	spinlock_acquire(&rwlock->rwlk_spin);
	
	/*reader should wait in the following conditions:
	1. if writer is accessing the resource (in this case lock will not be available).
	2. if writer is waiting and the no of readers currently accessing the resource is beyond
	   the permissible limit.
	*/
	while(rwlock->rwlk_iswriting ||	(!wchan_isempty(rwlock->rwlk_wwchan) && rwlock->rwlk_rcount > 10)) {
		wchan_lock(rwlock->rwlk_rwchan);
		spinlock_release(&rwlock->rwlk_spin);
		wchan_sleep(rwlock->rwlk_rwchan);
	}

	//increase the count before accessing the resource
	rwlock->rwlk_rcount++;
	spinlock_release(&rwlock->rwlk_spin);
}

void
rwlock_release_read(struct rwlock *rwlock){
	KASSERT(rwlock != NULL);

	spinlock_acquire(&rwlock->rwlk_spin);
	//decrement the count when releasing the resource
	rwlock->rwlk_rcount--;
	
	//reader should wake any thread only when there is no other reader accessing
	//the resource
	if(rwlock->rwlk_rcount > 0){
		return;
	}
	
	/*If rwlk_prevrelease value is true (writer was released from wait before), follow these conditions:
	Check if there are any readers in wait channel. 
	1. if yes, release one reader (loophole)???????
	2. else release one for write
	If rwlk_prevrelease value is false (reader was released before), check if there are any writers waiting
	1. if yes, release one writer
	2. else release all readers */
 
	if(rwlock->rwlk_prevrelease){
		if(!wchan_isempty(rwlock->rwlk_rwchan)){
			//should also check if there is any writer waiting. if so release only one to
			//prevent starvation.
			(wchan_isempty(rwlock->rwlk_wwchan)) ? wchan_wakeall(rwlock->rwlk_rwchan) : wchan_wakeone(rwlock->rwlk_rwchan);
			rwlock->rwlk_prevrelease = false;
		} else {
			wchan_wakeone(rwlock->rwlk_wwchan);
			rwlock->rwlk_prevrelease = true;
		}
	} else {
		if(!wchan_isempty(rwlock->rwlk_wwchan)){
			wchan_wakeone(rwlock->rwlk_wwchan);
			rwlock->rwlk_prevrelease = true;
		} else {
			wchan_wakeall(rwlock->rwlk_rwchan);
			rwlock->rwlk_prevrelease = false;
		}
	}

	spinlock_release(&rwlock->rwlk_spin);
}

void
rwlock_acquire_write(struct rwlock *rwlock){
	KASSERT(rwlock != NULL);

	spinlock_acquire(&rwlock->rwlk_spin);

	//writer should wait if there is any reader accessing the resource
	while(rwlock->rwlk_rcount > 0 || rwlock->rwlk_iswriting){
		wchan_lock(rwlock->rwlk_wwchan);
		//increase the counter if waiting
		//rwlock->rwlk_wcount++;
		spinlock_release(&rwlock->rwlk_spin);
		wchan_sleep(rwlock->rwlk_wwchan);

		spinlock_acquire(&rwlock->rwlk_spin);
	}

	rwlock->rwlk_iswriting = true;
	//lock_acquire(rwlock->rwlk_lock);

	//decrement the count once awaken from sleep
	//rwlock->rwlk_wcount--;
	spinlock_release(&rwlock->rwlk_spin);	
}

void
rwlock_release_write(struct rwlock *rwlock){
	KASSERT(rwlock != NULL);

	spinlock_acquire(&rwlock->rwlk_spin);
	rwlock->rwlk_iswriting = false;
	//lock_release(rwlock->rwlk_lock);

	/*No need to check for rwlk_prevrelease because if writer is releasing the lock then
	there is no probability that a reader will be accessing at the same time nor any writer
	thread would have been woken before/after this thread. Hence, reader should be waken up
	at any cost (unless there is no reader).*/	
	if(!wchan_isempty(rwlock->rwlk_rwchan)){
		//Check if any writer is waiting else wake all readers
		(!wchan_isempty(rwlock->rwlk_wwchan)) ? wchan_wakeone(rwlock->rwlk_rwchan) : wchan_wakeall(rwlock->rwlk_rwchan);
		rwlock->rwlk_prevrelease = false;
	} else {
		wchan_wakeone(rwlock->rwlk_wwchan);
		rwlock->rwlk_prevrelease = true;
	}

	spinlock_release(&rwlock->rwlk_spin);
}
