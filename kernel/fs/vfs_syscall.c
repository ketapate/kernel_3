/*
 *  FILE: vfs_syscall.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Wed Apr  8 02:46:19 1998
 *  $Id: vfs_syscall.c,v 1.1 2012/10/10 20:06:46 william Exp $
 */

#include "kernel.h"
#include "errno.h"
#include "globals.h"
#include "fs/vfs.h"
#include "fs/file.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/fcntl.h"
#include "fs/lseek.h"
#include "mm/kmalloc.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/stat.h"
#include "util/debug.h"

/* To read a file:
 *      o fget(fd)
 *      o call its virtual read f_op
 *      o update f_pos
 *      o fput() it
 *      o return the number of bytes read, or an error
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for reading.
 *      o EISDIR
 *        fd refers to a directory.
 *
 * In all cases, be sure you do not leak file refcounts by returning before
 * you fput() a file that you fget()'ed.
 */
int
do_read(int fd, void *buf, size_t nbytes)
{
    /* NOT_YET_IMPLEMENTED("VFS: do_read"); */
    dbg(DBG_PRINT,"do_read called with fd = %d\n", fd);
    
    /*ERROR!!! fd is outside of allowed range of file descriptors*/
    if ((fd >= NFILES) || ( fd < 0)) {
        dbg(DBG_PRINT,"ERROR!!! fd = %d is out of range\n", fd);
        return -EBADF;
    }
    
    /*      o fget(fd)*/
    file_t *cur_file_t = fget(fd);
    /* fd is not a valid file descriptor*/
    if (cur_file_t == NULL) {
        dbg(DBG_PRINT,"Invalid fd = %d\n", fd);
        return -EBADF;
    }
    /* file not open for reading */
    if ((cur_file_t->f_mode & FMODE_READ) != 0x1) {
        dbg(DBG_PRINT,"ERROR!!! File mode is %x. File not opened for reading. fd = %d\n", cur_file_t->f_mode, fd);
        fput(cur_file_t);
        return -EBADF;
    }
    /*fd refers to a directory*/
    if (S_ISDIR(cur_file_t->f_vnode->vn_mode)){
        dbg(DBG_PRINT,"ERROR!!! fd = %d refers to a directory\n", fd);
        fput(cur_file_t);
        return -EISDIR;
    }
    
    /*      o call its virtual read f_op*/
    int bytes_read = cur_file_t->f_vnode->vn_ops->read(cur_file_t->f_vnode, cur_file_t->f_pos, buf, nbytes);
    
    /*      o update f_pos*/
    cur_file_t->f_pos += bytes_read;
    
    /*      o fput() it*/
    fput(cur_file_t);

    return bytes_read;
}

/* Very similar to do_read.  Check f_mode to be sure the file is writable.  If
 * f_mode & FMODE_APPEND, do_lseek() to the end of the file, call the write
 * f_op, and fput the file.  As always, be mindful of refcount leaks.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for writing.
 */
