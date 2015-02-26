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
 * Driver code for whale mating problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>
#include <current.h>

int maleCount;
int femaleCount;
int matchCount;
struct lock *lock;
struct lock *female_lock;
struct lock *match_lock;
struct cv *male_cv;
struct cv *female_cv;
struct cv *match_cv;

/*
 * 08 Feb 2012 : GWA : Driver code is in kern/synchprobs/driver.c. We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

void whalemating_init() {
	maleCount = 0;
	femaleCount = 0;
	matchCount = 0;

	lock = lock_create("male lock");
	female_lock = lock_create("female lock");
	match_lock = lock_create("match lock");
	male_cv = cv_create("male cv");
	female_cv = cv_create("female cv");
	match_cv = cv_create("match cv");
	return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void whalemating_cleanup() {
  return;
}

void
male(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
	(void)which;
	bool wakeall = true;
	
	lock_acquire(lock);
	maleCount++;
	while(!(femaleCount > 0 && matchCount > 0)){
		cv_wait(male_cv,lock);	
		wakeall = false;
	}
	
	if(wakeall){
		cv_signal(female_cv, lock);	
		cv_signal(match_cv, lock);
	}
	
	lock_release(lock);
	male_start();

	// Implement this function 
	//if(wakeall){
	lock_acquire(lock);
	maleCount--;
	//femaleCount--;
	//matchCount--;
	lock_release(lock);
	//}
	male_end();

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

void
female(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  	(void)which;
	bool wakeall = true;

	lock_acquire(lock);
	femaleCount++;
  
	while(!(maleCount > 0 && matchCount > 0)){
		cv_wait(female_cv,lock);
		wakeall = false;
	}

	if(wakeall){
		cv_signal(male_cv,lock);
		cv_signal(match_cv,lock);
	}

	lock_release(lock);
  	female_start();

	// Implement this function 
	//if(wakeall){
	lock_acquire(lock);
	//maleCount--;
	femaleCount--;
	//matchCount--;
	lock_release(lock);
	//}
  	female_end();
  
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

void
matchmaker(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  	(void)which;
	bool wakeall = true;

	lock_acquire(lock);
	matchCount++;

	while(!(maleCount > 0 && femaleCount > 0)){
		cv_wait(match_cv,lock);
		wakeall = false;
	}

	if(wakeall){
		cv_signal(male_cv,lock);
		cv_signal(female_cv,lock);
	}

	lock_release(lock);
  	matchmaker_start();
  
	// Implement this function 
	//if(wakeall){
	lock_acquire(lock);
	//maleCount--;
	//femaleCount--;
	matchCount--;
	lock_release(lock);
	//}
  	matchmaker_end();
  
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

/*
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is,
 * of course, stable under rotation)
 *
 *   | 0 |
 * --     --
 *    0 1
 * 3       1
 *    3 2
 * --     --
 *   | 2 | 
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X
 * first.
 *
 * You will probably want to write some helper functions to assist
 * with the mappings. Modular arithmetic can help, e.g. a car passing
 * straight through the intersection entering from direction X will leave to
 * direction (X + 2) % 4 and pass through quadrants X and (X + 3) % 4.
 * Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in drivers.c.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

struct lock *lk;
struct lock *lk0;
struct lock *lk1;
struct lock *lk2;
struct lock *lk3;
bool lk0avl;
bool lk1avl;
bool lk2avl;
bool lk3avl;

void stoplight_init() {
	lk = lock_create("lk");
        lk0=lock_create("lock0");
        lk1=lock_create("lock1");
        lk2=lock_create("lock2");
        lk3=lock_create("lock3");
	lk0avl=true;
	lk1avl=true;
	lk2avl=true;
	lk3avl=true;
  return;
}


// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void stoplight_cleanup() {
  return;
}

void
gostraight(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
//  (void)direction;
  
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.

	//unsigned long dest = (direction+2)%4;
	
//	kprintf("\t%s starting to go straight\n",curthread->t_name);	
		switch(direction)
		{
			case 0:
				while(1)
				{	
					lock_acquire(lk);
					if(lk0avl && lk3avl)
					{
//						kprintf("%s ; %d %d\n",curthread->t_name,lk0avl,lk3avl);
						lk0avl=false;
						lk3avl=false;

						lock_acquire(lk0);
			//			kprintf("%s : lock0 of 0 & 3 acq\n",curthread->t_name);
						inQuadrant(0);

						lock_acquire(lk3);
			//			kprintf("%s : lock3 of 3 & 3 acq\n",curthread->t_name);

				//		leaveIntersection();

						inQuadrant(3);	
				//		kprintf("Car was going straight\n");
						lock_release(lk0);
						lock_release(lk3);
						leaveIntersection();
						lk0avl=true;
						lk3avl=true;
					lock_release(lk);
						break;
					}
					lock_release(lk);
					
				}
			break;
			case 1:
				while(1)
                              {
					lock_acquire(lk);
                                        if(lk0avl && lk1avl)
                                       {
				//		kprintf("%s ; %d %d\n",curthread->t_name,lk1avl,lk0avl);
                                                lk0avl=false;
                                                lk1avl=false;

                                                lock_acquire(lk1);
				//		kprintf("%s : lock1 of 1 & 0 acq\n",curthread->t_name);
                                                inQuadrant(1);

                                                lock_acquire(lk0);
				//		kprintf("%s : lock0 of 1 & 0 acq\n",curthread->t_name);

                                                inQuadrant(0);
				//		kprintf("Car was going straight\n");
                                                lock_release(lk1);
                                                lock_release(lk0);
                                                leaveIntersection();
                                                lk0avl=true;
                                                lk1avl=true;
					lock_release(lk);
                                                break;
                                        }
					lock_release(lk);
                                }

			break;
			case 2:
				 while(1)
                                {
					lock_acquire(lk);
                                        if(lk2avl && lk1avl)
                                        {
				//		kprintf("%s ; %d %d\n",curthread->t_name,lk2avl,lk1avl);
                                                lk2avl=false;
                                                lk1avl=false;
                
                                                lock_acquire(lk2);
				//		kprintf("%s : lock2 of 2 & 1 acq\n",curthread->t_name);
                                                inQuadrant(2);

                                                lock_acquire(lk1);
				//		kprintf("%s : lock1 of 2 & 1 acq\n",curthread->t_name);

                                                inQuadrant(1);
				//		kprintf("Car was going straight\n");
                                                lock_release(lk2);
                                                lock_release(lk1);
                                                leaveIntersection();
                                                lk2avl=true;
                                                lk1avl=true;
					lock_release(lk);
                                                break;
                                        }
					lock_release(lk);
                                }

			break;
			case 3:
				 while(1)
                                {
					lock_acquire(lk);
                                        if(lk3avl && lk2avl)
                                        {
				///		kprintf("%s : %d %d\n",curthread->t_name,lk3avl,lk2avl);
                                                lk3avl=false;
                                                lk2avl=false;
                
                                                lock_acquire(lk3);
				///		kprintf("%s : lock3 of 3 & 2 acq\n",curthread->t_name);
                                                inQuadrant(3);

                                                lock_acquire(lk2);
				///		kprintf("%s : lock2 of 3 & 2 acq\n",curthread->t_name);

                                                inQuadrant(2);
				//		kprintf("Car was going straight\n");
                                                lock_release(lk3);
                                                lock_release(lk2);
                                                leaveIntersection();

                                                lk3avl=true;
                                                lk2avl=true;
					lock_release(lk);
                                                break;
                                        }
					lock_release(lk);
                                }

			break;
                default:
                        kprintf("################################################################################\n");

		}

	//kprintf("\t%s finished going straight\n",curthread->t_name);	

  V(stoplightMenuSemaphore);
  return;
}

void
turnleft(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  //(void)direction;
  
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
	
//	kprintf("\t%s starting to turn left\n",curthread->t_name);	
	switch(direction)
	{
		case 0:
			while(1)
			{
					lock_acquire(lk);
				if(lk0avl && lk3avl && lk2avl)
				{
			//			kprintf("%s ; %d %d %d\n",curthread->t_name,lk0avl,lk3avl,lk2avl);
					lk0avl=false;
					lk3avl=false;
					lk2avl=false;
					lock_release(lk);

					lock_acquire(lk0);
			//		kprintf("%s : lock0 of 0 & 3 acq\n",curthread->t_name);
					inQuadrant(0);
					lock_acquire(lk3);
			//		kprintf("%s : lock3 of 0 & 3 acq\n",curthread->t_name);
					
					inQuadrant(3);
					lock_acquire(lk2);

					inQuadrant(2);

			//		kprintf("Car was going left\n");
					lock_release(lk0);
					lock_release(lk3);
					lock_release(lk2);
					leaveIntersection();

					lk0avl=true;
					lk3avl=true;
					lk2avl=true;
					break;
				}
					lock_release(lk);
			}
		break;
		case 1:
			 while(1)
                        {
					lock_acquire(lk);
                                if(lk1avl && lk0avl && lk3avl)
                                {
			//		kprintf("%s ; %d %d %d\n",curthread->t_name,lk1avl,lk0avl,lk3avl);
                                        lk1avl=false;
                                        lk0avl=false;
                                        lk3avl=false;
					lock_release(lk);

                                        lock_acquire(lk1);
                                        inQuadrant(1);
                                        lock_acquire(lk0);

                                        inQuadrant(0);
                                        lock_acquire(lk3);

                                        inQuadrant(3);

			///		kprintf("Car was going left\n");
                                        lock_release(lk1);
                                        lock_release(lk0);
                                        lock_release(lk3);
                                        leaveIntersection();

                                        lk1avl=true;
                                        lk0avl=true;
                                        lk3avl=true;
                                        break;
                                }
					lock_release(lk);

                        }

		break;
		case 2:
			 while(1)
                        {
					lock_acquire(lk);
                                if(lk2avl && lk1avl && lk0avl)
                                {
			//		kprintf("%s ; %d %d %d\n",curthread->t_name,lk2avl,lk1avl,lk0avl);
                                        lk2avl=false;
                                        lk1avl=false;
                                        lk0avl=false;
					lock_release(lk);

                                        lock_acquire(lk2);
                                        inQuadrant(2);
                                        lock_acquire(lk1);

                                        inQuadrant(1);
                                        lock_acquire(lk0);

                                        inQuadrant(0);

			//		kprintf("Car was going left\n");
                                        lock_release(lk2);
                                        lock_release(lk1);
                                        lock_release(lk0);
                                        leaveIntersection();

                                        lk2avl=true;
                                        lk1avl=true;
                                        lk0avl=true;

                                        break;
                                }
					lock_release(lk);
                        }

		break;
		case 3:
			 while(1)
                        {
					lock_acquire(lk);
                                if(lk3avl && lk2avl && lk1avl)
                                {
			//		kprintf("%s ; %d %d %d\n",curthread->t_name,lk3avl,lk2avl,lk1avl);
                                        lk3avl=false;
                                        lk2avl=false;
                                        lk1avl=false;
					lock_release(lk);

                                        lock_acquire(lk3);
                                        inQuadrant(3);
                                        lock_acquire(lk2);

                                        inQuadrant(2);
                                        lock_acquire(lk1);

                                        inQuadrant(1);

			//		kprintf("Car was going left\n");
                                        lock_release(lk3);
                                        lock_release(lk2);
                                        lock_release(lk1);
                                        leaveIntersection();

                                        lk3avl=true;
                                        lk2avl=true;
                                        lk1avl=true;
                                        break;
                                }
					lock_release(lk);
                        }

		break;
		default:
			kprintf("################################################################################\n");
	}
	

//	kprintf("\t%s finished turning left\n",curthread->t_name);	

  V(stoplightMenuSemaphore);
  return;
}

void
turnright(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
//  (void)direction;

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly

//	kprintf("\t%s starting to turn right\n",curthread->t_name);	
	switch(direction)
	{
		case 0:
			while(1)
			{
				lock_acquire(lk);
				if(lk0avl)
				{
			//		kprintf("%s ; %d\n",curthread->t_name,lk0avl);
					lk0avl=false;
					lock_acquire(lk0);
					inQuadrant(0);
			//		kprintf("Car was going right\n");
					lock_release(lk0);
					leaveIntersection();
					lk0avl=true;
					lock_release(lk);
					break;
				}
					lock_release(lk);
			}
		break;
		case 1:
			 while(1)
                        {
				lock_acquire(lk);
                                if(lk1avl)
                                {
			//		 kprintf("%s ; %d\n",curthread->t_name,lk1avl);
                                        lk1avl=false;
                                        lock_acquire(lk1);
                                        inQuadrant(1);
			//		kprintf("Car was going right\n");
                                        lock_release(lk1);
                                        leaveIntersection();
                                        lk1avl=true;
					lock_release(lk);
                                        break;
                                }
					lock_release(lk);
                        }

		break;
		case 2:
			 while(1)
                        {
				lock_acquire(lk);
                                if(lk2avl)
                                {
			//		 kprintf("%s ; %d\n",curthread->t_name,lk2avl);
                                        lk2avl=false;
                                        lock_acquire(lk2);
                                        inQuadrant(2);
			//		kprintf("Car was going right\n");
                                        lock_release(lk2);
                                        leaveIntersection();
                                        lk2avl=true;
					lock_release(lk);
                                        break;
                                }
					lock_release(lk);
                        }

		break;
		case 3:
			 while(1)
                        {
				lock_acquire(lk);
                                if(lk3avl)
                                {
			//		 kprintf("%s ; %d\n",curthread->t_name,lk3avl);
                                        lk3avl=false;
                                        lock_acquire(lk3);
                                        inQuadrant(3);
			//		kprintf("Car was going right\n");
                                        lock_release(lk3);
                                        leaveIntersection();
                                        lk3avl=true;
					lock_release(lk);
                                        break;
                                }
					lock_release(lk);
                        }

		break;
                default:
                        kprintf("################################################################################\n");

	}

//	kprintf("\t%s finished turning right\n",curthread->t_name);	
  V(stoplightMenuSemaphore);
  return;
}
