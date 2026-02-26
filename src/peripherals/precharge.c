// Header
#include "precharge.h"

// Includes
#include "peripherals.h"
#include "can_vehicle.h"
#include "can_charger.h"

// C Standard Library
#include <float.h>

// Constants ------------------------------------------------------------------------------------------------------------------

/// @brief The fraction of the accumulator voltage that the inverter voltage must reach for precharge to be considered
/// complete. This should be no lower than 90% (0.9).
#define PRECHARGE_THRESHOLD 0.95

// Functions ------------------------------------------------------------------------------------------------------------------

float prechargeGetInverterVoltage (void)
{
	float minVoltage = FLT_MAX;
	bool valid = true;

	for (size_t index = 0; index < AMK_COUNT; ++index)
	{
		canNodeLock ((canNode_t*) &amks [index]);

		if (amks [index].state != CAN_NODE_VALID)
			valid = false;
		else if (amks [index].dcBusVoltage < minVoltage)
			minVoltage = amks [index].dcBusVoltage;

		canNodeUnlock ((canNode_t*) &amks [index]);
	}

	if (!valid)
		return 0;

	return minVoltage;
}

float prechargeGetChargerVoltage (void)
{
	float voltage = 0;

	canNodeLock ((canNode_t*) &charger);

	if (charger.state == CAN_NODE_VALID)
		voltage = charger.outputVoltage;

	canNodeUnlock ((canNode_t*) &charger);

	return voltage;
}

bool prechargeCheck (float accumulatorVoltage, float inverterVoltage)
{
	// If the negative isolation relay is not closed yet, precharge cannot be completed.
	if (!shutdownMsdTsmsClosed)
		return false;

	// Otherwise, check if the inverter voltage exceeds the precharge threshold.
	return (inverterVoltage > accumulatorVoltage * PRECHARGE_THRESHOLD);
}