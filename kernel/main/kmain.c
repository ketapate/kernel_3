#include "types.h"
#include "globals.h"
#include "kernel.h"

#include "util/gdb.h"
#include "util/init.h"
#include "util/debug.h"
#include "util/string.h"
#include "util/printf.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/pagetable.h"
#include "mm/pframe.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "main/acpi.h"
#include "main/apic.h"
#include "main/interrupt.h"
#include "main/cpuid.h"
#include "main/gdt.h"

#include "proc/sched.h"
#include "proc/proc.h"
#include "proc/kthread.h"

#include "drivers/dev.h"
#include "drivers/blockdev.h"
#include "drivers/tty/virtterm.h"

#include "api/exec.h"
#include "api/syscall.h"

#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/fcntl.h"
#include "fs/stat.h"

#include "test/kshell/kshell.h"

#define ECHILD          10      /* No child processes */

extern void *sunghan_test(int, void*);
extern void *sunghan_deadlock_test(int arg1, void *arg2);
extern void *testproc(int arg1, void *arg2);
extern void *test1(int arg1, void *arg2);
extern void *test2(int arg1, void *arg2);
extern void *test3(int arg1, void *arg2);
extern void createProcessAndThreads();
extern void testKillAllWhenRunning();

/* Adding tests for VFS*/
extern void test_open();
extern void test_write();
extern void test_dup();
extern void test_mkdir_rmdir();
extern void test_rename();
extern void test_mknod();
extern void eatmem_main();
/* Adding tests for VM*/
extern int vmtest_link_unlink();
/* Adding tests for VM*/
extern int vmtest_map_destory();

/* add vfstest_main() */
extern int *vfstest_main(int, char**);
int vfstest_main_2();

GDB_DEFINE_HOOK(boot)
GDB_DEFINE_HOOK(initialized)
GDB_DEFINE_HOOK(shutdown)

static void      *bootstrap(int arg1, void *arg2);
static void      *idleproc_run(int arg1, void *arg2);
static kthread_t *initproc_create(void);
static void      *initproc_run(int arg1, void *arg2);
static void       hard_shutdown(void);

static context_t bootstrap_context;

static int gdb_wait = GDBWAIT;
/**
 * This is the first real C function ever called. It performs a lot of
 * hardware-specific initialization, then creates a pseudo-context to
 * execute the bootstrap function in.
 */
void
kmain()
{
    GDB_CALL_HOOK(boot);
    
    dbg_init();
    dbgq(DBG_CORE, "Kernel binary:\n");
    dbgq(DBG_CORE, "  text: 0x%p-0x%p\n", &kernel_start_text, &kernel_end_text);
    dbgq(DBG_CORE, "  data: 0x%p-0x%p\n", &kernel_start_data, &kernel_end_data);
    dbgq(DBG_CORE, "  bss:  0x%p-0x%p\n", &kernel_start_bss, &kernel_end_bss);
    
    page_init();
    
    pt_init();
    slab_init();
    pframe_init();
    
    acpi_init();
    apic_init();
    intr_init();
    
    gdt_init();
    
    /* initialize slab allocators */
#ifdef __VM__
    anon_init();
    shadow_init();
#endif
    vmmap_init();
    proc_init();
    kthread_init();
    
#ifdef __DRIVERS__
    bytedev_init();
    blockdev_init();
#endif
    
    void *bstack = page_alloc();
    pagedir_t *bpdir = pt_get();
    KASSERT(NULL != bstack && "Ran out of memory while booting.");
	/* This little loop gives gdb a place to synch up with weenix.  In the
	 * past the weenix command started qemu was started with -S which
	 * allowed gdb to connect and start before the boot loader ran, but
	 * since then a bug has appeared where breakpoints fail if gdb connects
	 * before the boot loader runs.  See
	 *
	 * https://bugs.launchpad.net/qemu/+bug/526653
	 *
	 * This loop (along with an additional command in init.gdb setting
	 * gdb_wait to 0) sticks weenix at a known place so gdb can join a
	 * running weenix, set gdb_wait to zero  and catch the breakpoint in
	 * bootstrap below.  See Config.mk for how to set GDBWAIT correctly.
	 *
	 * DANGER: if GDBWAIT != 0, and gdb isn't run, this loop will never
	 * exit and weenix will not run.  Make SURE the GDBWAIT is set the way
	 * you expect.
	 */
    while (gdb_wait) ;
    context_setup(&bootstrap_context, bootstrap, 0, NULL, bstack, PAGE_SIZE, bpdir);
    context_make_active(&bootstrap_context);
    
    panic("\nReturned to kmain()!!!\n");
}

