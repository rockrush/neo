#include	<stdlib.h>
#include	<unistd.h>
#include	<stdint.h>
#include	<stdio.h>
#include	<errno.h>
#include	<string.h>
#include	<signal.h>
#include	<pthread.h>
#include	<sys/prctl.h>
#include	<sys/sysinfo.h>
#include	<libconfig.h>

#include	<neo.h>

void *neo_worker(void *arg);

/* The last work to run, counts and fades current worker */
int neo_idle_work(struct neo_worker_s *worker)
{
	sleep(5);
	printf(HDR_NOTE "Worker %d is idle\n", worker->weight);
	return 0;
}

/* Management of worker pool */
void *neo_wpool_mgr(void *args)
{
	struct neo_cfg_s *cfg = (struct neo_cfg_s *)args;
	int worker_max = cfg->wpool.max;

	printf(HDR_NOTE "wpool_mgr: %d\n", cfg->env.lcores);
	//prctl(PR_SET_NAME, "neo-manager");

	while (cfg->wpool.migrate != NEO_FADE) {
		worker_max = (cfg->wpool.max) ? cfg->wpool.max : get_nprocs();
		if (worker_max > cfg->wpool.cur)
			neo_worker_new(&cfg->wpool);	// Grow gradually
		else if (worker_max < cfg->wpool.cur)
			neo_worker_fade(&cfg->wpool, NULL);	// CPU hot unplugged?

		usleep(200000);
		//printf(HDR_NOTE "wpool manager: %d -> %d\n", cfg->wpool.cur, worker_max);
	}

	// Exits the whole process
	while (cfg->wpool.workers)	// Fade out
		neo_worker_fade(&cfg->wpool, NULL);
	return NULL;
}

int neo_wpool_init(struct neo_cfg_s *cfg)
{
	int retval = EXIT_SUCCESS;
	pthread_attr_t mgr_attr;

	retval = pthread_attr_init(&mgr_attr);
	if (retval) {
		fprintf(stderr, HDR_ERR "Failed initializing pthread attribute: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	retval = pthread_attr_setdetachstate(&mgr_attr, PTHREAD_CREATE_JOINABLE);
	if (retval) {
		fprintf(stderr, HDR_ERR "Failed putting wpool manager into detached mode: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	retval = pthread_create(&cfg->wpool.mgr, &mgr_attr, neo_wpool_mgr, (void *)cfg);
	if (retval) {
		fprintf(stderr, HDR_ERR "Failed creating wpool manager: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	pthread_setname_np(cfg->wpool.mgr, "neo-manager");

	return EXIT_SUCCESS;
}

int neo_wpool_fini(struct neo_cfg_s *cfg)
{
	cfg->wpool.migrate = NEO_FADE;
	return pthread_join(cfg->wpool.mgr, NULL);
}

enum neo_state_cmd neo_wpool_migrate(struct neo_cfg_s *cfg, enum neo_state_cmd cmd)
{
	enum neo_state_cmd old_cmd = cfg->wpool.migrate;
	cfg->wpool.migrate = cmd;
	return old_cmd;
}

/* Management of worker */
struct neo_worker_s *neo_worker_new(struct neo_wpool_s *pool)
{
	int retval = EXIT_SUCCESS;
	pthread_attr_t mgr_attr;
	struct neo_worker_s *current = NULL;

	if (!pool)
		return NULL;

	current = (struct neo_worker_s *)malloc(sizeof(struct neo_worker_s));
	if (!current)
		return NULL;
	memset(current, 0, sizeof(struct neo_worker_s));
	current->weight = pool->cur;

	retval = pthread_create(&current->thread, NULL, neo_worker, (void *)current);
	pthread_setname_np(current->thread, "neo-worker");
	//pthread_detach(worker->thread);
	if (retval)
		return NULL;
	else if (!pool->workers)
		pool->workers = current->next = current->prev = current;
	else {
		current->next = pool->workers;
		current->prev = pool->workers->prev;
		pool->workers->prev->next = current;
		pool->workers->prev = current;
	}

	pool->cur = pool->cur + 1;
	printf(HDR_NOTE "Increase worker %d\n", current->weight);
	return current;
}

/* TODO: Pick a least recently used worker */
struct neo_worker_s *neo_worker_lru(struct neo_wpool_s *pool)
{
	if (!pool)
		return NULL;
	return pool->workers;
}

/* TODO: Stop a worker, also update relavent pointers */
/* worker: optional */
int neo_worker_fade(struct neo_wpool_s *pool, struct neo_worker_s *worker)
{
	int retval;
	struct neo_worker_s *current = NULL;
	if (!pool)
		return EXIT_FAILURE;

	/* If not specified, pick a LRU one */
	current = (worker) ? worker : neo_worker_lru(pool);
	if (!current)	/* Not specified, and founds no one */
		return EXIT_FAILURE;

	printf(HDR_NOTE "Decrease worker %d\n", current->weight);
	current->status = NEO_FADE;
	retval = pthread_join(current->thread, NULL);
	if (retval) {
		fprintf(stderr, HDR_ERR "Failed decrease worker\n");
		return retval;
	} else if (current->next == current)	// Decreasing the only worker
		pool->workers = NULL;
	else {
		if (current == pool->workers)
			pool->workers = current->next;
		current->next->prev = current->prev;
		current->prev->next = current->next;
	}

	pool->cur = (pool->cur > 0) ? (pool->cur - 1) : 0;
	free(current);
	return EXIT_SUCCESS;
}

/* Management of thread, only for internal usage */
void *neo_worker(void *arg)
{
	struct neo_work_s *work = NULL;
	struct neo_worker_s *worker = (struct neo_worker_s *)arg;
	prctl(PR_SET_NAME, "neo-worker");

	while (worker->status != NEO_FADE) {
		work = worker->works;
		if (!work) {
			// TODO: Tag worker as idle
			neo_idle_work(worker);
			continue;
		}

		work->exec(work);

		if (work == work->next)
			worker->works = NULL;
		else if (work->regular)
			worker->works = work->next;
		else {
			work->prev->next = work->next;
			work->next->prev = work->prev;
			worker->works = work->next;
		}
	}

	return NULL;
}

/* Management of workload */
int neo_add_work(void)
{
	return 0;
}

int neo_cancel_work(void)
{
	return 0;
}

#if 0
 46 3. management of work
 47    create
 48    destroy
 49    pause
 50    resume
 51    attach
 52    detach
#endif
