#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs, void *kstack)
{
    /* Pointer argument and dummy return address, and userland dummy return
     * address */
    uint32_t esp = ((uint32_t) kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
    *(void **)(esp + 4) = (void *)(esp + 8); /* Set the argument to point to location of struct on stack */
    memcpy((void *)(esp + 8), regs, sizeof(regs_t)); /* Copy over struct */
    return esp;
}


/*
 * The implementation of fork(2). Once this works,
 * you're practically home free. This is what the
 * entirety of Weenix has been leading up to.
 * Go forth and conquer.
 */
int
do_fork(struct regs *regs)
{
    dbg (DBG_PRINT, "ENTERED FORK!!\n");
    
    KASSERT(regs != NULL);
    dbg(DBG_PRINT, "(GRADING3A 7.a) regs is not NULL \n");
    KASSERT(curproc != NULL);
    dbg(DBG_PRINT, "(GRADING3A 7.a) curproc is not NULL \n");
    KASSERT(curproc->p_state == PROC_RUNNING);
    dbg(DBG_PRINT, "(GRADING3A 7.a) curproc->p_state is PROC_RUNNING \n");
    
    
    /*NOT_YET_IMPLEMENTED("VM: do_fork");*/
    /*
     1) Create a new process using proc create().
     */
    proc_t *new_proc;
    char *new_proc_name = "forked_process";
    /*new_proc = proc_create(new_proc_name);*/
    vmmap_t *old_map = curproc->p_vmmap;
    
    /*
     2) Copy the vmmap t from the parent process into the child using vmmap clone()
     (which you should write if you haven’t already). Remember to increase
     the reference counts on the underlying memory objects.
     */
    vmmap_t *new_map = vmmap_clone(old_map);
    
    
    /*Increment reference counts of all vm_objects*/
    /*
     vmarea_t *vma;
     list_iterate_begin(&new_map->vmm_list, vma, vmarea_t, vma_plink){
     vma->vma_obj->mmo_ops->ref(vma->vma_obj);
     }list_iterate_end();
     */
    
    
    
    /*
     3) For each private mapping in the original process, point the virtual memory
     areas of the new and old processes to two new shadow objects, which in
     turn should point to the original underlying memory object. This is how
     you know that pages corresponding to this mapping are copy-on-write. Be
     careful with reference counts. Also note that for shared mappings, there
     is no need to make a shadow object.
     */
    
    /* For new process*/
    vmarea_t *new_vma;
    list_t *list = &old_map->vmm_list;
    list_link_t *link = list->l_next;
    vmarea_t *old_vma;
    list_iterate_begin(&new_map->vmm_list, new_vma, vmarea_t, vma_plink){
        /*If it is MAP_PRIVATE*/
        old_vma = list_item(link, vmarea_t, vma_plink);
        if ((new_vma->vma_flags & MAP_PRIVATE) == MAP_PRIVATE)
        {
            /*      Step 1 - Create a shadow*/
            mmobj_t *new_proc_shadow = shadow_create();
            
            /* Corrected: need to check if new shadow object created successfully */
            if (new_proc_shadow == NULL) {
                vmmap_destroy(new_map);
                return -1;
            }
            
            /*      Step 2 - Map original underlying object to the new shadow.*/
            new_proc_shadow->mmo_shadowed = old_vma->vma_obj;
            
            /* Corrected: more things need to do here */
            new_proc_shadow->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(old_vma->vma_obj);
            list_insert_tail(mmobj_bottom_vmas(old_vma->vma_obj), &(new_vma->vma_olink));
            
            /*      Step 3 - Map shadow to their respective vma.*/
            new_vma->vma_obj = new_proc_shadow;
            
        } else if ((new_vma->vma_flags & MAP_SHARED) == MAP_SHARED){
            /*else if (MAP_SHARED)*/
            /*      Step 1 - Map original underlying object to the vma.*/
            new_vma->vma_obj = old_vma->vma_obj;
        }
        
        /* Corrected: increment refrence count for the original mmobj here, no matter MAP_SHARED or MAP_PRIVATE */
        old_vma->vma_obj->mmo_ops->ref(old_vma->vma_obj);
        
        link = link->l_next;
    }list_iterate_end();
    
    /* For old process*/
    list_iterate_begin(&old_map->vmm_list, old_vma, vmarea_t, vma_plink){
        /*If it is MAP_PRIVATE*/
        if ((old_vma->vma_flags & MAP_PRIVATE) == MAP_PRIVATE)
        {
            /*      Step 1 - Create a shadow*/
            mmobj_t *old_proc_shadow = shadow_create();
            
            /* Corrected: need to check if new shadow object created successfully */
            if (old_proc_shadow == NULL) {
                vmmap_destroy(new_map);
                return -1;
            }
            
            /*      Step 2 - Map original underlying object to the new shadow.*/
            old_proc_shadow->mmo_shadowed = old_vma->vma_obj;
            
            /* Corrected: more things need to do here */
            /* Note: we don't need to add this old_vma to the button mmobj again! */
            old_proc_shadow->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(old_vma->vma_obj);
            
            /*      Step 3 - Map shadow to their respective vma.*/
            old_vma->vma_obj = old_proc_shadow;
        }
        /* No else condition required. The old_vma is already mapped to vm_obj*/
    }list_iterate_end();
    
    /*
     4) Unmap the userland page table entries and flush the TLB using pt unmap range()
     and tlb flush all(). This is necessary because the parent process might
     still have some entries marked as “writable”, but since we are implement-
     ing copy-on-write we would like access to these pages to cause a trap to
     our page fault handler so it can dirty the page, which will invoke the
     copy-on-write actions.
     */
    
    pt_unmap_range(curproc->p_pagedir, USER_MEM_LOW, USER_MEM_HIGH);
    tlb_flush_all();
    
    /*
     5) Set up the new process thread context. You will need to set the following:
     – c pdptr - the page table pointer
     – c eip - function pointer for userland entry()
     – c esp - the value returned by fork setup stack()
     – c kstack - the top of the new thread’s kernel stack
     – c kstacksz - size of the new thread’s kernel stack
     */
    
    /* Now, it's time to create a new process and a new thread */
    /* If we create process at the beginning, and any error happened above, we need to do some cleanup. But create a process here can escape that */
    
    new_proc = proc_create(new_proc_name);
    
    KASSERT(new_proc->p_state == PROC_RUNNING);
    dbg(DBG_PRINT, "(GRADING3A 7.a) newproc->p_state == PROC_RUNNING \n");
    KASSERT(new_proc->p_pagedir != NULL);
    dbg(DBG_PRINT, "(GRADING3A 7.a) newproc->p_pagedir != NULL \n");
    
    /* updatde */
    vmmap_destroy(new_proc->p_vmmap);
    
    new_proc->p_vmmap = new_map;
    new_map->vmm_proc = new_proc;
    
    kthread_t *new_thr = kthread_clone(curthr);
    
    KASSERT(new_thr->kt_kstack != NULL);
    dbg(DBG_PRINT, "(GRADING3A 7.a) newthr->kt_kstack != NULL \n");
    
    new_thr->kt_proc = new_proc;
    list_insert_tail(&(new_proc->p_threads), &(new_thr->kt_plink));
    
    regs->r_eax = 0;
    
    context_t new_ctx;
    /*new_ctx.c_pdptr = pt_create_pagedir();*/
    new_ctx.c_pdptr = new_proc->p_pagedir;
    new_ctx.c_eip = (uint32_t)&userland_entry;
    
    /* Corrected */
    new_ctx.c_esp = fork_setup_stack(regs, new_thr->kt_kstack);
    new_ctx.c_kstack = (uintptr_t)new_thr->kt_kstack;
    new_ctx.c_kstacksz = curthr->kt_ctx.c_kstacksz;
    
    new_thr->kt_ctx = new_ctx;
    
    /*
     6) Copy the file table of the parent into the child. Remember to use fref()
     here.
     */
    int i = 0;
    for (i = 0; i < NFILES ; i++){
        if (curproc->p_files[i] != NULL) {
            new_proc->p_files[i] = curproc->p_files[i];
            fref(new_proc->p_files[i]);
        }
    }
    
    sched_make_runnable(new_thr);
    
    new_proc->p_status = curproc->p_status;
    
    new_proc->p_brk = curproc->p_brk;
    new_proc->p_start_brk = curproc->p_start_brk;
    
    return new_proc->p_pid;
}
