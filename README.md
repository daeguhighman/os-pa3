# 4190.307 Operating Systems (Fall 2024)
# Project #3: SNULE: A Simplified Nerdy ULE Scheduler
### Due: 11:59 PM, October 27 (Sunday)

## Introduction

Currently, the CPU scheduler of `xv6` uses a simple round-robin policy. The goal of this project is to understand the scheduling subsystem of `xv6` by implementing the simplified version of FreeBSD's ULE scheduler.

## The SNULE Scheduler

### The ULE Scheduler

The ULE scheduler is the default CPU scheduler in FreeBSD, designed to offer improved performance and scalability, particularly on multiprocessor systems. ULE implements a hybrid scheduling approach, combining aspects of both traditional priority-based scheduling and advanced features like load balancing and per-CPU run queues. It prioritizes threads based on their interactivity and CPU usage, dynamically adjusting priorities to favor interactive tasks while ensuring that CPU-bound tasks still get sufficient processing time. The scheduler is highly adaptive, using feedback mechanisms to tune its behavior for different workloads. In this project, we will develop a simplified version of the ULE scheduler, referred to as the SNULE scheduler. For more information on the ULE scheduler, please refer to the following papers.

[1] Jeff Roberson, "[ULE: A Modern Scheduler For FreeBSD](https://www.usenix.org/legacy/event/bsdcon03/tech/full_papers/roberson/roberson.pdf)," BSDCon, 2003. 

[2] Justinien Bouron et al., "[The Battle of the Schedulers: FreeBSD ULE vs. Linux CFS](https://www.usenix.org/system/files/conference/atc18/atc18-bouron.pdf)," USENIX ATC, 2018.

### Run Queues (RQs)

In SNULE, each CPU maintains two run queues (RQs): the current RQ and the next RQ. __Runnable__ processes are assigned to either the current RQ or the next RQ. A process is selected from the current RQ in priority order, and if it exhausts its time slice, it is moved to the next RQ. This continues until the current RQ is empty, at which point the current and next RQs are switched. This guarantees that each process will be given use of its slice once every queue switch, regardless of its priority. The running process is not linked to any of the RQs and is NOT preempted until it either voluntarily sleeps or exhausts its time slice. New processes created by `fork()` or those that are woken up from sleeping are placed into the current RQ to provide a low-latency response. 

### Priority

Similar to traditional UNIX-like operating systems, each process in SNULE is assigned a nice value (`p->nice`) ranging from -20 (`NICE_MIN`) to 19 (`NICE_MAX`), with a default value of zero. The nice value of a new process is inherited from its parent process. Based on this nice value and the process's interactivity, SNULE calculates and maintains an internal priority value (`p->prio`) that ranges from 80 to 139. If the interactivity of a process is not considered (as in Part 2), its priority is statically determined by the nice value. For example, a nice value of -20 corresponds to a priority value of 100, 0 to 120, and 19 to 139. In both cases, lower values indicate higher priority. 

In Part 3 of this project, we will take the process's _interactivity score_  into account, allowing the priority value to drop below 100, with a minimum of 80. If the priority is between 100 and 139, the process is called a _normal_ process, while processes with a priority below 100 are called _interactive_ processes.
Whenever a process is placed into the RQ, its priority is recalculated based on both its nice value and interactivity score. 

```
Priority range (80 - 139)
--------------------------------------------------------------------
 80 (PRIO_MIN_INTERACT) |  Interactive Process (Highest priority)
...                     |
 99 (PRIO_MAX_INTERACT) |  Interactive Process
100 (PRIO_MIN_NORMAL)   |  Normal Process (nice=-20)
...                     |
120                     |  Normal Process (nice=0, default)
...                     |
139 (PRIO_MAX_NORMAL)   |  Normal Process (nice=19, Lowest priority)
--------------------------------------------------------------------
```

### Time slice

SNULE bounds timeshare latency by reducing the time slice size as the system load increases. The _load_ is defined as the number of runnable processes in the current and next RQs, excluding the currently running process. When the load is high, i.e., `load` >= `SCHED_SLICE_MIN_DIVISOR` (default is 6), each process receives a minimum time slice of `SCHED_SLICE_MIN` (default is 1 tick). If the load is equal to or less than 1, processes receive a time slice of `SCHED_SLICE_DEFAULT` (default is 10 ticks). Otherwise, each process is allocated a time slice of (`SCHED_SLICE_DEFAULT` / `load`) ticks.
In Part 3 of this project, we modify this policy slightly by allocating only one tick to interactive processes.

