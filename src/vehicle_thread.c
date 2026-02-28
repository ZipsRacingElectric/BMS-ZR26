// Header
#include "vehicle_thread.h"

// Includes
#include "peripherals.h"
#include "peripherals/precharge.h"
#include "can/transmit.h"
#include "watchdog.h"

// Constants ------------------------------------------------------------------------------------------------------------------

// TODO(Barach): Need to validate it hits this with 5 segments installed.
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
		ltc6813Start (ltcBottom);
		ltc6813WakeupIdle (ltcBottom);

		// Sample the cell voltages and board peripherals
		ltc6813SampleCells (ltcBottom);
		peripheralsSample (THREAD_PERIOD);

		// Sample the temperature sensors
		ltc6813SampleGpio (ltcBottom);

		// Finish the LTC transaction
		ltc6813Stop (ltcBottom);
		chMtxUnlock (&peripheralMutex);

		// Check faults and update the global peripheral state.
		peripheralsCheckState ();

		// Check the precharge status
		float inverterVoltage = prechargeGetInverterVoltage ();
		bool prechargeComplete = prechargeCheck (packVoltage, inverterVoltage);
		peripheralsSetPrechargeComplete (prechargeComplete);

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