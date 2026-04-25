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

// ChibiOS
#include "hal.h"

// C Standard Library
#include <float.h>

// Interrupts -----------------------------------------------------------------------------------------------------------------

void hardFaultCallback (void)
{
	// Open the shutdown loop
	palWriteLine (LINE_BMS_FAULT_OUT, true);
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

	// Start the watchdog timer.
	watchdogStart ();

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
	}

	// Do nothing
	while (true)
		chThdSleepMilliseconds (500);
}