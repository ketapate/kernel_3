#include "kernel.h"
#include "errno.h"
#include "globals.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "proc/proc.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/fcntl.h"
#include "fs/vfs_syscall.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/mmobj.h"

static slab_allocator_t *vmmap_allocator;
static slab_allocator_t *vmarea_allocator;

void
vmmap_init(void)
{
        vmmap_allocator = slab_allocator_create("vmmap", sizeof(vmmap_t));
        KASSERT(NULL != vmmap_allocator && "failed to create vmmap allocator!");
        vmarea_allocator = slab_allocator_create("vmarea", sizeof(vmarea_t));
        KASSERT(NULL != vmarea_allocator && "failed to create vmarea allocator!");
}

vmarea_t *
vmarea_alloc(void)
{
        vmarea_t *newvma = (vmarea_t *) slab_obj_alloc(vmarea_allocator);
        if (newvma) {
                newvma->vma_vmmap = NULL;
        }
        return newvma;
}

void
vmarea_free(vmarea_t *vma)
{
        KASSERT(NULL != vma);
        slab_obj_free(vmarea_allocator, vma);
}

/* Create a new vmmap, which has no vmareas and does
 * not refer to a process. */
vmmap_t *
vmmap_create(void)
{
        /*NOT_YET_IMPLEMENTED("VM: vmmap_create");*/
    vmmap_t *new_vmmap = (vmmap_t *) slab_obj_alloc(vmmap_allocator);
    if(new_vmmap == NULL){
        dbg(DBG_PRINT, "vmmap_create failed because no enough memory \n");
        return NULL;
        }
    
    list_init(&new_vmmap->vmm_list);
    new_vmmap->vmm_proc = NULL;
    return new_vmmap;
}

/* Removes all vmareas from the address space and frees the
 * vmmap struct. */
void
vmmap_destroy(vmmap_t *map)
{
    KASSERT(NULL != map);
    dbg(DBG_PRINT, "(GRADING3A 3.a) The map passed to this function exists\n");
        /*NOT_YET_IMPLEMENTED("VM: vmmap_destroy");*/
        if(!(list_empty(&map->vmm_list))){
            vmarea_t *vma;
            list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink){
               /*
                if(vma->vma_flags == MAP_SHARED){
                    vma->vma_obj->mmo_ops->put(vma->vma_obj);
                }
                else if (vma->vma_flags == MAP_PRIVATE) {
                    vma->vma_obj->mmo_shadowed->mmo_ops->put(vma->vma_obj->mmo_shadowed);
                    vma->vma_obj->mmo_ops->put(vma->vma_obj);
                }
                list_remove(&vma->vma_plink);
                */ /*TODO removing Manan's code and adding Kai's code.*/

                list_remove(&vma->vma_plink);
                /* updated */
                if ((vma->vma_flags & MAP_PRIVATE) == MAP_PRIVATE){
                    list_remove(&vma->vma_olink);
                }
                if (vma->vma_obj != NULL) {
                     vma->vma_obj->mmo_ops->put(vma->vma_obj);
                }
                vmarea_free(vma);
            }list_iterate_end();
        }
        slab_obj_free(vmmap_allocator, map);
}

/* Add a vmarea to an address space. Assumes (i.e. asserts to some extent)
 * the vmarea is valid.  This involves finding where to put it in the list
 * of VM areas, and adding it. Don't forget to set the vma_vmmap for the
 * area. */
void
vmmap_insert(vmmap_t *map, vmarea_t *newvma)
{
    /*NOT_YET_IMPLEMENTED("VM: vmmap_insert");*/
    KASSERT(NULL != map && NULL != newvma);
    dbg(DBG_PRINT, "(GRADING3A 3.b) The map and vmarea arguments to this function exist\n");    
    KASSERT(NULL == newvma->vma_vmmap);
    dbg(DBG_PRINT, "(GRADING3A 3.b) The newvma is not mapped in a VA\n");    
    KASSERT(newvma->vma_start < newvma->vma_end);
    dbg(DBG_PRINT, "(GRADING3A 3.b) The newvma's start VFN is less than newvma's end VFN\n");    
    KASSERT(ADDR_TO_PN(USER_MEM_LOW) <= newvma->vma_start && ADDR_TO_PN(USER_MEM_HIGH) >= newvma->vma_end);
    dbg(DBG_PRINT, "(GRADING3A 3.b) The newvma lies between USER_MEM_LOW and USER_MEM_HIGH\n");    
    
    
    newvma->vma_vmmap = map; 

    int flag=0;
    vmarea_t *oldvma;

    if(!list_empty(&map->vmm_list)){
        list_iterate_begin(&map->vmm_list, oldvma, vmarea_t, vma_plink){


            if( (oldvma->vma_start > newvma->vma_start)&&(oldvma->vma_end > newvma->vma_end) ){
                dbg(DBG_PRINT,"Inserting new vmarea with start vfn=%d, old vfn=%d\n",newvma->vma_start,oldvma->vma_start);
                list_insert_before( &(oldvma->vma_plink), &(newvma->vma_plink) );
                flag = 1;
                goto done;
            }
        }list_iterate_end();
    } else {
        dbg(DBG_PRINT,"map->vmm_list is empty. So going to add the newvma to the tail of the list\n");
        goto done;
    }

done:
    if(flag==0){
        dbg(DBG_PRINT,"Inserting new vmarea with start vfn=%d at the tail of the list\n",newvma->vma_start);
        list_insert_tail(&map->vmm_list, &(newvma->vma_plink));
        dbg(DBG_PRINT,"Done Inserting new vmarea at the tail\n");
    }
}