int
do_write(int fd, const void *buf, size_t nbytes)
{
    dbg(DBG_PRINT,"do_write called with fd = %d, buf = %s and nbytes = %d\n", fd, (char *)buf, nbytes);
    /* NOT_YET_IMPLEMENTED("VFS: do_write"); */
    
    /*ERROR!!! fd is outside of allowed range of file descriptors*/
    if ((fd >= NFILES) || ( fd < 0)) {
        dbg(DBG_PRINT,"ERROR!!! fd = %d is out of range\n", fd);
        return -EBADF;
    }
    
    /*      o fget(fd)*/
    file_t *cur_file_t = fget(fd);
    int ret;
    /* fd is not a valid file descriptor*/
    if (cur_file_t == NULL) {
        dbg(DBG_PRINT,"Invalid fd = %d\n", fd);
        return -EBADF;
    }
    /* Check f_mode to be sure the file is writable.*/
    if ((cur_file_t->f_mode & FMODE_WRITE) != 0x2){
        dbg(DBG_PRINT,"ERROR!!! File mode is %x. File not opened for writing. fd = %d\n", cur_file_t->f_mode, fd);
        fput(cur_file_t);
        return -EBADF;
    }
    
    /* if f_mode & FMODE_APPEND, do_lseek() to the end of the file*/
    /* Corrected: should be 0x4 */
    /* Corrected: should be no cur_file_t->f_pos */
    if ((cur_file_t->f_mode & FMODE_APPEND) == 0x4){
        ret = do_lseek(fd, 0, SEEK_END);
        if (ret < 0) {
            
            /* Corrected: remember to fput */
            fput(cur_file_t);
            
            dbg(DBG_PRINT,"ERROR!!! do_write->do_lseek returned error %s %d\n", strerror(-ret), ret);
            return ret;
        }
    }

    /*Return errors returned by do_lseek*/
    
    /*      o call its virtual write f_op*/
    int bytes_written = cur_file_t->f_vnode->vn_ops->write(cur_file_t->f_vnode, cur_file_t->f_pos, buf, nbytes);
    
    /*      o update f_pos*/
    cur_file_t->f_pos += bytes_written;
    
    /* grading guideline required */
    if (bytes_written >= 0) {
        KASSERT((S_ISCHR(cur_file_t->f_vnode->vn_mode)) ||
                (S_ISBLK(cur_file_t->f_vnode->vn_mode)) ||
                ((S_ISREG(cur_file_t->f_vnode->vn_mode)) && (cur_file_t->f_pos <= cur_file_t->f_vnode->vn_len)));
        dbg(DBG_PRINT,"(GRADING2A 3.a) File's position is less than or equal to file's length. \n");
    }
    
    /*      o fput() it*/
    fput(cur_file_t);
    
    return bytes_written;
}

/*
 * Zero curproc->p_files[fd], and fput() the file. Return 0 on success
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't a valid open file descriptor.
 */
int
do_close(int fd)
{
    /* NOT_YET_IMPLEMENTED("VFS: do_close"); */
    dbg(DBG_PRINT,"do_close called for fd = %d\n", fd);
    
    /*ERROR!!! fd is outside of allowed range of file descriptors*/
    if ((fd >= NFILES) || ( fd < 0)) {
        dbg(DBG_PRINT,"ERROR!!! fd = %d is out of range\n", fd);
        return -EBADF;
    }
    
    file_t *cur_file_t = fget(fd);
    /* fd is not a valid file descriptor*/
    if (cur_file_t == NULL) {
    dbg(DBG_PRINT,"ERROR!!! No file descriptor entry for correspoding fd = %d\n", fd);
        /* fput(cur_file_t); */
        return -EBADF;
    }
    
    curproc->p_files[fd] = NULL;
    fput(cur_file_t);
    /*Fixng refcount leaks*/
    fput(cur_file_t);
    return 0;
}

/* To dup a file:
 *      o fget(fd) to up fd's refcount
 *      o get_empty_fd()
 *      o point the new fd to the same file_t* as the given fd
 *      o return the new file descriptor
 *
 * Don't fput() the fd unless something goes wrong.  Since we are creating
 * another reference to the file_t*, we want to up the refcount.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't an open file descriptor.
 *      o EMFILE
 *        The process already has the maximum number of file descriptors open
 *        and tried to open a new one.
 */
int
do_dup(int fd)
{
    dbg(DBG_PRINT,"do_dup called for fd = %d\n", fd);
    /* NOT_YET_IMPLEMENTED("VFS: do_dup"); */
    
    /*ERROR!!! fd is outside of allowed range of file descriptors*/
    if ((fd >= NFILES) || ( fd < 0)) {
        dbg(DBG_PRINT,"ERROR!!! fd = %d is out of range\n", fd);
        return -EBADF;
    }
    
    /*      o fget(fd) to up fd's refcount*/
    file_t *cur_file_t = fget(fd);
    /* fd is not a valid file descriptor*/
    if (cur_file_t == NULL) {
        dbg(DBG_PRINT,"ERROR!!! No file descriptor entry for correspoding fd = %d\n", fd);
        /* fput(cur_file_t); */
        return -EBADF;
    }
    
    /*      o get_empty_fd()*/
    int new_fd = get_empty_fd(curproc);
    /* ERROR!!! The process already has the maximum number of files open.*/
    if (new_fd == -EMFILE){
        dbg(DBG_PRINT,"ERROR!!! Max limit of file descriptors reached fd\n");
        fput(cur_file_t);
        return -EMFILE;
    }
    
    /*      o point the new fd to the same file_t* as the given fd*/
    curproc->p_files[new_fd] = cur_file_t;
    
    /*      o return the new file descriptor*/
    dbg(DBG_PRINT,"Returning fd = %d\n", new_fd);
    return new_fd;
}