/**
 * This function is called from kmain, however it is not running in a
 * thread context yet. It should create the idle process which will
 * start executing idleproc_run() in a real thread context.  To start
 * executing in the new process's context call context_make_active(),
 * passing in the appropriate context. This function should _NOT_
 * return.
 *
 * Note: Don't forget to set curproc and curthr appropriately.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
bootstrap(int arg1, void *arg2)
{
    /* necessary to finalize page table information */
    pt_template_init();
    
    /* NOT_YET_IMPLEMENTED("PROCS: bootstrap");*/
    proc_t *idleProc = proc_create("idleProc");
    kthread_t *idleThr = kthread_create(idleProc, idleproc_run, 0, NULL);
    
    curproc = idleProc;
    
    /* grading guideline required */
    KASSERT(NULL != curproc);
    dbg(DBG_PRINT, "(GRADING1 1.a) The idle process has been created successfully\n");
    
    /* grading guideline required */
    KASSERT(PID_IDLE == curproc->p_pid);
    dbg(DBG_PRINT, "(GRADING1 1.a) What has been created is the idle process\n");
    
    curthr = idleThr;
    /* grading guideline required */
    KASSERT(NULL != curthr);
    dbg(DBG_PRINT, "(GRADING1 1.a) The thread for the idle process has been created successfully\n");
    
    context_make_active(&(idleThr->kt_ctx));
    
    panic("weenix returned to bootstrap()!!! BAD!!!\n");
    return NULL;
}

/**
 * Once we're inside of idleproc_run(), we are executing in the context of the
 * first process-- a real context, so we can finally begin running
 * meaningful code.
 *
 * This is the body of process 0. It should initialize all that we didn't
 * already initialize in kmain(), launch the init process (initproc_run),
 * wait for the init process to exit, then halt the machine.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
idleproc_run(int arg1, void *arg2)
{
    int status;
    pid_t child;
    
    /* create init proc */
    kthread_t *initthr = initproc_create();
    
    init_call_all();
    GDB_CALL_HOOK(initialized);
    
    /* Create other kernel threads (in order) */
    
#ifdef __VFS__
    /* Once you have VFS remember to set the current working directory
     * of the idle and init processes */
    
    curproc->p_cwd = vfs_root_vn;
    initthr->kt_proc->p_cwd = vfs_root_vn;
    
    /* KZTODO: vref once or twice? */
    vref(vfs_root_vn);
    vref(vfs_root_vn);
    
    /* Here you need to make the null, zero, and tty devices using mknod */
    /* You can't do this until you have VFS, check the include/drivers/dev.h
     * file for macros with the device ID's you will need to pass to mknod */
    
    /* NOT_YET_IMPLEMENTED("VFS: idleproc_run"); */
    if (do_mkdir("/dev")) {
        dbg(DBG_PRINT,"Make directory: /dev failed!");
    }
    if (do_mknod("/dev/null", S_IFCHR, MEM_NULL_DEVID)) {
        dbg(DBG_PRINT,"do_mknod null device failed!");
    }
    if (do_mknod("/dev/zero", S_IFCHR, MEM_ZERO_DEVID)) {
        dbg(DBG_PRINT,"do_mknod zero device failed!");
    }
    if (do_mknod("/dev/tty0", S_IFCHR, MKDEVID(2, 0))) {
        dbg(DBG_PRINT,"do_mknod tty0 device failed!");
    }
    
#endif
    
    /* Finally, enable interrupts (we want to make sure interrupts
     * are enabled AFTER all drivers are initialized) */

    intr_enable();
    
    /* Run initproc */
    sched_make_runnable(initthr);
    /* Now wait for it */
    child = do_waitpid(-1, 0, &status);
    KASSERT(PID_INIT == child);
    
#ifdef __MTP__
    kthread_reapd_shutdown();
#endif
    
    
#ifdef __VFS__
    /* Shutdown the vfs: */
    dbg_print("weenix: vfs shutdown...\n");
    vput(curproc->p_cwd);
    if (vfs_shutdown())
        panic("vfs shutdown FAILED!!\n");
    
#endif
    
    /* Shutdown the pframe system */
#ifdef __S5FS__
    pframe_shutdown();
#endif
    
    dbg_print("\nweenix: halted cleanly!\n");
    GDB_CALL_HOOK(shutdown);
    hard_shutdown();
    return NULL;
}

/**
 * This function, called by the idle process (within 'idleproc_run'), creates the
 * process commonly refered to as the "init" process, which should have PID 1.
 *
 * The init process should contain a thread which begins execution in
 * initproc_run().
 *
 * @return a pointer to a newly created thread which will execute
 * initproc_run when it begins executing
 */
