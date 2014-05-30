#include "kernel.h"
#include "config.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"
#include "proc/proc.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/mm.h"
#include "mm/mman.h"

#include "vm/vmmap.h"

#include "fs/vfs.h"
#include "fs/vfs_syscall.h"
#include "fs/vnode.h"
#include "fs/file.h"

proc_t *curproc = NULL; /* global */
static slab_allocator_t *proc_allocator = NULL;

static list_t _proc_list;
static proc_t *proc_initproc = NULL; /* Pointer to the init process (PID 1) */

void
proc_init()
{
    list_init(&_proc_list);
    proc_allocator = slab_allocator_create("proc", sizeof(proc_t));
    KASSERT(proc_allocator != NULL);
}

static pid_t next_pid = 0;

/**
 * Returns the next available PID.
 *
 * Note: Where n is the number of running processes, this algorithm is
 * worst case O(n^2). As long as PIDs never wrap around it is O(n).
 *
 * @return the next available PID
 */
static int
_proc_getid()
{
    proc_t *p;
    pid_t pid = next_pid;
    while (1) {
    failed:
        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
            if (p->p_pid == pid) {
                if ((pid = (pid + 1) % PROC_MAX_COUNT) == next_pid) {
                    return -1;
                } else {
                    goto failed;
                }
            }
        } list_iterate_end();
        next_pid = (pid + 1) % PROC_MAX_COUNT;
        return pid;
    }
}

/*
 * The new process, although it isn't really running since it has no
 * threads, should be in the PROC_RUNNING state.
 *
 * Don't forget to set proc_initproc when you create the init
 * process. You will need to be able to reference the init process
 * when reparenting processes to the init process.
 */
/*
 * This function allocates and initializes a new process.
 *
 * @param name the name to give the newly created process
 * @return the newly created process
 */
proc_t *
proc_create(char *name)
{
    /* NOT_YET_IMPLEMENTED("PROCS: proc_create");*/
    
    proc_t *newProc = (proc_t *)slab_obj_alloc(proc_allocator);
    newProc->p_pid = _proc_getid();
    
    /* grading guideline required */
    KASSERT(PID_IDLE != newProc->p_pid || list_empty(&_proc_list));
    dbg(DBG_PRINT, "(GRADING1 2.a) pid can only be PID_IDLE if this is the first process\n");
    
    /* grading guideline required */
    KASSERT(PID_INIT != newProc->p_pid || PID_IDLE == curproc->p_pid);
    dbg(DBG_PRINT, "(GRADING1 2.a) pid can only be PID_INIT when creating from idle process\n");
    
    if (newProc->p_pid == PID_INIT) {
        proc_initproc = newProc;
    }
    
    /* newProc->p_comm = ""; Correction by Manan*/
    strncpy(newProc->p_comm, (const char *)name, PROC_NAME_LEN);
    
    /*list_link_init(&(newProc->p_threads));*/
    list_init(&(newProc->p_threads));
    
    /*list_link_init(&(newProc->p_children));*/
    list_init(&(newProc->p_children));
    
    if (newProc->p_pid != PID_IDLE){
        newProc->p_pproc = curproc;
    } else {
        newProc->p_pproc = NULL;
    }
    
    /* not 100% sure. Other number?*/
    newProc->p_status = 0;
    newProc->p_state = PROC_RUNNING;
    sched_queue_init(&(newProc->p_wait));
    
    /* not 100% sure. New or from parent?*/
    newProc->p_pagedir = pt_create_pagedir();
    list_link_init(&(newProc->p_list_link));
    list_insert_tail(&(_proc_list), &(newProc->p_list_link));
    list_link_init(&(newProc->p_child_link));
    
    /* add the new process to its parent process's children list*/
    if (newProc->p_pid != PID_IDLE) {
        list_insert_tail(&(curproc->p_children), &(newProc->p_child_link));
    }
    
    /* VFS-related: START*/
    /* set all the entries in the file descriptor table to be NULL */
    int fileDes;
    for (fileDes = 0; fileDes < NFILES; fileDes++) {
        newProc->p_files[fileDes] = NULL;
    }
    
    /* set the current working directory of the new process */
    if (newProc->p_pid == PID_IDLE){
        newProc->p_cwd = vfs_root_vn;
    } else {
        newProc->p_cwd = curproc->p_cwd;
    }
    
    /* increment the ref count of the vnode by 1 */
    /* Corrected: if is Idle or Init process, won't vref here */
    if (newProc->p_cwd != NULL && newProc->p_pid != PID_IDLE && newProc->p_pid != PID_INIT) {
        vref(newProc->p_cwd);
    }
    /* VFS-related: END*/
    
    /* VM-related: START*/
    
    /* updated */
    vmmap_t *newVMmap = vmmap_create();
    if (newVMmap == NULL) {
        slab_obj_free(proc_allocator, newProc);
        return NULL;
    }
    newProc->p_vmmap = newVMmap;
    newProc->p_vmmap->vmm_proc = newProc;
    
    /* VM-related: END*/
    
    return newProc;
}

