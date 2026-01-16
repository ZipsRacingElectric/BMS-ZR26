// Header
#include "vehicle_thread.h"

// Includes
#include "peripherals.h"
#include "can/transmit.h"
#include "watchdog.h"

// Constants ------------------------------------------------------------------------------------------------------------------

#define THREAD_PERIOD TIME_MS2I (30)

// Threads --------------------------------------------------------------------------------------------------------------------

static THD_WORKING_AREA (vehicleThreadWa, 512);
static void vehicleThread (void* arg)
{
	(void) arg;
	chRegSetThreadName ("vehicle");

	systime_t timePrevious = chVTGetSystemTimeX ();
	while (true)
	{
		// Reset the watchdog.
		// watchdogReset ();

		// Start the LTC transaction
		chMtxLock (&peripheralMutex);
		ltc6811Start (ltcBottom);
		ltc6811WakeupIdle (ltcBottom);

		// Sample the cell voltages and board peripherals
		ltc6811SampleCells (ltcBottom);
		peripheralsSample (THREAD_PERIOD);

		// Sample the temperature sensors
		ltc6811SampleGpio (ltcBottom);

		// Finish the LTC transaction
		ltc6811Stop (ltcBottom);
		chMtxUnlock (&peripheralMutex);

		// Check faults and update the global peripheral state.
		peripheralsCheckState ();

		// Transmit the CAN messages.
		transmitBmsMessages (THREAD_PERIOD);

		// Sleep until the next loop
		chThdSleepUntilWindowed (timePrevious, chTimeAddX (timePrevious, THREAD_PERIOD));
		timePrevious = chVTGetSystemTimeX ();
	}
}

// Functions ------------------------------------------------------------------------------------------------------------------

void vehicleThreadStart (tprio_t priority)
{
	chThdCreateStatic (vehicleThreadWa, sizeof (vehicleThreadWa), priority, vehicleThread, NULL);
}