On each timer tick, the SNULE scheduler recalculates the time slice based on the current `load` value.
If the total number of ticks used by the current process since it was scheduled is equal to or exceeds the recalculated time slice, the process is preempted and moved to the next RQ.

### Interactivity Score (For Part 3 only)

The interactivity scoring is central to the SNULE scheduler, playing a crucial role in enhancing user experience by ensuring fast responsiveness. In SNULE, the interactivity of a process is determined based on its voluntary sleep time and run time. 
Interactive processes typically have high sleep times as they wait for user input or I/O operations, followed by short bursts of CPU activity as they process those requests or results. 

The voluntary sleep time (`p->tick_sleep`) is the cumulative sum of all the ticks that have passed while the process was in the sleeping state, whereas the run time (`p->tick_run`) represents the total number of ticks the process spends actively running on the CPU. 
Whenever the sleep time or run time is updated, the SNULE scheduler checks if their sum exceeds `SCHED_SLP_RUN_MAX` (50 ticks or 5 seconds by default). If the sum does exceed this threshold, both values are halved. This approach gradually decays the sleep and run times over time, ensuring that the interactivity score continues to reflect the recent behavior of the process.
To minimize error during this decay process, SNULE maintains the sleep and run times in a fixed-point representation by shifting both tick counts left by `TICK_SHIFT` (default is 10) bits. This ensures greater precision when tracking sleep and run times throughout the decay process. 

An interactivity score is calculated based on the relationship between a process's sleep time and run time. The interactivity score ranges from 0 to `SCHED_INTERACT_MAX` (default is 50). If the sleep time exceeds the run time, the score is computed as the ratio of sleep time to run time, scaled to the interactivity score range from 0 to `SCHED_INTERACT_MAX`, as shown below. Let $T_s$ and $T_r$ represent the sleep time and run time of the process $p$, respectively. In all other cases, the interactivity score is simply set to `SCHED_INTERACT_MAX`, which does not influence the process's priority. 

|           |                                                   |                   |
| --------- | --------------------------------------------------| ----------------- |
| $is(p)$ = | $T_r$ / ($max$(1, $T_s$ / `SCHED_INTERACT_MAX`)) | if $T_s$ > $T_r$  |
|           | `SCHED_INTERACT_MAX`                             | otherwise         |

If the interactivity score of a process is less than `SCHED_INTERACT_THRESH` (default is 30), its priority is calculated using the formula below, with `PRIO_MIN_INTERACT` and `PRIO_INTERACT_RANGE` set to 80 and 20, respectively, by default. 
This ensures that the priorities of such processes are evenly distributed within the interactive range, from 80 to 99, according to their interactivity scores. 
For processes with interactivity scores above the threshold, their priority is determined solely by their nice values.

```C
// Only for interactive processes
p->prio = PRIO_MIN_INTERACT + 
          ((PRIO_INTERACT_RANGE * is(p)) / SCHED_INTERACT_THRESH);
```

The interactivity score and priority of a process are recalculated each time the process is placed into the RQ.
As mentioned earlier, interactive processes are assigned a time slice of 1 tick. 

## Problem Specification

### Part 1. Tracking CPU bursts and system load (10 points)

In CPU scheduling, a ___CPU burst___ refers to a period of time during which a process is actively executing instructions on the CPU. Processes typically alternate between CPU bursts and I/O bursts. The length and frequency of CPU bursts can significantly influence the behavior of scheduling algorithms. For example, _interactive_ processes may have short CPU bursts followed by I/O waits, while CPU-intensive processes have longer CPU bursts.

In Part 1, your task is to track CPU bursts and system load in the current `xv6` scheduler. A CPU burst starts when a process begins its execution and ends when the process either exhausts its time slice, sleeps due to I/O operations, or terminates. By analyzing the `xv6` scheduler code, you are required to log each instance when a CPU burst starts and ends as follows:

```
xv6 kernel is booting

4169341 1 starts 0
4169386 1 ends 0
4169597 1 starts 0
4170327 1 ends 0
4170539 1 starts 0
4171269 1 ends 0
4171481 1 starts 0
4172376 1 ends 0
...
```

The log format is as follows: `[timestamp] [pid] ["starts"|"ends"] [load]`.
* The `timestamp` is obtained using the `r_time()` function, which returns the current cycle count, where 1,000,000 cycles correspond to 1 timer tick or 100 milliseconds (see `clockintr()` @ `kernel/trap.c`). 
* The `pid` indicates the process ID of the current process. 
* The actions `starts` or `ends` signify whether the corresponding CPU burst starts or ends at that specific moment. 
* The `load` represents the number of runnable processes in the system, excluding the currently running process. 

