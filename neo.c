#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <getopt.h>
#include <string.h>
#include "include/neo.h"
#include <config.h>

struct support sys_info;
struct plugin *plugins_head = NULL;

//供动态库使用的注册函数
void _register(struct plugin *p) {
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

static int load_plugins(char *dir)
{
	DIR *dir_s = NULL;
	void *handler = NULL;
	struct dirent *file = NULL;
	char plugin_file[PATH_MAX];
	char *file_ext = NULL;

	/* check for necessary vars */
	if (dir == NULL)
		dir = NEURON_DIR;	/* take "plugins/" as default */

	dir_s = opendir(dir);
	if (dir_s == NULL)
		return EXIT_FAILURE;

	while ((file = readdir(dir_s)) != NULL) {
		sprintf(plugin_file, "%s/%s", dir, file->d_name);
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

	while ((opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
		switch(opt) {
		case 'p':	plugins_dir = optarg; break;
		default:	continue;
		}
	}

	retval = load_plugins(plugins_dir);
	if (retval) {
		perror("[ERROR] loading of plugins failed");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