static kthread_t *
initproc_create(void)
{
    /* NOT_YET_IMPLEMENTED("PROCS: initproc_create");*/
    
    proc_t *initProc = proc_create("initProc");
    /* grading guideline required */
    KASSERT(NULL != initProc);
    dbg(DBG_PRINT, "(GRADING1 1.b) The init process has been created successfully\n");
    
    /* grading guideline required */
    KASSERT(PID_INIT == initProc->p_pid);
    dbg(DBG_PRINT, "(GRADING1 1.b) The new created process is init process\n");
    
    kthread_t *initThr = kthread_create(initProc, initproc_run, 0, NULL);
    
    /* grading guideline required */
    KASSERT(initThr != NULL);
    dbg(DBG_PRINT, "(GRADING1 1.b) The thread for the init process has been created successfully\n");
    
    return initThr;
}

/**
 * The init thread's function changes depending on how far along your Weenix is
 * developed. Before VM/FI, you'll probably just want to have this run whatever
 * tests you've written (possibly in a new process). After VM/FI, you'll just
 * exec "/bin/init".
 *
 * Both arguments are unused.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
initproc_run(int arg1, void *arg2)
{
    /*NOT_YET_IMPLEMENTED("PROCS: initproc_run");*/
#ifdef __DRIVERS__

    char *argv[] = { NULL };
    char *envp[] = { NULL };
    /*char *argv1[] = {'a','b' cde fghi j};*/
    const char *argv1[3];
    argv1[0] = "abc";
    argv1[1] = "def";
    argv1[2] = "ghi";
    char *const *argv1_ptr = (char * const *)argv1;
#define SEGFAULT 0
#define HELLO 1
#define VFSTEST 2
#define UNAME 3
#define ARGS 4
#define INIT 5
#define FORKANDWAIT 6
#define KSHELL 7

    int test = INIT;
    switch (test){
        case SEGFAULT :
            kernel_execve("/usr/bin/segfault", argv, envp);
            break;
        case HELLO :
            kernel_execve("/usr/bin/hello", argv, envp);
            break;
        case VFSTEST :
            kernel_execve("/usr/bin/vfstest", argv, envp);
            break;
        case UNAME :
            kernel_execve("/bin/uname", argv, envp);
            break;
        case ARGS :
            kernel_execve("/usr/bin/args", argv1_ptr, envp);
            break;
        case INIT :
            kernel_execve("/sbin/init", argv, envp);
            break;
        case FORKANDWAIT :
            kernel_execve("/usr/bin/fork-and-wait", argv, envp);
            break;
        case KSHELL :
            /**/    
            kshell_add_command("sunghan_test", (kshell_cmd_func_t)sunghan_test, "invoke sunghan_test() to print a message...");
            kshell_add_command("sunghan_deadlock", (kshell_cmd_func_t)sunghan_deadlock_test, "invoke sunghan_deadlock_test() to print a message...");
            kshell_add_command("testproc", (kshell_cmd_func_t)testproc, "invoke testproc() to print a message...");
            kshell_add_command("ProcessAndThreads", (kshell_cmd_func_t)createProcessAndThreads, "Creating processes and threads");
            kshell_add_command("proc_kill_all", (kshell_cmd_func_t)test1, "testing - proc_kill_all and other proc/thread functions");
            kshell_add_command("do_waitpid_test", (kshell_cmd_func_t)test2, "testing - do_waitpid");
            kshell_add_command("killAllWhenRunning", (kshell_cmd_func_t)testKillAllWhenRunning, "kill all processs when they are running");
            kshell_add_command("sched_switch", (kshell_cmd_func_t)test3, "Testing - testing sched_switch");
            
            /*Tests for VFS*/
            kshell_add_command("test_open", (kshell_cmd_func_t)test_open, "Test if a file opens with flag = 3...");
            kshell_add_command("test_write", (kshell_cmd_func_t)test_write, "Simple write test...");
            kshell_add_command("test_dup", (kshell_cmd_func_t)test_dup, "Test various failure cases for dup ...");
            kshell_add_command("test_mkdir_rmdir", (kshell_cmd_func_t)test_mkdir_rmdir, "Try mkdir and rmdir ...");
            kshell_add_command("test_rename", (kshell_cmd_func_t)test_rename, "Simple rename test to find bugs...");
            kshell_add_command("test_mknod", (kshell_cmd_func_t)test_mknod, "Test if mknod fails on existing file... ");
            kshell_add_command("vfstest", (kshell_cmd_func_t)vfstest_main_2, "vfstest_main starts...");
            kshell_add_command("vm_test_1", (kshell_cmd_func_t)vmtest_link_unlink, "Test for do_link(),do_unlink(),do_read(),do_write(), do_open(), do_close() starts...");
            kshell_add_command("vm_test_2", (kshell_cmd_func_t)vmtest_map_destory, "Test for vmmap_create(),vmmap_insert(),vmmap_find_range(), vmmap_destory() starts...");

            
            
            kshell_t *kshell = kshell_create(0);
            if (NULL == kshell) panic("init: Couldn't create kernel shell\n");
            while (kshell_execute_next(kshell));
            kshell_destroy(kshell);
        
    
    break;
    }
        
    
