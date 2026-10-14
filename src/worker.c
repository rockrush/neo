#include "neo.h"

static bool lfq_init(struct lf_queue *q, size_t cap)
{
	size_t n = 1;
	while (n < cap) n <<= 1;
	q->cells = (struct q_cell *)calloc(n, sizeof(struct q_cell));
	if (!q->cells) return false;
	q->mask = n - 1;
	for (size_t i = 0; i < n; ++i)
		atomic_init(&q->cells[i].seq, i);
	atomic_init(&q->enq, 0);
	atomic_init(&q->deq, 0);
	return true;
}

static void lfq_destroy(struct lf_queue *q)
{
	free(q->cells);
	q->cells = NULL;
}

/* 无锁入队; 队列满返回 false */
static inline bool lfq_push(struct lf_queue *q, struct task t)
{
	size_t pos = atomic_load_explicit(&q->enq, memory_order_relaxed);
	for (;;) {
		struct q_cell *cell = &q->cells[pos & q->mask];
		size_t seq = atomic_load_explicit(&cell->seq, memory_order_acquire);
		intptr_t diff = (intptr_t)seq - (intptr_t)pos;
		if (diff == 0) {
			if (atomic_compare_exchange_weak_explicit(&q->enq, &pos, pos + 1,
					memory_order_relaxed, memory_order_relaxed)) {
				cell->task = t;
				atomic_store_explicit(&cell->seq, pos + 1, memory_order_release);
				return true;
			}
		} else if (diff < 0)
			return false;	/* 队列满 */

		pos = atomic_load_explicit(&q->enq, memory_order_relaxed);
	}
}

/* 无锁出队; 队列空返回 false */
static inline bool lfq_pop(struct lf_queue *q, struct task *out)
{
	size_t pos = atomic_load_explicit(&q->deq, memory_order_relaxed);
	for (;;) {
		struct q_cell *cell = &q->cells[pos & q->mask];
		size_t seq = atomic_load_explicit(&cell->seq, memory_order_acquire);
		intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
		if (diff == 0) {
			if (atomic_compare_exchange_weak_explicit(&q->deq, &pos, pos + 1,
					memory_order_relaxed, memory_order_relaxed)) {
				*out = cell->task;
				atomic_store_explicit(&cell->seq, pos + q->mask + 1, memory_order_release);
				return true;
			}
		} else if (diff < 0)
			return false;	/* 队列空 */

		pos = atomic_load_explicit(&q->deq, memory_order_relaxed);
	}
}

static inline size_t lfq_size(const struct lf_queue *q)
{
	size_t e = atomic_load_explicit(&q->enq, memory_order_relaxed);
	size_t d = atomic_load_explicit(&q->deq, memory_order_relaxed);
	return e - d;
}

/* =============================================================
 *  3. Worker 主循环
 * ============================================================= */
/* 线程退出清理：递减计数并标记状态（正常返回 / pthread_exit 都会走到） */
static void worker_cleanup(void *arg)
{
	worker_t *w = (worker_t *)arg;
	struct runner *p = w->pool;
	pthread_mutex_lock(&p->lock);
	w->state = WS_EXITED;
	pthread_mutex_unlock(&p->lock);
	atomic_fetch_sub_explicit(&p->w_cnt, 1, memory_order_relaxed);
}