The required macros, `PRINTLOG_START` and `PRINTLOG_END`, are defined in `kernel/snule.h`. Your task is to place these macros in the appropriate locations within the kernel to accurately track CPU bursts. Note that you need to add `-DLOG` (along with `-DSNU`) into the `CFLAGS` in `Makefile` to enable these macros. To monitor system load, there is a global variable `sysload` defined in the `kernel/snule.c` file, which must be updated whenever a process becomes runnable or stops running. Additionally, a Python script, `graph.py`, is provided to help you visualize your log output. 

Please ensure that the tracking of CPU bursts and system load continues to function correctly after implementing Part 2 and Part 3 of this project. 

### Part 2. The Partial SNULE Scheduler (50 points)

In Part 2, you are required to implement a subset of the SNULE scheduler that does not consider interactive processes. Compared to the current `xv6` scheduler, the new scheduler introduces the following features:

* Each process has a nice value (`p->nice`) in the range [-20, 19] and a priority value (`p->prio`) in the range [100, 139].
* The priority of a process is statically determined based on its nice value, with `p->prio` = `p->nice` + 120.
* Conceptually, there are two run queues (RQs): the current RQ and the next RQ. Only the runnable processes are placed in these queues. When the current RQ becomes empty, the scheduler alternates the queues, making the next RQ the current RQ, and the previously current RQ becomes the next RQ.
* The scheduler allocates the CPU to the process with the highest priority in the current RQ.
* Runnable processes should be organized in the order of their priorities in RQs, allowing the scheduler to find the highest-priority process in constant time.
* Each process is allocated a time slice between 1 to `SCHED_SLICE_DEFAULT` ticks depending on the system load.
* On each timer interrupt, the scheduler recomputes the time slice of the current process based on the `sysload` value. If the number of ticks used by the current process exceeds its recomputed time slice, the process is preempted.

Additionally, you are required to implement the `nice()` system call. The system call number for `nice()` has already been assigned as 22 in the `./kernel/syscall.h` file. 

__SYNOPSYS__
```
    int nice(int inc);
```

__DESCRIPTION__

The `nice()` system call adds `inc` to the nice value for the calling process. The range of the nice value is 19 (low priority) to -20 (high priority). Attempts to set a nice value outside the range are clamped to the range. Even though the `nice()` system call is executed, the priority does not change immediately because it is recalculated when the calling process re-enters the run queue.

__RETURN VALUE__

* `nice()` returns the new nice value of the calling process. 
  

### Part 3: The Complete SNULE Scheduler (20 points)

In Part 3, you will implement the complete SNULE scheduler, incorporating all the features described earlier. Compared to the scheduler from Part 2, the complete SNULE scheduler introduces the following differences:

* The run time (`p->tick_run`) and sleep time (`p->tick_sleep`) of each process are tracked by the system. 
* The scheduler calculates the interactivity score to identify interactive processes.
* Interactive processes have a priority value in the range [80, 99].
* Time slices for interactive processes are fixed at 1 tick.

To track a process's run time and sleep time, you can use the `ticks` variable (located in `kernel/trap.c`), which is incremented by one with each timer tick. 

### Part 4. Test Cases and Documentation (20 points)

#### Test Cases (10 points)

Create test cases that include a mix of interactive and CPU-bound processes to validate your scheduler functions correctly. Include these test cases in the `./user` directory of the source code you submit.

#### Design Document (10 points)

Prepare a design document detailing your implementation in a single PDF file. Your document should include the following sections:

1. Behavior of your scheduler
  * Visualize the CPU bursts resulting from running `task1` using the `graph.py` script for the following schedulers: the default `xv6` scheduler, the scheduler from part 2, and the complete SNULE scheduler
  * Discuss whether there are any differences compared to those shown [above](#Examples)
2. New data structures
  * Describe any new data structures introduced for the SNULE scheduler.
  * Explain why these data structures were necessary and how they contribute to the implementation.
3. Algorithm design
  * Provide an explanation of how you implemented the run queues.
  * Detail any corner cases you considered and the strategies you used to address them.
  * Discuss any optimizations you applied to improve code efficiency, both in terms of time and space.
4. Testing and validation
  * Outline the test cases you created to validate the implementation of the SNULE scheduler.
  * Include the graphs generated from running your test cases and provide an explanation of why the results should appear as they do.
  * Describe how you verified the correct handling of the corner cases mentioned in Section 2. 
     
## Restrictions

* We will use three macros, `PART1`, `PART2`, and `PART3`, during automatic grading to verify if your code has successfully implemented all the corresponding specifications. Therefore, it is important to wrap your code within these macros accordingly. For instance, if a section of your code should only be enabled for Part1, you should enclose it like this:
```C
#ifdef PART1
// do something
...
#endif
```
Similarly, if a section of code is required for both Part2 and Part3, you should mark it like this:
```C
#if defined(PART2) || defined(PART3)
// do another thing
...
#endif
```
* To make the problem easier, we assume a single-processor RISC-V system in this project. The `CPUS` variable that represents the number of CPUs in the target QEMU machine emulator is already set to 1 in the `Makefile` for the `pa3` branch.
* Your implementation should pass the following test programs available on `xv6`:
  - `$ forktest`
  - `$ usertests -q`
* Do not add any system calls other than `nice()`.
* Please use the `qemu` version 8.2.0 or later. To determine the `qemu` version, use the command: `$ qemu-system-riscv64 --version`
* We will run `qemu-system-riscv64` with the `-icount shift=0` option, which enables aligning the host and virtual clocks. This setting is already included in the `Makefile`.
* You are required to modify only the files in the `./kernel` directory, except for `./kernel/systest.c`, which will be used for automatic grading. For your test cases, place them in the `./user` directory. Any other changes will be ignored during grading.

## Skeleton Code

The skeleton code for this project assignment (PA3) is available as a branch named `pa3`. Therefore, you should work on the `pa3` branch as follows:

```
$ git clone https://github.com/snu-csl/xv6-riscv-snu
$ git checkout pa3
```
After downloading, you must first set your `STUDENTID` in the `Makefile` again.

The `proc` structure in the `./kernel/proc.h` file now includes additional fields such as `nice`, `prio`, `tick_run`, and `tick_sleep` to support the SNULE scheduler.
You are free to add any other fields to the `proc` structure if necessary.

The skeleton code also includes two new files, `snule.h` and `snule.c`, located in the `./kernel` directory. 
Some macros (such as `PRINTLOG_START` and `PRINTLOG_END`) and various scheduling parameters are already defined in `snule.h`. If you need to define any new macros or functions to implement the SNULE scheduler, make sure to include them in these files. 

The `pa3` branch includes a user-level program called `task1`, with its source code located in `./user/task1.c`. 
The `task1` program begins by sleeping for 1 tick, after which it forks four child processes, `C1` to `C4`.
Each child process has its nice value incremented by one, ranging from 1 to 4, respectively, making `C1` the highest-priority process. 
The child processes perform some computations and then terminate. 
After creating the child processes, the parent process increases its nice value to the maximum, effectively becoming the lowest-priority process. The parent process is occasionally woken up from `wait()` to reap the terminated child processes. 

We provide you with a Python script called `graph.py` in the `./xv6-riscv-snu` directory. 
You can use this Python script to convert the log of CPU bursts and system load into a graph image.
This graph will visually represent the order and timing of process executions, allowing you to see how different processes are scheduled and executed over time as the scheduling policy changes. 
Note that the `graph.py` script requires the Python matplotlib package. Please install it using the following command in Ubuntu:

```
$ sudo apt install python3-matplotlib
```

To generate a graph, you should run `xv6` using the `make qemu-log` command that saves all the output into the file named `xv6.log`.
And then, run the `make png` command to generate the `graph.png` file using the Python script `graph.py` as shown below.

```
$ make qemu-log
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 1 -nographic -icount shift=0 -global virtio-mmio.force-legacy=false -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 | tee xv6.log

xv6 kernel is booting

4169341 1 starts 0
4169386 1 ends 0
4169597 1 starts 0
4170327 1 ends 0
4170539 1 starts 0
4171269 1 ends 0
4171481 1 starts 0
4172376 1 ends 0
...
$ init: starting sh
...
$ task1                           <--- Run the task1 program
18554754 2 starts 0 
18557992 2 ends 1 
18558034 3 starts 0 
18566343 3 ends 0
...

$ QEMU: Terminated                <--- Quit qemu using ^a-x
*** The output of xv6 is logged in the `xv6.log` file.

$ make png                        <--- Generate the graph (this should be done on the host machine, not on xv6)
graph saved in the `graph.png` file
```

## Examples

### Under the default `xv6`scheduler

The default `xv6` scheduler does not utilize the nice value, meaning all the processes are treated equally.
Each process is scheduled in a round-robin fashion, with each receiving a time slice of 1 tick. 

![task1-rr](https://github.com/snu-csl/os-pa3/blob/master/graph-task1-rr.png)

### Under the partial SNULE scheduler (Part 2)

The partial SNULE scheduler maintains two run queues. Initially, all runnable processes are placed into the current RQ. 
The scheduler first runs `C1` (pid 4), which has the highest priority. Since the load is 3 (due to the presence of other runnable processes, `C2` (pid 5), `C3` (pid 6), and `C4` (pid 7)), the time slice of `C1` is calculated as 10/3, giving it 3 ticks.
After tick 21, `C1` exits, waking up its parent process (pid 3). 
However, the parent process (pid 3) is not scheduled until `C4` (pid 7) releases the CPU at tick 30 due to its lower priority. 
For the same reason, after `C2` (pid 5) terminates at tick 32, the parent process (pid 3) is only scheduled after `C4` (pid 7) terminates at tick 43.
Once `C3` (pid 6) exits, the load decreases to 1 as the parent process remains in the run queue, and `C4` receives a time slice of 10 ticks. 

![task1-psnule](https://github.com/snu-csl/os-pa3/blob/master/graph-task1-psnule.png)

### Under the complete SNULE scheduler (Part 3)

Under the complete SNULE scheduler, the parent process (pid 3) is classified as an interactive process, as it spends most of its time in waiting for child processes to terminate in `wait()`. 
As a result, the parent process is scheduled immediately when one of its child processes terminates.

![task1-snule](https://github.com/snu-csl/os-pa3/blob/master/graph-task1-snule.png)

## Tips

* Read Chap. 7 of the [xv6 book](http://csl.snu.ac.kr/courses/4190.307/2024-2/book-riscv-rev4.pdf) to understand the scheduling subsystem of `xv6`.

* For your reference, the following roughly shows the amount of changes you need to make for this project assignment. Each `+` symbol indicates 1~10 lines of code that should be added, deleted, or altered.
   ```
   kernel/defs.h      |  +
   kernel/proc.c      |  +++++++
   kernel/proc.h      |  +
   kernel/start.c     |  +
   kernel/sysproc.c   |  +
   kernel/snule.h     |  +
   kernel/snule.c     |  +++++++++++++++++
   ```

## Hand in instructions

* First, make sure you are on the `pa3` branch in your `xv6-riscv-snu` directory. And then perform the `make submit` command to generate a compressed tar file named `xv6-{PANUM}-{STUDENTID}.tar.gz` in the `../xv6-riscv-snu` directory. Upload this file to the submission server. Additionally, your design document should be uploaded as the report for this project assignment.

* The total number of submissions for this project assignment will be limited to 30. Only the version marked as `FINAL` will be considered for the project score. Please remember to designate the version you wish to submit using the `FINAL` button.

* The `sys` server will no longer accept submissions after three days (72 hours) past the deadline.
  
* Note that the submission server is only accessible inside the SNU campus network. If you want off-campus access (from home, cafe, etc.), you can add your IP address by submitting a Google Form whose URL is available in the eTL. Now, adding your new IP address is automated by a script that periodically checks the Google Form at minutes 0, 20, and 40 during the hours between 09:00 and 00:40 the following day, and at minute 0 every hour between 01:00 and 09:00.
     + If you cannot reach the server a minute after the update time, check your IP address, as you might have sent the wrong IP address.
     + If you still cannot access the server after a while, it is likely due to an error in the automated process. The TAs will check if the script is running properly, but since that is a ___manual___ process, please do not expect it to be completed immediately.

## Logistics

* You will work on this project alone.
* Only the upload submitted before the deadline will receive the full credit. 25% of the credit will be deducted for every single day delayed.
* __You can use up to _3 slip days_ during this semester__. If your submission is delayed by one day and you decide to use one slip day, there will be no penalty. In this case, you should explicitly declare the number of slip days you want to use on the QnA board of the submission server before the next project assignment is announced. Once slip days have been used, they cannot be canceled later, so saving them for later projects is highly recommended!
* Any attempt to copy others' work will result in a heavy penalty (for both the copier and the originator). Don't take a risk.

Have fun!

[Jin-Soo Kim](mailto:jinsoo.kim_AT_snu.ac.kr)  
[Systems Software and Architecture Laboratory](http://csl.snu.ac.kr)  
[Dept. of Computer Science and Engineering](http://cse.snu.ac.kr)  
[Seoul National University](http://www.snu.ac.kr)