/* Find a contiguous range of free virtual pages of length npages in
 * the given address space. Returns starting vfn for the range,
 * without altering the map. Returns -1 if no such range exists.
 *
 * Your algorithm should be first fit. If dir is VMMAP_DIR_HILO, you
 * should find a gap as high in the address space as possible; if dir
 * is VMMAP_DIR_LOHI, the gap should be as low as possible. */
int
vmmap_find_range(vmmap_t *map, uint32_t npages, int dir)
{
   /* NOT_YET_IMPLEMENTED("VM: vmmap_find_range");*/
    KASSERT(NULL != map);
    dbg(DBG_PRINT, "(GRADING3A 3.c) The map passed to this function exists\n");

    KASSERT(0 < npages);
    dbg(DBG_PRINT, "(GRADING3A 3.c) The npages argument to this function is > 0\n");

    /* LOHI */

    if(dir == VMMAP_DIR_LOHI){
    /*Fix by Manan - Adding case when list is empty*/
        if(list_empty(&map->vmm_list)){
            return ADDR_TO_PN(USER_MEM_LOW);

        }
        vmarea_t *vma_prev = list_head(&map->vmm_list, vmarea_t, vma_plink);
        vmarea_t *vma_cur = vma_prev;
        
        /* Checking for the case: [0 - vma_cur->vma_start) */
        if(vma_cur->vma_start != ADDR_TO_PN(USER_MEM_LOW)){
            if( vma_cur->vma_start - ADDR_TO_PN(USER_MEM_LOW) >= npages){
                return ADDR_TO_PN(USER_MEM_LOW);
            }
        }

        /* Checking for the general case: [vma_prev->vma_end - vma_cur->vma_start) */
        list_iterate_begin(&map->vmm_list, vma_cur, vmarea_t, vma_plink){
            if(vma_prev != vma_cur){
                if((vma_cur->vma_start - vma_prev->vma_end) >= npages){
                    return vma_prev->vma_end;
                }
                vma_prev = vma_cur;
            }
        }list_iterate_end();

        /* Checking for the case: [tail->vma_end - USER_MEM_HIGH) */
        uint32_t last_vaddr = (uint32_t)USER_MEM_HIGH;

       if( (ADDR_TO_PN(last_vaddr) - vma_prev->vma_end) >= npages){
            return vma_prev->vma_end;
        } else {
            dbg (DBG_PRINT, "OUT OF MEMORY\n");
            return -1;
        }
    }
    /* HILO */
    else if (dir == VMMAP_DIR_HILO){
        /*Fix by Manan - Adding case when list is empty*/
        if(list_empty(&map->vmm_list)){
            return ADDR_TO_PN(USER_MEM_HIGH) - npages;
        }
        vmarea_t *vma_prev = list_tail(&map->vmm_list, vmarea_t, vma_plink);
        vmarea_t *vma_cur = vma_prev;
        /* Checking for the case: [tail->vma_end - 2^20) */
        uint32_t last_vaddr = (uint32_t)USER_MEM_HIGH;
        
        if((ADDR_TO_PN(last_vaddr) - vma_cur->vma_end) >= npages){
            return ADDR_TO_PN(last_vaddr) - npages;
        }

        /* Checking for the general case: [vma_prev->vma_end - vma_cur->vma_start) */
        list_iterate_reverse(&map->vmm_list, vma_cur, vmarea_t, vma_plink){
            if(vma_prev != vma_cur){
                if((vma_prev->vma_start - vma_cur->vma_end) >= npages){
                    return vma_prev->vma_start - npages;
                }
            }
            vma_prev = vma_cur;
        }list_iterate_end();

        /* Checking for the case: [0 - vma_cur->vma_start) */
        if(vma_prev->vma_start >= npages){
            if ((vma_prev->vma_start - npages) >= ADDR_TO_PN(USER_MEM_LOW)){
                return vma_prev->vma_start - npages;
            } else {
                dbg (DBG_PRINT, "OUT OF MEMORY\n");
                return -1;
            }
        }
    }
    
    return -1;
}

/* Find the vm_area that vfn lies in. Simply scan the address space
 * looking for a vma whose range covers vfn. If the page is unmapped,
 * return NULL. */
vmarea_t *
vmmap_lookup(vmmap_t *map, uint32_t vfn)
{
    /*NOT_YET_IMPLEMENTED("VM: vmmap_lookup");*/
    KASSERT(NULL != map);
    dbg(DBG_PRINT, "(GRADING3A 3.d) The map passed to this function exists\n");

    vmarea_t *vma;
    list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
        if ((vma->vma_start <= vfn) && (vma->vma_end > vfn)){
            dbg(DBG_PRINT, "Found vmarea correspoinding to vfn\n");
            return vma;
        }
    } list_iterate_end();
    
    dbg(DBG_PRINT, "Cannot find vfn in this vmmap\n");
    return NULL;
}

/* Allocates a new vmmap containing a new vmarea for each area in the
 * given map. The areas should have no mmobjs set yet. Returns pointer
 * to the new vmmap on success, NULL on failure. This function is
 * called when implementing fork(2). */