/**
 * Cleans up as much as the process as can be done from within the
 * process. This involves:
 *    - Closing all open files (VFS)
 *    - Cleaning up VM mappings (VM)
 *    - Waking up its parent if it is waiting
 *    - Reparenting any children to the init process
 *    - Setting its status and state appropriately
 *
 * The parent will finish destroying the process within do_waitpid (make
 * sure you understand why it cannot be done here). Until the parent
 * finishes destroying it, the process is informally called a 'zombie'
 * process.
 *
 * This is also where any children of the current process should be
 * reparented to the init process (unless, of course, the current
 * process is the init process. However, the init process should not
 * have any children at the time it exits).
 *
 * Note: You do _NOT_ have to special case the idle process. It should
 * never exit this way.
 *
 * @param status the status to exit the process with
 */
void
proc_cleanup(int status)
{
    /* NOT_YET_IMPLEMENTED("PROCS: proc_cleanup");*/
    
    /* grading guideline required */
    KASSERT(NULL != proc_initproc);
    dbg(DBG_PRINT, "(GRADING1 2.b) Do have an init process\n");
    /* grading guideline required */
    KASSERT(1 <= curproc->p_pid);
    dbg(DBG_PRINT, "(GRADING1 2.b) This process is not the idle process\n");
    /* grading guideline required */
    KASSERT(NULL != curproc->p_pproc);
    dbg(DBG_PRINT, "(GRADING1 2.b) This process has parent process\n");
    
    /* VFS-related: START*/
    /* check the entries in the file descriptor table and do_close */
    int fileDes;
    for (fileDes = 0; fileDes < NFILES; fileDes++) {
        if (curproc->p_files[fileDes] != NULL) {
			do_close(fileDes);
		}
    }
    
    /* decrement the ref count of the vnode by 1 */
    if (curproc->p_cwd != NULL) {
        vput(curproc->p_cwd);
        curproc->p_cwd = NULL;
    }
    /* VFS-related: END*/
    
    /* VM-related: START*/
    
    if (curproc->p_vmmap) {
        vmmap_destroy(curproc->p_vmmap);
    }
    
    /* VM-related: END*/
    
    /* Waking up its parent if it is waiting*/
    if (sched_queue_empty(&(curproc->p_pproc->p_wait)) != 1) {
        sched_wakeup_on(&(curproc->p_pproc->p_wait));
    }
    /* Reparenting any children to the init process*/
    if (curproc != proc_initproc) {
        proc_t *childProc;
        list_iterate_begin(&(curproc->p_children), childProc, proc_t, p_child_link) {
            /* change parent*/
            childProc->p_pproc = proc_initproc;
            /* remove from original process's children list*/
            list_remove(&(childProc->p_child_link));
            /* insert to init process's children list*/
            list_insert_tail(&(proc_initproc->p_children),
                             &(childProc->p_child_link));
        } list_iterate_end();
    }
    
    /* Setting its status and state appropriately*/
    curproc->p_status = status;
    curproc->p_state = PROC_DEAD;
    
    /* grading guideline required */
    KASSERT(NULL != curproc->p_pproc);
    dbg(DBG_PRINT, "(GRADING1 2.b) This process has parent process\n");
}

/*
 * This has nothing to do with signals and kill(1).
 *
 * Calling this on the current process is equivalent to calling
 * do_exit().
 *
 * In Weenix, this is only called from proc_kill_all.
 */
/*
 * Stops another process from running again by cancelling all its
 * threads.
 *
 * @param p the process to kill
 * @param status the status the process should exit with
 */
