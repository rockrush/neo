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
#include <json-c/json.h>

#include "include/neo.h"
#include "include/linux.h"

struct host sys_info;
struct neuron *plugins_head = NULL;
const char *neuron_dir = NULL;

struct neo_cfg_s neo_cfg = {
	.neuron_dir = "neurons",
	.db_type = "mysql"
};

//供动态库使用的注册函数
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

void _on_exit(void) {
	printf(HDR_NOTE "Cleaning up\n");
}

static int load_config(const char *cfg_file) {
	json_object *neo_cfg_json = NULL;
	struct json_object *cur = NULL, *db_cfg = NULL;
	const char *cfg_path = cfg_file;
	if (!cfg_path)
		cfg_path = NEO_CFG_FILE;
	if (!cfg_path) {
		printf(HDR_ERR "Config file path not specified.\n");
		return EXIT_FAILURE;
	}

	neo_cfg_json = json_object_from_file(cfg_path);
	if (!neo_cfg_json) {
		printf(HDR_ERR "Unkown error reading config: %s.\n", cfg_path);
		return EXIT_FAILURE;
	}

	json_object_object_get_ex(neo_cfg_json, "neuron_dir", &cur);
	if (json_object_get_string(cur))
		neo_cfg.neuron_dir = realpath(json_object_get_string(cur), NULL);
	printf(HDR_NOTE "Neuron dir: %s\n", neo_cfg.neuron_dir);

	json_object_object_get_ex(neo_cfg_json, "db_type", &cur);
	if (json_object_get_string(cur))
		neo_cfg.db_type = json_object_get_string(cur);

	json_object_object_get_ex(neo_cfg_json, neo_cfg.db_type, &db_cfg);
	if (!db_cfg) {
		printf(HDR_ERR "Database config not found\n");
		json_object_put(neo_cfg_json);
		return EXIT_FAILURE;
	}

	for (int i = 0; i < 4; i++)
		neo_cfg.db_opts[i] = NULL;
	if (strncmp(neo_cfg.db_type, "mysql", 6) == 0) {
		json_object_object_get_ex(db_cfg, "host", &cur);
		if (cur)
			neo_cfg.db_opts[0] = json_object_get_string(cur);
		json_object_object_get_ex(db_cfg, "user", &cur);
		if (cur)
			neo_cfg.db_opts[1] = json_object_get_string(cur);
		json_object_object_get_ex(db_cfg, "passwd", &cur);
		if (cur)
			neo_cfg.db_opts[2] = json_object_get_string(cur);
	}

	json_object_put(neo_cfg_json);
	return EXIT_SUCCESS;
}

static int load_neurons(void)
{
	void *handler = NULL;
	struct dirent *file = NULL;
	char plugin_file[PATH_MAX];
	char *file_ext = NULL;
	DIR *dir_s = opendir(neo_cfg.neuron_dir);
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
		}
	}
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
	int retval, opt;
	char *plugins_dir, *optarg = NULL;
	const char *short_options = "p:";
	struct option long_options[] = {
		{"plugin", required_argument, NULL, 'p'},
		{ NULL, 0, NULL, 0 }
	};

	/* initialization */
	get_sysinfo(&sys_info);
	printf(HDR_NOTE "System info: Linux %s, CPU arch: %s\n", sys_info.kern_ver, sys_info.arch);

	atexit(&_on_exit);
	while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
		switch(opt) {
		case 'p':	plugins_dir = optarg; break;
		default:	continue;
		}
	}

	retval = load_config(NULL);
	if (retval)
		return EXIT_FAILURE;

	retval = load_neurons();
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