/* Same as do_dup, but insted of using get_empty_fd() to get the new fd,
 * they give it to us in 'nfd'.  If nfd is in use (and not the same as ofd)
 * do_close() it first.  Then return the new file descriptor.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        ofd isn't an open file descriptor, or nfd is out of the allowed
 *        range for file descriptors.
 */
int
do_dup2(int ofd, int nfd)
{
    int ret;
    
    dbg(DBG_PRINT,"do_dup2 called for ofd = %d and nfd = %d\n", ofd, nfd);
    /* NOT_YET_IMPLEMENTED("VFS: do_dup2"); */
    
    /*ERROR!!! ofd is outside of allowed range of file descriptors*/
    if ((ofd >= NFILES) || ( ofd < 0)) {
        dbg(DBG_PRINT,"ERROR!!! ofd = %d is out of range\n", ofd);
        return -EBADF;
    }
    
    /*ERROR!!! nfd is outside of allowed range of file descriptors*/
    if ((nfd >= NFILES) || ( nfd < 0)){
        dbg(DBG_PRINT,"ERROR!!! nfd = %d is out of range\n",nfd);
        return -EBADF;
    }

    /*      o fget(fd) to up fd's refcount*/
    file_t *cur_file_t = fget(ofd);
    /* fd is not a valid file descriptor*/
    if (cur_file_t == NULL) {
        dbg(DBG_PRINT,"ERROR!!! ofd = %d is not a valid file descriptor\n", ofd);
        /* fput(cur_file_t); */
        return -EBADF;
    }
    
    /*if nfd is same as ofd, simply return*/
    if (ofd == nfd)
    {
        /* TODO simply return ?? */
        /* We need to do fput() */
        fput(cur_file_t);
        return nfd;
    }
    
    /* If nfd is in use (and not the same as ofd)
     do_close() it first. */
    if (curproc->p_files[nfd] != NULL) {
        
        /* Corrected: need to check the return value here */
        ret = do_close(nfd);
        if (ret) {
            fput(cur_file_t);
            return ret;
        }
    }
    
    /*      o point the new fd to the same file_t* as the given fd*/
    curproc->p_files[nfd] = cur_file_t;
    
    /*      o return the new file descriptor*/
    dbg(DBG_PRINT,"Returning nfd = %d\n", nfd);
    return nfd;
}