vmmap_t *
vmmap_clone(vmmap_t *map)
{
    /*NOT_YET_IMPLEMENTED("VM: vmmap_clone");*/
    vmmap_t *new_vmmap = vmmap_create();
    
    /* Corrected: */
    if (new_vmmap == NULL) {
        dbg(DBG_PRINT, "vmmap_clone failed because vmmap_create failed.\n");
        return NULL;
    }
    vmarea_t *vma;

    if(!list_empty(&map->vmm_list)){
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink){
            vmarea_t *new_vma = vmarea_alloc();

            /* Corrected: */
            if (new_vma == NULL) {
                dbg(DBG_PRINT, "vmmap_clone failed because vmarea_alloc failed.\n");
                vmmap_destroy(new_vmmap);
                return NULL;
            }

            /*new_vma = vma;*/ /* Corrected. new_vma is a pointer and cannot copy
            a struct like this.*/
            new_vma->vma_start = vma->vma_start;
            new_vma->vma_end = vma->vma_end;
            new_vma->vma_off = vma->vma_off;
            new_vma->vma_prot = vma->vma_prot;
            new_vma->vma_flags = vma->vma_flags;
            
            new_vma->vma_vmmap = new_vmmap;
            new_vma->vma_obj = NULL;
            
            /* Corrected: */
            list_link_init(&(new_vma->vma_plink));
            list_link_init(&(new_vma->vma_olink));
            
            list_insert_tail(&new_vmmap->vmm_list, &new_vma->vma_plink);
        }list_iterate_end();
    }
    
    dbg(DBG_PRINT,"Success on cloning the vmmap\n");
    return new_vmmap;
}

/* Insert a mapping into the map starting at lopage for npages pages.
 * If lopage is zero, we will find a range of virtual addresses in the
 * process that is big enough, by using vmmap_find_range with the same
 * dir argument.  If lopage is non-zero and the specified region
 * contains another mapping that mapping should be unmapped.
 *
 * If file is NULL an anon mmobj will be used to create a mapping
 * of 0's.  If file is non-null that vnode's file will be mapped in
 * for the given range.  Use the vnode's mmap operation to get the
 * mmobj for the file; do not assume it is file->vn_obj. Make sure all
 * of the area's fields except for vma_obj have been set before
 * calling mmap.
 *
 * If MAP_PRIVATE is specified set up a shadow object for the mmobj.
 *
 * All of the input to this function should be valid (KASSERT!).
 * See mmap(2) for for description of legal input.
 * Note that off should be page aligned.
 *
 * Be very careful about the order operations are performed in here. Some
 * operation are impossible to undo and should be saved until there
 * is no chance of failure.
 *
 * If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it.
 */