static void *worker_main(void *arg)
{
	worker_t *w = (worker_t *)arg;
	struct runner *p = w->pool;

	pthread_mutex_lock(&p->lock);
	w->state = WS_RUNNING;
	pthread_mutex_unlock(&p->lock);
	atomic_fetch_add_explicit(&p->w_cnt, 1, memory_order_relaxed);

	pthread_cleanup_push(worker_cleanup, w);

	for (;;) {
		/* ---- 关停 / 主动退出检查 ---- */
		if (atomic_load_explicit(&p->shutdown, memory_order_acquire) ||
			atomic_load_explicit(&w->stop_requested, memory_order_acquire)) {
			break;
		}

		/* ---- 暂停检查 ---- */
		if (atomic_load_explicit(&p->paused, memory_order_acquire)) {
			pthread_mutex_lock(&p->lock);
			while (atomic_load_explicit(&p->paused, memory_order_acquire) &&
				   !atomic_load_explicit(&p->shutdown, memory_order_acquire) &&
				   !atomic_load_explicit(&w->stop_requested, memory_order_acquire)) {
				pthread_cond_wait(&p->cond_resume, &p->lock);
			}
			pthread_mutex_unlock(&p->lock);
			continue;
		}

		/* ---- 无锁取任务 ---- */
		struct task t;
		if (lfq_pop(&p->queue, &t)) {
			t.func(t.arg);
			atomic_fetch_add_explicit(&p->stat_completed, 1, memory_order_relaxed);
			atomic_fetch_add_explicit(&w->tasks_done, 1, memory_order_relaxed);
			continue;
		}

		/* ---- 队列空：进入等待，避免忙等 ---- */
		pthread_mutex_lock(&p->lock);

		/* 再次检查，防止丢失唤醒 */
		if (atomic_load_explicit(&p->shutdown, memory_order_relaxed) ||
			atomic_load_explicit(&p->paused, memory_order_relaxed) ||
			atomic_load_explicit(&w->stop_requested, memory_order_relaxed) ||
			lfq_size(&p->queue) > 0) {
			pthread_mutex_unlock(&p->lock);
			continue;
		}

		atomic_fetch_add_explicit(&p->w_idle, 1, memory_order_relaxed);

		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		ts.tv_nsec += WORKER_WAIT_MS * 1000000L;
		if (ts.tv_nsec >= 1000000000L) {
			ts.tv_sec  += 1;
			ts.tv_nsec -= 1000000000L;
		}
		pthread_cond_timedwait(&p->cond_work, &p->lock, &ts);

		atomic_fetch_sub_explicit(&p->w_idle, 1, memory_order_relaxed);
		pthread_mutex_unlock(&p->lock);
	}

	pthread_cleanup_pop(1);   /* 执行清理 */
	return NULL;
}

/*  4. 启动一个 Worker */
static bool start_worker(struct runner *p, worker_t *w)
{
	w->pool = p;
	atomic_store_explicit(&w->stop_requested, 0, memory_order_release);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	/* 可按需调整栈大小: pthread_attr_setstacksize(&attr, 256*1024); */

	int r = pthread_create(&w->tid, &attr, worker_main, w);
	pthread_attr_destroy(&attr);

	if (r != 0) {
		w->tid = 0;
		pthread_mutex_lock(&p->lock);
		w->state = WS_FREE;
		pthread_mutex_unlock(&p->lock);
		return false;
	}
	pthread_setname_np(w->tid, "neo-worker");
	return true;
}

/* 分配并初始化一个空闲 worker 槽位（需持有 p->lock） */
static bool start_new_worker(struct runner *p)
{
	for (size_t i = 0; i < p->ncpu; ++i) {
		if (p->workers[i].tid == 0)
			return start_worker(p, &p->workers[i]);
	}
	return false;
}

