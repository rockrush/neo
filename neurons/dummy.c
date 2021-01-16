#include <stdio.h>
#include <stdlib.h>

/* 包含了基本数据的定义 */
#include "../include/neo.h"

static int dummy_thread(void) {
	return 0;
} 

static int support(struct support *p) {
	return 1;
}

NEW_NEURON("dummy", dummy_thread, support);

void NEO_INIT init(void) {
	printf("Extra init code\n");
}