int
vmmap_map(vmmap_t *map, vnode_t *file, uint32_t lopage, uint32_t npages,
          int prot, int flags, off_t off, int dir, vmarea_t **new)
{
    /*NOT_YET_IMPLEMENTED("VM: vmmap_map");*/
    KASSERT(NULL != map);
    dbg(DBG_PRINT, "(GRADING3A 3.f) The map passed to this function exists\n");
    KASSERT(0 < npages);
    dbg(DBG_PRINT, "(GRADING3A 3.f) The npages argument to this function is > 0\n");
    KASSERT(!(~(PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC) & prot));
    dbg(DBG_PRINT, "(GRADING3A 3.f) The prot input argument is a right combination of PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC\n");
    KASSERT((MAP_SHARED & flags) || (MAP_PRIVATE & flags));
    dbg(DBG_PRINT, "(GRADING3A 3.f) The flags input is either MAP_SHARED or MAP_PRIVATE\n");
    KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_LOW) <= lopage));
    dbg(DBG_PRINT, "(GRADING3A 3.f) lopage is >= the lower bound for user address space\n");
    KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_HIGH) >= (lopage + npages)));
    dbg(DBG_PRINT, "(GRADING3A 3.f) lopage is <= the upper bound for user address space\n");
    KASSERT(PAGE_ALIGNED(off));
    dbg(DBG_PRINT, "(GRADING3A 3.f) The offset is page aligned\n");
    
    dbg(DBG_PRINT,"vmmap_map called for obj = 0x%x\nlopage = %x\nnpages = %u\nprot = %x\nflags = %x\noff = %u\ndir = %d\n", (uint32_t)file, lopage, npages, prot, flags, off, dir);
    KASSERT(PAGE_ALIGNED(off));
    dbg(DBG_PRINT,"Offset aligned\n");
    
    
    
    /* use the sart vfn provided by lopage */
    int retval=0;
    uint32_t off_page = ADDR_TO_PN(off);
    uint32_t start_vfn = lopage; 
    if(lopage){
        /* lopage is within existing mapping */
        dbg(DBG_PRINT,"lopage is specified\n");
        if(vmmap_is_range_empty(map, lopage, npages) == 0){
            dbg(DBG_PRINT,"vmmap_range is not empty\n");
            vmmap_remove(map, lopage, npages);
            start_vfn = lopage;
            dbg(DBG_PRINT,"So startvfn is now = %d\n", start_vfn);
        }
    /* find free area in the map for mapping */
    }
    else{
        dbg(DBG_PRINT,"lopage is not specified\n");
        retval = vmmap_find_range(map, npages, dir);
        dbg(DBG_PRINT,"vmmap_find_range returned = %d\n", retval);
        if(retval < 0){
            dbg(DBG_PRINT,"Error: Cannot find free address space to insert the mapping\n");
            return -1;
        }
        start_vfn = retval;
        dbg(DBG_PRINT,"So new startvfn = %d\n", start_vfn);
    }
    
    /* Got the proper start vfn now */
    dbg(DBG_PRINT,"The final startvfn is %d\n",start_vfn);
    mmobj_t *new_mmobj;
    vmarea_t *file_vma = vmarea_alloc();
    /* Corrected */
    if(file_vma == NULL){
        dbg(DBG_PRINT, "vmmap failed because vmarea_alloc failed\n");
        return -1;
    }
    
    file_vma->vma_start = start_vfn;
    file_vma->vma_end = start_vfn + npages;
    file_vma->vma_off = off_page; /* Because the input offset is an address and not a page*/
    file_vma->vma_prot = prot;
        
    file_vma->vma_flags = flags;
    
    /*file_vma->vma_vmmap = map;*/ /* Corrected by Manan - This mapping is actually doen in vmmap_insert.*/
    
    if(file == NULL){
        new_mmobj = anon_create();
        /* Corrected*/
        if(new_mmobj == NULL){
            dbg(DBG_PRINT,"Error: vmmap failed because anon_create() failed\n");
            vmarea_free(file_vma);
            return -1;
        }
    } else {
        retval = file->vn_ops->mmap(file, file_vma, &new_mmobj);
        if(retval < 0){
            dbg(DBG_PRINT,"Error: mmobj couldn't be mapped\n");
            vmarea_free(file_vma);
            return retval;
        }
        
    }
    dbg(DBG_PRINT,"anon/file memobj = 0x%x and has refcount = %d\n before manually incrementing", (uint32_t)new_mmobj, new_mmobj->mmo_refcount);
    
    /* Corrected: make sure ref count will be set to 1 */
    if (new_mmobj->mmo_refcount == 0) { /* TODO What about >1 refcounts? Shouldn't we increment?*/
        new_mmobj->mmo_ops->ref(new_mmobj);
    }
    
    /* Check for private mapping */
    
    /* We convert this ---> file_vma -> vma_obj
     * to --->               file_vma -> shadow_memobj -> vma_obj*/


    if((flags & MAP_PRIVATE) == MAP_PRIVATE){                
        dbg(DBG_PRINT,"flag == MAP_PRIVATE\n");
        mmobj_t *shadow_mmobj = shadow_create();
        dbg(DBG_PRINT,"Shadow object 0x%x created\n", (uint32_t)shadow_mmobj);
        /* Corrected here */
        if(shadow_mmobj == NULL){
            dbg(DBG_PRINT,"Error: vmmap failed because shadow_create() failed\n");
            new_mmobj->mmo_ops->put(new_mmobj);
            vmarea_free(file_vma);
            return -1;
        }
        
        shadow_mmobj->mmo_shadowed = new_mmobj;
        
        /* Corrected here */
        file_vma->vma_obj = shadow_mmobj;
        
        shadow_mmobj->mmo_un.mmo_bottom_obj = new_mmobj;
        list_insert_tail(&(new_mmobj->mmo_un.mmo_vmas), &(file_vma->vma_olink));
    } else if((flags & MAP_SHARED) == MAP_SHARED){                
        file_vma->vma_obj = new_mmobj;
    }
    

    vmmap_insert(map, file_vma);
    if(new != NULL){
        *new = file_vma;
    }
    
    char debug[1024];
    vmmap_mapping_info(map, debug, 1024);
    dbg(DBG_PRINT, "The debug info is \n%s \n", debug);

    return retval;        
}

/*
 * We have no guarantee that the region of the address space being
 * unmapped will play nicely with our list of vmareas.
 *
 * You must iterate over each vmarea that is partially or wholly covered
 * by the address range [addr ... addr+len). The vm-area will fall into one
 * of four cases, as illustrated below:
 *
 * key:
 *          [             ]   Existing VM Area
 *        *******             Region to be unmapped
 *
 * Case 1:  [   ******    ]
 * The region to be unmapped lies completely inside the vmarea. We need to
 * split the old vmarea into two vmareas. be sure to increment the
 * reference count to the file associated with the vmarea.
 *
 * Case 2:  [      *******]**
 * The region overlaps the end of the vmarea. Just shorten the length of
 * the mapping.
 *
 * Case 3: *[*****        ]
 * The region overlaps the beginning of the vmarea. Move the beginning of
 * the mapping (remember to update vma_off), and shorten its length.
 *
 * Case 4: *[*************]**
 * The region completely contains the vmarea. Remove the vmarea from the
 * list.
 */