#endif /* __DRIVERS__ */
    
    return NULL;
}



/* Tests for VM**********************************/




/* Tests for VM*/


int vmtest_map_destory(){
    vmmap_t *newMap = vmmap_create();
    dbg(DBG_PRINT, "(GRADING3E) vmmap_create success!\n");
    vmarea_t *newVma;
    int ret;
    ret = vmmap_map(newMap, NULL, 0, 1, 7,2, 0, 2, &newVma);
    if(ret < 0){
        dbg(DBG_PRINT, "(GRADING3E) vmmap_map fails!\n");
        return ret;
    }else{
        dbg(DBG_PRINT, "(GRADING3E) vmmap_map success!\n");
    }
    vmmap_destroy(newMap);
    dbg(DBG_PRINT, "(GRADING3E) map_destroy passed!\n");
    dbg(DBG_PRINT, "\n\n**************************** TEST PASSED ****************************\n\n");
    return 0;
}
/* Tests for VM**********************************/

/*************************VM tests**************************/
#define VM_STR  "Line-1 in the file for VM test " \
    " Line-2 in the file for VM test"
int vmtest_link_unlink(){
    int fd1,fd2,ret;
    void * addr;
    
    fd1 = do_open("test_file_1", O_RDWR | O_CREAT);    
    ret = do_link("test_file_1","test_file_2");
    
    ret = do_write(fd1, VM_STR, strlen(VM_STR));
    ret = do_close(fd1);
    if(ret < 0){
        dbg(DBG_PRINT,"(GRADING3E) Test for do_link(), do_unlink(), do_read(), do_write, do_open() and do_close() failed\n");    
        return ret;
    }

    fd2 = do_open("test_file_2", O_RDWR);
    char char_contents[PAGE_SIZE];
    ret = do_read(fd2, char_contents, strlen(VM_STR));
    if((unsigned)ret != strlen(VM_STR)){
        dbg(DBG_PRINT,"(GRADING3E) Test for do_link(), do_unlink(), do_read(), do_write, do_open() and do_close() failed\n");    
        return -1;
    }
   
    ret = do_close(fd2);
    if(ret < 0){        
        dbg(DBG_PRINT,"(GRADING3E) Test for do_link(), do_unlink(), do_read(), do_write, do_open() and do_close() failed\n");       
        return ret;    
    }

    ret = do_unlink("test_file_2");
    if(ret < 0){        
        dbg(DBG_PRINT,"(GRADING3E) Test for do_link(), do_unlink(), do_read(), do_write, do_open() and do_close() failed\n");              
        return ret;    
    }

    fd2 = do_open("test_file_2", O_RDWR);
    if(fd2 > 0){
        dbg(DBG_PRINT,"(GRADING3E) Test for do_link(), do_unlink(), do_read(), do_write, do_open() and do_close() failed\n");                     
        return -1;
    }
    
    dbg(DBG_PRINT,"(GRADING3E) Test for do_link(), do_unlink(), do_read(), do_write, do_open() and do_close() PASSED\n");              
    dbg(DBG_PRINT, "\n\n**************************** TEST PASSED ****************************\n\n");
    return ret;
}
/*************************VM tests************************/


/*************************VFS tests************************/

int vfstest_main_2(){
    vfstest_main(1, NULL);        
    return 0;
}


/*************************simple VFS tests************************/
/* Global variables for VFS test*/
static char vfs_root_dir[64];


void test_open(){
    dbg(DBG_PRINT, "Entered test_open\n");
    int fd1;
    fd1 = do_open("/file1", 0x3);
    if (fd1 < 0 ){
        dbg(DBG_PRINT, "\n\n**************************** TEST PASSED ****************************\n\n");
    } else {
        dbg(DBG_PRINT, "\n\nxxxxxxxxxxxxxxxxxxxxxxxxxxxx TEST FAILED xxxxxxxxxxxxxxxxxxxxxxxxxxxx\n\n");
    }
}
void test_write(){
    dbg(DBG_PRINT, "Entered write test\n");
    sprintf(vfs_root_dir, "vfstest_dir");
    do_mkdir(vfs_root_dir);
    int retval;
    int badFd = -10;
    
    retval = do_read(badFd, "write_test", 9);
    if(retval){
        dbg(DBG_PRINT, "Can not read, the given file decriptor is %d\n",  badFd);
    } else {
        dbg(DBG_PRINT, "\n\nxxxxxxxxxxxxxxxxxxxxxxxxxxxx TEST FAILED xxxxxxxxxxxxxxxxxxxxxxxxxxxx\n\n");
    }
    
    int fd = do_open("/vfstest_dir/read", O_CREAT | O_WRONLY);
    do_close(fd);
    fd = do_open("/vfstest_dir/read", O_RDWR);
    dbg(DBG_PRINT, "Right before do_write\n");
    retval = do_write(fd, "write_test", 10);
    if(retval){
        dbg(DBG_PRINT, "Have writen write_test'\n");
    } else {
        dbg(DBG_PRINT, "\n\nxxxxxxxxxxxxxxxxxxxxxxxxxxxx TEST FAILED xxxxxxxxxxxxxxxxxxxxxxxxxxxx\n\n");
    }
    do_close(fd);
    do_rmdir(vfs_root_dir);
    dbg(DBG_PRINT, "\n\n**************************** TEST PASSED ****************************\n\n");
}

