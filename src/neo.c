#include <stdlib.h>
#include <stdbool.h>

#include <neuron.h>

#define SPARE_RATIO			2
#define DEFAULT_ARRAY_SIZE	16

int main(void)
{
	struct neuron *sensor = neuron_new(4);
	struct neuron *anal = neuron_new(9);
	struct neuron *act = neuron_new(1);
	struct neuron *cur_neuron = sensor;

	neuron_connect(sensor, anal);
	neuron_connect(sensor, act);

	neuron_print(sensor, true);
	return EXIT_SUCCESS;
}