int
vmmap_remove(vmmap_t *map, uint32_t lopage, uint32_t npages)
{
    /*NOT_YET_IMPLEMENTED("VM: vmmap_remove");*/
    
    vmarea_t *vma;
    uint32_t endvfn = lopage + npages;
    uint32_t startvfn = lopage;
    
    if(!list_empty(&map->vmm_list)){
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
            /* Case 1:      [  *****    ]     */
            if((startvfn > vma->vma_start)&&(endvfn < vma->vma_end) ){
                dbg(DBG_PRINT,"Case1 found\n");
                uint32_t start1 = vma->vma_start;
                uint32_t end1 = startvfn;
                uint32_t start2 = endvfn;
                uint32_t end2 = vma->vma_end;
                
                vmarea_t *new_vma = vmarea_alloc();
                
                /* Corrected */
                if (new_vma == NULL) {
                    dbg(DBG_PRINT,"Error: vmmap_remove failed because vmarea_alloc() failed\n");
                    return -1;
                }
                
                vma->vma_start = start1;
                vma->vma_end = end1;
                uint32_t off = vma->vma_off + start2 - start1; /* Corrected off by Manan */
                
                new_vma->vma_start = start2;
                new_vma->vma_end = end2;
                new_vma->vma_off = off;
                
                new_vma->vma_prot = vma->vma_prot;
                new_vma->vma_flags = vma->vma_flags;
                new_vma->vma_vmmap = NULL; /*Corrected by Manan*/
                
                if ((vma->vma_flags & MAP_PRIVATE) == MAP_PRIVATE){
                    /* Create two new shadow objects to each vmarea */
                    mmobj_t *shadow_mmobj1 = shadow_create();
                    if(shadow_mmobj1 == NULL){
                        dbg(DBG_PRINT,"Error: vmmap_remove failed because shadow_create() failed\n");
                        vmarea_free(new_vma);
                        return -1;
                    }
                    
                    mmobj_t *shadow_mmobj2 = shadow_create();
                    if(shadow_mmobj2 == NULL){
                        dbg(DBG_PRINT,"Error: vmmap_remove failed because shadow_create() failed\n");
                        vmarea_free(new_vma);
                        return -1;
                    }
                    
                    mmobj_t *temp = vma->vma_obj;
                    
                    shadow_mmobj1->mmo_shadowed = temp;
                    shadow_mmobj2->mmo_shadowed = temp;
                    
                    vma->vma_obj = shadow_mmobj1;
                    new_vma->vma_obj = shadow_mmobj2;
                    
                    shadow_mmobj1->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(temp);
                    shadow_mmobj2->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(temp);
                    
                    list_insert_tail(mmobj_bottom_vmas(shadow_mmobj1), &(vma->vma_olink));
                    list_insert_tail(mmobj_bottom_vmas(shadow_mmobj2), &(new_vma->vma_olink));
                    
                    /* updated */
                    vma->vma_obj->mmo_shadowed->mmo_ops->ref(vma->vma_obj->mmo_shadowed);

                } else if ((vma->vma_flags & MAP_SHARED) == MAP_SHARED){
                    new_vma->vma_obj = vma->vma_obj;
                    /* updated */
                    vma->vma_obj->mmo_ops->ref(vma->vma_obj);

                }
                vmmap_insert(map, new_vma);
                pt_unmap_range(curproc->p_pagedir, (uintptr_t)PN_TO_ADDR(startvfn), (uintptr_t)PN_TO_ADDR(endvfn));
            }
            /* Case 2:      [      *****]***  */
            else if( (startvfn > vma->vma_start)&&(startvfn < vma->vma_end)&&
                    (endvfn >= vma->vma_end) ){
                dbg(DBG_PRINT,"Case2 found\n");
                uint32_t temp_vfn = vma->vma_end;
                vma->vma_end = startvfn;
                pt_unmap_range(curproc->p_pagedir, (uintptr_t)PN_TO_ADDR(startvfn), (uintptr_t)PN_TO_ADDR(temp_vfn));
            }
            /* Case 3: *****[*****      ]     */
            else if( (startvfn <= vma->vma_start)&&
                    (endvfn > vma->vma_start)&&(endvfn < vma->vma_end) ){
                dbg(DBG_PRINT,"Case3 found\n");
                vma->vma_off = vma->vma_off + (endvfn - vma->vma_start);
                uint32_t temp_vfn = vma->vma_start;
                vma->vma_start = endvfn;
                pt_unmap_range(curproc->p_pagedir, (uintptr_t)PN_TO_ADDR(temp_vfn), (uintptr_t)PN_TO_ADDR(endvfn));
            }
            /* Case 4:   ***[***********]***  */
            else if( (startvfn <= vma->vma_start)&&(endvfn >= vma->vma_end) ){
                uint32_t temp_start_vfn = vma->vma_start;
                uint32_t temp_end_vfn = vma->vma_end;
                if(!list_empty(&map->vmm_list)){
                    if (vma->vma_obj != NULL) {
                        vma->vma_obj->mmo_ops->put(vma->vma_obj);
                    }

                    list_remove(&vma->vma_plink);
                    /* Corrected: */
                    if ((vma->vma_flags & MAP_PRIVATE) == MAP_PRIVATE){
                        list_remove(&vma->vma_olink);
                    }
                    vmarea_free(vma);
                    pt_unmap_range(curproc->p_pagedir, (uintptr_t)PN_TO_ADDR(temp_start_vfn), (uintptr_t)PN_TO_ADDR(temp_end_vfn));
                }
            } 
        } list_iterate_end();
    }
    
    char debug[1024];
    vmmap_mapping_info(map, debug, 1024);
    dbg(DBG_PRINT, "The debug info is \n%s \n", debug);
    
    return 0; /*Successfully completed*/
}

/*
 * Returns 1 if the given address space has no mappings for the
 * given range, 0 otherwise.
 */
