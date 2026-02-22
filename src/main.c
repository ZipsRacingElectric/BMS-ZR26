// Battery Management System --------------------------------------------------------------------------------------------------
//
// Author: Cole Barach
// Date Created: 2024.11.05
//
// Description: Entrypoint and interrupt handlers for the vehicle's battery management system.
//
// TODO(Barach):
// - Charger command message
// - Move sense line check to power up
// - Speed up sampling rate
//
// - Balancing + sense line test doesn't work.
// - Introduce sense-board object for abstracting thermistor and LTC mapping.
// - Replace LTC init with sequence of 'append' functions to remove unnecessary arrays.
// - Have sense-board take over fault tolerance of LTCs
// - Have LTCs dump cell values into user-provided array so single array can be used.
// - Fix ADC polling.

// Includes -------------------------------------------------------------------------------------------------------------------

// Includes
#include "can_vehicle.h"
#include "can_charger.h"
#include "charger_thread.h"
#include "debug.h"
#include "peripherals.h"
#include "watchdog.h"
#include "vehicle_thread.h"
#include "algorithm/sort.h"

// ChibiOS
#include "hal.h"

// C Standard Library
#include <float.h>

// Interrupts -----------------------------------------------------------------------------------------------------------------

void hardFaultCallback (void)
{
	// Open the shutdown loop
	palWriteLine (LINE_BMS_FAULT_OUT, false);
}

// Entrypoint -----------------------------------------------------------------------------------------------------------------

void vehicleEntrypoint (void);

void chargerEntrypoint (void);

int main (void)
{
	// ChibiOS Initialization
	halInit ();
	chSysInit ();

	// Debug Initialization
	ioline_t ledLine = LINE_LED_HEARTBEAT;
	debugHeartbeatStart (&ledLine, LOWPRIO);

	// Peripheral Initialization
	if (!peripheralsInit ())
	{
		hardFaultCallback ();
		while (true);
	}

	// TODO(Barach): EEPROM switch
	// Start the watchdog timer.
	// watchdogStart ();

	// If detect line is low, accumulator is on charger. Otherwise, accumulator is in vehicle.
	if (palReadLine (LINE_CHARGER_DETECT))
	{
		// Initialize the CAN interface.
		if (!canVehicleInit (NORMALPRIO))
		{
			hardFaultCallback ();
			while (true);
		}

		// Start the vehicle monitoring thread.
		vehicleThreadStart (NORMALPRIO);

		// Do nothing
		while (true)
			chThdSleepMilliseconds (500);
	}
	else
	{
		// Initialize the CAN interface.
		if (!canChargerInit (NORMALPRIO))
		{
			hardFaultCallback ();
			while (true);
		}

		// Start the charger monitoring thread.
		chargerThreadStart (NORMALPRIO);

		// Main loop
		systime_t timePrevious = chVTGetSystemTimeX ();
		while (true)
		{
			chMtxLock (&peripheralMutex);

			// TODO(Barach): Move to charger thread

			// balancing = physicalEepromMap->balancingEnabled;
			// if (prechargeComplete && !bmsFault && balancing)
			// {
			// 	// Search the pack for the min cell voltage
			// 	float minVoltage = ltcs [0].cellVoltages [0];
			// 	for (uint16_t ltc = 0; ltc < LTC_COUNT; ++ltc)
			// 		for (uint16_t cell = 0; cell < LTC6813_CELL_COUNT; ++cell)
			// 			if (ltcs [ltc].cellVoltages [cell] < minVoltage)
			// 				minVoltage = ltcs [ltc].cellVoltages [cell];

			// 	// Only balance the highest 4 deltas. This is to compensate for the LTCs overheating.
			// 	uint8_t balanceCount = 4;
			// 	float sortedVoltages [LTC6813_CELL_COUNT];
			// 	uint8_t sortedIndices [LTC6813_CELL_COUNT];
			// 	for (uint16_t ltc = 0; ltc < LTC_COUNT; ++ltc)
			// 	{
			// 		// Cursed sorting algorithm.
			// 		sortValues (ltcs [ltc].cellVoltages, LTC6813_CELL_COUNT, sortedVoltages, sortedIndices, balanceCount, >, FLT_MIN);

			// 		for (uint16_t cell = 0; cell < LTC6813_CELL_COUNT; ++cell)
			// 			ltcs [ltc].cellsDischarging [cell] = false;

			// 		for (uint16_t cell = 0; cell < balanceCount; ++cell)
			// 			ltcs [ltc].cellsDischarging [sortedIndices [cell]] =
			// 				ltcs [ltc].cellVoltages [sortedIndices [cell]] - minVoltage > physicalEepromMap->balancingThreshold;
			// 	}
			// }
			// else
			// {
			// 	for (uint16_t ltc = 0; ltc < LTC_COUNT; ++ltc)
			// 		for (uint16_t cell = 0; cell < LTC6813_CELL_COUNT; ++cell)
			// 			ltcs [ltc].cellsDischarging [cell] = false;
			// }

			charging = physicalEepromMap->chargingEnabled;
			if (prechargeComplete && !bmsFault && charging)
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
			else
			{
				// Disable the charger
				tcChargerSendCommand (&charger, TC_WORKING_MODE_SLEEP, 0, 0, TIME_MS2I (100));
			}

			chMtxUnlock (&peripheralMutex);

			// Sleep until the next loop
			chThdSleepUntilWindowed (timePrevious, chTimeAddX (timePrevious, TIME_MS2I (500)));
			timePrevious = chVTGetSystemTimeX ();
		}
	}
}