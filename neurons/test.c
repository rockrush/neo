#include <stdio.h>
#include <stdlib.h>
#include "../include/neo.h"

/* 返回 1 表示该模块支持当前系统 */
static int support(struct host *p) {
	if (p->kern_version < 330240)
		return -1;
	// if (p->distro & LOCAL_DISTRO_MASK = NUM)
	//	do_something();
	return 1;
}

/* 测试神经元 */
static int test_thread(void) {
	return 0;
} 

NEW_NEURON("test", test_thread, support);