void test_rename(){
    dbg(DBG_PRINT, "Entered test_rename\n");
    sprintf(vfs_root_dir, "/vfstest_dir");
    do_mkdir(vfs_root_dir);
    dbg(DBG_PRINT, "Created /vfstest_dir\n");

    int retval = do_chdir("/vfstest_dir");
    if(retval){
        dbg(DBG_PRINT, "Unable to change the directory\n");
    }
    else{
        dbg(DBG_PRINT, "Changed to the directory \"vfstest_dir\"\n");
    }

    int fd = do_open("/abc", O_CREAT);
    retval = do_rename("/abc","/ABC");
    if(retval){
        dbg(DBG_PRINT, "Unable to rename the file\n");
    }
    else{
        dbg(DBG_PRINT, "file \"abc\" is renamed to \"ABC\"\n");
    }
    

    retval = do_chdir("/");
    if(retval){
        dbg(DBG_PRINT, "Unable to change back to the root directory\n");
    }
    else{
        dbg(DBG_PRINT, "Changed to the root directory\n");
    }

    do_rmdir(vfs_root_dir);
    dbg(DBG_PRINT, "Removed test root directory: %s\n", vfs_root_dir);
    dbg(DBG_PRINT, "\n\n**************************** TEST PASSED ****************************\n\n");

}

void test_dup(){
    dbg(DBG_PRINT, "Entered test_dup\n");
    int fd1;
    int fd2;
    fd1 = do_dup(50);
    if (fd1 >= 0) {
        dbg(DBG_PRINT, "\n\nxxxxxxxxxxxxxxxxxxxxxxxxxxxx TEST FAILED xxxxxxxxxxxxxxxxxxxxxxxxxxxx\n\n");
    }
    fd1 = do_dup(-5);
    if (fd1 >= 0) {
        dbg(DBG_PRINT, "\n\nxxxxxxxxxxxxxxxxxxxxxxxxxxxx TEST FAILED xxxxxxxxxxxxxxxxxxxxxxxxxxxx\n\n");
    }
    fd2 = do_open("/file1",O_RDONLY | O_CREAT);
    fd1 = do_dup(fd2 + 1);
    if (fd1 >= 0) {
        dbg(DBG_PRINT, "\n\nxxxxxxxxxxxxxxxxxxxxxxxxxxxx TEST FAILED xxxxxxxxxxxxxxxxxxxxxxxxxxxx\n\n");
    }
    do_close(fd2);
    
    dbg(DBG_PRINT, "\n\n**************************** TEST PASSED ****************************\n\n");

}
void test_mkdir_rmdir(){
    dbg(DBG_PRINT, "Entered test_mkdir\n");
    do_mkdir ("/a");
    do_mkdir ("/a/b");
    do_rmdir("/a/b");
    do_rmdir("/a");
    dbg(DBG_PRINT, "\n\n**************************** TEST PASSED ****************************\n\n");
}

void test_mknod(){
    dbg(DBG_PRINT, "Entered test_rename\n");
    int fd1;
    int fd2;
    fd1 = do_open("/file1", O_RDONLY | O_CREAT);
    fd2 = do_mknod("/file1", S_IFCHR, MEM_NULL_DEVID);
    if (fd2 >= 0) {
        dbg(DBG_PRINT, "\n\nxxxxxxxxxxxxxxxxxxxxxxxxxxxx TEST FAILED xxxxxxxxxxxxxxxxxxxxxxxxxxxx\n\n");
    }
    do_close(fd1);
    
    dbg(DBG_PRINT, "\n\n**************************** TEST PASSED ****************************\n\n");
}
/*************************simple VFS tests************************/





/*************************Ziyu's test************************/
/* our own test cases */
/*************************Ziyu's test************************/
typedef struct {
    struct proc *p;
    struct kthread *t;
} proc_thread_t;

