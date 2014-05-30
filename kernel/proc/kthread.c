#include "config.h"
#include "globals.h"

#include "errno.h"

#include "util/init.h"
#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"

#include "mm/slab.h"
#include "mm/page.h"

kthread_t *curthr; /* global */
static slab_allocator_t *kthread_allocator = NULL;

#ifdef __MTP__
/* Stuff for the reaper daemon, which cleans up dead detached threads */
static proc_t *reapd = NULL;
static kthread_t *reapd_thr = NULL;
static ktqueue_t reapd_waitq;
static list_t kthread_reapd_deadlist; /* Threads to be cleaned */

static void *kthread_reapd_run(int arg1, void *arg2);
#endif

void
kthread_init()
{
        kthread_allocator = slab_allocator_create("kthread", sizeof(kthread_t));
        KASSERT(NULL != kthread_allocator);
}

/**
 * Allocates a new kernel stack.
 *
 * @return a newly allocated stack, or NULL if there is not enough
 * memory available
 */
static char *
alloc_stack(void)
{
        /* extra page for "magic" data */
        char *kstack;
        int npages = 1 + (DEFAULT_STACK_SIZE >> PAGE_SHIFT);
        kstack = (char *)page_alloc_n(npages);

        return kstack;
}

/**
 * Frees a stack allocated with alloc_stack.
 *
 * @param stack the stack to free
 */
static void
free_stack(char *stack)
{
        page_free_n(stack, 1 + (DEFAULT_STACK_SIZE >> PAGE_SHIFT));
}

/*
 * Allocate a new stack with the alloc_stack function. The size of the
 * stack is DEFAULT_STACK_SIZE.
 *
 * Don't forget to initialize the thread context with the
 * context_setup function. The context should have the same pagetable
 * pointer as the process.
 */
/*
 * Allocates and initializes a kernel thread.
 *
 * @param p the process in which the thread will run
 * @param func the function that will be called when the newly created
 * thread starts executing
 * @param arg1 the first argument to func
 * @param arg2 the second argument to func
 * @return the newly created thread
 */
kthread_t *
kthread_create(struct proc *p, kthread_func_t func, long arg1, void *arg2)
{
    /* NOT_YET_IMPLEMENTED("PROCS: kthread_create"); */
    
    /* grading guideline required */
    KASSERT(NULL != p);
    dbg(DBG_PRINT, "(GRADING1 3.a) This thread has associated process\n");
    
    kthread_t *newThr = (kthread_t *)slab_obj_alloc(kthread_allocator);
    
    newThr->kt_kstack = alloc_stack();
    newThr->kt_retval = NULL;
    newThr->kt_errno = 0;
    newThr->kt_proc = p;
    newThr->kt_cancelled = 0;
    newThr->kt_wchan = NULL;
    newThr->kt_state = KT_NO_STATE;
    /* first, initial list link*/
    list_link_init(&(newThr->kt_qlink));
    
    list_link_init(&(newThr->kt_plink));
    /* add this thread to its process*/
    list_insert_tail(&(p->p_threads), &(newThr->kt_plink));
    
    /* we don't need to init _MTP_ part because in weenix, there is only one thread in each process.
      even we hope to support MTP, we can add it letter.
     */
    
    /* Initialize the thread context*/
    context_setup(&(newThr->kt_ctx), func, (int) arg1, arg2, newThr->kt_kstack, DEFAULT_STACK_SIZE, p->p_pagedir);
     
    return newThr;
}

void
kthread_destroy(kthread_t *t)
{
        KASSERT(t && t->kt_kstack);
        free_stack(t->kt_kstack);
        if (list_link_is_linked(&t->kt_plink))
                list_remove(&t->kt_plink);

        slab_obj_free(kthread_allocator, t);
}

/*
 * If the thread to be cancelled is the current thread, this is
 * equivalent to calling kthread_exit. Otherwise, the thread is
 * sleeping and we need to set the cancelled and retval fields of the
 * thread.
 *
 * If the thread's sleep is cancellable, cancelling the thread should
 * wake it up from sleep.
 *
 * If the thread's sleep is not cancellable, we do nothing else here.
 */
/*
 * Cancel a thread.
 *
 * @param kthr the thread to be cancelled
 * @param retval the return value for the thread
 */