int
vmmap_is_range_empty(vmmap_t *map, uint32_t startvfn, uint32_t npages)
{
    /*NOT_YET_IMPLEMENTED("VM: vmmap_is_range_empty");*/
    uint32_t endvfn = startvfn + npages;
    KASSERT((startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn));
    dbg(DBG_PRINT, "(GRADING3A 3.e) The newvma has endvfn > startvfn and lies between USER_MEM_LOW and USER_MEM_HIGH\n");    

    vmarea_t *vma;
    
    /* Improved time complexity */
    if(!list_empty(&map->vmm_list)){
        list_iterate_begin(&(map->vmm_list), vma, vmarea_t, vma_plink) {
            if (startvfn >= vma->vma_end || endvfn <= vma->vma_start) {
            } else {
                dbg(DBG_PRINT, "vma->start = 0x%x vma->end = 0x%x startpage = 0x%x endvfn = 0x%x\n",vma->vma_start, vma->vma_end, startvfn, endvfn);
                return 0;
            }
        } list_iterate_end();
    }
    return 1;
}

/* Read into 'buf' from the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do so, you will want to find the vmareas
 * to read from, then find the pframes within those vmareas corresponding
 * to the virtual addresses you want to read, and then read from the
 * physical memory that pframe points to. You should not check permissions
 * of the areas. Assume (KASSERT) that all the areas you are accessing exist.
 * Returns 0 on success, -errno on error.
 */
int
vmmap_read(vmmap_t *map, const void *vaddr, void *buf, size_t count)
{
        /*NOT_YET_IMPLEMENTED("VM: vmmap_read");
        return 0;*/
    dbg(DBG_PRINT, "vmmap_read called with vaddr = 0x%x, buf = 0x%x and count = %d\n", (uint32_t)vaddr, (uint32_t)buf, count);
    void *last_vaddr = (void *)((size_t)vaddr + count - 1);
    uint32_t first_vfn = ADDR_TO_PN(vaddr);
    uint32_t vaddr_offset = (uint32_t)PAGE_OFFSET(vaddr);
    uint32_t last_vfn  = ADDR_TO_PN(last_vaddr); /*TODO not entirely sure about the -1*/
    uint32_t pagenum;
    vmarea_t *vma;
    pframe_t *pf;

    uint32_t i = 0;
    for (i = first_vfn; i <= last_vfn; i++){
        vma = vmmap_lookup(map, i);
        /*put kassert here*/
        pagenum = vma->vma_off + (i - vma->vma_start);/*TODO!! Not sure about
                                                        this logic*/
        /*Find pframes*/
        int forwrite = 0;
        int ret;
        dbg(DBG_PRINT, "Looking for pagenum %d in pframe_lookup \n", pagenum);
        ret = pframe_lookup(vma->vma_obj, pagenum, forwrite, &pf);
        if (ret < 0){
            dbg(DBG_PRINT, "DO SOMETHING\n");
            return ret;
        }

        /*We have the page frame now. Now we have to Read from the phy memory*/

        /* Logic
         *
         * Say each page is just 4 bytes(PAGE_OFFSET = 2) and the entire VA = 12 bytes as shown below.
         * For simplicity, lets assume unity mapping (entire VA maps directly to PA 1-1).
         *
         * So VA & PA -> (00)(01)(02)(03)--(04)(05)(06)(07)--(08)(09)(10)(11)|
         *
         * Say we want to read from vaddr(or paddr) = 02 and count = 7;
         *
         *Thus we will have to read the first page partially, second page completely and third page
         partially again.

         To achieve this, we the first iteration, we read, from pf1, bytes 02 and 03. So the 
         start_addr is set to 02 and num_bytes to 2. Also the buf, to which this is being copied to
         is incremented by num_bytes.

         In the second iteration, the start_addr is updated to 04 and one full page of bytes, that is
         num_bytes = 4 is copied. Again buf is incremented by num_bytes.

         In the last iteration, the start_addr is 08 and the num_bytes is 1. Incrementing buf is 
         not necessary here.

        TODO: Need to add prints to this so that the logic is verified at runtime with a small test.
        Logic */


        /*Explaining the code below -
         * We want to memcpy num_bytes between source_addr to dest_addr
         * Here source_addr = pf_addr + offset. dest_addr = buf.
         *
         * Case1 - first page
         *      Since we start reading from the middle of the page, we need to calculate source_addr = pf_addr + offset
         *      Now num_bytes depends on how big the count is. 
         *          Case1 - If count is such that the source_addr + count fall in the same
         *          page, then we make num_bytes = count
         *          Case2 - If count is such that source_addr + count falls outside the same page,
         *          then we make num_bytes = 4KB - offset (That is, the remaining bytes of that page)
         *      At the end, we increment dest_addr to num_bytes.
         *
         *Case2 - middle page
                If it is not the first or last page, we will be reading this entire page.
                In this case, things are simpler.
                start_addr = pf_addr of this middle page
                num_bytes = 4KB
                At the end dest_addr will be incremented by 4KB.

          Case3 - Last page
                The source_addr is simply pf_addr
                The num_bytes is not 4KB. Depends on the count. Basically find the vfn that
                vaddr + count belong to and subtract (vfaddr + count) - vfn address.
                dest_addr may or may not be incremented by num_bytes. Not really required.
                    
        */
        /*We have the page frame now. Now we have to Read from the phy memory*/
        size_t num_bytes;
        void *source_addr;
        if (i == first_vfn){
            void *page_start_addr = pf->pf_addr;
            void *start_addr = (void *)((uint32_t)page_start_addr + vaddr_offset);
            void *page_end_addr = (void *)((uint32_t)pf->pf_addr + PAGE_SIZE);
            num_bytes = (((size_t)start_addr + count) < (size_t)page_end_addr) ? count : (size_t)((uint32_t)page_end_addr - (uint32_t)start_addr);
            source_addr = start_addr;
            dbg(DBG_PRINT, "The start address of the page is 0x%x\n", (uint32_t)page_start_addr);
            dbg(DBG_PRINT, "The end address of the   page is 0x%x\n", (uint32_t)page_end_addr);
            dbg(DBG_PRINT, "Offset from the start of the page is 0x%x\n", (uint32_t)vaddr_offset);
            dbg(DBG_PRINT, "About to copy from \n0x%x to \n0x%x for \nnum_bytes = %d\n", (uint32_t)source_addr, (uint32_t)buf, num_bytes);
            memcpy((void *)buf, source_addr, num_bytes);
            buf = (void *)((size_t)buf + num_bytes); /* Increment buf pointer so that the next write can follow the previous one*/
            if (first_vfn == last_vfn){
                return 0;
            }
        } else if (i == last_vfn) {
            void *page_start_addr = pf->pf_addr;
            void *page_end_addr = (void *)((uint32_t)pf->pf_addr + PAGE_SIZE);
            num_bytes = (size_t)((uint32_t)last_vaddr - (uint32_t)PN_TO_ADDR(last_vfn));
            source_addr = pf->pf_addr;
            dbg(DBG_PRINT, "The start address of the page is 0x%x\n", (uint32_t)page_start_addr);
            dbg(DBG_PRINT, "The end address of the   page is 0x%x\n", (uint32_t)page_end_addr);
            dbg(DBG_PRINT, "Offset from the start of the page is 0x%x\n", (uint32_t)vaddr_offset);
            dbg(DBG_PRINT, "About to copy from \n0x%x to \n0x%x for \nnum_bytes = %d\n", (uint32_t)source_addr, (uint32_t)buf, num_bytes);
            memcpy((void *)buf, source_addr, num_bytes);
            buf = (void *)((size_t)buf + num_bytes); /* Increment buf pointer so that the next write can follow the previous one*/
        } else {
            void *page_start_addr = pf->pf_addr;
            void *page_end_addr = (void *)((uint32_t)pf->pf_addr + PAGE_SIZE);
            num_bytes = PAGE_SIZE;
            source_addr = pf->pf_addr;
            dbg(DBG_PRINT, "The start address of the page is 0x%x\n", (uint32_t)page_start_addr);
            dbg(DBG_PRINT, "The end address of the   page is 0x%x\n", (uint32_t)page_end_addr);
            dbg(DBG_PRINT, "Offset from the start of the page is 0x%x\n", (uint32_t)vaddr_offset);
            dbg(DBG_PRINT, "About to copy from \n0x%x to \n0x%x for \nnum_bytes = %d\n", (uint32_t)source_addr, (uint32_t)buf, num_bytes);
            memcpy((void *)buf, source_addr, num_bytes);
            buf = (void *)((size_t)buf + num_bytes); /* Increment buf pointer so that the next write can follow the previous one*/
            }
    }
    return 0;
}

