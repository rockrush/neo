#ifndef _HAS_NEO_H
#define _HAS_NEO_H

#include <stdint.h>
#include <libconfig.h>
#include <worker.h>

#define NEO_INIT	__attribute__((constructor))
#define NEO_FINI	__attribute__((destructor))

#define HDR_ERR		"\033[1;31m[ ERROR]\033[0m "
#define HDR_NOTE	"\033[1;32m[  NOTE]\033[0m "
#define HDR_TRACE	"\033[1;32m[ TRACE]\033[0m "
#define HDR_NEURON	"\t\033[1;42mNEURON\033[0m "

#define NEW_NEURON(neuron_name, thread, check_support)	\
struct neuron _config = {		\
	.name = neuron_name,		\
	.support = check_support,	\
	.neuron = thread,		\
	.prev = NULL,			\
	.next = NULL,			\
};	\
void NEO_INIT __init_##neoron_name(void) {_register(&_config);} \
void NEO_FINI __fini_##neoron_name(void) {} \

#define ARCH_MAXLEN	16

enum db_types {
	DB_MYSQL = 1,
	DB_UNKNOWN
};

struct environ_s {
	uint16_t lcores;
	int kern_version;
	char kern_ver[12];
	char arch[ARCH_MAXLEN];
	int accel_devs;	/* TODO: Should be a list of devices */
};

struct db_s {
	enum db_types type;
	const char *host;
	const char *user;
	const char *passwd;
};

struct neo_cfg_s {
	const char *neuron_dir;

	struct neo_wpool_s wpool;
	struct db_s db;
	struct environ_s env;
	struct neuron *plugins;
	int plugin_cnt;
	const char *cfg_file;
	const char *stage_file;
};

int neo_init(struct neo_cfg_s *cfg);
int neo_fini(struct neo_cfg_s *cfg);

int neo_wpool_init(struct neo_cfg_s *cfg);
int neo_wpool_fini(struct neo_cfg_s *cfg);
enum neo_state_cmd neo_wpool_migrate(struct neo_cfg_s *cfg, enum neo_state_cmd cmd);

struct neo_worker_s *neo_worker_new(struct neo_wpool_s *pool);
struct neo_worker_s *neo_worker_lru(struct neo_wpool_s *pool);
int neo_worker_fade(struct neo_wpool_s *pool, struct neo_worker_s *worker);

#endif	/* _HAS_NEO_H */
