#include "types.h"
#include "globals.h"
#include "kernel.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/proc.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/pagetable.h"

#include "vm/pagefault.h"
#include "vm/vmmap.h"

/*
 * This gets called by _pt_fault_handler in mm/pagetable.c The
 * calling function has already done a lot of error checking for
 * us. In particular it has checked that we are not page faulting
 * while in kernel mode. Make sure you understand why an
 * unexpected page fault in kernel mode is bad in Weenix. You
 * should probably read the _pt_fault_handler function to get a
 * sense of what it is doing.
 *
 * Before you can do anything you need to find the vmarea that
 * contains the address that was faulted on. Make sure to check
 * the permissions on the area to see if the process has
 * permission to do [cause]. If either of these checks does not
 * pass kill the offending process, setting its exit status to
 * EFAULT (normally we would send the SIGSEGV signal, however
 * Weenix does not support signals).
 *
 * Now it is time to find the correct page (don't forget
 * about shadow objects, especially copy-on-write magic!). Make
 * sure that if the user writes to the page it will be handled
 * correctly.
 *
 * Finally call pt_map to have the new mapping placed into the
 * appropriate page table.
 *
 * @param vaddr the address that was accessed to cause the fault
 *
 * @param cause this is the type of operation on the memory
 *              address which caused the fault, possible values
 *              can be found in pagefault.h
 */
void
handle_pagefault(uintptr_t vaddr, uint32_t cause)
{
        /*NOT_YET_IMPLEMENTED("VM: handle_pagefault");*/
     /* Before you can do anything you need to find the vmarea that
     * contains the address that was faulted on. Make sure to check
     * the permissions on the area to see if the process has
     * permission to do [cause]. If either of these checks does not
     * pass kill the offending process, setting its exit status to
     * EFAULT (normally we would send the SIGSEGV signal, however
     * Weenix does not support signals).*/


    /*STEP1: Find vma where this vaddr lies*/
    uint32_t vfn = ADDR_TO_PN(vaddr);
    vmmap_t *map = curproc->p_vmmap;
    vmarea_t *vma = vmmap_lookup(map, vfn); 
    if (vma == NULL){
        dbg(DBG_PRINT, "No corresponding vma for addr = 0x%x. So pagefault will kill the process\n", (uint32_t)vaddr);
        proc_kill(curproc, EFAULT);
    }

    /*STEP2: Check for permissions. TODO. Not sure about RESERVED.*/
    if (!(vma->vma_prot & PROT_READ)) {
        dbg(DBG_PRINT, "Permission check failed. vma->prot = 0x%x and cause = 0x%x", vma->vma_prot, cause);
        proc_kill(curproc, EFAULT);
    } else if ((cause & FAULT_WRITE) && !(vma->vma_prot & PROT_WRITE)){ 
        dbg(DBG_PRINT, "Permission check failed. vma->prot = 0x%x and cause = 0x%x (FAULT_WRITE)", vma->vma_prot, cause);
        proc_kill(curproc, EFAULT);
    } else if (cause & FAULT_RESERVED){ /* Assuming that the pagefault must never occur with FAULT_RESERVED.*/
        dbg(DBG_PRINT, "Permission check failed. vma->prot = 0x%x and cause = 0x%x (FAULT_RESERVED)", vma->vma_prot, cause);
        proc_kill(curproc, EFAULT);
    } else if ((cause & FAULT_EXEC) && !(vma->vma_prot & PROT_EXEC)){
        dbg(DBG_PRINT, "Permission check failed. vma->prot = 0x%x and cause = 0x%x (FAULT_EXEC)", vma->vma_prot, cause);
        proc_kill(curproc, EFAULT);
    }
 
    /*STEP3: TODO Find correct page*/
    int ret;
    pframe_t *pf = NULL;
    uint32_t pagenum = vma->vma_off + (vfn - vma->vma_start); /*TODO Not very sure about this pagenum logic.*/
    /*******************************************************
     * Introducing this piece of logic to know whether the page that is 
     * to be added could be written. Along with it, also setting ptflags
     * and pdflags.
     * *****************************************************/
    int forwrite = 0; /*Flag that will be passed to lookuppage*/
    uint32_t pdflags = PD_PRESENT | PD_USER;
    uint32_t ptflags = PT_PRESENT | PT_USER;
    if ((cause & FAULT_WRITE) == FAULT_WRITE){ /* Does the vma, in which this vaddr lie, have write access*/
        pdflags |= PD_WRITE; /* If so, set the flags, which will be used later in pt_map*/
        ptflags |= PT_WRITE; /* If so, set the flags, which will be used later in pt_map*/
        forwrite = 1; /* Tell lookuppage that the page might be written to*/
    }

/*JUMP TO UPDATE*/
   /*   * Need to handle shadow magic here. 
    *   * Step1 - Find the pframe using lookuppage
    *   *      
    *   *      Case1 - The pframe belongs to the topmost shadow in the chain
    *   *          Do nothing. Just return this pframe to the processor to work on.
    *   *      
    *   *      
    *   *      Case2 - The pframe does not belong to the topmost shadow in the chain.
    *   *          We cannot write to this pframe (since other forked processes might be 
    *   *          pointing to this). We need to copy this to the topmost shadow
    *   *          and then write to that. 
    *   *          
    *   *          Step 1 - So create a new pframe in the topmost shadow, 
    *   *          Step 2 - Copy the contents of the pframe returned by lookuppage 
    *   *          (by calling fillpage) in the new pframe. 
    *   *          Step 3 - Return this freshly created pframe.
    *   *
    *
    *
    * UPDATE -> No need to follow these steps it seems. On calling lookuppage, it automatically calls
           pframe_get and fillpage to manage copy-on-write. Not sure what more needs to handled here.*/

    ret = vma->vma_obj->mmo_ops->lookuppage(vma->vma_obj, pagenum, forwrite, &pf); /* Kai/Ziyu to check that all copy-on-write 
                                                                                    is handled in shadow functions.*/
    if (ret < 0){
        dbg (DBG_PRINT, "shadow_lookuppage returned -1\n");
        proc_kill(curproc, EFAULT);
        return;
    }
            
     /* Finally call pt_map to have the new mapping placed into the
     * appropriate page table.
     */
    pagedir_t *pd = curproc->p_pagedir;
    
    uintptr_t paddr = (uintptr_t)pt_virt_to_phys((uint32_t)pf->pf_addr);
    
    pt_map(pd, (uintptr_t)PAGE_ALIGN_DOWN(vaddr), paddr, pdflags, ptflags);
}