/*
 * This routine creates a special file of the type specified by 'mode' at
 * the location specified by 'path'. 'mode' should be one of S_IFCHR or
 * S_IFBLK (you might note that mknod(2) normally allows one to create
 * regular files as well-- for simplicity this is not the case in Weenix).
 * 'devid', as you might expect, is the device identifier of the device
 * that the new special file should represent.
 *
 * You might use a combination of dir_namev, lookup, and the fs-specific
 * mknod (that is, the containing directory's 'mknod' vnode operation).
 * Return the result of the fs-specific mknod, or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        mode requested creation of something other than a device special
 *        file.
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mknod(const char *path, int mode, unsigned devid)
{
    dbg(DBG_PRINT,"Calling do_mknod for path = %s, mode = %x and devid = %x\n", path, mode, devid);
    /* NOT_YET_IMPLEMENTED("VFS: do_mknod"); */
    
    size_t nameLen;
    const char *name;
    vnode_t *parent;
    vnode_t *result;
    int ret;
    int ret2;
    
    /* Error checking for EINVAL */
    if ((mode != S_IFBLK) && (mode != S_IFCHR)) {
        dbg(DBG_PRINT,"Invalid mode passed\n");
        return -EINVAL;
    }
    
    ret = dir_namev(path, &nameLen, &name, NULL, &parent);
    
    /* Don't need to vput, because our implementation guaranteed if dir_namev return an error, refcounting doesn't increment. */
    if (ret) {
        dbg(DBG_PRINT,"dir_namev returned %s\n", strerror(-ret));
        return ret;
    }
    
    /* After a successful dir_namev, we need to do vput */
    /* Corrected for kernel 3 */
    /* vput(parent); */
    
    ret = lookup(parent, name, nameLen, &result);
    /* -ENOENT means no such file, that's usually expected, so we create a new file */
    if (ret == -ENOENT) {
        
        /* grading guideline required */
        KASSERT(NULL != parent->vn_ops->mknod);
        dbg(DBG_PRINT,"(GRADING2A 3.b) The corresponding vnode has mknod function. \n");
        
        ret2 = parent->vn_ops->mknod(parent, name, nameLen, mode, devid);
        dbg(DBG_PRINT,"new node created!!\n");
        
        /* Corrected for kernel 3 */
        vput(parent);
        
        return ret2;
    } else {
        /* If there is any other error, just return it */
        if (ret) {
            dbg(DBG_PRINT,"Error in lookup %s\n", strerror(-ret));
            if (result != NULL){
                vput(result);
            }
            
            /* Corrected for kernel 3 */
            vput(parent);
            
            return ret;
        }

        if (result != NULL){
            vput(result);
        }
        /* ret == 0 means there is already such a file, it's an error! */
        dbg(DBG_PRINT,"Node alrady existed! \n");
        
        /* Corrected for kernel 3 */
        vput(parent);
        
        return -EEXIST;
    }
    
}

/* Use dir_namev() to find the vnode of the dir we want to make the new
 * directory in.  Then use lookup() to make sure it doesn't already exist.
 * Finally call the dir's mkdir vn_ops. Return what it returns.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mkdir(const char *path)
{
    /* NOT_YET_IMPLEMENTED("VFS: do_mkdir"); */
    
    /* Use dir_namev() to find the vnode of the dir we want to make the new
     * directory in.
     */
    vnode_t *parent_vnode = NULL;
    vnode_t *dir_vnode= NULL;
    size_t namelen;
    const char *name;
    int ret;
    int ret2;
    
    dbg(DBG_PRINT,"Calling dir_namev for path = %s\n", path);
    ret = dir_namev(path, &namelen, &name, NULL, &parent_vnode);
    
    if (ret){
        dbg(DBG_PRINT,"ERROR!!! A directory component in the path = %s does not exist!!!\n", path);
        dbg(DBG_PRINT,"ERROR!!! dir_namev returned error. \n");
        return ret;
    }

    if (name == NULL){
        vput(vfs_root_vn);
        return -EEXIST;
    }
    
    /* Need to do vput */
    /* Corrected for kernel 3 */
    /* vput(parent_vnode); */
    
    /* Actually we don't need to check it explicitly here, because lookup will first check if parent_vnode is a dir */
    if (!S_ISDIR(parent_vnode->vn_mode)){
        dbg(DBG_PRINT,"ERROR!!! A directory component in the path = %s is not a dir!!! \n", path);
        
        /* Corrected for kernel 3 */
        vput(parent_vnode);
        
        return -ENOTDIR;
    }
    
    /*Then use lookup() to make sure it doesn't already exist.*/
    ret = lookup(parent_vnode, name, namelen, &dir_vnode);
    if (ret == -ENOENT) {
        /* Finally call the dir's mkdir vn_ops.*/
        
        /* grading guideline required */
        KASSERT(NULL != parent_vnode->vn_ops->mkdir);
        dbg(DBG_PRINT,"(GRADING2A 3.c) The corresponding vnode has mkdir function. \n");
        
        ret2 = parent_vnode->vn_ops->mkdir(parent_vnode, name, namelen);
        
        /* Corrected for kernel 3 */
        vput(parent_vnode);
        
        /* Return what it returns.*/
        return ret2;
    }
    
    /* Corrected for kernel 3 */
    vput(parent_vnode);
    
    if (ret) {
        return ret;
    } else {
        vput(dir_vnode);
        return -EEXIST;
    }
}