void
proc_kill(proc_t *p, int status)
{
    /* NOT_YET_IMPLEMENTED("PROCS: proc_kill");*/
    if (p != curproc) {
        /* cancle all the threads of proc p (which is not the curproc, so its threads will not be the current thread)*/
        kthread_t *oneThr;
        list_iterate_begin(&(p->p_threads), oneThr, kthread_t, kt_plink) {
            kthread_cancel(oneThr, (void *)status);
        } list_iterate_end();
    } else {
        do_exit(status);
    }
}

/*
 * Remember, proc_kill on the current process will _NOT_ return.
 * Don't kill direct children of the idle process.
 *
 * In Weenix, this is only called by sys_halt.
 */
/*
 * Kill every process except for the idle process and direct children of
 * the idle process.
 */
void
proc_kill_all()
{
    /* NOT_YET_IMPLEMENTED("PROCS: proc_kill_all");*/
    proc_t *oneProc;
    list_iterate_begin(&_proc_list, oneProc, proc_t, p_list_link) {
        if (oneProc != curproc && (oneProc->p_pid != PID_IDLE) /*&& (oneProc->p_pid != PID_INIT)*/ && (oneProc->p_pproc->p_pid != PID_IDLE)) {
            proc_kill(oneProc, oneProc->p_status); /* Corrected by Manan.*/
        }
    } list_iterate_end();
    if (curproc->p_pproc->p_pid != PID_IDLE){
        proc_kill(curproc, curproc->p_status); /* Correction by Manan. */
    }
}

proc_t *
proc_lookup(int pid)
{
    proc_t *p;
    list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
        if (p->p_pid == pid) {
            return p;
        }
    } list_iterate_end();
    return NULL;
}

list_t *
proc_list()
{
    return &_proc_list;
}

/*
 * This function is only called from kthread_exit.
 *
 * Unless you are implementing MTP, this just means that the process
 * needs to be cleaned up and a new thread needs to be scheduled to
 * run. If you are implementing MTP, a single thread exiting does not
 * necessarily mean that the process should be exited.
 */
/*
 * Alerts the process that the currently executing thread has just
 * exited.
 *
 * @param retval the return value for the current thread
 */
void
proc_thread_exited(void *retval)
{
    /* NOT_YET_IMPLEMENTED("PROCS: proc_thread_exited");*/
    /* retval can show the kthread status, which will affect the process status*/
    proc_cleanup((int)retval);
    /* schedule a new thread to run*/
	sched_switch();
}

/* If pid is -1 dispose of one of the exited children of the current
 * process and return its exit status in the status argument, or if
 * all children of this process are still running, then this function
 * blocks on its own p_wait queue until one exits.
 *
 * If pid is greater than 0 and the given pid is a child of the
 * current process then wait for the given pid to exit and dispose
 * of it.
 *
 * If the current process has no children, or the given pid is not
 * a child of the current process return -ECHILD.
 *
 * Pids other than -1 and positive numbers are not supported.
 * Options other than 0 are not supported.
 */
/*
 * This function implements the waitpid(2) system call.
 *
 * @param pid see waitpid man page, only -1 or positive numbers are supported
 * @param options see waitpid man page, only 0 is supported
 * @param status used to return the exit status of the child
 *
 * @return the pid of the child process which was cleaned up, or
 * -ECHILD if there are no children of this process
 */
