#include <stdio.h>
#include <stdlib.h>

#include <neuron.h>

/* Dummy pulse that only echos self->num. Should be generic */
/* Each data input activates a neuron or many, and outputs something, can be data, or device */
int neuron_default_cb(struct neuron *self, const int in_cnt, void *in, int *out_cnt, void *out)
{
	printf("%d\n", self->num);
	return EXIT_SUCCESS;
}
