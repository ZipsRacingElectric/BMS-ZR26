// Header
#include "eeprom_map.h"

// Includes
#include "peripherals.h"
#include "watchdog.h"

// C Standard Library
#include <string.h>

// Constants ------------------------------------------------------------------------------------------------------------------

#define READONLY_COUNT (sizeof (READONLY_ADDRS) / sizeof (READONLY_ADDRS [0]))
static const uint16_t READONLY_ADDRS [] =
{
	0x0000,
	0x0002
};

static const void* READONLY_DATA [READONLY_COUNT] =
{
	&currentSensor.channel1.sample,
	&currentSensor.channel2.sample,
};

static const uint16_t READONLY_SIZES [READONLY_COUNT] =
{
	sizeof (uint16_t),
	sizeof (uint16_t)
};

// Functions ------------------------------------------------------------------------------------------------------------------

bool eepromReadonlyRead (void* object, uint16_t addr, void* data, uint16_t dataCount)
{
	(void) object;

	for (uint16_t index = 0; index < READONLY_COUNT; ++index)
	{
		if (addr != READONLY_ADDRS [index])
			continue;

		if (dataCount != READONLY_SIZES [index])
			return false;

		memcpy (data, READONLY_DATA [index], dataCount);
		return true;
	}

	return false;
}

bool eepromWriteonlyWrite (void* object, uint16_t addr, const void* data, uint16_t dataCount)
{
	(void) object;

	uint8_t ltcIndex;
	uint8_t cellIndex;

	switch (addr)
	{
	case 0x0000: // Watchdog trigger command.
		watchdogTrigger ();
		return true;

	case 0x0001: // Cell balancing enable / disable command.
		return true;

	case 0x0002: // Cell discharge disable command.
		if (dataCount != 1)
			return false;

		ltcIndex = *((uint8_t*) data) / LTC6813_CELL_COUNT;
		cellIndex = *((uint8_t*) data) % LTC6813_CELL_COUNT;
		if (ltcIndex > LTC_COUNT)
			return false;

		ltcs [ltcIndex].cellsDischarging [cellIndex] = false;
		// TODO(Barach): Reimplement
		// ltc6811WriteConfig (ltcBottom);
		return true;

	case 0x0003: // Cell discharge enable command.
		if (dataCount != 1)
			return false;

		ltcIndex = *((uint8_t*) data) / LTC6813_CELL_COUNT;
		cellIndex = *((uint8_t*) data) % LTC6813_CELL_COUNT;
		if (ltcIndex > LTC_COUNT)
			return false;

		ltcs [ltcIndex].cellsDischarging [cellIndex] = true;
		// TODO(Barach): Reimplement
		// ltc6811WriteConfig (ltcBottom);
		return true;
	}

	return false;
}