pid_t
do_waitpid(pid_t pid, int options, int *status)
{
    /* NOT_YET_IMPLEMENTED("PROCS: do_waitpid");*/
    
    KASSERT(pid == -1 || pid > 0);
    KASSERT(options == 0);
    
    if (list_empty(&(curproc->p_children))) {
        return -ECHILD;
    }
    
    proc_t *childP;
    kthread_t *thrToDestroy;
    if (pid == -1) {
        while (1) {
            list_iterate_begin(&(curproc->p_children), childP, proc_t, p_child_link) {
                if (childP->p_state == PROC_DEAD) {
                    
                    /* grading guideline required */
                    KASSERT(NULL != childP);
                    dbg(DBG_PRINT, "(GRADING1 2.c) The process is not NULL\n");
                    /* grading guideline required */
                    KASSERT(-1 == pid || childP->p_pid == pid);
                    dbg(DBG_PRINT, "(GRADING1 2.c) Did find the process\n");
                    
                    /* save the target child's PID, used when return*/
                    pid_t targetPID = childP->p_pid;
                    
                    /* return its exit status in the status argument*/
                    if (status != NULL){
                        *status = childP->p_status;
                    }
                    
                    /* destroy all threads belongs to this process*/
                    list_iterate_begin(&(childP->p_threads), thrToDestroy, kthread_t, kt_plink) {
                        
                        /* grading guideline required */
                        KASSERT(KT_EXITED == thrToDestroy->kt_state);
                        dbg(DBG_PRINT, "(GRADING1 2.c) The thread to be destroied is exited\n");
                        
                        kthread_destroy(thrToDestroy);
                    } list_iterate_end();
                    
                    /* remove from parent process's children process list*/
                    /* list_remove(&(p_child_link)); Corrected by Manan. */
                    list_remove(&(childP->p_child_link));
                    /* remove from _proc_list*/
                    /* list_remove(&(p_list_link)); Corrected by Manan .*/
                    list_remove(&(childP->p_list_link));
                    /* remove pagedir*/
                    
                    /* grading guideline required */
                    KASSERT(NULL != childP->p_pagedir);
                    dbg(DBG_PRINT, "(GRADING1 2.c) This process has pagedir\n");
                    
                    pt_destroy_pagedir(childP->p_pagedir);
                    /* put back memory slab*/
                    slab_obj_free(proc_allocator, childP);
                    
                    return targetPID;
                }
            } list_iterate_end();
            /* reach here means all children processes are running*/
            /* so we need to sleep to wait for one of them*/
            /* not 100% sure, use sleep_on or sleep_on_cancellable*/
            sched_sleep_on(&(curproc->p_wait));
        }
    } else {
        /* first, check if the given pid is a child of the current process*/
        int hasChildPid = 0;
        list_iterate_begin(&(curproc->p_children), childP, proc_t, p_child_link) {
            if (childP->p_pid == pid) {
                hasChildPid = 1;
            }
        } list_iterate_end();
        if(hasChildPid == 0){
            return -ECHILD;
        }
        
        /* now let's handle the specific child process*/
        while (1) {
            list_iterate_begin(&(curproc->p_children), childP, proc_t, p_child_link) {
                if (childP->p_pid == pid) {
                    if (childP->p_state == PROC_DEAD) {
                        
                        /* grading guideline required */
                        KASSERT(NULL != childP);
                        dbg(DBG_PRINT, "(GRADING1 2.c) The process is not NULL\n");
                        /* grading guideline required */
                        KASSERT(-1 == pid || childP->p_pid == pid);
                        dbg(DBG_PRINT, "(GRADING1 2.c) Did find the process\n");
                        
                        /* return its exit status in the status argument*/
                        if (status != NULL){
                            *status = childP->p_status;
                        }
                        
                        /* destroy all threads belongs to this process*/
                        list_iterate_begin(&(childP->p_threads), thrToDestroy, kthread_t, kt_plink) {
                            
                            /* grading guideline required */
                            KASSERT(KT_EXITED == thrToDestroy->kt_state);
                            dbg(DBG_PRINT, "(GRADING1 2.c) The thread to be destroied is exited\n");
                            
                            kthread_destroy(thrToDestroy);
                        } list_iterate_end();
                        /* remove from parent process's children process list*/
                        /* list_remove(&(p_child_link)); Correction by Manan. */
                        list_remove(&(childP->p_child_link));
                        /* remove from _proc_list*/
                        /* list_remove(&(p_list_link)); Correction by Manan. */
                        list_remove(&(childP->p_list_link));
                        /* remove pagedir*/
                        
                        /* grading guideline required */
                        KASSERT(NULL != childP->p_pagedir);
                        dbg(DBG_PRINT, "(GRADING1 2.c) This process has pagedir\n");
                        
                        pt_destroy_pagedir(childP->p_pagedir);
                        /* put back memory slab*/
                        slab_obj_free(proc_allocator, childP);
                        
                        return pid;
                    }
                }
            } list_iterate_end();
            /* reach here means the specific process is running
             so we need to sleep to wait for it
             not 100% sure, use sleep_on or sleep_on_cancellable
             */
            sched_sleep_on(&(curproc->p_wait));
        }
    }
}

