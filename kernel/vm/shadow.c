#include "globals.h"
#include "errno.h"

#include "util/string.h"
#include "util/debug.h"

#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/mm.h"
#include "mm/page.h"
#include "mm/slab.h"
#include "mm/tlb.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/shadowd.h"

#define SHADOW_SINGLETON_THRESHOLD 5

int shadow_count = 0; /* for debugging/verification purposes */
#ifdef __SHADOWD__
/*
 * number of shadow objects with a single parent, that is another shadow
 * object in the shadow objects tree(singletons)
 */
static int shadow_singleton_count = 0;
#endif

static slab_allocator_t *shadow_allocator;

static void shadow_ref(mmobj_t *o);
static void shadow_put(mmobj_t *o);
static int  shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  shadow_fillpage(mmobj_t *o, pframe_t *pf);
static int  shadow_dirtypage(mmobj_t *o, pframe_t *pf);
static int  shadow_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t shadow_mmobj_ops = {
        .ref = shadow_ref,
        .put = shadow_put,
        .lookuppage = shadow_lookuppage,
        .fillpage  = shadow_fillpage,
        .dirtypage = shadow_dirtypage,
        .cleanpage = shadow_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * shadow page sub system. Currently it only initializes the
 * shadow_allocator object.
 */
void
shadow_init()
{
    /* NOT_YET_IMPLEMENTED("VM: shadow_init"); */
    shadow_allocator = slab_allocator_create("shadow", sizeof(mmobj_t));
    KASSERT(NULL != shadow_allocator && "failed to create shadow allocator!");
    
    KASSERT(shadow_allocator);
    dbg(DBG_PRINT, "(GRADING3A 6.a) shadow_allocator exists. \n");
}

/*
 * You'll want to use the shadow_allocator to allocate the mmobj to
 * return, then then initialize it. Take a look in mm/mmobj.h for
 * macros which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
shadow_create()
{
    /* NOT_YET_IMPLEMENTED("VM: shadow_create"); */
    /* allocate the mmobj */
    mmobj_t* newOne = (mmobj_t*)slab_obj_alloc(shadow_allocator);
    
    /* make sure we get a new one! */
    if (newOne == NULL) {
        dbg(DBG_PRINT, "shadow_create failed because no enough memmory!\n");
        return NULL;
    }
    
    /* Initialize */
    newOne->mmo_ops = &shadow_mmobj_ops;
    /* refcount is 1 when created! */
    newOne->mmo_refcount = 1;
    newOne->mmo_nrespages = 0;
    list_init(&newOne->mmo_respages);
    
    /*list_init(&newOne->mmo_un.mmo_vmas);*/
    newOne->mmo_un.mmo_bottom_obj = NULL;
    newOne->mmo_shadowed = NULL;
    
    return newOne;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
shadow_ref(mmobj_t *o)
{
    /* NOT_YET_IMPLEMENTED("VM: shadow_ref"); */
    KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
    dbg(DBG_PRINT, "(GRADING3A 6.b) mmo_refcount > 0 and shadow ops are set correctly \n");
    
    (o->mmo_refcount)++;
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is a shadow object, it will never
 * be used again. You should unpin and uncache all of the object's
 * pages and then free the object itself.
 */
static void
shadow_put(mmobj_t *o)
{
    /* NOT_YET_IMPLEMENTED("VM: shadow_put"); */
    KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
    dbg(DBG_PRINT, "(GRADING3A 6.c) mmo_refcount > 0 and shadow ops are set correctly \n");
    
    (o->mmo_refcount)--;
    int refcount = o->mmo_refcount;
    int nrespages = o->mmo_nrespages;
    if(o->mmo_refcount == o->mmo_nrespages) {
        /* unpin and uncache all of the object's pages */
        while (!list_empty(&o->mmo_respages)) {
            pframe_t *onePF = list_tail(&o->mmo_respages, pframe_t, pf_olink);
            while (pframe_is_pinned(onePF)) {
                pframe_unpin(onePF);
            }
            pframe_free(onePF);
            
            /* updated */
            if (refcount != 0) {
                return;
            }
        }
        /* then free the object itself */
        if (refcount == 0 && nrespages == 0) {
            if (o->mmo_shadowed != NULL) {
                o->mmo_shadowed->mmo_ops->put(o->mmo_shadowed);
            }
            dbg(DBG_PRINT, "Right before free the shadow object, mmo_refcount is: %d \n", o->mmo_refcount);
            dbg(DBG_PRINT, "Right before free the shadow object, mmo_nrespages is: %d \n", o->mmo_nrespages);
            slab_obj_free(shadow_allocator,o);
        }
    }
}

/* This function looks up the given page in this shadow object. The
 * forwrite argument is true if the page is being looked up for
 * writing, false if it is being looked up for reading. This function
 * must handle all do-not-copy-on-not-write magic (i.e. when forwrite
 * is false find the first shadow object in the chain which has the
 * given page resident). copy-on-write magic (necessary when forwrite
 * is true) is handled in shadow_fillpage, not here. */
static int
shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
    /* NOT_YET_IMPLEMENTED("VM: shadow_lookuppage"); */
    
    if (forwrite == 0) {
        /* for read */
        mmobj_t *oneMMObj = o;
        
        while (oneMMObj != NULL) {
            *pf = pframe_get_resident(oneMMObj, pagenum);
            if (*pf) {
                /* truely found a pframe */
                if (pframe_is_busy(*pf)) {
                    sched_sleep_on(&(*pf)->pf_waitq);
                }
                return 0;
            }
            /* reach here means not found a pframe */
            /* update oneMMObj to find next mmobj */
            oneMMObj = oneMMObj->mmo_shadowed;
        }
        /* if reached here, does it mean that the page must be called using pframe_get? Yes! */
        
        if( o->mmo_un.mmo_bottom_obj->mmo_ops->lookuppage(o->mmo_un.mmo_bottom_obj, pagenum, forwrite, pf) < 0){
        /*if( pframe_get(o->mmo_un.mmo_bottom_obj, pagenum, pf) < 0){*/
            return -1;
        }
        return 0;
        
    } else {
        /* for write */
        /* create a new pframe, fillpage will fill the new page! */
        if( pframe_get(o, pagenum, pf) < 0){
            return -1;
        }
        return 0;
    }
}

/* As per the specification in mmobj.h, fill the page frame starting
 * at address pf->pf_addr with the contents of the page identified by
 * pf->pf_obj and pf->pf_pagenum. This function handles all
 * copy-on-write magic (i.e. if there is a shadow object which has
 * data for the pf->pf_pagenum-th page then we should take that data,
 * if no such shadow object exists we need to follow the chain of
 * shadow objects all the way to the bottom object and take the data
 * for the pf->pf_pagenum-th page from the last object in the chain). */
static int
shadow_fillpage(mmobj_t *o, pframe_t *pf)
{
    /* NOT_YET_IMPLEMENTED("VM: shadow_fillpage"); */
    KASSERT(pframe_is_busy(pf));
    dbg(DBG_PRINT, "(GRADING3A 6.d) pframe is busy \n");
    KASSERT(!pframe_is_pinned(pf));
    dbg(DBG_PRINT, "(GRADING3A 6.d) pframe is not pinned \n");
    
    pframe_t *oldPF;
    int ret;
    
    ret = o->mmo_shadowed->mmo_ops->lookuppage(o->mmo_shadowed, pf->pf_pagenum, 0, &oldPF);
    if (ret < 0) {
        /* reach here means not found the source pframe */
        return -1;
    } else {
        /* truely found the source pframe */
        
        pframe_pin(pf);
        memcpy(pf->pf_addr, oldPF->pf_addr, PAGE_SIZE);
        return 0;
    }
}

/* These next two functions are not difficult. */

static int
shadow_dirtypage(mmobj_t *o, pframe_t *pf)
{
    /* NOT_YET_IMPLEMENTED("VM: shadow_dirtypage"); */

    if (!pframe_is_dirty(pf)) {
        pframe_set_dirty(pf);
    }

    if (pframe_is_dirty(pf)) {
        return 0;
    } else {
        return -1;
    }
}

static int
shadow_cleanpage(mmobj_t *o, pframe_t *pf)
{
    /* NOT_YET_IMPLEMENTED("VM: shadow_cleanpage"); */
    
    if (pframe_is_pinned(pf)) {
        dbg (DBG_PRINT, "Called cleanpage on a pinned page\n");
        pframe_clear_dirty(pf);
        return 0;
    } else {
        pframe_clear_dirty(pf);
        return 0;
    }
}
