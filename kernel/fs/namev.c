#include "kernel.h"
#include "globals.h"
#include "types.h"
#include "errno.h"

#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"

#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"

/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function, but you may want to special case
 * "." and/or ".." here depnding on your implementation.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{
    
    /* grading guideline required */
    KASSERT(NULL != dir);
    dbg(DBG_PRINT,"(GRADING2A 2.a) The dir vnode is not NULL. \n");
    
    /* grading guideline required */
    KASSERT(NULL != name);
    dbg(DBG_PRINT,"(GRADING2A 2.a) The name for result is not NULL. \n");
    
    /* grading guideline required */
    KASSERT(NULL != result);
    dbg(DBG_PRINT,"(GRADING2A 2.a) The result vnode is not NULL. \n");
    
    
    dbg(DBG_PRINT,"lookup called with the below args:\nname = %s\nnamelen = %d\n",name, len);
    /* NOT_YET_IMPLEMENTED("VFS: lookup"); */
    
    if(dir->vn_ops->lookup == NULL || dir->vn_mode != S_IFDIR) {
        dbg(DBG_PRINT,"The lookup failed because the vnode passed is not a directory\n");
        return -ENOTDIR;
    }
    
    /* ATTENTION! We don't handle special case . or .. here. */
    /* Comment implies the '.' and '..' are handled by vnode's implementation specific lookup()*/
    
    /* result's refcount will be incremented here, by the specific lookup() */
    /* So we know, if there is any error returned, the refcount isn't incremented! */
    return dir->vn_ops->lookup(dir, name, len, result);
}


/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */
int
dir_namev(const char *pathname, size_t *namelen, const char **name,
          vnode_t *base, vnode_t **res_vnode)
{
    /* NOT_YET_IMPLEMENTED("VFS: dir_namev"); */
    
    /* grading guideline required */
    KASSERT(NULL != pathname);
    dbg(DBG_PRINT,"(GRADING2A 2.b) The pathname is not NULL. \n");
    
    /* grading guideline required */
    KASSERT(NULL != namelen);
    dbg(DBG_PRINT,"(GRADING2A 2.b) The namelen is not NULL. \n");
    
    /* grading guideline required */
    KASSERT(NULL != name);
    dbg(DBG_PRINT,"(GRADING2A 2.b) The name is not NULL. \n");
    
    /* grading guideline required */
    KASSERT(NULL != res_vnode);
    dbg(DBG_PRINT,"(GRADING2A 2.b) The res_vnode is not NULL. \n");
    
    
    /* local variables */
    vnode_t *curPathBase = NULL;
    vnode_t *nextPathBase = NULL;
    /*Fix my Manan - pathnameCopy changed from char pointer to char array.*/
    char pathnameCopy[MAXPATHLEN + 1];
    char *curComName = NULL;
    char *nextComName = NULL;
    int singleFilename = 1;
    int needToDecrease = 0;
    
    /* make sure pathname is not NULL */
    if (pathname == NULL) {
        dbg(DBG_PRINT,"The path name is null\n");
        return -EINVAL;
    }
    
    /* check the length of the whole pathname is OK */
    size_t pathnameLen = strnlen(pathname, MAXPATHLEN + 1);
    dbg(DBG_PRINT,"The pathname length is %d\n", (int)pathnameLen);
    if (pathnameLen > MAXPATHLEN || pathnameLen <= 0) {
        dbg(DBG_PRINT,"The pathname is too long\n");
        return -EINVAL;
    }
    
    /* copy the pathname to pathnameCopy, get ready to process pathname */
    strncpy(pathnameCopy, pathname, pathnameLen);
    
    /* get the start position of pathnameCopy */
    char *startPoint = pathnameCopy;

    /*Fix by Manan - Making the last character as '\0'*/
    pathnameCopy[pathnameLen] = '\0';
    
    /* set the correct curPathBase */
    if (pathnameCopy[0] == '/') {
        dbg(DBG_PRINT,"pathname starts with a '/'. So ignore *base\n");
        curPathBase = vfs_root_vn;
    } else {
        if (base != NULL) {
            dbg(DBG_PRINT,"pathname does not start with a '/' and base != NULL. So *base is used.\n");
            curPathBase = base;
        } else {
            dbg(DBG_PRINT,"pathname does not start with a '/' and base == NULL. So base = curproc->p_cwd.\n");
            curPathBase = curproc->p_cwd;
        }
    }
    
    int allAreSlashes = 1;
    char *runner = pathnameCopy;
    while (*runner != '\0') {
        if (*runner != '/') {
            allAreSlashes = 0;
            break;
        }
        /*Fix by Manan - runner was never incrementing... hence hung
         * here inifinitely earlier*/
        runner++;
    }
    
    /* if all are slashes, we need to handle root case */
    if (allAreSlashes) {
        dbg(DBG_PRINT,"All characters of pathname = %s are slashes\n", pathnameCopy);
        vref(vfs_root_vn);
        *res_vnode = vfs_root_vn;
        /* KZTODO: not sure what should be set here, null or '/'? But our implementation should work.*/
        *name = NULL;
        *namelen = 0;
        return 0;
    }
    
    curComName = pathnameCopy;
    
    /* locate the first component name */
    while (*curComName == '/') {
        curComName++;
    }
    
    /* delete all the tail '/'s */
    char *end = pathnameCopy + pathnameLen - 1;
    while (*end == '/') {
        *end = '\0';
        end--;
    }
    dbg(DBG_PRINT,"All trailing slashes removed\n");
    dbg(DBG_PRINT,"Now pathnameCopy = %s\n", pathnameCopy);

    
    nextComName = curComName;
    while (*nextComName != '/' && *nextComName != '\0') {
        nextComName++;
    }
    
    while (*nextComName != '\0') {
        /* locate the next component name */
        while (*nextComName == '/') {
            *nextComName = '\0';
            nextComName++;
        }
        
        size_t comNameLen = strnlen(curComName, NAME_LEN + 1);
        if (comNameLen > NAME_LEN) {
            return -ENAMETOOLONG;
        }
    
        dbg(DBG_PRINT,"Object of name %s of length %d\n", curComName, comNameLen);
        int lookupResult = lookup(curPathBase, curComName, comNameLen, &nextPathBase);
        
        /* TODO: may need to change place */
        if (needToDecrease) {
            vput(curPathBase);
        }
        
        if (lookupResult) {
            return lookupResult;
        }
        
        curPathBase = nextPathBase;
        curComName = nextComName;
        while (*nextComName != '/' && *nextComName != '\0') {
            nextComName++;
        }
        
        needToDecrease = 1;
        singleFilename = 0;
    }
    
    size_t comNameLen = strnlen(curComName, NAME_LEN + 1);
    
    
    if (singleFilename) {
        vref(curPathBase);
    }
    
    if (comNameLen > NAME_LEN) {
        vput(curPathBase);
        return -ENAMETOOLONG;
    }
    
    /* grading guideline required */
    KASSERT(NULL != curPathBase);
    dbg(DBG_PRINT,"(GRADING2A 2.b) The corresponding vnode is not NULL. \n");
    
    *res_vnode = curPathBase;
    
    /* Now, we don't need to use kmalloc. */
    int offset = curComName - startPoint;
    *name = pathname + offset;
    *namelen = comNameLen;
    
    
    /* 
    char *resultName =(char *) kmalloc( strnlen(curComName, NAME_LEN + 1) * sizeof(char));
    strncpy(resultName, curComName, NAME_LEN + 1);
    *name = resultName;
    *namelen = strnlen(curComName, NAME_LEN + 1);
     */
    
    return 0;
}