void
kthread_cancel(kthread_t *kthr, void *retval)
{
    /* NOT_YET_IMPLEMENTED("PROCS: kthread_cancel"); */
    
    /* grading guideline required */
    KASSERT(NULL != kthr);
    dbg(DBG_PRINT, "(GRADING1 3.b) This thread not NULL\n");
    
    if (kthr == curthr) {
        kthread_exit(retval);
    } else {
        kthr->kt_cancelled = 1;
        kthr->kt_retval = retval;
        if (kthr->kt_state == KT_SLEEP_CANCELLABLE) {
            sched_cancel(kthr);
        }
    }
}

/*
 * You need to set the thread's retval field, set its state to
 * KT_EXITED, and alert the current process that a thread is exiting
 * via proc_thread_exited.
 *
 * It may seem unneccessary to push the work of cleaning up the thread
 * over to the process. However, if you implement MTP, a thread
 * exiting does not necessarily mean that the process needs to be
 * cleaned up.
 */
/*
 * Exits the current thread.
 *
 * @param retval the return value for the thread
 */
void
kthread_exit(void *retval)
{
    /* NOT_YET_IMPLEMENTED("PROCS: kthread_exit"); */
    
    /* grading guideline required */
    KASSERT(!curthr->kt_wchan);
    dbg(DBG_PRINT, "(GRADING1 3.c) The queue this thread is blocked on is empty\n");
    /* grading guideline required */
    KASSERT(!curthr->kt_qlink.l_next && !curthr->kt_qlink.l_prev);
    dbg(DBG_PRINT, "(GRADING1 3.c) The queue is empty\n");
    /* grading guideline required */
    KASSERT(curthr->kt_proc == curproc);
    dbg(DBG_PRINT, "(GRADING1 3.c) This thread's process is the current process\n");
    
    curthr->kt_retval = retval;
    curthr->kt_state = KT_EXITED;
    proc_thread_exited(retval);
}

/*
 * The new thread will need its own context and stack. Think carefully
 * about which fields should be copied and which fields should be
 * freshly initialized.
 *
 * You do not need to worry about this until VM.
 */
kthread_t *
kthread_clone(kthread_t *thr)
{
    /*NOT_YET_IMPLEMENTED("VM: kthread_clone");*/
    
    KASSERT(KT_RUN == thr->kt_state);
    dbg(DBG_PRINT, "(GRADING3A 8.a) KT_RUN == thr->kt_state \n");

    /* Same code from kthread_create copied here.*/
    kthread_t *newThr = (kthread_t *)slab_obj_alloc(kthread_allocator);
    newThr->kt_kstack = alloc_stack();
    newThr->kt_retval = thr->kt_retval;
    newThr->kt_errno = thr->kt_errno;
    newThr->kt_proc = NULL; /* After implementing fork, it seems that this cannot be set yet. Needs to be set in fork.*/
    newThr->kt_cancelled = thr->kt_cancelled;
    newThr->kt_wchan = NULL;
    newThr->kt_state = thr->kt_state;
    /* first, initial list link*/
    list_link_init(&(newThr->kt_qlink));
    list_link_init(&(newThr->kt_plink));
    
    /* Initialize the thread context*/
    
    /* It seems like we are leaving the context unintialized. However this will be done in 
     * fork by calling fork_setup_stack*/
    
    KASSERT(KT_RUN == newThr->kt_state);
    dbg(DBG_PRINT, "(GRADING3A 8.a) KT_RUN == newthr->kt_state \n");
    
    return newThr;
}

/*
 * The following functions will be useful if you choose to implement
 * multiple kernel threads per process. This is strongly discouraged
 * unless your weenix is perfect.
 */
#ifdef __MTP__
int
kthread_detach(kthread_t *kthr)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_detach");
        return 0;
}

int
kthread_join(kthread_t *kthr, void **retval)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_join");
        return 0;
}

/* ------------------------------------------------------------------ */
/* -------------------------- REAPER DAEMON ------------------------- */
/* ------------------------------------------------------------------ */
static __attribute__((unused)) void
kthread_reapd_init()
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_init");
}
init_func(kthread_reapd_init);
init_depends(sched_init);

void
kthread_reapd_shutdown()
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_shutdown");
}

static void *
kthread_reapd_run(int arg1, void *arg2)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_run");
        return (void *) 0;
}
#endif