/*  5. 监控线程：异常重启 + 动态扩缩容 */
static void *monitor_main(void *arg)
{
	struct runner *p = (struct runner *)arg;

	while (!atomic_load_explicit(&p->shutdown, memory_order_acquire)) {
		struct timespec ts = {
			.tv_sec  = 0,
			.tv_nsec = MONITOR_INTERVAL_MS * 1000000L
		};
		nanosleep(&ts, NULL);

		if (atomic_load_explicit(&p->shutdown, memory_order_acquire))
			break;

		/* ---------- 5.1 检测并拉起异常退出的线程 ---------- */
		pthread_mutex_lock(&p->lock);
		for (int i = 0; i < (int)p->ncpu; ++i) {
			worker_t *w = &p->workers[i];
			if (w == NULL || w->tid == 0) continue;

			void *ret;
			int r = pthread_tryjoin_np(w->tid, &ret);
			if (r == EBUSY) continue;	/* 仍在运行 */
			if (r != 0)	 continue;	/* 其它错误忽略 */

			/* 线程确实已经退出 */
			bool requested = atomic_load_explicit(&w->stop_requested, memory_order_relaxed);
			memset(w, 0, sizeof(worker_t));

			if (!requested && !atomic_load_explicit(&p->shutdown, memory_order_relaxed)) {
				/* 非主动退出 → 异常，自动拉起 */
				start_worker(p, w);
				atomic_fetch_add_explicit(&p->stat_restarted, 1, memory_order_relaxed);
				fprintf(stderr, "[pool] worker#%d 异常退出, 已自动拉起\n", w->id);
			}
		}
		pthread_mutex_unlock(&p->lock);

		if (atomic_load_explicit(&p->paused, memory_order_relaxed))
			continue;

		/* ---------- 5.2 动态扩容 ---------- */
		size_t qsize = lfq_size(&p->queue);
		size_t nw = atomic_load_explicit(&p->w_cnt, memory_order_relaxed);
		size_t wmin  = atomic_load_explicit(&p->startup, memory_order_relaxed);

		if (qsize > nw * SCALE_UP_FACTOR && nw < p->ncpu) {
			pthread_mutex_lock(&p->lock);
			start_new_worker(p);
			pthread_mutex_unlock(&p->lock);
		}
		/* ---------- 5.3 动态缩容 ---------- */
		else if (qsize == 0 && nw > wmin) {
			size_t idle = atomic_load_explicit(&p->w_idle, memory_order_relaxed);
			if (idle > 0) {
				pthread_mutex_lock(&p->lock);
				for (int i = 0; i < (int)p->ncpu; ++i) {
					worker_t *w = &p->workers[i];
					if (w == NULL || w->tid == 0) continue;
					if (w->state != WS_RUNNING) continue;

					atomic_store_explicit(&w->stop_requested, 1, memory_order_release);
					break;
				}
				pthread_mutex_unlock(&p->lock);

				pthread_mutex_lock(&p->lock);
				pthread_cond_broadcast(&p->cond_work);
				pthread_cond_broadcast(&p->cond_resume);
				pthread_mutex_unlock(&p->lock);
			}
		}
	}
	return NULL;
}

/*  6. 公共 API */
int runner_create(struct runner *p)
{
	if (!p) return EXIT_FAILURE;
	long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpu == -1) {
		perror("sysconf");
		return EXIT_FAILURE;
	}

	if (!lfq_init(&p->queue, p->queue_size ? p->queue_size : NEO_WORKQUEUE_SIZE))
		return EXIT_FAILURE;

	int _min = p->startup ? p->startup : NEO_WORKERS_MIN;
	_min = (_min > ncpu) ? ncpu : _min;
	atomic_init(&p->startup, _min);
	atomic_init(&p->ncpu, ncpu);
	pthread_mutex_init(&p->lock, NULL);

	pthread_condattr_t cattr;
	pthread_condattr_init(&cattr);
	pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
	pthread_cond_init(&p->cond_work,   &cattr);
	pthread_cond_init(&p->cond_resume, &cattr);
	pthread_condattr_destroy(&cattr);

	p->workers = (worker_t *)calloc(ncpu, sizeof(worker_t));
	if (!p->workers) {
		pthread_cond_destroy(&p->cond_work);
		pthread_cond_destroy(&p->cond_resume);
		pthread_mutex_destroy(&p->lock);
		lfq_destroy(&p->queue);
		return EXIT_FAILURE;
	}

	/* 启动监控线程 */
	if (pthread_create(&p->mid, NULL, monitor_main, p) == 0) {
		atomic_store(&p->monitor_started, true);
		pthread_setname_np(p->mid, "neo-worker-pool");
	} else {
		lfq_destroy(&p->queue);
		return EXIT_FAILURE;
	}

	/* 启动最小线程数 */
	for (size_t i = 0; i < _min; ++i)
		start_new_worker(p);
	return EXIT_SUCCESS;
}

