#ifndef _CTP_H
#define _CTP_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <sched.h>
#include <errno.h>

#define MONITOR_INTERVAL_MS	500	/* 监控线程巡检间隔 */
#define WORKER_WAIT_MS		50	/* 空闲 worker 等待超时 */
#define SCALE_UP_FACTOR		4	/* 队列积压 > 线程数*N 时扩容 */

struct task {
	void (*func)(void *);
	void  *arg;
};

struct q_cell {
	_Atomic(size_t) seq;
	struct task task;
};

/* 无锁 MPMC 队列 (bounded, Dmitry Vyukov) */
struct lf_queue {
	struct q_cell *cells;
	size_t mask;	/* capacity - 1, capacity 为 2 的幂 */
	_Atomic(size_t) enq;
	_Atomic(size_t) deq;
};

struct runner;
enum worker_state {
	WS_FREE = 0,	/* 槽位空置 */
	WS_RUNNING,	/* 线程在运行 */
	WS_EXITED	/* 线程已退出，等待回收 */
};

typedef struct {
	int id;
	pthread_t tid;
	struct runner *pool;
	enum worker_state state;	/* 受 pool->lock 保护 */
	_Atomic(int) stop_requested;	/* 主动要求退出（缩容/关停） */
	_Atomic(uint64_t) tasks_done;
} worker_t;

struct runner {
	struct lf_queue queue;
	_Atomic(uint32_t) queue_size;
	worker_t *workers;	/* 动态指针数组，容量由 ncpu 上限决定 */
	uint16_t ncpu;		/* 在线 CPU 核心数 */
	pthread_t mid;		/* Monitor thread ID */
	_Atomic(bool) monitor_started;

	_Atomic(uint16_t) w_cnt;/* 存活线程数 */
	_Atomic(uint16_t) w_idle;
	_Atomic(uint16_t) startup;
	_Atomic(bool) paused;
	_Atomic(bool) shutdown;

	pthread_mutex_t lock;
	pthread_cond_t cond_work;	/* 有新任务时唤醒空闲线程 */
	pthread_cond_t cond_resume;	/* 暂停/恢复/关停事件 */

	/* 统计 */
	_Atomic(uint64_t) stat_submitted;
	_Atomic(uint64_t) stat_completed;
	_Atomic(uint64_t) stat_restarted;
};

int runner_create(struct runner *runner);
void runner_destroy(struct runner *p);
bool runner_submit(struct runner *p, void (*func)(void *), void *arg);	// 提交任务：暂停/关停状态返回false
void runner_pause(struct runner *p);	/* 暂停：工作线程停止取新任务；已在执行的任务允许跑完 */
void runner_resume(struct runner *p);	/* 恢复 */
void runner_resize(struct runner *p, size_t n);	/* 动态调整线程数目标 */
void runner_reset(struct runner *p);	/* 重置：清空队列 + 恢复运行 + 线程数收敛到最小值 */
void runner_stats(struct runner *p);	/* 打印状态 */
#endif