/* Write from 'buf' into the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do this, you will need to find the correct
 * vmareas to write into, then find the correct pframes within those vmareas,
 * and finally write into the physical addresses that those pframes correspond
 * to. You should not check permissions of the areas you use. Assume (KASSERT)
 * that all the areas you are accessing exist. Remember to dirty pages!
 * Returns 0 on success, -errno on error.
 */
int
vmmap_write(vmmap_t *map, void *vaddr, const void *buf, size_t count)
{
        /*NOT_YET_IMPLEMENTED("VM: vmmap_write");*/
    dbg(DBG_PRINT, "vmmap_write called with vaddr = 0x%x, buf = 0x%x and count = %d\n", (uint32_t)vaddr, (uint32_t)buf, count);
    void *last_vaddr = (void *)((size_t)vaddr + count - 1);
    uint32_t first_vfn = ADDR_TO_PN(vaddr);
    uint32_t vaddr_offset = (uint32_t)PAGE_OFFSET(vaddr);
    uint32_t last_vfn  = ADDR_TO_PN(last_vaddr); /*TODO not entirely sure about the -1*/
    uint32_t pagenum;
    vmarea_t *vma;
    pframe_t *pf;

    uint32_t i = 0;
    for (i = first_vfn; i <= last_vfn; i++){
        vma = vmmap_lookup(map, i);
        /*put kassert here*/
        pagenum = vma->vma_off + (i - vma->vma_start);/*TODO!! Not sure about
                                                        this logic*/
        /*Find pframes*/
        int forwrite = 0;
        int ret;
        dbg(DBG_PRINT, "Looking for pagenum %d in pframe_lookup \n", pagenum);
        char debug[1024];
        vmmap_mapping_info(map, debug, 1024);
        dbg(DBG_PRINT, "The debug info is \n%s \n", debug);
        ret = pframe_lookup(vma->vma_obj, pagenum, forwrite, &pf);
        if (ret < 0){
            dbg(DBG_PRINT, "DO SOMETHING\n");
            return ret;
        }
        
        pframe_dirty(pf); /* As of now,this is the only change compared vmmap_read*/
        
        /*We have the page frame now. Now we have to Read from the phy memory*/
        size_t num_bytes;
        void *source_addr;
        if (i == first_vfn){
            void *page_start_addr = pf->pf_addr;
            void *start_addr = (void *)((uint32_t)page_start_addr + vaddr_offset);
            void *page_end_addr = (void *)((uint32_t)pf->pf_addr + PAGE_SIZE);
            num_bytes = (((size_t)start_addr + count) < (size_t)page_end_addr) ? count : (size_t)((uint32_t)page_end_addr - (uint32_t)start_addr);
            source_addr = start_addr;
            dbg(DBG_PRINT, "The start address of the page is 0x%x\n", (uint32_t)page_start_addr);
            dbg(DBG_PRINT, "The end address of the   page is 0x%x\n", (uint32_t)page_end_addr);
            dbg(DBG_PRINT, "Offset from the start of the page is 0x%x\n", (uint32_t)vaddr_offset);
            dbg(DBG_PRINT, "About to copy from \n0x%x to \n0x%x for \nnum_bytes = %d\n", (uint32_t)source_addr, (uint32_t)buf, num_bytes);
            memcpy(source_addr, (void *)buf, num_bytes);
            buf = (void *)((size_t)buf + num_bytes); /* Increment buf pointer so that the next write can follow the previous one*/
            if (first_vfn == last_vfn){
                return 0;
            }
        } else if (i == last_vfn) {
            void *page_start_addr = pf->pf_addr;
            void *page_end_addr = (void *)((uint32_t)pf->pf_addr + PAGE_SIZE);
            num_bytes = (size_t)((uint32_t)last_vaddr - (uint32_t)PN_TO_ADDR(last_vfn));
            source_addr = pf->pf_addr;
            dbg(DBG_PRINT, "The start address of the page is 0x%x\n", (uint32_t)page_start_addr);
            dbg(DBG_PRINT, "The end address of the   page is 0x%x\n", (uint32_t)page_end_addr);
            dbg(DBG_PRINT, "Offset from the start of the page is 0x%x\n", (uint32_t)vaddr_offset);
            dbg(DBG_PRINT, "About to copy from \n0x%x to \n0x%x for \nnum_bytes = %d\n", (uint32_t)source_addr, (uint32_t)buf, num_bytes);
            memcpy(source_addr, (void *)buf, num_bytes);
            buf = (void *)((size_t)buf + num_bytes); /* Increment buf pointer so that the next write can follow the previous one*/
        } else {
            void *page_start_addr = pf->pf_addr;
            void *page_end_addr = (void *)((uint32_t)pf->pf_addr + PAGE_SIZE);
            num_bytes = PAGE_SIZE;
            source_addr = pf->pf_addr;
            dbg(DBG_PRINT, "The start address of the page is 0x%x\n", (uint32_t)page_start_addr);
            dbg(DBG_PRINT, "The end address of the   page is 0x%x\n", (uint32_t)page_end_addr);
            dbg(DBG_PRINT, "Offset from the start of the page is 0x%x\n", (uint32_t)vaddr_offset);
            dbg(DBG_PRINT, "About to copy from \n0x%x to \n0x%x for \nnum_bytes = %d\n", (uint32_t)source_addr, (uint32_t)buf, num_bytes);
            memcpy(source_addr, (void *)buf, num_bytes);
            buf = (void *)((size_t)buf + num_bytes); /* Increment buf pointer so that the next write can follow the previous one*/
            }
    }
    return 0;

}