static void start_proc(proc_thread_t *ppt, char *name, kthread_func_t f, int arg1) {
    proc_thread_t pt;
    
    pt.p = proc_create(name);
    pt.t = kthread_create(pt.p, f, arg1, NULL);
    KASSERT(pt.p && pt.t && "Cannot create thread or process");
    sched_make_runnable(pt.t);
    if (ppt != NULL) {
        memcpy(ppt, &pt, sizeof(proc_thread_t));
    }
}


void *
normalTest(){
    dbg_print("This is process %d running", curproc->p_pid);
    return NULL;
}
void
createProcessAndThreads(){
    int i = 0;
    dbg_print("Processes and Thread Creation test");
    for (i = 0;  i < 10 ; i++) {
        start_proc(NULL, "normal test", normalTest, NULL);
    }
    sched_make_runnable(curthr);
    sched_switch();
}

/*************************Ziyu's test************************/

/*   kshell_add_command("killAllWhenRunning", (kshell_cmd_func_t)testKillAllWhenRunning, "To kill all the process when they are running"); */

void

increment(long arg1, void *arg2){
    
    while (1) {
        
        arg1++;
        
        if(arg1 > 5) {
            
            dbg_print("Done with thread work and exiting\n");
            do_exit(1);
            
        }
        sched_make_runnable(curthr);
        sched_switch();
        
        if (curproc->p_pid == 1) {
            
            sched_switch();
            
        }
        
    }
    
}

void

testKillAllWhenRunning() {
    dbg_print("Test to Kill All When Running\n");
    
    
    
    long count = 0;
    
    proc_t *process1 = proc_create("p1");
    
    kthread_t *thread1 = kthread_create(process1, (kthread_func_t)increment, count, NULL);
    
    sched_make_runnable(thread1);
    
    
    proc_t *process2 = proc_create("p1");
    
    kthread_t *thread2 = kthread_create(process2, (kthread_func_t)increment, count,NULL);
    
    sched_make_runnable(thread2);
    
    
    
    proc_t *process3 = proc_create("p1");
    
    kthread_t *thread3 = kthread_create(process3, (kthread_func_t)increment, count,NULL);
    
    sched_make_runnable(thread3);
    
    sched_make_runnable(curthr);
    sched_switch();
    proc_kill_all();
    
    
    
}




/*************************Keta's test************************/

void* p1_run(int arg1, void* arg2){
    dbg_print("Process_1 running has PID:%d",curproc->p_pid);
    return NULL;
}
void* p2_run(int arg1, void* arg2){
    dbg_print("Process_2 running has PID:%d",curproc->p_pid);
    return NULL;
}
void* p3_run(int arg1, void* arg2){
    dbg_print("Process_3 running has PID:%d",curproc->p_pid);
    return NULL;
}


void *test1(int arg1, void *arg2){
    /* creating 3 new processes using proc_create() and
     * 1 thread for each process using kthread_create()
     */
    
    /* Process 1 */
    proc_t* proc_1 = proc_create("proc_1");
    KASSERT((proc_1 != NULL)&&(proc_1->p_pid != PID_IDLE)&&(proc_1->p_pid != PID_INIT));
    dbg_print("Process 1 is created. It is neither Idle Process nor Init Process");
    
    kthread_t *thr_1 = kthread_create(proc_1, (kthread_func_t)p1_run, 0, NULL);
    KASSERT(thr_1 != NULL);
    dbg_print("Thread for Process 1 has been created\n");
    sched_make_runnable(thr_1);
    dbg_print("It's added to the run queue\n");
    
    
    /* Process 2 */
    proc_t* proc_2 = proc_create("proc_2");
    KASSERT((proc_2 != NULL)&&(proc_2->p_pid != PID_IDLE)&&(proc_2->p_pid != PID_INIT));
    dbg_print("Process 2 is created. It is neither Idle Process nor Init Process");
    
    kthread_t *thr_2 = kthread_create(proc_2, (kthread_func_t)p2_run, 0, NULL);
    KASSERT(thr_2 != NULL);
    dbg_print("Thread for Process 2 has been created\n");
    sched_make_runnable(thr_2);
    dbg_print("It's added to the run queue\n");
    
    
    /* Process 3 */
    proc_t* proc_3 = proc_create("proc_3");
    KASSERT((proc_3 != NULL)&&(proc_3->p_pid != PID_IDLE)&&(proc_3->p_pid != PID_INIT));
    dbg_print("Process 3 is created. It is neither Idle Process nor Init Process");
    
    kthread_t *thr_3 = kthread_create(proc_3, (kthread_func_t)p3_run, 0, NULL);
    KASSERT(thr_3 != NULL);
    dbg_print("Thread for Process 3 has been created\n");
    sched_make_runnable(thr_3);
    dbg_print("It's added to the run queue\n");
    
    
    
    sched_make_runnable(curthr);
    sched_switch();
    dbg_print("Entering proc_kill_all() now\nIf all processes are killed!!!\n");
    proc_kill_all();
    dbg_print("All but init and idle processes killed. Test exited!!!!\n");
    
    
    /*while(do_waitpid(-1, 0, 5) != -ECHILD);
     dbg_print("Shouldn't display this line of proc_kill_all() works fine!!"); */
    
    
    
    return NULL;
}

