#include <stdio.h>
#include <stdlib.h>
#include "../include/neo.h"
#include "../include/neuron.h"

/* 返回 1 表示该模块支持当前系统 */
static int support(struct environ_s *p) {
	if (p->kern_version < 330240)
		return -1;
	// if (p->distro & LOCAL_DISTRO_MASK = NUM)
	//	do_something();
	return 1;
}

/* 示例神经元 */
static int sample_thread(void) {
	return 0;
} 

NEW_NEURON("sample", sample_thread, support);