/* a debugging routine: dumps the mappings of the given address space. */
size_t
vmmap_mapping_info(const void *vmmap, char *buf, size_t osize)
{
        KASSERT(0 < osize);
        KASSERT(NULL != buf);
        KASSERT(NULL != vmmap);

        vmmap_t *map = (vmmap_t *)vmmap;
        vmarea_t *vma;
        ssize_t size = (ssize_t)osize;

        int len = snprintf(buf, size, "%21s %5s %7s %8s %10s %12s\n",
                           "VADDR RANGE", "PROT", "FLAGS", "MMOBJ", "OFFSET",
                           "VFN RANGE");

        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                size -= len;
                buf += len;
                if (0 >= size) {
                        goto end;
                }

                len = snprintf(buf, size,
                               "%#.8x-%#.8x  %c%c%c  %7s 0x%p %#.5x %#.5x-%#.5x\n",
                               vma->vma_start << PAGE_SHIFT,
                               vma->vma_end << PAGE_SHIFT,
                               (vma->vma_prot & PROT_READ ? 'r' : '-'),
                               (vma->vma_prot & PROT_WRITE ? 'w' : '-'),
                               (vma->vma_prot & PROT_EXEC ? 'x' : '-'),
                               (vma->vma_flags & MAP_SHARED ? " SHARED" : "PRIVATE"),
                               vma->vma_obj, vma->vma_off, vma->vma_start, vma->vma_end);
        } list_iterate_end();

end:
        if (size <= 0) {
                size = osize;
                buf[osize - 1] = '\0';
        }
        /*
        KASSERT(0 <= size);
        if (0 == size) {
                size++;
                buf--;
                buf[0] = '\0';
        }
        */
        return osize - size;
}
