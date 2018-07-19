# PintOS 

Implementing a more efficient operating system for PintOS for CMPS 111 course.
Consists of three labs. 

## Lab 1: More Efficient Timer 

In the original Pintos, threads call timer_sleep(int64_t ticks) to put themselves to sleep.

The original implementation was inefficient because it calls thread_yield() in a loop until until enough time has passed.
Instead, changed the scheduler to implement a thread control block to avoid scheduling sleeping threads. 
This way, the thread is BLOCKED and will not  be scheduled until sleeping_wakeup_time

## Lab 2: Priority Based Thread Scheduling

Implemented priority based thread scheduling using a ready queue. 
Handled priority in concurrency primitives. 
Implemented priority donation between threads through concurrency primitives. 

## Lab 3: User Processes and System Calls 

Allow user process to run. 
Support argument passing to user processes
Implement system calls. 
