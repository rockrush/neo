#ifndef _NEO_WORKER_H
#define _NEO_WORKER_H

#include <stdbool.h>

enum neo_state_cmd {
	NEO_NORMAL,
	NEO_PAUSE,
	NEO_CONST,
	NEO_FADE,
	NEO_IDLE,
	NEO_UNKNOWN
};

// func should be general, in/out should be designed for least memcopy
struct neo_work_s {
	// TODO: tasklet description
	uint64_t (*exec)(struct neo_work_s *self);
	struct neo_work_s *prev, *next;
	bool regular;	// Whether to run again and again

	void *in;
	void *out;
	void *args;
};

struct neo_worker_s {
	pthread_t thread;
	struct neo_worker_s *prev, *next;
	pthread_attr_t attr;
	uint16_t weight;	// for tasklet scheduling
	uint8_t ctrl;		// do not change inside worker
	uint8_t status;		// do not change outside worker

	// Take an "idle" tasklet, when no more tasklet left in queue.
	// Added to tail, and executed from head,
	struct neo_work_s *works;	// ring of attached works

	// worker and net_pulse() may modify *tasks
	// Could this lock be eliminated ?
	pthread_mutex_t lock;
};

struct neo_wpool_s {
	int max;
	int cur;
	pthread_t mgr;
	struct neo_worker_s *workers;
	struct neo_work_s *works;	// tasks are sent to pool
	uint8_t status; // normal, pause
	uint8_t migrate;
};

#endif
