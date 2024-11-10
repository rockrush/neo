#ifndef NEO_NEURON_H
#define NEO_NEURON_H

#include <stdbool.h>

#include <neo.h>
#include <utils.h>

#define SPARE_RATIO			2
#define DEFAULT_ARRAY_SIZE	16

struct neuron_life {
	int age;
	int health;
	bool grow;
};

/* Can be local, or via RDMA, GPU-Direct */
struct neuron_exchange {
	//unsigned flags;
	uint16_t data_size;     /* Size of each data item */
	uint16_t bp_size;       /* Size of each bp item */
	uint32_t len;           /* item counts */
	int8_t state;           /* up to date, partial, done */

	/* data: |  input data of data_size[len]  | bp of bp_size[len]  | */
	/* As neuron_exchange could be accessed from several nodes, its address may vary,
	 * so we simply store an offset, and limit neuron_exchange to be physically continuous.
	 * It's not a problem if memory been properly mapped with userfaultfd. */
	uint64_t bp_offset;     /* bp_offset = data + data_size + len */
	char data[0];   /* Data should follow */
};

/* Should be the first element of actual Neuron implementation,
 * so container_of is simplified into type convertion */
struct neuron {
	int id;			/* We take memory address as neuron ID, seems to be not enough */
	const char *name;	/* optional, if we implemented hash table on neuron set */
	char *host;             /* TODO: Need a means of identify host */
	void *data_len;		/* Should be void *, for any kind of data, depending on what cb() needs */
	void *handler;
	struct neuron *prev;
	struct neuron *next;

	int (*cb)(struct neuron *, const int, void *, int *, void *);
	int (* neuron)(void);   /* 神经元的执行线程，负责数据处理 */
	int (* support)(struct environ_s *e);        /* if supported */

	/* Whether a neuron can change:
	 * CONSTANT	no any kind of change
	 * NORMAL	accepts any kind of change
	 * EDGE		cannot grow or die
	 * ACTOR	thread function cannot change
	 */
	enum {CONSTANT, NORMAL, EDGE, ACTOR, UNKNOWN} type;

	int hw;		/* CPU, CUDA, OpenCL, FPGA, ... */
	struct neuron_life life;
	struct neuron **in;	/* It's set, can be simplified as array */
	struct neuron **out;
	struct array_cap in_cnt;
	struct array_cap out_cnt;
	//struct neuron_exchange *din;
	//struct neuron_exchange *dout;
	//void *in_bp_addr;               /* neuron_exchange address may vary on different nodes */
	//void *out_bp_addr;              /* out_bp_addr = out->data + out->data_size * out->len */

	union {
		void *mem;
		int num;
	};
};

void _register(struct neuron *);
struct neuron *neuron_new(int num);
void neuron_connect(struct neuron *from, struct neuron *to);
void neuron_print(struct neuron *cur, bool recursive);

#endif
