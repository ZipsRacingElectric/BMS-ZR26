// Header
#include "charging.h"

// Includes
#include "peripherals.h"
#include "can_charger.h"

void chargingUpdate (void)
{
	// Calculate the maximum requestable current, based on the power limit.
	float currentLimit = physicalEepromMap->chargingPowerLimit / packVoltage;

	// Saturate based on the current limit.
	if (currentLimit > physicalEepromMap->chargingCurrentLimit)
		currentLimit = physicalEepromMap->chargingCurrentLimit;

	// Send the power request.
	tcChargerSendCommand (&charger, TC_WORKING_MODE_STARTUP,
		physicalEepromMap->chargingVoltageLimit, currentLimit, TIME_MS2I (100));
}

void chargingDisable (void)
{
	// Disable the charger
	tcChargerSendCommand (&charger, TC_WORKING_MODE_SLEEP, 0, 0, TIME_MS2I (100));
}