#include <errno.h>
#include <sys/stat.h>
#include <getopt.h>
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
	else if (!_validate_path(cfg->stage_file, S_IFREG))
		cfg->stage_file[0] = 0;

	if (fy_document_scanf(fyd, "/workers %hu", &cfg->runner.startup) != 1)
		cfg->runner.startup = NEO_WORKERS_MIN;
	if (fy_document_scanf(fyd, "/queue %u", &cfg->runner.queue_size) != 1)
		cfg->runner.queue_size = NEO_WORKQUEUE_SIZE;

	pr_info("Workers: %u", cfg->runner.startup);
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
		pr_neo("New %s", p->name);
	}
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

// 一些测试任务
static _Atomic(int) g_done = 0;

static void busy_task(void *arg) {
	(void)arg;
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 200 * 1000000L }; /* 3ms */
	nanosleep(&ts, NULL);
	atomic_fetch_add(&g_done, 1);
}

/* 故意让工作线程异常退出，用于演示自动拉起 */
static void suicidal_task(void *arg) {
	(void)arg;
	fprintf(stderr, "[task] 触发 pthread_exit(), 模拟线程异常\n");
	pthread_exit(NULL);
}

static void test_task(void *arg) {
	int id = (int)(intptr_t)arg;
	printf("[task %d] test on %zu\n", id, pthread_self());
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 50 * 1000000L }; /* 3ms */
	nanosleep(&ts, NULL);
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
		goto final_ret;
	if (neo_load_config(&neo_cfg))
		goto final_ret;
	pr_info("Loaded configuration: %s", neo_cfg.cfg_file);

	neo_init(&neo_cfg);	// Init environment, includes modifying pid_max

	pr_neo("Develop breakpoint");
	// load_net();		// load staged network

	// load local neuron type automatically
	// TODO: fanotify on neuron_dir for update/add/del
	if (neo_load_neurons(&neo_cfg))
		return EXIT_FAILURE;

//====== worker pool testing ===================================================
	struct timespec s = { .tv_sec = 1, .tv_nsec = 0 };
	/* --- 1) 基础提交 --- */
	pr_info(">>> 提交 1000 个任务");
	for (int i = 0; i < 1000; ++i)
		runner_submit(&neo_cfg.runner, busy_task, (void *)(intptr_t)i);
	nanosleep(&s, NULL);
	runner_stats(&neo_cfg.runner);

	/* --- 2) 暂停 --- */
	pr_info(">>> 暂停线程池，尝试提交任务应失败");
	runner_pause(&neo_cfg.runner);
	nanosleep(&s, NULL);
	bool ok = runner_submit(&neo_cfg.runner, busy_task, NULL);
	pr_warn("暂停中提交结果: %s", ok ? "成功" : "被拒绝");

	/* --- 3) 恢复 --- */
	pr_info(">>> 恢复线程池");
	runner_resume(&neo_cfg.runner);
	for (int i = 0; i < 200; ++i)
		runner_submit(&neo_cfg.runner, busy_task, (void *)(intptr_t)i);

	/* --- 4) 异常线程自动拉起 --- */
	pr_info(">>> 触发异常线程, 观察自动拉起");
	runner_submit(&neo_cfg.runner, suicidal_task, NULL);
	nanosleep(&s, NULL);
	runner_submit(&neo_cfg.runner, busy_task, NULL);
	nanosleep(&s, NULL);
	runner_stats(&neo_cfg.runner);

	/* --- 5) 动态扩容 --- */
	const uint16_t busy_cnt = 4000;
	pr_info(">>> 提交 %u 个耗时任务, 触发扩容", busy_cnt);
	atomic_store(&g_done, 0);
	for (int i = 0; i < busy_cnt; ++i)
		runner_submit(&neo_cfg.runner, busy_task, (void *)(intptr_t)i);

	struct timespec ms = { .tv_sec = 0, .tv_nsec = 50 * 1000000L };
	nanosleep(&ms, NULL);
	runner_stats(&neo_cfg.runner);
	pr_info("扩容后线程数: %u", atomic_load(&neo_cfg.runner.w_cnt));

	while (atomic_load(&g_done) < busy_cnt) nanosleep(&ms, NULL);
	pr_info("全部任务完成");

	/* --- 6) 缩容 --- */
	pr_info(">>> 等待缩容");
	nanosleep(&s, NULL);
	runner_stats(&neo_cfg.runner);

	/* --- 7) 重置 --- */
	pr_info(">>> 清空队列");
	runner_reset(&neo_cfg.runner);
	struct timespec s8 = { .tv_sec = 8, .tv_nsec = 0 };
	nanosleep(&s8, NULL);
	runner_stats(&neo_cfg.runner);
//====== worker pool testing ===================================================

	neo_fini(&neo_cfg);
	retval = EXIT_SUCCESS;
final_ret:
	return retval;
}
