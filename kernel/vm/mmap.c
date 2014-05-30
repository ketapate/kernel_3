#include "globals.h"
#include "errno.h"
#include "types.h"

#include "mm/mm.h"
#include "mm/tlb.h"
#include "mm/mman.h"
#include "mm/page.h"

#include "proc/proc.h"

#include "util/string.h"
#include "util/debug.h"

#include "fs/vnode.h"
#include "fs/vfs.h"
#include "fs/file.h"

#include "vm/vmmap.h"
#include "vm/mmap.h"

/*
 * This function implements the mmap(2) syscall, but only
 * supports the MAP_SHARED, MAP_PRIVATE, MAP_FIXED, and
 * MAP_ANON flags.
 *
 * Add a mapping to the current process's address space.
 * You need to do some error checking; see the ERRORS section
 * of the manpage for the problems you should anticipate.
 * After error checking most of the work of this function is
 * done by vmmap_map(), but remember to clear the TLB.
 */
int
do_mmap(void *addr, size_t len, int prot, int flags,
        int fd, off_t off, void **ret)
{
    /*NOT_YET_IMPLEMENTED("VM: do_mmap");*/
    
    if(((flags & MAP_FIXED) == MAP_FIXED) && ((size_t)addr < USER_MEM_LOW || (size_t)addr >= USER_MEM_HIGH)){
        return (int)MAP_FAILED;
    }
    
    
    if(!PAGE_ALIGNED(addr) || /*!PAGE_ALIGNED(len) ||*/ !PAGE_ALIGNED(off)){
        dbg(DBG_PRINT,"Error: do_mmap failed due to addr or len or off is not page aligned!\n");
        return (int)MAP_FAILED;
    }
    
    if((len <= 0) || (len >= USER_MEM_HIGH - USER_MEM_LOW)){
        dbg(DBG_PRINT,"Error: do_mmap failed due to len is <= 0!\n");
        return (int)MAP_FAILED;
    }
    
    if (!(((flags & MAP_PRIVATE) == MAP_PRIVATE) || ((flags & MAP_SHARED) == MAP_SHARED))) {
        return (int)MAP_FAILED;
    }
    
    if (((fd >= NFILES) || ( fd < 0)) && ((flags & MAP_ANON) != MAP_ANON)) {
        dbg(DBG_PRINT,"ERROR!!! fd = %d is out of range\n", fd);
        return (int)MAP_FAILED;
    }
   
    file_t *file = NULL;
    if ((flags & MAP_ANON) != MAP_ANON) {
        file = fget(fd);
    
    
        if (file == NULL) {
            return (int)MAP_FAILED;
        }
        if (((flags & MAP_PRIVATE) == MAP_PRIVATE) && ((file->f_mode & FMODE_READ) != FMODE_READ)) {
            fput(file);
            return (int)MAP_FAILED;
        }
        if (((flags & MAP_SHARED)==MAP_SHARED) && ((prot & PROT_WRITE) == PROT_WRITE) && /*(((file->f_mode & FMODE_READ )!=FMODE_READ)&&*/((file->f_mode &FMODE_WRITE)!=FMODE_WRITE)) {
            fput(file);
            return (int)MAP_FAILED;
        }
        if (((prot & PROT_WRITE)==PROT_WRITE)&&(file->f_mode==FMODE_APPEND)) {
            fput(file);
            return (int)MAP_FAILED;
        }
        if(file->f_vnode->vn_flags == VN_BUSY){
            fput(file);
            return (int)MAP_FAILED;
        }

    }

    *ret = NULL;
    vmmap_t *map = curproc->p_vmmap;
    uint32_t lopage;
    uint32_t npages;
    vmarea_t *vma;
    
    lopage = ADDR_TO_PN(addr);
    
    uint32_t hipage = ADDR_TO_PN((size_t)addr + len - 1) + 1; 
    /*uint32_t hipage = ADDR_TO_PN((size_t)addr + len) + 1;*/
    npages = hipage - lopage;
    int dir = VMMAP_DIR_HILO; /* see elf32.c */
    
    int retval;
    if ((flags & MAP_ANON) != MAP_ANON) {
        retval = vmmap_map(map, file->f_vnode, lopage, npages, prot, flags, off, dir, &vma);
    } else {
        retval = vmmap_map(map, NULL, lopage, npages, prot, flags, off, dir, &vma);
    }

    if(retval < 0){
        if ((flags & MAP_ANON) != MAP_ANON) {
            fput(file);
        }
        dbg(DBG_PRINT,"Error: The mapping of the vmarea was unsuccessful\n");
        return (int)MAP_FAILED;
    }

    *ret = PN_TO_ADDR (vma->vma_start);
    /* clear TLB for this vaddr*/
    tlb_flush_range((uintptr_t)(*ret), npages);
    
    if ((flags & MAP_ANON) != MAP_ANON) {
        fput(file);
    }
    
    return 0;
}


/*
 * This function implements the munmap(2) syscall.
 *
 * As with do_mmap() it should perform the required error checking,
 * before calling upon vmmap_remove() to do most of the work.
 * Remember to clear the TLB.
 */
int
do_munmap(void *addr, size_t len)
{
    /*NOT_YET_IMPLEMENTED("VM: do_munmap");*/

    if ((size_t)addr < USER_MEM_LOW || (size_t)addr >= USER_MEM_HIGH) {
        return -EINVAL;
    }

    if(len == PAGE_SIZE * 15)
    {
        dbg(DBG_PRINT, "BREAK\n");
    }


    if(!PAGE_ALIGNED(addr)){
        dbg(DBG_PRINT,"Error: do_munmap failed due to addr or len is not page aligned!\n");
        return -EINVAL;
    }

    if((len <= 0) || (len >= USER_MEM_HIGH - USER_MEM_LOW)){
        dbg(DBG_PRINT,"Error: do_munmap failed due to len is <= 0!\n");
        return -EINVAL;
    }

    vmmap_t *map = curproc->p_vmmap;
    uint32_t lopage;
    uint32_t npages;
    lopage = ADDR_TO_PN(addr);
    
    /* updated */
    /* TODO: Check later: may change to: uint32_t hipage = ADDR_TO_PN((size_t)addr + len - 1) + 1; */
    uint32_t hipage = ADDR_TO_PN((size_t)addr + len - 1) + 1; 
    /*uint32_t hipage = ADDR_TO_PN((size_t)addr + len) + 1;*/
    npages = hipage - lopage;
    
    int retval = vmmap_remove(map, lopage, npages);
    if(retval < 0){
        dbg(DBG_PRINT,"Error: The unmapping of the vmarea was unsuccessful\n");
        return retval;
    }
    /* clear TLB for this vaddr*/
    /* Corrected */
    tlb_flush_range((uintptr_t)addr, npages);
    
    return 0;
}

