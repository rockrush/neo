#ifndef _HAS_NEO_H
#define _HAS_NEO_H

#define NEO_INIT	__attribute__((constructor))
#define NEO_FINI	__attribute__((destructor))

#define NEW_NEURON(neuron_name, thread, check_support)	\
struct plugin config = {		\
	.name = neuron_name,		\
	.support = check_support,	\
	.neuron = thread,			\
};	\
void NEO_INIT __init_##neoron_name(void) {_register(&config);}

struct support {
	int kern_ver;
	long long distro;	/* support 64 distros */
	int plugins;
};

struct plugin {
	char *name;
	int (* support)(struct support *p);	/* 是否支持当前系统 */
	int (* neuron)(void);	/* 神经元的执行线程 */
	int hw;		/* CPU, CUDA, OpenCL, FPGA, ... */
	struct plugin *prev;
};

void _register(struct plugin *);
#endif	/* _HAS_PLUGINS_H */
