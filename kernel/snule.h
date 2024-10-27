//----------------------------------------------------------------
//
//  4190.307 Operating Systems (Fall 2024)
//
//  Project #3: SNULE: A Simplified Nerdy ULE Scheduler
//
//  October 10, 2024
//
//  Jin-Soo Kim (jinsoo.kim@snu.ac.kr)
//  Systems Software & Architecture Laboratory
//  Dept. of Computer Science and Engineering
//  Seoul National University
//
//----------------------------------------------------------------

#ifdef SNU

#ifdef LOG
// p should point to the current process's proc structure.
// You can append any debugging information to the end of the log message,
// which will be ignored by the graph.py script.
#define PRINTLOG_START    printf("%ld %d starts %d\n", r_time(), p->pid, sysload);
#define PRINTLOG_END      printf("%ld %d ends %d\n", r_time(), p->pid, sysload);
#else
#define PRINTLOG_START  
#define PRINTLOG_END    
#endif
#endif

// Priorities
#define NICE_MIN                  (-20)
#define NICE_MAX                  (19)
#define PRIO_MIN_NORMAL           (100)
#define PRIO_MAX_NORMAL           (139)
#define PRIO_NORMAL_RANGE         (PRIO_MAX_NORMAL - PRIO_MIN_NORMAL + 1)
#define PRIO_MIN_INTERACT         (80)
#define PRIO_MAX_INTERACT         (99)
#define PRIO_INTERACT_RANGE       (PRIO_MAX_INTERACT - PRIO_MIN_INTERACT + 1)

// Time slices
#define SCHED_SLICE_DEFAULT       (10)
#define SCHED_SLICE_MIN           (1)
#define SCHED_SLICE_MIN_DIVISOR   (6)

// Interactivity score
#define HZ                        (10)                        // 100 msec/tick
#define TICK_SHIFT                (10)      
#define SCHED_SLP_RUN_MAX         ((5 * HZ) << TICK_SHIFT)    // 5 seconds

#define SCHED_INTERACT_MAX        (50)
#define SCHED_INTERACT_THRESH     (30)
extern int sysload;
#if defined(PART2) || defined(PART3)
// Run Queue
struct rq {
  struct proc *procs[NPROC];
  int count; // number of processes in the run queue
};

extern struct rq current_rq;
extern struct rq next_rq;


void add_to_rq(struct rq *queue, struct proc *p);
struct proc* select_highest_priority(struct rq *queue);
void switch_rq();
void remove_from_rq(struct rq *queue, struct proc *p);
#ifdef PART3
void calculate_interactivity_score(struct proc *p);
void apply_tick_decay(struct proc *p);
#endif
#endif