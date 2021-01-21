#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <linux/version.h>

#include "../include/neo.h"

void get_sysinfo(struct support *info)
{
	struct utsname version;
	int ver_major, ver_minor, ver_patch;

	uname(&version);
	sscanf(version.release, "%d.%d.%d", &ver_major, &ver_minor, &ver_patch);
	info->kern_ver = KERNEL_VERSION(ver_major, ver_minor, ver_patch);
	info->plugins = 0;
	strncpy(info->arch, version.machine, ARCH_MAXLEN);
	info->arch[ARCH_MAXLEN - 1] = 0;
}
