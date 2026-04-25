// Header
#include "cell_balancing.h"

// Includes
#include "peripherals.h"

// C Standard Library
#include <stdint.h>

void cellBalancingUpdate (void)
{
	// Find the minimum cell voltage
	float minCell = ltcs [0].cellVoltages [0];
	for (uint16_t ltc = 0; ltc < LTC_COUNT; ++ltc)
		for (uint16_t cell = 0; cell < CELLS_PER_LTC; ++cell)
			if (ltcs [ltc].cellVoltages [cell] < minCell)
				minCell = ltcs [ltc].cellVoltages [cell];

	// Balance all cells exceeding the balancing threshold.
	for (uint16_t ltc = 0; ltc < LTC_COUNT; ++ltc)
	{
		for (uint16_t cell = 0; cell < CELLS_PER_LTC; ++cell)
		{
			float delta = ltcs [ltc].cellVoltages [cell] - minCell;
			ltcs [ltc].cellsDischarging [cell] = delta > physicalEepromMap->balancingThreshold;
		}
	}
}

void cellBalancingDisable (void)
{
	for (uint16_t ltc = 0; ltc < LTC_COUNT; ++ltc)
		for (uint16_t cell = 0; cell < LTC6813_CELL_COUNT; ++cell)
			ltcs [ltc].cellsDischarging [cell] = false;
}