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
#include <libfyaml.h>

#include "neo.h"
#include "neuron.h"

struct neo_cfg_s neo_cfg;
bool _quiet_quit = false;

static bool _validate_path(const char *path, mode_t type) {
	struct stat fst;
	bool retval = false;
	char *tmp_str = realpath(path, NULL);
	if (!tmp_str)
		pr_warn("Failed resolving %s: %s", path, strerror(errno));
	else if (stat(tmp_str, &fst))
		pr_err("Failed reading %s: %s", path, strerror(errno));
	else if (!(fst.st_mode & type))
		pr_err("%s is not a %s", path, (type == S_IFREG) ? "file" : "directory");
	else
		retval = true;
valpath_done:
	free(tmp_str);
	return retval;
}

static int neo_load_config(struct neo_cfg_s *cfg) {
	struct fy_document *fyd = NULL;

	/* Verify existence of config file */
	if (!cfg)
		return EXIT_FAILURE;
	else if (!cfg->cfg_file)
		cfg->cfg_file = NEO_CFG_FILE;
	if (!_validate_path(cfg->cfg_file, S_IFREG))
		return EXIT_FAILURE;

	/* Load config file into memory */
	fyd = fy_document_build_from_file(NULL, cfg->cfg_file);
	if (!fyd) {
		pr_err("Failed loading %s", cfg->cfg_file);
		return EXIT_FAILURE;
	}
	if (fy_document_scanf(fyd, "/neuron_dir %255s", cfg->neuron_dir) != 1)
		strncpy(cfg->neuron_dir, NEO_NEURON_DIR, NEO_PATH_MAX);
	else if (!_validate_path(cfg->neuron_dir, S_IFDIR))
		goto failed_fyd;

	if (fy_document_scanf(fyd, "/stage_file %255s", cfg->stage_file) != 1)
		cfg->stage_file[0] = 0;
	else if (!_validate_path(cfg->stage_file, S_IFREG)) {
		pr_warn("Invalid stage file: %s", cfg->stage_file);
		cfg->stage_file[0] = 0;
	}

	if (fy_document_scanf(fyd, "/workers %lu", &cfg->workers) != 1)
		cfg->workers = NEO_MIN_WORKERS;

	pr_info("Workers: %lu", cfg->workers);
	char db_type[NEO_VAR_MAX] = {0};
	if (fy_document_scanf(fyd, "/db/type %63s /db/host %63s /db/user %63s /db/passwd %63s",
			db_type, cfg->db.host, cfg->db.user, cfg->db.passwd) == 4) {
		if (!strncmp(db_type, "mysql", NEO_VAR_MAX))
			cfg->db.type = DB_MYSQL;
	} else {
		pr_warn("Invalid db settings");
		goto failed_fyd;
	}

	fy_document_destroy(fyd);
	return EXIT_SUCCESS;
failed_fyd:
	if (fyd) fy_document_destroy(fyd);
	return EXIT_FAILURE;
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
		pr_neuron("New %s", p->name);
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
		pr_err("Failed reading neuron_dir setting");
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
			pr_warn("loading of plugin %s failed.", file->d_name);
			continue;
		} else
			dlclose(handler);	// TODO: need means of free a plugin
	}
	closedir(dir_s);
	return EXIT_SUCCESS;
}

void _on_exit(void) {
	if (!_quiet_quit)
		pr_info("Cleaning up");
}

void neo_version(void) {
	pr_info("%s: %d.%d", PROJECT_NAME, NEO_VERSION_MAJOR, NEO_VERSION_MINOR);
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
	int opt, retval = EXIT_FAILURE;
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
			retval = EXIT_SUCCESS;
			_quiet_quit = true;
			goto final_ret;
		case 'n':
			strncpy(neo_cfg.neuron_dir, optarg, NEO_PATH_MAX);
			break;
		case 's':
			strncpy(neo_cfg.stage_file, optarg, NEO_PATH_MAX);
			break;
		case 'v':
			neo_version();
			retval = EXIT_SUCCESS;
			_quiet_quit = true;
			goto final_ret;
		default:
			continue;
		}
	}

	if (get_sysinfo(&neo_cfg.env) != EXIT_SUCCESS)
		goto final_ret;	// TODO: free allocs
	if (neo_load_config(&neo_cfg))
		goto final_ret;
	pr_info("Loaded configuration: %s", neo_cfg.cfg_file);

	// TODO: Devel
	/* initialization */
	neo_init(&neo_cfg);	// Init environment, includes modifying pid_max

	pr_info("Develop breakpoint");
	// load_net();		// load staged network

	// load local neuron type automatically
	// TODO: fanotify on neuron_dir for update/add/del
	if (neo_load_neurons()) {
		pr_err("loading of plugins failed");
		return EXIT_FAILURE;
	}
	sleep(1);

	neo_fini(&neo_cfg);
	retval = EXIT_SUCCESS;
final_ret:
	return retval;
}
