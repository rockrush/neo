#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <getopt.h>
#include <string.h>
#include <config.h>
#include <libconfig.h>

#include "neo.h"
#include "neuron.h"
#include "environ.h"

struct neo_cfg_s neo_cfg;
bool _quiet_quit = false;

static bool _validate_path(const char *path, mode_t type) {
	struct stat fst;
	bool retval = false;
	char *tmp_str = realpath(path, NULL);
	if (!tmp_str)
		fprintf(stderr, "[%s] Failed resolving %s: %s.]\n", __func__, path, strerror(errno));
	else if (stat(tmp_str, &fst))
		printf("[%s] Failed reading %s: %s.\n", __func__, path, strerror(errno));
	else if (!(fst.st_mode & type))
		printf("[%s] %s is not a %s.\n", __func__, path, (type == S_IFREG) ? "file" : "directory");
	else
		retval = true;
valpath_done:
	free(tmp_str);
	return retval;
}

static int neo_load_config(struct neo_cfg_s *cfg) {
	config_t libcfg;
	struct stat fst;
	const char *tmp_str = NULL;
	int tmp_int = 0;
	int retval = CONFIG_TRUE;
	config_setting_t *tmp_cfg = NULL;

	/* Verify existence of config file */
	if (!cfg)
		return EXIT_FAILURE;
	else if (!cfg->cfg_file)
		cfg->cfg_file = NEO_CFG_FILE;
	if (!_validate_path(cfg->cfg_file, S_IFREG))
		return EXIT_FAILURE;

	/* Load config file into memory */
	config_init(&libcfg);
	if (config_read_file(&libcfg, cfg->cfg_file) != CONFIG_TRUE) {
		fprintf(stderr, "[%s] %s:%d - %s\n", __func__, config_error_file(&libcfg),
			config_error_line(&libcfg), config_error_text(&libcfg));
		config_destroy(&libcfg);
		return EXIT_FAILURE;
	}

	// Fill neo_cfg with config file
	/* Priority: commandline argument > config file > default value */
	if (!cfg->neuron_dir) {
		retval = config_lookup_string(&libcfg, "neuron_dir", &tmp_str);
		cfg->neuron_dir = (retval == CONFIG_TRUE) ? strdup(tmp_str) : strdup(NEO_NEURON_DIR);
	}
	if (!_validate_path(cfg->neuron_dir, S_IFDIR))
		return EXIT_FAILURE;

	if (!cfg->stage_file) {
		if (config_lookup_string(&libcfg, "stage_file", &tmp_str) == CONFIG_TRUE)
			cfg->stage_file = strdup(tmp_str);
	}
	if ((cfg->stage_file) && (!_validate_path(cfg->stage_file, S_IFREG)))
		return EXIT_FAILURE;

	/* Default: one worker for each logical CPU core */
	config_lookup_int(&libcfg, "workers", &cfg->wpool.max);
	if (cfg->wpool.max < 0) {
		fprintf(stderr, HDR_ERR "Invalid workers setting: %d\n", cfg->wpool.max);
		return EXIT_FAILURE;
	}

	if (config_lookup_string(&libcfg, "db.type", &tmp_str) == CONFIG_TRUE)
		cfg->db.type = strncmp(tmp_str, "mysql", 6) ? DB_UNKNOWN : DB_MYSQL;
	config_lookup_string(&libcfg, "db.host", &tmp_str);
		cfg->db.host = strdup(tmp_str);
	config_lookup_string(&libcfg, "db.user", &tmp_str);
		cfg->db.user = strdup(tmp_str);
	config_lookup_string(&libcfg, "db.passwd", &tmp_str);
		cfg->db.passwd = strdup(tmp_str);
	config_destroy(&libcfg);
	return EXIT_SUCCESS;
}

static void neo_destroy_config(struct neo_cfg_s *cfg) {
	if (!cfg)
		return;
	if (cfg->db.passwd) {
		free((void *)cfg->db.passwd);
		cfg->db.passwd = NULL;
	}
	if (cfg->db.user) {
		free((void *)cfg->db.user);
		cfg->db.user = NULL;
	}
	if (cfg->db.host) {
		free((void *)cfg->db.host);
		cfg->db.host = NULL;
	}
	if (cfg->stage_file) {
		free((void *)cfg->stage_file);
		cfg->stage_file = NULL;
	}
	if (cfg->neuron_dir) {
		free((void *)cfg->neuron_dir);
		cfg->neuron_dir = NULL;
	}
}