/* Use dir_namev() to find the vnode of the directory containing the dir to be
 * removed. Then call the containing dir's rmdir v_op.  The rmdir v_op will
 * return an error if the dir to be removed does not exist or is not empty, so
 * you don't need to worry about that here. Return the value of the v_op,
 * or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        path has "." as its final component.
 *      o ENOTEMPTY
 *        path has ".." as its final component.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_rmdir(const char *path)
{
    /* NOT_YET_IMPLEMENTED("VFS: do_rmdir"); */
    
    /* Use dir_namev() to find the vnode of the directory containing the dir to be
     * removed.
     * */
    vnode_t *parent_vnode = NULL;
    size_t namelen;
    const char *name;
    int ret;
    
    ret = dir_namev(path, &namelen, &name, NULL, &parent_vnode);
    if (ret == -ENOENT){
        return -ENOENT;
    }
    /* ERROR!!! A component of filename was too long.*/
    if (ret == -ENAMETOOLONG){
        return -ENAMETOOLONG;
    }
    if (ret) {
        return ret;
    }
    
    /* Corrected for kernel 3 */
    /* vput(parent_vnode); */
    
    if (!S_ISDIR(parent_vnode->vn_mode)) {
        
        /* Corrected for kernel 3 */
        vput(parent_vnode);
        
        return -ENOTDIR;
    }
    /*        path has "." as its final component.*/
    /* Corrected: need to use strncmp */
    if (strncmp(name, ".", namelen) == 0) {
        
        /* Corrected for kernel 3 */
        vput(parent_vnode);
        
        return -EINVAL;
    }
    /*        path has ".." as its final component.*/
    /* Corrected: need to use strncmp */
    if (strncmp(name, "..", namelen) == 0) {
        
        /* Corrected for kernel 3 */
        vput(parent_vnode);
        
        return -ENOTEMPTY;
    }
    
    /*Then call the containing dir's rmdir v_op.  The rmdir v_op will
     * return an error if the dir to be removed does not exist or is not empty, so
     * you don't need to worry about that here.
     * */
    
    /* grading guideline required */
    KASSERT(NULL != parent_vnode->vn_ops->rmdir);
    dbg(DBG_PRINT,"(GRADING2A 3.d) The corresponding vnode has rmdir function. \n");
    
    ret = parent_vnode->vn_ops->rmdir(parent_vnode, name, namelen);
    
    /* Corrected for kernel 3 */
    vput(parent_vnode);
    
    /*Return the value of the v_op,
     * or an error. */
    return ret;
}

/*
 * Same as do_rmdir, but for files.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EISDIR
 *        path refers to a directory.
 *      o ENOENT
 *        A component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_unlink(const char *path)
{
    dbg(DBG_PRINT,"do_unlink called with path = %s \n", path);
    /* NOT_YET_IMPLEMENTED("VFS: do_unlink"); */

    /* Use dir_namev() to find the vnode of the directory containing the **FILE** to be
    * removed. 
    * */
    vnode_t *parent_vnode = NULL;
    size_t namelen;
    const char *name;
    int ret;

    ret = dir_namev(path, &namelen, &name, NULL, &parent_vnode);
    if (ret == -ENOENT){
        return -ENOENT;
    }
    
    if (ret == -ENOTDIR){
        return -ENOTDIR;
    }
    
    /* ERROR!!! A component of filename was too long.*/
    if (ret == -ENAMETOOLONG){
        return -ENAMETOOLONG;
    }
    
    if (ret) {
        return ret;
    }
    
    
    vnode_t *result;
    ret = lookup(parent_vnode, name, namelen, &result);
    
    if (ret) {
        vput(parent_vnode);
        return ret;
    }
    
    vput(result);
    
    if (result->vn_mode == S_IFDIR) {
        vput(parent_vnode);
        return -EISDIR;
    }

    /*Then call the containing dir's rmdir v_op.  The rmdir v_op will
    * return an error if the dir to be removed does not exist or is not empty, so
    * you don't need to worry about that here. 
    * */
    
    /* grading guideline required */
    dbg(DBG_PRINT,"parent_vnode = %p, parent_vnode->vn_ops = %p \n", parent_vnode, parent_vnode->vn_ops);
    KASSERT(NULL != parent_vnode->vn_ops->unlink);
    dbg(DBG_PRINT,"(GRADING2A 3.e) The corresponding vnode has unlink function. \n");
    
    ret = parent_vnode->vn_ops->unlink(parent_vnode, name, namelen);
   
    /*Return the value of the v_op,
    * or an error.
    * */
    vput(parent_vnode);
    return ret;
}

