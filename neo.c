#include <stdlib.h>
#include <limits.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <getopt.h>
#include <string.h>
#include <config.h>
#include <libconfig.h>

#include "include/neo.h"
#include "include/linux.h"

struct host sys_info;
struct neuron *plugins_head = NULL;
config_setting_t *neo_cfg;

static int load_config(const char *cfg_file) {
	config_t libcfg;

	if (!cfg_file) {
		printf("[%s] Config file not specified.\n", __func__);
		return EXIT_FAILURE;
	}

	config_init(&libcfg);
	if (!config_read_file(&libcfg, cfg_file)) {
		fprintf(stderr, "[%s] %s:%d - %s\n", __func__, config_error_file(&libcfg),
			config_error_line(&libcfg), config_error_text(&libcfg));
		config_destroy(&libcfg);
		return EXIT_FAILURE;
	}

	neo_cfg = config_root_setting(&libcfg);
	return EXIT_SUCCESS;
}

// 供动态库使用的注册函数
void _register(struct neuron *p) {
	int supported = 0;
	if (p->support)
		supported = p->support(&sys_info);

	if (supported) {
		sys_info.plugins += 1;
		p->prev = plugins_head;
		plugins_head = p;

		printf(HDR_NOTE "[NEW NEURON] %s\n", p->name);
	}
}

static int load_neurons(void)
{
	void *handler = NULL;
	struct dirent *file = NULL;
	char plugin_file[PATH_MAX];
	const char *neuron_dir = NULL;
	char *file_ext = NULL;
	DIR *dir_s = NULL;

	config_setting_lookup_string(neo_cfg, "neuron_dir", &neuron_dir);
	if (!neuron_dir) {
		printf(HDR_ERR "Failed reading neuron_dir setting\n");
		return EXIT_FAILURE;
	}

	dir_s = opendir(neuron_dir);
	if (dir_s == NULL)
		return EXIT_FAILURE;

	while ((file = readdir(dir_s)) != NULL) {
		sprintf(plugin_file, "%s/%s", neuron_dir, file->d_name);
		file_ext = strrchr(plugin_file, '.');
		if (file_ext == NULL)
			continue;
		else if (strcmp(file_ext, ".neo"))
			continue;
		handler = dlopen(plugin_file, RTLD_LAZY);
		if (handler == NULL) {
			printf(HDR_ERR "loading of plugin %s failed.\n", file->d_name);
			continue;
		}
	}
	return EXIT_SUCCESS;
}

void _on_exit(void) {
	printf(HDR_NOTE "Cleaning up\n");
}

void neo_version(void) {
	printf("%s: %d.%d\n", PROJECT_NAME, NEO_VERSION_MAJOR, NEO_VERSION_MINOR);
}

void neo_help(void) {
	neo_version();
	printf(
"	-c, --config <PATH>		path of config file\n"
"	-n, --neurons <PATH>		path of neuron type files\n"
"	-h, --help			this help info\n"
"	-v, --version			program version\n");
}

int main(int argc, char *argv[]) {
	int retval, opt;
	char *neuron_dir = NULL, *optarg = NULL;
	const char *short_options = "c:hn:v";
	const char *cfg_file = NEO_CFG_FILE;
	struct option long_options[] = {
		{"config", required_argument, NULL, 'c'},
		{"help", no_argument, NULL, 'h'},
		{"neurons", required_argument, NULL, 'n'},
		{"version", no_argument, NULL, 'v'},
		{ NULL, 0, NULL, 0 }
	};
	config_setting_t *cfg_temp = NULL;

	atexit(&_on_exit);

	// 1. Parse commandline
	while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
		switch(opt) {
		case 'c':	cfg_file = optarg; break;
		case 'h':	neo_help(); exit(EXIT_SUCCESS);
		case 'n':	neuron_dir = optarg; break;
		case 'v':	neo_version(); exit(EXIT_SUCCESS);
		default:	continue;
		}
	}

	// 2. Reads config file
	retval = load_config(cfg_file);
	if (retval)
		return EXIT_FAILURE;

	// 3. Push commandline
	if (neuron_dir) {
		cfg_temp = config_setting_get_member(neo_cfg, "neuron_dir");
		if (!cfg_temp) {
			printf(HDR_NOTE "neuron_dir setting not found\n");
			cfg_temp = config_setting_add(neo_cfg, "neuron_dir", CONFIG_TYPE_STRING);
		}
		config_setting_set_string(cfg_temp, neuron_dir);
	}


	/* initialization */
	// neo_init();		// Init environment, includes modifying pid_max
	get_sysinfo(&sys_info);
	printf(HDR_NOTE "System info: Linux %s, CPU arch: %s\n", sys_info.kern_ver, sys_info.arch);

	// load_net();		// load staged network

	retval = load_neurons();	// load neuron type automatically
	if (retval) {
		perror(HDR_ERR "loading of plugins failed");
		return EXIT_FAILURE;
	}

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

	return EXIT_SUCCESS;
}
