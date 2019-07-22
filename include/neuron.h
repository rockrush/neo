#ifndef NEO_NEURON_H
#define NEO_NEURON_H

#include <stdbool.h>

#include <utils.h>

#define SPARE_RATIO			2
#define DEFAULT_ARRAY_SIZE	16

struct neuron_life {
	int age;
	int health;
	bool grow;
};

struct neuron {
	const char *name;	/* optional, if we implemented hash table on neuron set */
	//int id;			// We take memory address as neuron ID, seems to be enough
	int data_len;		/* Should be void *, for any kind of data, depending on what cb() needs */
	int (*cb)(struct neuron *, const int, void *, int *, void *);
	struct neuron_life life;

	struct array_cap in_cnt;
	struct neuron **in;	/* It's set, can be simplified as array */
	struct array_cap out_cnt;
	struct neuron **out;

	union {
		void *mem;
		int num;
	};
};

struct neuron * neuron_new(int num);
void neuron_connect(struct neuron *from, struct neuron *to);
void neuron_print(struct neuron *cur, bool recursive);

#endif