/* To link:
 *      o open_namev(from)
 *      o dir_namev(to)
 *      o call the destination dir's (to) link vn_ops.
 *      o return the result of link, or an error
 *
 * Remember to vput the vnodes returned from open_namev and dir_namev.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        to already exists.
 *      o ENOENT
 *        A directory component in from or to does not exist.
 *      o ENOTDIR
 *        A component used as a directory in from or to is not, in fact, a
 *        directory.
 *      o ENAMETOOLONG
 *        A component of from or to was too long.
 */
int
do_link(const char *from, const char *to)
{
    /* NOT_YET_IMPLEMENTED("VFS: do_link"); */
    
    /*      o open_namev(from)*/
    int ret = 0;
    vnode_t *from_vnode = NULL;
    vnode_t *to_vnode = NULL;
    size_t namelen;
    const char *name;
    
    int flag = 0; /* Not modifying the file, so flag should be 0? */
    ret = open_namev(from, flag, &from_vnode, NULL);
    if (ret == -ENOENT){
        return -ENOENT;
    }
    if (ret == -ENOTDIR){
        return -ENOTDIR;
    }
    /* ERROR!!! A component of filename was too long.*/
    if (ret == -ENAMETOOLONG){
        return -ENAMETOOLONG;
    }
    
    /* In case there are other errors */
    if (ret) {
        return ret;
    }
    /* Remember to vput the vnodes returned from open_namev and dir_namev.*/
    /* Corrected for kernel 3 */
    /* vput(from_vnode); */
    
    /* The textbook page 30 says hard link can not be made to directory */
    if (from_vnode->vn_mode == S_IFDIR) {
        
        /* Corrected for kernel 3 */
        vput(from_vnode);
        
        return -EISDIR;
    }
    
    /*      o dir_namev(to)*/
    ret = dir_namev(to, &namelen, &name, NULL, &to_vnode);
    if (ret == -ENOENT) {
        
        /* Corrected for kernel 3 */
        vput(from_vnode);
        
        return -ENOENT;
    }
    if (ret == -ENOTDIR) {
        
        /* Corrected for kernel 3 */
        vput(from_vnode);
        
        return -ENOTDIR;
    }
    /* ERROR!!! A component of filename was too long.*/
    if (ret == -ENAMETOOLONG) {
        
        /* Corrected for kernel 3 */
        vput(from_vnode);
        
        return -ENAMETOOLONG;
    }
    /* In case there are other errors */
    if (ret) {
        
        /* Corrected for kernel 3 */
        vput(from_vnode);
        
        return ret;
    }
    
    /* Remember to vput the vnodes returned from open_namev and dir_namev.*/
    /* Corrected for kernel 3 */
    /* vput(to_vnode); */
    
    /*o EEXIST
    *     to already exists.
    * do lookup to check if to already exists */
    vnode_t *testNode;
    ret = lookup(to_vnode, name, namelen, &testNode);
    
    if (ret == 0) {
        
        /* Corrected for kernel 3 */
        vput(from_vnode);
        vput(to_vnode);
        
        vput(testNode);
        return -EEXIST;
    } else if (ret == -ENOTDIR) {
        
        /* Corrected for kernel 3 */
        vput(from_vnode);
        vput(to_vnode);
        
        return -ENOTDIR;
    } else {
        /*      o call the destination dir's (to) link vn_ops.*/
        ret = to_vnode->vn_ops->link(from_vnode, to_vnode, name, namelen);
        
        /* Corrected for kernel 3 */
        vput(from_vnode);
        vput(to_vnode);
        
        /*      o return the result of link, or an error*/
        return ret;
    }
}

/*      o link newname to oldname
 *      o unlink oldname
 *      o return the value of unlink, or an error
 *
 * Note that this does not provide the same behavior as the
 * Linux system call (if unlink fails then two links to the
 * file could exist).
 */
