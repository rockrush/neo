#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <dlfcn.h>
#include <dirent.h>

#include "neo.h"
#include "neuron.h"

#if 0
int _demo_main(void)
{
	struct neuron *sensor = neuron_new(4);
	struct neuron *anal = neuron_new(9);
	struct neuron *strip = neuron_new(963);
	struct neuron *act = neuron_new(1);
	struct neuron *cur_neuron = sensor;

/* sensor -> anal -> strip --> act
 *    \--------------------/
 */
	neuron_connect(sensor, anal);
	neuron_connect(anal, strip);
	neuron_connect(strip, act);
	neuron_connect(sensor, act);

	neuron_print(sensor, true);
	return EXIT_SUCCESS;
}
#endif

int neo_init(struct neo_cfg_s *cfg)
{
	if (!cfg) return EXIT_FAILURE;
	int retval = runner_create(&cfg->runner);
	if (retval)
		return retval;
	return EXIT_SUCCESS;
}

int neo_fini(struct neo_cfg_s *cfg)
{
	runner_destroy(&cfg->runner);
	return EXIT_SUCCESS;
}

int neo_load_neurons(struct neo_cfg_s *cfg)
{
	void *handler = NULL;
	struct dirent *file = NULL;
	char plugin_file[PATH_MAX];
	char *file_ext = NULL;
	DIR *dir_s = NULL;

	if (!cfg->neuron_dir) {
		pr_err("Failed reading neuron_dir setting");
		return EXIT_FAILURE;
	}

	dir_s = opendir(cfg->neuron_dir);
	if (dir_s == NULL) {
		pr_err("Failed opening neuron_dir: %s", cfg->neuron_dir);
		return EXIT_FAILURE;
	}

	while ((file = readdir(dir_s)) != NULL) {
		sprintf(plugin_file, "%s/%s", cfg->neuron_dir, file->d_name);
		file_ext = strrchr(plugin_file, '.');
		if ((file_ext == NULL) || strcmp(file_ext, ".neo"))
			continue;
		handler = dlopen(plugin_file, RTLD_LAZY);
		if (handler == NULL) {
			pr_warn("loading of plugin %s failed.", file->d_name);
			continue;
		} else
			dlclose(handler);       // TODO(emerg): free a plugin, not here
	}
	closedir(dir_s);
	return EXIT_SUCCESS;
}