/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fnctl.h>.  If the O_CREAT flag is specified, and the file does
 * not exist call create() in the parent directory vnode.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
    /* NOT_YET_IMPLEMENTED("VFS: open_namev"); */
    
    dbg(DBG_PRINT,"DEBUG - open_namev called with the below args:\npathname = %s, flag = 0x%x,\n", pathname, flag);
    
    size_t len;
    const char *name;
    vnode_t *parent;
    
    int retval = dir_namev(pathname, &len, &name, base, &parent);
    
    if(retval) {
        dbg(DBG_PRINT,"ERROR!!! Call to open_namev->dir_namev has returned the below.\n%s\n", strerror(-retval));
        return retval;
    }
    
    /* handle root case */
    if (name == NULL && len == 0) {
        *res_vnode = vfs_root_vn;
        /* Corrected: need to increment the refcounting of root vnode */
        vref(*res_vnode);
        vput(parent);
        return 0;
    }
    
    retval = lookup(parent, name, len, res_vnode);
    
    if (retval == -ENOENT && ((flag & O_CREAT) != 0x100)) {
        dbg(DBG_PRINT,"ERROR!!! Call to open_namev->lookup has returned -ENOENT and the flag does not have O_CREAT. So returning below\n%s\n", strerror(-retval));
        vput(parent);
        return -ENOENT;
    }
    
    if(retval == -ENOENT && (flag & O_CREAT)) {
        /* grading guideline required */
        KASSERT(NULL != parent->vn_ops->create);
        dbg(DBG_PRINT,"(GRADING2A 2.c) The corresponding vnode has create function. \n");
        
        retval = parent->vn_ops->create(parent, name, len, res_vnode);
        if (retval < 0){
        dbg(DBG_PRINT,"ERROR!!! Call to open_namev->parent->vn_ops->create has returned the below.\n%s\n", strerror(-retval));
        }
        vput(parent);
        return retval;
    }
    
    vput(parent);
    return retval;
}

#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
        return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
