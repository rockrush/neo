#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <linux/version.h>
#include <libconfig.h>

#include "../include/neo.h"

int get_sysinfo(struct environ_s *env)
{
	struct utsname version;
	int ver_major, ver_minor, ver_patch;
	int cpu_cores = 0;

	uname(&version);
	sscanf(version.release, "%d.%d.%d", &ver_major, &ver_minor, &ver_patch);
	env->kern_version = KERNEL_VERSION(ver_major, ver_minor, ver_patch);
	snprintf(env->kern_ver, 12, "%d.%d.%d", ver_major, ver_minor, ver_patch);
	strncpy(env->arch, version.machine, ARCH_MAXLEN);
	env->arch[ARCH_MAXLEN - 1] = 0;

	env->lcores = get_nprocs();	// May change at runtime
	if (env->lcores < 1)	// How can it be possible ?
		return EXIT_FAILURE;
	printf(HDR_NOTE "System info: Linux %s, CPU arch: %s, threads: %d\n",
		env->kern_ver, env->arch, env->lcores);
	return EXIT_SUCCESS;
}