int
do_rename(const char *oldname, const char *newname)
{
    /* NOT_YET_IMPLEMENTED("VFS: do_rename"); */
    
    /* Make sure name is not NULL. */
    if ((oldname == NULL) || (newname == NULL)) {
	    return -EINVAL;
    }
    
    int ret = 0;
    ret = do_link(oldname, newname); /*Order is right now.*/
    if (ret) {
        return ret;
    }
    ret = do_unlink(oldname);
    return ret;
}

/* Make the named directory the current process's cwd (current working
 * directory).  Don't forget to down the refcount to the old cwd (vput()) and
 * up the refcount to the new cwd (open_namev() or vget()). Return 0 on
 * success.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        path does not exist.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 *      o ENOTDIR
 *        A component of path is not a directory.
 */
int
do_chdir(const char *path)
{
    /* NOT_YET_IMPLEMENTED("VFS: do_chdir"); */
    
    int ret = 0;
    int flag = 0; /* Should this be 0 */
    vnode_t *dir;
    
    vnode_t *temp = curproc->p_cwd;
    ret = open_namev(path, flag, &dir, NULL);
    if (ret == -ENOENT){
        return -ENOENT;
    }
    if (ret == -ENOTDIR){
        return -ENOTDIR;
    }
    /* ERROR!!! A component of filename was too long.*/
    if (ret == -ENAMETOOLONG){
        return -ENAMETOOLONG;
    }
    if (ret) {
        return ret;
    }
    
    if (dir->vn_mode != S_IFDIR) {
        vput(dir);
        return -ENOTDIR;
    }
    
    /*Since the change_dir has been successful, vput the old cwd.*/
    vput(temp);
    
    curproc->p_cwd = dir;
    
    return 0;
}


/* Call the readdir f_op on the given fd, filling in the given dirent_t*.
 * If the readdir f_op is successful, it will return a positive value which
 * is the number of bytes copied to the dirent_t.  You need to increment the
 * file_t's f_pos by this amount.  As always, be aware of refcounts, check
 * the return value of the fget and the virtual function, and be sure the
 * virtual function exists (is not null) before calling it.
 *
 * Return either 0 or sizeof(dirent_t), or -errno.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        Invalid file descriptor fd.
 *      o ENOTDIR
 *        File descriptor does not refer to a directory.
 */
int
do_getdent(int fd, struct dirent *dirp)
{
    /* NOT_YET_IMPLEMENTED("VFS: do_getdent"); */
    
    /*
     *      o EBADF
     *        Invalid file descriptor fd.
     */
    
    if (fd >= NFILES || fd < 0) {
        return -EBADF;
    }
    
    file_t *oneFileEntry;
    int ret;
    
    oneFileEntry = fget(fd);
    if (oneFileEntry == NULL) {
        return -EBADF;
    }
    
    /* Corrected for kernel 3 */
    /* fput(oneFileEntry); */
    
    /* File descriptor does not refer to a directory. */
    if (oneFileEntry->f_vnode->vn_mode != S_IFDIR || oneFileEntry->f_vnode->vn_ops->readdir == NULL) {
        
        /* Corrected for kernel 3 */
        fput(oneFileEntry);
        
        return -ENOTDIR;
    }
    
    ret = oneFileEntry->f_vnode->vn_ops->readdir(oneFileEntry->f_vnode, oneFileEntry->f_pos, dirp);
    oneFileEntry->f_pos += ret;
    
    /* Corrected for kernel 3 */
    fput(oneFileEntry);
    
    if (ret) {
        return sizeof(*dirp);
    }
    return 0;
}

/*
 * Modify f_pos according to offset and whence.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not an open file descriptor.
 *      o EINVAL
 *        whence is not one of SEEK_SET, SEEK_CUR, SEEK_END; or the resulting
 *        file offset would be negative.
 */
