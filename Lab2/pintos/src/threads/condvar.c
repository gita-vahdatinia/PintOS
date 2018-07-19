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
 * Modifications Copyright (C) 2017 David C. Harrison. All rights reserved.
 */

#include <stdio.h>
#include <string.h>

#include "threads/condvar.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

/* 
 * Initializes condition variable COND.  A condition variable
 * allows one piece of code to signal a condition and cooperating
 * code to receive the signal and act upon it. 
 */
void
condvar_init(struct condvar *cond)
{
    ASSERT(cond != NULL);
    list_init(&cond->waiters);
}
static bool 
waiter_compare(const struct list_elem *a, 
                const struct list_elem *b, 
                void *aux UNUSED){
    return(list_entry(a, struct thread, elem)->priority > (list_entry(b, struct thread, elem)->priority));
}
static bool thread_compare(const struct list_elem *a, 
                const struct list_elem *b, 
                void *aux UNUSED){
    struct semaphore *sema_a = list_entry(a, struct semaphore, elem);
    struct semaphore *sema_b = list_entry(b, struct semaphore, elem);

    list_sort(&sema_a->waiters, waiter_compare, NULL);
    list_sort(&sema_b->waiters, waiter_compare, NULL);

	return(list_entry(list_begin(&sema_a->waiters), struct thread, elem)->priority > 
	list_entry(list_begin(&sema_b->waiters), struct thread, elem)->priority);
}
/* 
 * Atomically releases LOCK and waits for COND to be signaled by
 * some other piece of code.  After COND is signaled, LOCK is
 * reacquired before returning.  LOCK must be held before calling
 * this function.
 *
 * The monitor implemented by this function is "Mesa" style, not
 * "Hoare" style, that is, sending and receiving a signal are not
 * an atomic operation.  Thus, typically the caller must recheck
 * the condition after the wait completes and, if necessary, wait
 * again.
 *
 * A given condition variable is associated with only a single
 * lock, but one lock may be associated with any number of
 * condition variables.  That is, there is a one-to-many mapping
 * from locks to condition variables.
 *
 * This function may sleep, so it must not be called within an
 * interrupt handler.  This function may be called with
 * interrupts disabled, but interrupts will be turned back on if
 * we need to sleep. 
 */
//update the condvar priority when the thread compare
void
condvar_wait(struct condvar *cond, struct lock *lock)
{
    ASSERT(cond != NULL);
    ASSERT(lock != NULL);
    ASSERT(!intr_context());
    ASSERT(lock_held_by_current_thread(lock));


    struct semaphore waiter;
    int sempri = thread_get_priority();
    init_semaphore(&waiter, 0,sempri);
    list_insert_ordered(&cond->waiters, &waiter.elem, thread_compare, NULL);   
//    list_push_back(&cond->waiters, &waiter.elem);
    lock_release(lock);

    list_sort (&cond->waiters, thread_compare, NULL);
    semaphore_down(&waiter);

    lock_acquire(lock);
//	thread_yield();
}

/* 
 * If any threads are waiting on COND(protected by LOCK), then
 * this function signals one of them to wake up from its wait.
 * LOCK must be held before calling this function.
 *
 * An interrupt handler cannot acquire a lock, so it does not
 * make sense to try to signal a condition variable within an
 * interrupt handler. 
 */
void
condvar_signal(struct condvar *cond, struct lock *lock UNUSED)
{
    ASSERT(cond != NULL);
    ASSERT(lock != NULL);
    ASSERT(!intr_context());
    ASSERT(lock_held_by_current_thread(lock));

    if (!list_empty(&cond->waiters)) {
        list_sort (&cond->waiters, thread_compare, NULL);
        semaphore_up(list_entry(list_pop_front(&cond->waiters), struct semaphore, elem));
    }
}

/* 
 * Wakes up all threads, if any, waiting on COND(protected by
 * LOCK).  LOCK must be held before calling this function.
 *
 * An interrupt handler cannot acquire a lock, so it does not
 * make sense to try to signal a condition variable within an
 * interrupt handler. 
 */
void
condvar_broadcast(struct condvar *cond, struct lock *lock)
{
    ASSERT(cond != NULL);
    ASSERT(lock != NULL);

    while (!list_empty(&cond->waiters)) {
        condvar_signal(cond, lock);
    }
}