/* waitpid test*/
/*
 void* proc_1_run(int arg1, void* arg2);
 void* proc_2_run(int arg1, void* arg2);
 void* proc_3_run(int arg1, void* arg2);*/
void* proc_4_run(int arg1, void* arg2);

proc_t* proc_4;
proc_t* proc_1;
kthread_t* thr_4;


void* proc_1_run(int arg1, void* arg2){
    dbg_print("Process_1 running has PID:%d",curproc->p_pid);
    
    proc_4 = proc_create("proc_4");
    KASSERT((proc_4 != NULL)&&(proc_4->p_pid != PID_IDLE)&&(proc_4->p_pid != PID_INIT));
    dbg_print("Process 4 is created. It is neither Idle Process nor Init Process\n");
    
    thr_4 = kthread_create(proc_4, (kthread_func_t)proc_4_run, 0, NULL);
    KASSERT(thr_4 != NULL);
    dbg_print("Thread for Process 4 has been created\n");
    sched_make_runnable(thr_4);
    dbg_print("It's added to the run queue\n");
    
    
    return NULL;
}

void* proc_2_run(int arg1, void* arg2){
    dbg_print("Process_2 running has PID:%d",curproc->p_pid);
    return NULL;
}
void* proc_3_run(int arg1, void* arg2){
    dbg_print("Process_3 running has PID:%d",curproc->p_pid);
    return NULL;
}
void* proc_4_run(int arg1, void* arg2){
    dbg_print("Process_4 running has PID:%d",curproc->p_pid);
    return NULL;
}



void *test2(int arg1, void* arg2){
    /* Process 1 */
    proc_1 = proc_create("proc_1");
    KASSERT((proc_1 != NULL)&&(proc_1->p_pid != PID_IDLE)&&(proc_1->p_pid != PID_INIT));
    dbg_print("Process 1 is created. It is neither Idle Process nor Init Process\n");
    
    kthread_t *thr_1 = kthread_create(proc_1, (kthread_func_t)proc_1_run, 0, NULL);
    KASSERT(thr_1 != NULL);
    dbg_print("Thread for Process 1 has been created\n");
    sched_make_runnable(thr_1);
    dbg_print("It's added to the run queue\n");
    
    
    /* Process 2 */
    proc_t* proc_2 = proc_create("proc_2");
    KASSERT((proc_2 != NULL)&&(proc_2->p_pid != PID_IDLE)&&(proc_2->p_pid != PID_INIT));
    dbg_print("Process 2 is created. It is neither Idle Process nor Init Process\n");
    
    kthread_t *thr_2 = kthread_create(proc_2, (kthread_func_t)proc_2_run, 0, NULL);
    KASSERT(thr_2 != NULL);
    dbg_print("Thread for Process 2 has been created\n");
    sched_make_runnable(thr_2);
    dbg_print("It's added to the run queue\n");
    
    
    /* Process 3 */
    proc_t* proc_3 = proc_create("proc_3");
    KASSERT((proc_3 != NULL)&&(proc_3->p_pid != PID_IDLE)&&(proc_3->p_pid != PID_INIT));
    dbg_print("Process 3 is created. It is neither Idle Process nor Init Process\n");
    
    kthread_t *thr_3 = kthread_create(proc_3, (kthread_func_t)proc_3_run, 0, NULL);
    KASSERT(thr_3 != NULL);
    dbg_print("Thread for Process 3 has been created\n");
    sched_make_runnable(thr_3);
    dbg_print("It's added to the run queue\n");
    
    
    pid_t child;
    int status;
    
    
    /* dbg_print("Waiting for Process 1 to exit\n");*/
    dbg_print("Waiting while all children are running.......\n");
    child = do_waitpid(-1, 0, &status);
    dbg_print("The process with PID:%d has exited with status:%d\n", child, status);
    
    /* waiting for a particular process */
    dbg_print("Waiting for Process 2 to exit\n");
    child = do_waitpid(proc_2->p_pid, 0, &status);
    dbg_print("The process with PID:%d has exited with status:%d\n", child, status);
    
    
    /* pid is not a child of the current process */
    dbg_print("Waiting for random Process 24 (non-child process) to exit\n");
    child = do_waitpid(24, 0, &status);
    if(child==-10){
        dbg_print("Returning -ECHILD: The pid passed is not a child process of the current process\n");
    }
    else{
        dbg_print("The process with PID:%d has exited with status:%d\n", child, status);
    }
    
    
    /* waiting for remaining child processes to exit */
    dbg_print("Waiting for remaining child processes to exit\n");
    child = do_waitpid(-1, 0, &status);
    dbg_print("The process with PID:%d has exited with status:%d\n", child, status);
    
    
    dbg_print("Waiting for child processes of Process 1 to exit\n");
    child = do_waitpid(-1, 0, &status);
    if(child==-10){
        dbg_print("Returning -ECHILD: There no more child processes of the current process to exit\n");
    }
    else{
        dbg_print("The process with PID:%d has exited with status:%d\n", child, status);
    }
    
    
    dbg_print("All 4 newly created processes have exited cleanly\n");
    return NULL;
}

