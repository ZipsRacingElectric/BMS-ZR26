#ifndef EEPROM_MAP_H
#define EEPROM_MAP_H

// EEPROM Mapping -------------------------------------------------------------------------------------------------------------
//
// Author: Cole Barach
// Date Created: 2025.02.20
//
// Description: Structing mapping the data of an EEPROM data to variables.

// Includes -------------------------------------------------------------------------------------------------------------------

// Includes
#include "peripherals/adc/dhab_s124.h"
#include "peripherals/adc/thermistor_pulldown.h"

// Constants ------------------------------------------------------------------------------------------------------------------

/// @brief The magic string of the EEPROM. Update this value every time the memory map changes to force manual re-programming.
#define EEPROM_MAP_STRING "BMS_2025_06_01"

// Datatypes ------------------------------------------------------------------------------------------------------------------

typedef struct
{
	uint8_t pad0 [16];								// 0x0000
	thermistorPulldownConfig_t thermistorConfig;	// 0x0010
	dhabS124Config_t currentSensorConfig;			// 0x0030
	float chargingVoltageLimit;						// 0x0050
	float chargingCurrentLimit;						// 0x0054
	float chargingPowerLimit;						// 0x0058
	float chargingThreshold;						// 0x005C
	bool balancingEnabled;							// 0x0060
	bool chargingEnabled;							// 0x0061
	float balancingThreshold;						// 0x0064
	float ltcTemperatureMax;						// 0x0068
	uint16_t powerRollingAverageCount;				// 0x006C
	float cellVoltageMin;							// 0x0070
	float cellVoltageMax;							// 0x0074
} eepromMap_t;

// Functions ------------------------------------------------------------------------------------------------------------------

bool eepromReadonlyRead (void* object, uint16_t addr, void* data, uint16_t dataCount);

bool eepromWriteonlyWrite (void* object, uint16_t addr, const void* data, uint16_t dataCount);

#endif // EEPROM_MAP_H