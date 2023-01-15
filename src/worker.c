#include	<stdlib.h>
#include	<stdint.h>
#include	<stdio.h>
#include	<errno.h>
#include	<string.h>
#include	<signal.h>
#include	<pthread.h>
#include	<sys/prctl.h>
#include	<sys/sysinfo.h>

enum neo_state_cmd {
	NEO_NORMAL,
	NEO_PAUSE,
	NEO_CONST,
	NEO_FADE,
	NEO_IDLE,
	NEO_UNKNOWN
};

// func should be general, in/out should be designed for least memcopy
struct neo_tasklet_s {
	// TODO: tasklet description
	uint64_t (*func)(void *in, void *out, void *args);
	struct neo_tasklet_s *prev, *next;
};

struct neo_worker_s {
	pthread_t thread;
	pthread_attr_t attr;
	uint16_t weight;		// for tasklet scheduling
	uint8_t ctrl;			// do not change inside worker
	uint8_t status;			// do not change outside worker

	// Take an "idle" tasklet, when no more tasklet left in queue.
	// Added to tail, and executed from head,
	struct neo_tasklet_s *tasks;	// ring of tasklets

	// worker and net_pulse() may modify *tasks
	// Could this lock be eliminated ?
	pthread_mutex_t lock;
};

struct neo_worker_pool_s {
	struct {
		volatile uint32_t limit;
		volatile uint32_t working;
	} count;

	uint8_t status;			// normal, constant, pause

	// preallocate memory
	struct neo_worker_s workers[0];
};

void *neo_worker(void *arg)
{
	struct neo_worker_s *worker = (struct neo_worker_s *)arg;
	prctl(PR_SET_NAME, "neo-worker");

	while (worker->status != NEO_FADE) {
		;	// iterate on worker->tasks
		;	//		process worker->ctrl, don't interrupt tasklet processing
	}

	return NULL;
}

struct neo_worker_pool_s * neo_worker_pool_init(int32_t limit)
{
	int pool_mem_size = 0;
	int max_workers = get_nprocs_conf();
	struct neo_worker_pool_s *pool = NULL;

	if (!limit || (limit > max_workers))
		limit = get_nprocs_conf();
	else
		limit = max_workers;
	pool_mem_size = sizeof(struct neo_worker_pool_s)
		+ limit * sizeof(struct neo_worker_s);

	pool = (struct neo_worker_pool_s *)malloc(pool_mem_size);

	if (!pool) {
		perror("neo_worker_init");
		return NULL;
	}

	memset(pool, 0, pool_mem_size);
	pool->count.limit = limit;
	pool->count.working = 0;
	return pool;
}

int neo_worker_pool_fini(struct neo_worker_pool_s *pool)
{
	// for worker in pool:
	//		neo_worker_fade(worker)
	// free(pool)
}

int neo_worker_pool_migrate(struct neo_worker_pool_s *pool, enum neo_state_cmd cmd)
{
	for (int n = 0; n < pool->count.working; n++)
		if (pool->workers[n].status != cmd)
			pool->workers[n].ctrl = cmd;

	// TODO: wait workers to respond
	return 0;
}

pthread_t *neo_worker_grow(struct neo_worker_pool_s *pool)
{
	struct neo_worker_s *worker = &pool->workers[pool->count.working];

	pool->count.working += 1;
	pthread_attr_init(&worker->attr);
	//pthread_attr_setstacksize(&worker->attr, stack_size);
	worker->status = NEO_UNKNOWN;	// accept tasklet only after self check
	worker->tasks = NULL;

	pthread_create(&worker->thread, &worker->attr, neo_worker, (void *)worker);
	pthread_detach(worker->thread);
	return NULL;
}

pthread_t *neo_worker_fade(struct neo_worker_pool_s *pool, struct neo_worker_s *worker)
{
	// swap pool->worker array, or bind worker to core(array index tells core id)
	worker->ctrl = NEO_FADE;
	// TODO: wait  worker->status to be NEO_FADE
	pthread_attr_destroy(&worker->attr);
	pool->count.working -= 1;
}