/* sched switch test*/
void* proc1_run(int arg1, void* arg2);
void* proc2_run(int arg1, void* arg2);
void* proc3_run(int arg1, void* arg2);



void* proc1_run(int arg1, void* arg2){
    dbg_print("ENTERING: Process_1 running has PID:%d\n",curproc->p_pid);
    dbg_print("Scheduling the switching of threads now...\n");
    sched_make_runnable(curthr);
    sched_switch();
    dbg_print("EXITING: Process_1 running has PID:%d\n",curproc->p_pid);
    
    return NULL;
}

void* proc2_run(int arg1, void* arg2){
    dbg_print("ENTERING: Process_2 running has PID:%d\n",curproc->p_pid);
    dbg_print("Scheduling the switching of threads now...\n");
    sched_make_runnable(curthr);
    sched_switch();
    dbg_print("EXITING: Process_2 running has PID:%d\n",curproc->p_pid);
    return NULL;
}

void* proc3_run(int arg1, void* arg2){
    dbg_print("ENTERING: Process_3 running has PID:%d\n",curproc->p_pid);
    dbg_print("Scheduling the switching of threads now...\n");
    sched_make_runnable(curthr);
    sched_switch();
    dbg_print("EXITING: Process_3 running has PID:%d\n",curproc->p_pid);
    
    return NULL;
}



void* test3(int arg1, void* arg2){
    
    proc_t* proc_1 = proc_create("proc_1");
    KASSERT((proc_1 != NULL)&&(proc_1->p_pid != PID_IDLE)&&(proc_1->p_pid != PID_INIT));
    dbg_print("Process 1 is created. It is neither Idle Process nor Init Process\n");
    
    kthread_t *thr_1 = kthread_create(proc_1, (kthread_func_t)proc1_run, 0, NULL);
    KASSERT(thr_1 != NULL);
    dbg_print("Thread for Process 1 has been created\n");
    sched_make_runnable(thr_1);
    dbg_print("It's added to the run queue\n");
    
    
    proc_t* proc_2 = proc_create("proc_2");
    KASSERT((proc_2 != NULL)&&(proc_2->p_pid != PID_IDLE)&&(proc_2->p_pid != PID_INIT));
    dbg_print("Process 2 is created. It is neither Idle Process nor Init Process\n");
    
    kthread_t *thr_2 = kthread_create(proc_2, (kthread_func_t)proc2_run, 0, NULL);
    KASSERT(thr_2 != NULL);
    dbg_print("Thread for Process 2 has been created\n");
    sched_make_runnable(thr_2);
    dbg_print("It's added to the run queue\n");
    
    
    
    proc_t* proc_3 = proc_create("proc_3");
    KASSERT((proc_3 != NULL)&&(proc_3->p_pid != PID_IDLE)&&(proc_3->p_pid != PID_INIT));
    dbg_print("Process 3 is created. It is neither Idle Process nor Init Process\n");
    
    kthread_t *thr_3 = kthread_create(proc_3, (kthread_func_t)proc3_run, 0, NULL);
    KASSERT(thr_3 != NULL);
    dbg_print("Thread for Process 3 has been created\n");
    sched_make_runnable(thr_3);
    dbg_print("It's added to the run queue\n");
    
    int status;
    pid_t child=0;
    while(child!=-10){
        child = do_waitpid(-1, 0, &status);
        if(child!=-10){
            dbg_print("Process with pid:%d exited\n",child);
        }
    }
    
    dbg_print("All processes have exited cleanly now\n");
    return NULL;
}


/*************************Keta's test************************/


/**
 * Clears all interrupts and halts, meaning that we will never run
 * again.
 */
static void
hard_shutdown()
{
#ifdef __DRIVERS__
    vt_print_shutdown();
#endif
    __asm__ volatile("cli; hlt");
}
