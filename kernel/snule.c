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
#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "snule.h"


int sysload = 0;
#if defined(PART2) || defined(PART3)
struct rq current_rq;
struct rq next_rq;
#define max(a, b) ((a) > (b) ? (a) : (b))
// 우선순위 기반으로 프로세스를 추가하는 함수
void add_to_rq(struct rq *queue, struct proc *p) {
  if (queue->count < NPROC) {
    queue->procs[queue->count++] = p;
  }
}
// 우선순위에 따라 프로세스 선택
struct proc* select_highest_priority(struct rq *queue) {
  struct proc *best_proc = 0;
  int best_priority = PRIO_MAX_NORMAL + 1; // 처음에 큰 값으로 설정

  for (int i = 0; i < queue->count; i++) {
    struct proc *p = queue->procs[i];
    if (p->prio < best_priority) {
      best_priority = p->prio;
      best_proc = p;
    }
  }
  return best_proc;
}

// 현재 RQ가 비면, RQ 교환
void switch_rq() {
  struct rq temp = current_rq;
  current_rq = next_rq;
  next_rq = temp;
  next_rq.count = 0;  // 다음 RQ 초기화
}

// 큐에서 특정 프로세스를 제거하는 함수
void remove_from_rq(struct rq *queue, struct proc *p) {
    int found = 0;
    for (int i = 0; i < queue->count; i++) {
        if (queue->procs[i] == p) {
            found = 1;
        }
        if (found && i < queue->count - 1) {
            queue->procs[i] = queue->procs[i + 1]; // 프로세스를 제거하고 나머지 프로세스들을 이동
        }
    }
    if (found) {
        queue->count--; // 큐에서 프로세스 제거 후 카운트 감소
    }
}
//ifdef PART3
#ifdef PART3
// Function to handle decay of sleep and run ticks
void apply_tick_decay(struct proc *p) {
  int tick_sleep_norm = p->tick_sleep << TICK_SHIFT;
  int tick_run_norm = p->tick_run << TICK_SHIFT;

  // Check if the sum exceeds SCHED_SLP_RUN_MAX, then apply decay
  if (p->tick_sleep + p->tick_run > SCHED_SLP_RUN_MAX) {
    tick_sleep_norm = tick_sleep_norm >> 1;
    tick_run_norm = tick_run_norm >> 1;

    // Update the original ticks after decay
    p->tick_sleep = tick_sleep_norm >> TICK_SHIFT;
    p->tick_run = tick_run_norm >> TICK_SHIFT;
  }
}

// Function to calculate interactivity score and adjust priority
void calculate_interactivity_score(struct proc *p) {
  int tick_sleep_norm = p->tick_sleep << TICK_SHIFT;
  int tick_run_norm = p->tick_run << TICK_SHIFT;
  int interactivity_score;
  int interactivity_score_norm;

  // Calculate interactivity score
  if (p->tick_sleep > p->tick_run) {
    interactivity_score_norm = tick_run_norm / (max(1, tick_sleep_norm / SCHED_INTERACT_MAX));
    interactivity_score = interactivity_score_norm >> TICK_SHIFT;
  } else {
    interactivity_score = SCHED_INTERACT_MAX;
  }

  // Adjust the priority based on the interactivity score
  if (interactivity_score < SCHED_INTERACT_THRESH) {
    p->prio = PRIO_MIN_INTERACT + 
              ((PRIO_INTERACT_RANGE * interactivity_score) / SCHED_INTERACT_THRESH);
  } else {
    p->prio = p->nice + 120;
  }
}
#endif
#endif
#endif