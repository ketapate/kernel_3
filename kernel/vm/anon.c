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

int anon_count = 0; /* for debugging/verification purposes */

static slab_allocator_t *anon_allocator;

static void anon_ref(mmobj_t *o);
static void anon_put(mmobj_t *o);
static int  anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  anon_fillpage(mmobj_t *o, pframe_t *pf);
static int  anon_dirtypage(mmobj_t *o, pframe_t *pf);
static int  anon_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t anon_mmobj_ops = {
        .ref = anon_ref,
        .put = anon_put,
        .lookuppage = anon_lookuppage,
        .fillpage  = anon_fillpage,
        .dirtypage = anon_dirtypage,
        .cleanpage = anon_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * anonymous page sub system. Currently it only initializes the
 * anon_allocator object.
 */
void
anon_init()
{
    /* NOT_YET_IMPLEMENTED("VM: anon_init"); */
    anon_allocator = slab_allocator_create("anon", sizeof(mmobj_t));
    KASSERT(anon_allocator);
    dbg(DBG_PRINT, "(GRADING3A 4.a) anon_allocator exists. \n");
    
}

/*
 * You'll want to use the anon_allocator to allocate the mmobj to
 * return, then then initialize it. Take a look in mm/mmobj.h for
 * macros which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
anon_create()
{
    /* NOT_YET_IMPLEMENTED("VM: anon_create"); */
    
    /* allocate the mmobj */
    mmobj_t* newOne = (mmobj_t*)slab_obj_alloc(anon_allocator);
    
    /* Initialize */
    newOne->mmo_ops = &anon_mmobj_ops;
    /* refcount is 1 when created! */
    newOne->mmo_refcount = 1;
    newOne->mmo_nrespages = 0;
    list_init(&newOne->mmo_respages);
    list_init(&newOne->mmo_un.mmo_vmas);
    /*newOne->mmo_un.mmo_bottom_obj = NULL;*/
    newOne->mmo_shadowed = NULL;
    
    return newOne;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
anon_ref(mmobj_t *o)
{
    /* NOT_YET_IMPLEMENTED("VM: anon_ref"); */
    KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));
    dbg(DBG_PRINT, "(GRADING3A 4.b) mmo_refcount > 0 and anon ops are set correctly \n");
    (o->mmo_refcount)++;
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is an anonymous object, it will
 * never be used again. You should unpin and uncache all of the
 * object's pages and then free the object itself.
 */
static void
anon_put(mmobj_t *o)
{
    /* NOT_YET_IMPLEMENTED("VM: anon_put"); */
    KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));
    dbg(DBG_PRINT, "(GRADING3A 4.c) mmo_refcount > 0 and anon ops are set correctly \n");
    
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
            dbg(DBG_PRINT, "Right before free the anon object, mmo_refcount is: %d \n", o->mmo_refcount);
            dbg(DBG_PRINT, "Right before free the anon object, mmo_nrespages is: %d \n", o->mmo_nrespages);
            slab_obj_free(anon_allocator,o);
        }
    }
}

/* Get the corresponding page from the mmobj. No special handling is
 * required. */
static int
anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
    /* NOT_YET_IMPLEMENTED("VM: anon_lookuppage"); */
    return pframe_get(o, pagenum, pf);
}

/* The following three functions should not be difficult. */

static int
anon_fillpage(mmobj_t *o, pframe_t *pf)
{
    /* NOT_YET_IMPLEMENTED("VM: anon_fillpage"); */
    KASSERT(pframe_is_busy(pf));
    dbg(DBG_PRINT, "(GRADING3A 4.d) pframe is busy \n");
    KASSERT(!pframe_is_pinned(pf));
    dbg(DBG_PRINT, "(GRADING3A 4.d) pframe is not pinned \n");
    
    memset((void*)pf->pf_addr, 0, PAGE_SIZE);
    pframe_pin(pf);
    return 0;
}

static int
anon_dirtypage(mmobj_t *o, pframe_t *pf)
{
    /* NOT_YET_IMPLEMENTED("VM: anon_dirtypage"); */
    
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
anon_cleanpage(mmobj_t *o, pframe_t *pf)
{
    /* NOT_YET_IMPLEMENTED("VM: anon_cleanpage"); */
    
    if (pframe_is_pinned(pf)) {
        dbg (DBG_PRINT, "Called cleanpage while the page is free! Not cleaning!\n");
        pframe_clear_dirty(pf);
        return 0;
    } else {
        pframe_clear_dirty(pf);
        return 0;
    }
}