/* 无锁提交任务（暂停或关停状态返回 false） */
bool runner_submit(struct runner *p, void (*func)(void *), void *arg)
{
	if (!p || !func) return false;
	if (atomic_load_explicit(&p->shutdown, memory_order_acquire)) return false;
	if (atomic_load_explicit(&p->paused,   memory_order_acquire)) return false;

	struct task t = { .func = func, .arg = arg };

	int spins = 0;
	while (!lfq_push(&p->queue, t)) {
		if (atomic_load_explicit(&p->shutdown, memory_order_relaxed))
			return false;
		if (++spins > 1000) { sched_yield(); spins = 0; }
	}

	atomic_fetch_add_explicit(&p->stat_submitted, 1, memory_order_relaxed);

	/* 仅在确实有等待者时才发信号，避免无谓的锁竞争 */
	if (atomic_load_explicit(&p->w_idle, memory_order_relaxed) > 0) {
		pthread_mutex_lock(&p->lock);
		pthread_cond_signal(&p->cond_work);
		pthread_mutex_unlock(&p->lock);
	}
	return true;
}

/* 暂停：工作线程停止取新任务；已在执行的任务允许跑完 */
void runner_pause(struct runner *p)
{
	if (!p) return;
	atomic_store_explicit(&p->paused, true, memory_order_release);
}

/* 恢复 */
void runner_resume(struct runner *p)
{
	if (!p) return;
	atomic_store_explicit(&p->paused, false, memory_order_release);
	pthread_mutex_lock(&p->lock);
	pthread_cond_broadcast(&p->cond_resume);
	pthread_cond_broadcast(&p->cond_work);
	pthread_mutex_unlock(&p->lock);
}

/* 动态调整线程数目标 */
void runner_resize(struct runner *p, size_t n)
{
	if (!p || (n > p->ncpu)) return;	// ncpu 是上限，更多worker没意义
	if (n < 1) n = 1;

	pthread_mutex_lock(&p->lock);
	atomic_store_explicit(&p->startup, n, memory_order_release);

	size_t cur = atomic_load(&p->w_cnt);
	if (n > cur) {
		for (size_t i = cur; i < n; ++i)
			start_new_worker(p);
	} else if (n < cur) {
		size_t to_remove = cur - n;
		for (size_t j = 0; j < p->ncpu && to_remove > 0; ++j) {
			worker_t *w = &p->workers[j];
			if (w == NULL || w->tid == 0) continue;
			atomic_store(&w->stop_requested, 1);
			to_remove--;
		}
	}
	pthread_mutex_unlock(&p->lock);

	if (n < cur) {
		pthread_mutex_lock(&p->lock);
		pthread_cond_broadcast(&p->cond_work);
		pthread_cond_broadcast(&p->cond_resume);
		pthread_mutex_unlock(&p->lock);
	}
}

/* 重置：清空队列 + 恢复运行 + 线程数收敛到最小值 */
void runner_reset(struct runner *p)
{
	if (!p) return;
	struct task t;
	while (lfq_pop(&p->queue, &t)) { /* 丢弃未执行任务 */ }
	runner_resume(p);
	runner_resize(p, atomic_load(&p->startup));
}

/* 销毁 */
void runner_destroy(struct runner *p)
{
	if (!p) return;
	atomic_store_explicit(&p->shutdown, true, memory_order_release);

	pthread_mutex_lock(&p->lock);
	pthread_cond_broadcast(&p->cond_work);
	pthread_cond_broadcast(&p->cond_resume);
	pthread_mutex_unlock(&p->lock);

	if (atomic_load(&p->monitor_started))
		pthread_join(p->mid, NULL);

	for (size_t i = 0; i < p->ncpu; ++i) {
		worker_t *w = &p->workers[i];
		if (w->tid != 0)
			pthread_join(w->tid, NULL);
	}
	free(p->workers);
	p->workers = NULL;

	struct task t;
	while (lfq_pop(&p->queue, &t)) { }

	lfq_destroy(&p->queue);
	pthread_mutex_destroy(&p->lock);
	pthread_cond_destroy(&p->cond_work);
	pthread_cond_destroy(&p->cond_resume);
}

/* 打印状态 */
void runner_stats(struct runner *p)
{
	if (!p) return;
	printf("  Idle/Workers\tQueue len\tCommit/Complete\n  -----------------------------------------------\n");
	printf("  %u/%u\t\t%zu\t\t%lu/%lu\n", atomic_load(&p->w_idle), atomic_load(&p->w_cnt), lfq_size(&p->queue),
		(unsigned long)atomic_load(&p->stat_submitted), (unsigned long)atomic_load(&p->stat_completed));
}
