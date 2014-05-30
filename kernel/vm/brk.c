#include "globals.h"
#include "errno.h"
#include "util/debug.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/mman.h"

#include "vm/mmap.h"
#include "vm/vmmap.h"

#include "proc/proc.h"

/*
 * This function implements the brk(2) system call.
 *
 * This routine manages the calling process's "break" -- the ending address
 * of the process's "dynamic" region (often also referred to as the "heap").
 * The current value of a process's break is maintained in the 'p_brk' member
 * of the proc_t structure that represents the process in question.
 *
 * The 'p_brk' and 'p_start_brk' members of a proc_t struct are initialized
 * by the loader. 'p_start_brk' is subsequently never modified; it always
 * holds the initial value of the break. Note that the starting break is
 * not necessarily page aligned!
 *
 * 'p_start_brk' is the lower limit of 'p_brk' (that is, setting the break
 * to any value less than 'p_start_brk' should be disallowed).
 *
 * The upper limit of 'p_brk' is defined by the minimum of (1) the
 * starting address of the next occuring mapping or (2) USER_MEM_HIGH.
 * That is, growth of the process break is limited only in that it cannot
 * overlap with/expand into an existing mapping or beyond the region of
 * the address space allocated for use by userland. (note the presence of
 * the 'vmmap_is_range_empty' function).
 *
 * The dynamic region should always be represented by at most ONE vmarea.
 * Note that vmareas only have page granularity, you will need to take this
 * into account when deciding how to set the mappings if p_brk or p_start_brk
 * is not page aligned.
 *
 * You are guaranteed that the process data/bss region is non-empty.
 * That is, if the starting brk is not page-aligned, its page has
 * read/write permissions.
 *
 * If addr is NULL, you should NOT fail as the man page says. Instead,
 * "return" the current break. We use this to implement sbrk(0) without writing
 * a separate syscall. Look in user/libc/syscall.c if you're curious.
 *
 * Also, despite the statement on the manpage, you MUST support combined use
 * of brk and mmap in the same process.
 *
 * Note that this function "returns" the new break through the "ret" argument.
 * Return 0 on success, -errno on failure.
 */
int
do_brk(void *addr, void **ret)
{
    /*NOT_YET_IMPLEMENTED("VM: do_brk");*/
    
    if (addr == NULL) {
        *ret = curproc->p_brk;
        return 0;
    }
    

    vmmap_t *map = curproc->p_vmmap;
    uint32_t start_brk_vfn = ADDR_TO_PN((uint32_t)curproc->p_start_brk - 1);
    uint32_t prev_brk_vfn = ADDR_TO_PN((uint32_t)curproc->p_brk - 1);
    uint32_t target_brk_vfn = ADDR_TO_PN((uint32_t)addr - 1);
    
    vmarea_t *start_vma = vmmap_lookup(map, start_brk_vfn); 
    vmarea_t *prev_vma = vmmap_lookup(map, prev_brk_vfn);
    vmarea_t *target_vma = vmmap_lookup(map, target_brk_vfn);

    if (start_vma == NULL){
        /*Assumption that the start_vma will always exist. Can be wrong*/
        dbg (DBG_PRINT, "MANAN - This was not supposed to happen. The start_vma is null!!\n");
    }
    
    /* Check if the input brk is between break_lower_limit and break_upper_limit*/
    /* Decide what should be the break_upper_limit and break_lower_unit*/
    uint32_t break_lower_limit = (uint32_t)curproc->p_start_brk;

    vmarea_t *next_segment_vma = list_item(prev_vma->vma_plink.l_next, vmarea_t, vma_plink);
    uint32_t next_segment_vma_start_vfn;
    void *next_segment_vma_start_addr;
    uint32_t break_upper_limit;
    if (next_segment_vma == NULL) {
        break_upper_limit = (uint32_t)USER_MEM_HIGH;
    } else {
        next_segment_vma_start_vfn = next_segment_vma->vma_start;
        next_segment_vma_start_addr = PN_TO_ADDR(next_segment_vma_start_vfn);
        break_upper_limit = ((uint32_t)next_segment_vma_start_addr < (uint32_t)USER_MEM_HIGH) ? (uint32_t)next_segment_vma_start_addr: (uint32_t)USER_MEM_HIGH;
    }
    /* Decide what should be the break_upper_limit and break_lower_unit*/
        

    if (((uint32_t)addr < break_lower_limit) || ((uint32_t)addr > break_upper_limit)){
        dbg(DBG_PRINT, "The new break value = %u is out of range\n", (uint32_t)addr);
        return -ENOMEM;
    }
    /* Check if the input brk is between break_lower_limit and break_upper_limit*/

/*
 psuedo code - The main assumption is that the p_start_brk is already present in a vma
 * If not, it is not too difficult to update this code
     if (target_brk_vfn == prev_brk_vfn){
          simply update the p_brk to addr and return
     }else if (target_brk_vfn > prev_brk_vfn){
           if (second vma is not present){
              1) Call vmmap_map to create a new vma.
              2) Return the new break address
           } else{
              1) second vma already exists, so just modify it
              2) Return the new break address
           }
     }else if (target_brk_vfn < prev_brk_vfn){ 
              1) vmmap_remove (target_brk_vfn + 1) <-> prev_brk_vfn
              2) Return the new break address
          }
     }
*/

    int retval;
    if (target_brk_vfn == prev_brk_vfn){
        *ret = addr;
        curproc->p_brk = addr;
        return 0;
    
    } else if (target_brk_vfn > prev_brk_vfn){
        if (prev_vma == start_vma){
            int npages = target_brk_vfn - start_brk_vfn;
            retval = vmmap_map(map, NULL, start_brk_vfn + 1, npages, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_FIXED, 0, VMMAP_DIR_LOHI, &target_vma);
            if(retval < 0){
                panic("Something went wrong with mmap in brk\n");
                return retval;
            }
            *ret = addr;
            curproc->p_brk = addr;
            return 0;
        } else {
            uint32_t prev_vma_new_end_vfn = target_brk_vfn + 1;
            prev_vma->vma_end = prev_vma_new_end_vfn;
            *ret = addr;
            curproc->p_brk = addr;
            return 0;
        }
    
    } else if (target_brk_vfn < prev_brk_vfn){ 
         int lopage = target_brk_vfn + 1;
         int npages = prev_brk_vfn - target_brk_vfn;
         vmmap_remove(map, lopage, npages);
         *ret = addr;
         curproc->p_brk = addr;
         return 0;
    }
    dbg (DBG_PRINT, "Shouldn't reach here\n");
    return -1; 

}