// 供动态库使用的注册函数
void _register(struct neuron *p) {
	int supported = 0;
	if (p->support)
		supported = p->support(&neo_cfg.env);

	if (supported) {
		neo_cfg.plugin_cnt += 1;
		p->prev = neo_cfg.plugins;
		neo_cfg.plugins = p;

		printf(HDR_NEURON "New %s\n", p->name);
	}
}

static int neo_load_neurons(void)
{
	void *handler = NULL;
	struct dirent *file = NULL;
	char plugin_file[PATH_MAX];
	char *file_ext = NULL;
	DIR *dir_s = NULL;

	if (!neo_cfg.neuron_dir) {
		printf(HDR_ERR "Failed reading neuron_dir setting\n");
		return EXIT_FAILURE;
	}

	dir_s = opendir(neo_cfg.neuron_dir);
	if (dir_s == NULL)
		return EXIT_FAILURE;

	while ((file = readdir(dir_s)) != NULL) {
		sprintf(plugin_file, "%s/%s", neo_cfg.neuron_dir, file->d_name);
		file_ext = strrchr(plugin_file, '.');
		if (file_ext == NULL)
			continue;
		else if (strcmp(file_ext, ".neo"))
			continue;
		handler = dlopen(plugin_file, RTLD_LAZY);
		if (handler == NULL) {
			printf(HDR_ERR "loading of plugin %s failed.\n", file->d_name);
			continue;
		} else
			dlclose(handler);	// TODO: need means of free a plugin
	}
	closedir(dir_s);
	return EXIT_SUCCESS;
}

void _on_exit(void) {
	if (!_quiet_quit)
		printf(HDR_NOTE "Cleaning up\n");
}

void neo_version(void) {
	printf("%s: %d.%d\n", PROJECT_NAME, NEO_VERSION_MAJOR, NEO_VERSION_MINOR);
}

void neo_help(void) {
	neo_version();
	printf(
"	-c, --config <PATH>		path to config file\n"
"	-h, --help			this help info\n"
"	-n, --neurons <PATH>		path to neuron type files\n"
"	-s, --stage <PATH>		path to saved network stage file\n"
"	-v, --version			program version\n");
}

int main(int argc, char *argv[]) {
	int opt, retval = EXIT_SUCCESS;
	const char *short_options = "c:hn:sv";
	struct option long_options[] = {
		{"config",	required_argument,	NULL,	'c'},
		{"help",	no_argument,		NULL,	'h'},
		{"neurons",	required_argument,	NULL,	'n'},
		{"stage",	required_argument,	NULL,	's'},
		{"version",	no_argument,		NULL,	'v'},
		{ NULL, 0, NULL, 0 }
	};

	atexit(&_on_exit);
	memset(&neo_cfg, 0, sizeof(struct neo_cfg_s));

	// Parse commandline
	while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
		switch(opt) {
		case 'c':
			neo_cfg.cfg_file = optarg;
			break;
		case 'h':
			neo_help();
			_quiet_quit = true;
			goto clear_cfg;
		case 'n':
			neo_cfg.neuron_dir = strdup(optarg);
			break;
		case 's':
			neo_cfg.stage_file = strdup(optarg);
			break;
		case 'v':
			neo_version();
			_quiet_quit = true;
			goto clear_cfg;
		default:
			continue;
		}
	}

	if (get_sysinfo(&neo_cfg.env) != EXIT_SUCCESS)
		goto clear_cfg;	// TODO: free allocs
	if (neo_load_config(&neo_cfg))
		goto clear_cfg;
	printf(HDR_NOTE "Loaded configuration: %s\n", neo_cfg.cfg_file);


	// TODO: Devel
	/* initialization */
	neo_init(&neo_cfg);	// Init environment, includes modifying pid_max

	printf(HDR_NOTE "Develop breakpoint\n");
	// load_net();		// load staged network

	// load local neuron type automatically
	// TODO: fanotify on neuron_dir for update/add/del
	if (neo_load_neurons()) {
		perror(HDR_ERR "loading of plugins failed");
		return EXIT_FAILURE;
	}
	sleep(1);

	// thread poll
	//	controller thread: processes data IO among nodes, epoll?
	//	and worker threads: process data IO in node
	//		each thread maintains load status 2-D array, and pick the most idle one for next hop;
	//		load status is read by any thread, and writen by only owner thread (rwlock);
	//		load status array is in several load level(time percentage), where to put a thread is only
	//		determined by load status value, no sorting, no frequent moving
	//
	//		{start_level(int), levels[MAX_LEVEL]}
	//			new tasks dispatched to the 1st of start_level, making the others idle, and been moved
	//			to lower level

	neo_fini(&neo_cfg);
clear_cfg:
	neo_destroy_config(&neo_cfg);
final_ret:
	return retval;
}
