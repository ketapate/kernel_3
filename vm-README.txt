Kernel3-vm-README

Team members:
Keta Patel
Manan Patel
Ziyu Ou
Kai Lu

Split the points:
25%, 25%, 25%, 25%

Justification:
Ziyu Ou, Kai Lu, Manan Patel and Keta Patel together built and debugged the OS.

How to create the executable for your assignment:
Type "make" to compile the OS, and type "./weenix -n" to run the OS. 

Any design decisions you made that is not in the spec:
No

Known bugs:
Based on the provided test cases and our own test cases, there is no bug so far.

Any deviation from the spec:
No

How to run our own tests cases:
To run our program without user space shell, please change int test = KSHELL on line 359 in main/kmain.c.
Note - All thebelow tests run on kshell and usershell!!


Additional note:
Hi grader, for part D, please set DYNAMIC=1 to run our program. We can get extra credit!
By default, the user shell is launched.


1) do_waitpid_test :	
tests the do_waitpid() function. All the boundary conditions are tested here. 
- Whether the processes that is waited up is a specific process or a random one. 
- Whether the process that is waited upon is a child of the current process.
- Whether the current process has any more child processes remaining.
Four processes are created in total, 3 are children of INIT process and one is child of one of the child processes. The conditions for any child exiting and a specific child exiting are tested first. This is followed by testing for the 4th non-chip process to exit, which returns -ECHILD. This is followed by waiting for the 3rd child process of INIT to exit. Now the INIT process doesn't have any child process. The condition for any child of the current process is tested now.  

2) sched_switch :	
tests the sched_switch() function. 3 processes are created and switched within their run functions. The statements show how the various switches occur.

3) proc_kill_all : 	
tests the proc_kill_all() function. Similar to the above cases, 3 processes are created and made runnable. Then the proc_kill_all() function is called. All the newly created processes except the IDLE process and children of the IDLE process are killed. The control returns back to the kshell which being the INIT process is not killed. 

4) ProcessAndThreads:	
test the proc_create(char *name) and kthread_cancel(kthread_t *kthr, void *retval) functions. 10 more processes was created and make them runnable and let them run and exit naturally.

5) KillAllWhenRunning: 	
test the proc_kill_all() when the processes is running. 3 processes and 3 threads are created and running and when meets some requirement, we call proc_kill_all() to kill them all. 

6) test_open:
Test if a file opens with flag = 3 ...

7) test_write 
Try to open a file with fd = -10. Try to write 10 bytes to a file.

8) test_dup 
Test various failure cases for dup. Try duplicating a negative fd. Try duplicating a positive non-existant fd.

9) test_mkdir_rmdir
Create two directories and remove them.

10) test_rename
Rename one file to another name.

11) test_mknod
Test if mknod fails when tried on an existing file.

12) vm_test_1
The test makes use of do_link(), do_unlink(), do_read(), do_write(), do_read() and do_write() functions.
We check whether one file is linked to another via do_link() function. For this, we first create a new file in read-write mode and link that file with another new file. We then write into the first file and to see if the linking is proper or not, we read from the linked file. If the number of read bytes is the same as that of the written bytes, then our do_link() function works as desired. We then unlink the new file from the original file and try to read from it again. This time since the file is unlinked, it cannot be opened and the test must return an invalid file descriptor. Once we reach here, we can conclude that our do_unlink() along with do_read(), do_write(), do_open() and do_close() functions work as desired.

13) vm_test_2
vm_test_2: this test is used for test functions of vmmap_create(), vmmap_insert(), vmmap_map(), vmmap_find_range(), vmmap_destroy(). First create a new map. Then use vmmap_map() function to map one vm area given lopage = 0, which will calls the vmmap_find_range() and vmmap_insert() to find a suitable area and insert in the given map. Finally, we destroy the map by calling vmmap_destroy().
