#include <stdio.h>
#include <stdlib.h>

#include <neuron.h>

/* Dummy pulse that only echos self->num. Should be generic */
int neuron_default_cb(struct neuron *self, const int in_cnt, void *in, int *out_cnt, void *out)
{
	printf("%d\n", self->num);
	return EXIT_SUCCESS;
}