/*
 * Cancel all threads, join with them, and exit from the current
 * thread.
 *
 * @param status the exit status of the process
 */
/*
 * This function implements the _exit(2) system call.
 *
 * @param status the exit status of the process
 */
void
do_exit(int status)
{
    /* NOT_YET_IMPLEMENTED("PROCS: do_exit");
     cancle all threads except current thread*/
    kthread_t *thr;
    list_iterate_begin(&(curproc->p_threads), thr, kthread_t, kt_plink) {
        if (thr != curthr) {
			kthread_cancel(thr, (void *)status);
        }
    } list_iterate_end();
    /* exit current thread*/
    kthread_exit((void *)status);
}

size_t
proc_info(const void *arg, char *buf, size_t osize)
{
    const proc_t *p = (proc_t *) arg;
    size_t size = osize;
    proc_t *child;
    
    KASSERT(NULL != p);
    KASSERT(NULL != buf);
    
    iprintf(&buf, &size, "pid:          %i\n", p->p_pid);
    iprintf(&buf, &size, "name:         %s\n", p->p_comm);
    if (NULL != p->p_pproc) {
        iprintf(&buf, &size, "parent:       %i (%s)\n",
                p->p_pproc->p_pid, p->p_pproc->p_comm);
    } else {
        iprintf(&buf, &size, "parent:       -\n");
    }
    
#ifdef __MTP__
    int count = 0;
    kthread_t *kthr;
    list_iterate_begin(&p->p_threads, kthr, kthread_t, kt_plink) {
        ++count;
    } list_iterate_end();
    iprintf(&buf, &size, "thread count: %i\n", count);
#endif
    
    if (list_empty(&p->p_children)) {
        iprintf(&buf, &size, "children:     -\n");
    } else {
        iprintf(&buf, &size, "children:\n");
    }
    list_iterate_begin(&p->p_children, child, proc_t, p_child_link) {
        iprintf(&buf, &size, "     %i (%s)\n", child->p_pid, child->p_comm);
    } list_iterate_end();
    
    iprintf(&buf, &size, "status:       %i\n", p->p_status);
    iprintf(&buf, &size, "state:        %i\n", p->p_state);
    
#ifdef __VFS__
#ifdef __GETCWD__
    if (NULL != p->p_cwd) {
        char cwd[256];
        lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
        iprintf(&buf, &size, "cwd:          %-s\n", cwd);
    } else {
        iprintf(&buf, &size, "cwd:          -\n");
    }
#endif /* __GETCWD__ */
#endif
    
#ifdef __VM__
    iprintf(&buf, &size, "start brk:    0x%p\n", p->p_start_brk);
    iprintf(&buf, &size, "brk:          0x%p\n", p->p_brk);
#endif
    
    return size;
}

size_t
proc_list_info(const void *arg, char *buf, size_t osize)
{
    size_t size = osize;
    proc_t *p;
    
    KASSERT(NULL == arg);
    KASSERT(NULL != buf);
    
#if defined(__VFS__) && defined(__GETCWD__)
    iprintf(&buf, &size, "%5s %-13s %-18s %-s\n", "PID", "NAME", "PARENT", "CWD");
#else
    iprintf(&buf, &size, "%5s %-13s %-s\n", "PID", "NAME", "PARENT");
#endif
    
    list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
        char parent[64];
        if (NULL != p->p_pproc) {
            snprintf(parent, sizeof(parent),
                     "%3i (%s)", p->p_pproc->p_pid, p->p_pproc->p_comm);
        } else {
            snprintf(parent, sizeof(parent), "  -");
        }
        
#if defined(__VFS__) && defined(__GETCWD__)
        if (NULL != p->p_cwd) {
            char cwd[256];
            lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
            iprintf(&buf, &size, " %3i  %-13s %-18s %-s\n",
                    p->p_pid, p->p_comm, parent, cwd);
        } else {
            iprintf(&buf, &size, " %3i  %-13s %-18s -\n",
                    p->p_pid, p->p_comm, parent);
        }
#else
        iprintf(&buf, &size, " %3i  %-13s %-s\n",
                p->p_pid, p->p_comm, parent);
#endif
    } list_iterate_end();
    return size;
}
