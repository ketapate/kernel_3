#include "globals.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/kthread.h"
#include "proc/kmutex.h"


/*
 * IMPORTANT: Mutexes can _NEVER_ be locked or unlocked from an
 * interrupt context. Mutexes are _ONLY_ lock or unlocked from a
 * thread context.
 */

/*
 * Initializes the fields of the specified kmutex_t.
 *
 * @param mtx the mutex to initialize
 */
void
kmutex_init(kmutex_t *mtx)
{
    /* NOT_YET_IMPLEMENTED("PROCS: kmutex_init"); */
    
    sched_queue_init(&(mtx->km_waitq));
    mtx->km_holder = NULL;
}

/*
 * This should block the current thread (by sleeping on the mutex's
 * wait queue) if the mutex is already taken.
 *
 * No thread should ever try to lock a mutex it already has locked.
 */
/*
 * Locks the specified mutex.
 *
 * Note: This function may block.
 *
 * Note: These locks are not re-entrant
 *
 * @param mtx the mutex to lock
 */
/* In computing, a computer program or subroutine is called reentrant if it can be interrupted in the middle of its execution and then safely called again ("re-entered") before its previous invocations complete execution.
 */
void
kmutex_lock(kmutex_t *mtx)
{
    /* NOT_YET_IMPLEMENTED("PROCS: kmutex_lock"); */
    
    /* grading guideline required */
    KASSERT(curthr && (curthr != mtx->km_holder));
    dbg(DBG_PRINT, "(GRADING1 5.a) Current thread is not NULL and is not the target mutex's holder\n");
    
    if (mtx->km_holder == NULL) {
        dbg(DBG_PRINT, "No holder before, so this thread becomes the holder\n");
        mtx->km_holder = curthr;
    } else {
        dbg(DBG_PRINT, "Mutex already locked (has holder), so this thread just goes to sleep\n");
        sched_sleep_on(&(mtx->km_waitq));
    }
}

/*
 * This should do the same as kmutex_lock, but use a cancellable sleep
 * instead.
 */
/*
 * Locks the specified mutex, but puts the current thread into a
 * cancellable sleep if the function blocks.
 *
 * Note: This function may block.
 *
 * Note: These locks are not re-entrant.
 *
 * @param mtx the mutex to lock
 * @return 0 if the current thread now holds the mutex and -EINTR if
 * the sleep was cancelled and this thread does not hold the mutex
 */
int
kmutex_lock_cancellable(kmutex_t *mtx)
{
    /* NOT_YET_IMPLEMENTED("PROCS: kmutex_lock_cancellable");*/
    
    /* grading guideline required */
    KASSERT(curthr && (curthr != mtx->km_holder));
    dbg(DBG_PRINT, "(GRADING1 5.b) Current thread is not NULL and is not the target mutex's holder\n");
    
    if (mtx->km_holder == NULL) {
        dbg(DBG_PRINT, "No holder before, so this thread becomes the holder\n");
        mtx->km_holder = curthr;
        return 0;
    } else {
        dbg(DBG_PRINT, "Mutex already locked (has holder), so this thread just goes to sleep\n");
        return sched_cancellable_sleep_on(&(mtx->km_waitq));
    }
}

/*
 * If there are any threads waiting to take a lock on the mutex, one
 * should be woken up and given the lock.
 *
 * Note: This should _NOT_ be a blocking operation!
 *
 * Note: Don't forget to add the new owner of the mutex back to the
 * run queue.
 *
 * Note: Make sure that the thread on the head of the mutex's wait
 * queue becomes the new owner of the mutex.
 *
 * @param mtx the mutex to unlock
 */
void
kmutex_unlock(kmutex_t *mtx)
{
    /*NOT_YET_IMPLEMENTED("PROCS: kmutex_unlock");*/
    
    /* grading guideline required */
    KASSERT(curthr && (curthr == mtx->km_holder));
    dbg(DBG_PRINT, "(GRADING1 5.c) Current thread is not NULL and is the target mutex's holder\n");
    
    if (sched_queue_empty(&(mtx->km_waitq))) {
        dbg(DBG_PRINT, "Mutex waiting queue is empty\n");
        mtx->km_holder = NULL;
    } else {
        dbg(DBG_PRINT, "Mutex waiting queue is not empty\n");
        /* wake the first on in waiting queue, mark it runnable, add it to run queue, then set it as the mutex holder
         */
        mtx->km_holder = sched_wakeup_on(&(mtx->km_waitq));
    }
    
    /* grading guideline required */
    KASSERT(curthr != mtx->km_holder);
    dbg(DBG_PRINT, "(GRADING1 5.c) Current thread is not the mutex's holder now\n");
}
