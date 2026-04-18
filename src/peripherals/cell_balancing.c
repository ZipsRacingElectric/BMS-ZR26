// Header
#include "cell_balancing.h"

// Includes
#include "peripherals.h"

// C Standard Library
#include <stdint.h>

void cellBalancingUpdate (void)
{
	// TODO(Barach):

	for (uint16_t ltc = 0; ltc < LTC_COUNT; ++ltc)
	{
		for (uint16_t cell = 0; cell < LTC6813_CELL_COUNT; ++cell)
		{
			// ltcs [ltc].cellsDischarging [cell] = ltcs [ltc].cellVoltages [];
		}
	}
}

void cellBalancingDisable (void)
{
	for (uint16_t ltc = 0; ltc < LTC_COUNT; ++ltc)
		for (uint16_t cell = 0; cell < LTC6813_CELL_COUNT; ++cell)
			ltcs [ltc].cellsDischarging [cell] = false;
}