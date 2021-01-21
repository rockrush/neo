#ifndef _HAS_NEO_H
#define _HAS_NEO_H

#include <stdint.h>

#define NEO_INIT	__attribute__((constructor))
#define NEO_FINI	__attribute__((destructor))

#define HDR_ERR		"\033[1;31m[ERROR]\033[0m "
#define HDR_NOTE	"\033[1;32m[ NOTE]\033[0m "
#define HDR_TRACE	"\033[1;32m[TRACE]\033[0m "

#define NEW_NEURON(neuron_name, thread, check_support)	\
struct neuron _config = {		\
	.name = neuron_name,		\
	.support = check_support,	\
	.neuron = thread,			\
};	\
void NEO_INIT __init_##neoron_name(void) {_register(&_config);}

#define ARCH_MAXLEN	16

struct host {
	int kern_ver;
	char arch[ARCH_MAXLEN];
	int plugins;
};

/* Can be local, or via RDMA, GPU-Direct */
struct neuron_exchange {
	//unsigned flags;
	uint16_t data_size;	/* Size of each data item */
	uint16_t bp_size;	/* Size of each bp item */
	uint32_t len;		/* item counts */
	int8_t state;		/* up to date, partial, done */

	/* data: |  input data of data_size[len]  | bp of bp_size[len]  | */
	/* As neuron_exchange could be accessed from several nodes, its address may vary,
	 * so we simply store an offset, and limit neuron_exchange to be physically continuous.
	 * It's not a problem if memory been properly mapped with userfaultfd. */
	uint64_t bp_offset;	/* bp_offset = data + data_size + len */
	char data[0];	/* Data should follow */
};

/* Neuron is the key concept of Neo */
struct neuron {
	char *name;
	char *host;		/* TODO: Need a means of identify host */
	struct neuron *prev;
	struct neuron *next;
	int (* support)(struct host *p);	/* if supported */
	int hw;		/* CPU, CUDA, OpenCL, FPGA, ... */


	int (* neuron)(void);	/* 神经元的执行线程，负责数据处理 */
	struct neuron_exchange *in;
	struct neuron_exchange *out;
	void *in_bp_addr;		/* neuron_exchange address may vary on different nodes */
	void *out_bp_addr;		/* out_bp_addr = out->data + out->data_size * out->len */
};

struct neo_cfg_s {
	const char *neuron_dir;
	const char *db_type;
	const char *db_opts[4];	/* TODO: hope 4 is enough */
};

void _register(struct neuron *);
#endif	/* _HAS_PLUGINS_H */
