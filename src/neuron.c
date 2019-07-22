#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <config.h>
#include <neuron.h>
#include <pulse.h>
#include <utils.h>

/* Core neuron members, a typical neuron should be super set of it, recall container_of ? */
struct neuron * neuron_new(int num)
{
	struct neuron *n = malloc(sizeof(struct neuron));
	memset(n, 0, sizeof(struct neuron));

	n->in = malloc(sizeof(char *) * DEFAULT_ARRAY_SIZE);
	memset(n->in, 0, sizeof(char *) * DEFAULT_ARRAY_SIZE);
	n->in_cnt.total = DEFAULT_ARRAY_SIZE;
	n->in_cnt.used = 0;
	n->out = malloc(sizeof(char *) * DEFAULT_ARRAY_SIZE);
	memset(n->out, 0, sizeof(char *) * DEFAULT_ARRAY_SIZE);
	n->out_cnt.total = DEFAULT_ARRAY_SIZE;
	n->out_cnt.used = 0;
	n->cb = neuron_default_cb;

	n->num = num;
	return n;
}

/* connects neurons, with the info recorded, a neuron that got input
 * will process and transfer positively */
void neuron_connect(struct neuron *from, struct neuron *to)
{
	from->out[from->out_cnt.used] = to;
	from->out_cnt.used++;
	to->in[to->in_cnt.used] = from;
	to->in_cnt.used++;
}

/* TODO: for testing only */
void neuron_print(struct neuron *cur, bool recursive)
{
	if (cur->cb)
		cur->cb(cur, 0, NULL, NULL, NULL);
	else
		printf("[Neuron] %d\n", cur->num);

	for (int i = 0; i < cur->out_cnt.used; i++)
		neuron_print(cur->out[i], recursive);
}


/* We define a neuron, and create a thread on it.
 * The thread accepts input, and transfers to output, it may have side effects like driving servo, toggle LED, etc.
 */
// neuron_born(struct neuron *n)

/* TODO: A neuron can take short break, while may not exist in real world scenario */
// neuron_rest(struct neuron *n)

/* TODO: Neuron dies, may be extended into healthcare, that destroys neurons with bad health status */
// neuron_fadeout(struct neuron *n)
