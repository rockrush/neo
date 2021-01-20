#ifndef _HAS_NEO_H
#define _HAS_NEO_H

#define NEO_INIT	__attribute__((constructor))
#define NEO_FINI	__attribute__((destructor))

#define NEW_NEURON(neuron_name, thread, check_support)	\
struct neuron _config = {		\
	.name = neuron_name,		\
	.support = check_support,	\
	.neuron = thread,			\
};	\
void NEO_INIT __init_##neoron_name(void) {_register(&_config);}

struct support {
	int kern_ver;
	long long distro;	/* support 64 distros */
	int plugins;
};

struct neuron {
	char *name;
	int (* support)(struct support *p);	/* 是否支持当前系统 */
	int (* neuron)(void);	/* 神经元的执行线程 */
	int hw;		/* CPU, CUDA, OpenCL, FPGA, ... */
	struct neuron *prev;
};

struct neo_cfg_s {
	const char *neuron_dir;
	const char *db_type;
	const char *db_opts[4];	/* TODO: hope 4 is enough */
};

void _register(struct neuron *);
#endif	/* _HAS_PLUGINS_H */
