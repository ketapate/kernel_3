/*
 *  FILE: open.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Mon Apr  6 19:27:49 1998
 */

#include "globals.h"
#include "errno.h"
#include "fs/fcntl.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/stat.h"
#include "util/debug.h"

/* find empty index in p->p_files[] */
int
get_empty_fd(proc_t *p)
{
        int fd;

        for (fd = 0; fd < NFILES; fd++) {
                if (!p->p_files[fd])
                        return fd;
        }

        dbg(DBG_ERROR | DBG_VFS, "ERROR: get_empty_fd: out of file descriptors "
            "for pid %d\n", curproc->p_pid);
        return -EMFILE;
}

/*
 * There a number of steps to opening a file:
 *      1. Get the next empty file descriptor.
 *      2. Call fget to get a fresh file_t.
 *      3. Save the file_t in curproc's file descriptor table.
 *      4. Set file_t->f_mode to OR of FMODE_(READ|WRITE|APPEND) based on
 *         oflags, which can be O_RDONLY, O_WRONLY or O_RDWR, possibly OR'd with
 *         O_APPEND.
 *      5. Use open_namev() to get the vnode for the file_t.
 *      6. Fill in the fields of the file_t.
 *      7. Return new fd.
 *
 * If anything goes wrong at any point (specifically if the call to open_namev
 * fails), be sure to remove the fd from curproc, fput the file_t and return an
 * error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        oflags is not valid.
 *      o EMFILE
 *        The process already has the maximum number of files open.
 *      o ENOMEM
 *        Insufficient kernel memory was available.
 *      o ENAMETOOLONG
 *        A component of filename was too long.
 *      o ENOENT
 *        O_CREAT is not set and the named file does not exist.  Or, a
 *        directory component in pathname does not exist.
 *      o EISDIR
 *        pathname refers to a directory and the access requested involved
 *        writing (that is, O_WRONLY or O_RDWR is set).
 *      o ENXIO
 *        pathname refers to a device special file and no corresponding device
 *        exists.
 */

int
do_open(const char *filename, int oflags)
{
    /* Commented out by Manan
        NOT_YET_IMPLEMENTED("VFS: do_open");
        return -1;
    */
    
    dbg(DBG_PRINT,"Call to do_open with filename = %s and oflags = 0x%x \n", filename, oflags);
    /* Manan - very bad code to check all possible combinations. Should come up with something simpler
     * update - Found a better way to do this below*/
    /*
    if (!((oflags == 0x000) || (oflags == 0x001) || (oflags == 0x002) || 
          (oflags == 0x100) || (oflags == 0x101) || (oflags == 0x102) ||
          (oflags == 0x200) || (oflags == 0x201) || (oflags == 0x202) ||
          (oflags == 0x300) || (oflags == 0x301) || (oflags == 0x302) || 
          (oflags == 0x400) || (oflags == 0x401) || (oflags == 0x402) || 
          (oflags == 0x500) || (oflags == 0x501) || (oflags == 0x502) || 
          (oflags == 0x600) || (oflags == 0x601) || (oflags == 0x602) || 
          (oflags == 0x700) || (oflags == 0x701) || (oflags == 0x702) )){
        return -EINVAL;
    }
    */
   
    /*Found a better way to do this!*/

    /*
    step 1 - First check for bits that are supposed to be zero using the mask
     * 1111-1111-1111-1111-1111-1000-1111-1100.
     *
    step 2 - Check for first two bits. They both cannot be set simultaneously*/
    
    /* Improved: more portable */
    /* if ((oflags & 0xFFFFF80C) != 0){ */
    
    if ((oflags & ~(0x703)) != 0) {
        dbg(DBG_PRINT,"ERROR!!! The oflags has invalid bits set to 1. \n");
        return -EINVAL;
    } else if (((oflags & 0x1) == 0x1) && ((oflags & 0x2) == 0x2)){
        dbg(DBG_PRINT,"ERROR!!! The 2 LSBs of oflags are set. Invalid combination. \n");
        return -EINVAL;
    }
    /* Add more conditions here as else if.*/

    
    /*      1. Get the next empty file descriptor.*/
    int new_fd = get_empty_fd(curproc);
    if (new_fd == -EMFILE){
        return -EMFILE;
    }
 
    /*      2. Call fget to get a fresh file_t.*/
    file_t *new_file_t = fget(-1); /* -1 is passed as argument to get a new file object. */
    if (new_file_t == NULL) {
        dbg(DBG_PRINT,"ERROR!!! Insufficient memory returned by fget.\n");
        return -ENOMEM;
    }
 
    /*      3. Save the file_t in curproc's file descriptor table.*/
    curproc->p_files[new_fd] = new_file_t;

    /*      4. Set file_t->f_mode to OR of FMODE_(READ|WRITE|APPEND) based on
          oflags, which can be O_RDONLY, O_WRONLY or O_RDWR, possibly OR'd with
          O_APPEND.*/
    int fmode = 0;
     /* Masking with 0100-0000-0000 to make sure that the
                             read/write/append bits are taken in.*/
    
    /* Corrected: fmode should be assigned FMODE_APPEND, not 0x400. FMODE_APPEND is 4. */
    if((oflags & 0x400) == 0x400){
        fmode = FMODE_APPEND;
    }

    /* Below logic fixes read/write fmodes*/
    dbg(DBG_PRINT,"oflags = 0x%x\n",oflags);
    if ((oflags & 0x1) == 0x0) {
        dbg(DBG_PRINT,"FMODE is READ\n");
        fmode |= FMODE_READ;
    }
    if ((oflags & 0x1) == 0x1){
        dbg(DBG_PRINT,"FMODE is WRITE\n");
        fmode |= FMODE_WRITE;
    }
    if ((oflags & 0x2) == 0x2){
        dbg(DBG_PRINT,"FMODE is READ | WRITE\n");
        fmode |= FMODE_READ | FMODE_WRITE;
    }
    new_file_t->f_mode = fmode;

    /*      5. Use open_namev() to get the vnode for the file_t.*/
    vnode_t *result_vnode;
    int ret = open_namev(filename, oflags, &result_vnode, NULL);
    if (ret) {
        fput(new_file_t);
        curproc->p_files[new_fd] = NULL;
        dbg(DBG_PRINT,"Call to do_open->open_namev returned the below \n%s\n", strerror(-ret));
        return ret;
    }
    
    new_file_t->f_vnode = result_vnode;
    /* ERROR!!!  pathname refers to a directory and the access requested involved 
     * writing (that is, O_WRONLY or O_RDWR is set).*/
    /*Corrections from Ziyu*/ 
    if(S_ISDIR( result_vnode->vn_mode ) && ( new_file_t->f_mode & FMODE_WRITE )){
        dbg(DBG_PRINT,"ERROR!!! pathname refers to a directory and the access requested involved writing (that is, O_WRONLY or O_RDWR is set).\n");
        fput(new_file_t);
        curproc->p_files[new_fd] = NULL;
        dbg(DBG_PRINT,"Returning the below\n%s\n", strerror(EISDIR));
        return -EISDIR;
    }
    
    /*      6. Fill in the fields of the file_t.*/
    new_file_t->f_pos = 0; /* initialize to 0  */

    /*      7. Return new fd.*/
    dbg(DBG_PRINT,"Returning fd = %d\n", new_fd);
    return new_fd;
}
