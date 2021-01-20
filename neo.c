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

struct support sys_info;
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

		printf("\t[NEW NEURON] %s\n", p->name);
	}
}

void _on_exit(void) {
	printf("\tCleaning up\n");
}

static int load_config(const char *cfg_file) {
	json_object *neo_cfg_json = NULL;
	struct json_object *cur = NULL, *db_cfg = NULL;
	const char *cfg_path = cfg_file;
	if (!cfg_path)
		cfg_path = NEO_CFG_FILE;
	if (!cfg_path) {
		printf("[ERROR] Config file path not specified.\n");
		return EXIT_FAILURE;
	}

	neo_cfg_json = json_object_from_file(cfg_path);
	if (!neo_cfg_json) {
		printf("[ERROR] Unkown error reading config: $s.\n", cfg_path);
		return EXIT_FAILURE;
	}

	json_object_object_get_ex(neo_cfg_json, "neuron_dir", &cur);
	if (json_object_get_string(cur))
		neo_cfg.neuron_dir = realpath(json_object_get_string(cur), NULL);
	printf("Neuron dir: %s\n", neo_cfg.neuron_dir);

	json_object_object_get_ex(neo_cfg_json, "db_type", &cur);
	if (json_object_get_string(cur))
		neo_cfg.db_type = json_object_get_string(cur);

	json_object_object_get_ex(neo_cfg_json, neo_cfg.db_type, &db_cfg);
	if (!db_cfg) {
		printf("[ERROR] Database config not found\n");
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
			printf("[WARNING] loading of plugin %s failed.\n", file->d_name);
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
	sys_info.plugins = 0;
	sys_info.kern_ver = 200000;

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
		perror("[ERROR] loading of plugins failed");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