int
do_lseek(int fd, int offset, int whence)
{
    /* NOT_YET_IMPLEMENTED("VFS: do_lseek"); */
    dbg(DBG_PRINT,"do_lseek called with fd = %d, offset = %d and whence = %d\n", fd, offset, whence);
    
    if (fd >= NFILES || fd < 0) {
        return -EBADF;
    }
    
    file_t *oneFileEntry = fget(fd);
    if (oneFileEntry == NULL) {
        dbg(DBG_PRINT,"ERROR!!! Not a valid fd\n");
        return -EBADF;
    }
    
    /* Corrected for kernel 3 */
    /* fput(oneFileEntry); */
    
    if(!(whence == SEEK_SET || whence == SEEK_CUR || whence == SEEK_END)){
        dbg(DBG_PRINT,"ERROR!!! whence is not correct\n");
        
        /* Corrected for kernel 3 */
        fput(oneFileEntry);
        
        return -EINVAL;
    }
    
    off_t retPos;
    if (whence == SEEK_SET && offset >= 0) {
        oneFileEntry->f_pos = offset;
        
        /* Corrected for kernel 3 */
        retPos = oneFileEntry->f_pos;
        fput(oneFileEntry); 
        return retPos;
    } else if (whence == SEEK_CUR && oneFileEntry->f_pos + offset >= 0) {
        oneFileEntry->f_pos += offset;

        /* Corrected for kernel 3 */
        retPos = oneFileEntry->f_pos;
        fput(oneFileEntry);
        return retPos;
    } else if (whence == SEEK_END && oneFileEntry->f_vnode->vn_len + offset >= 0) {
        oneFileEntry->f_pos = oneFileEntry->f_vnode->vn_len + offset;
        
        /* Corrected for kernel 3 */
        retPos = oneFileEntry->f_pos;
        fput(oneFileEntry);
        return retPos;
    }
    
    /* Corrected for kernel 3 */
    fput(oneFileEntry);
    
    /* reach here means the resulting file offset is negative */
    dbg(DBG_PRINT,"ERROR!!! The resulting file offset is negative!\n");
    return -EINVAL;
}

/*
 * Find the vnode associated with the path, and call the stat() vnode operation.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        A component of path does not exist.
 *      o ENOTDIR
 *        A component of the path prefix of path is not a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_stat(const char *path, struct stat *buf)
{
    /* NOT_YET_IMPLEMENTED("VFS: do_stat"); */
    
    if (path == NULL) {
        return -EINVAL;
    }
    
    vnode_t *result;
    int ret;
    ret = open_namev(path, 0, &result, NULL);
    if (ret){
        return ret;
    }
    
    /* Corrected for kernel 3 */
    /* vput(result); */
    
    /* grading guideline required */
    KASSERT(result->vn_ops->stat);
    dbg(DBG_PRINT,"(GRADING2A 3.f) The corresponding vnode has stat function. \n");
    
    /* Corrected for kernel 3 */
    int retVal = result->vn_ops->stat(result, buf);
    vput(result);
    return retVal;
}

#ifdef __MOUNTING__
/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutely sure your Weenix is perfect.
 *
 * This is the syscall entry point into vfs for mounting. You will need to
 * create the fs_t struct and populate its fs_dev and fs_type fields before
 * calling vfs's mountfunc(). mountfunc() will use the fields you populated
 * in order to determine which underlying filesystem's mount function should
 * be run, then it will finish setting up the fs_t struct. At this point you
 * have a fully functioning file system, however it is not mounted on the
 * virtual file system, you will need to call vfs_mount to do this.
 *
 * There are lots of things which can go wrong here. Make sure you have good
 * error handling. Remember the fs_dev and fs_type buffers have limited size
 * so you should not write arbitrary length strings to them.
 */
int
do_mount(const char *source, const char *target, const char *type)
{
    NOT_YET_IMPLEMENTED("MOUNTING: do_mount");
    return -EINVAL;
}

/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutley sure your Weenix is perfect.
 *
 * This function delegates all of the real work to vfs_umount. You should not worry
 * about freeing the fs_t struct here, that is done in vfs_umount. All this function
 * does is figure out which file system to pass to vfs_umount and do good error
 * checking.
 */
int
do_umount(const char *target)
{
    NOT_YET_IMPLEMENTED("MOUNTING: do_umount");
    return -EINVAL;
}
#endif
