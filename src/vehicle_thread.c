// Header
#include "vehicle_thread.h"

// Includes
#include "peripherals.h"
#include "peripherals/precharge.h"
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
	chThdSleep (THREAD_PERIOD);
	systime_t timeCurrent = chVTGetSystemTimeX ();
	while (true)
	{
		volatile uint32_t t0 = DWT->CYCCNT; t0 = t0;

		// Reset the watchdog.
		watchdogReset ();

		volatile uint32_t t1 = DWT->CYCCNT; t1 = t1;

		// Start the LTC transaction
		chMtxLock (&peripheralMutex);
		ltc6813Start (ltcBottom);
		ltc6813WakeupIdle (ltcBottom);

		volatile uint32_t t2 = DWT->CYCCNT; t2 = t2;

		// Sample the cell voltages and board peripherals
		ltc6813SampleCells (ltcBottom);
		peripheralsSample (chTimeDiffX (timePrevious, timeCurrent));

		volatile uint32_t t3 = DWT->CYCCNT; t3 = t3;

		// Sample the temperature sensors
		ltc6813SampleGpio (ltcBottom);

		volatile uint32_t t4 = DWT->CYCCNT; t4 = t4;

		// Finish the LTC transaction
		ltc6813Stop (ltcBottom);
		chMtxUnlock (&peripheralMutex);

		volatile uint32_t t5 = DWT->CYCCNT; t5 = t5;

		// Check faults and update the global peripheral state.
		peripheralsCheckState ();

		volatile uint32_t t6 = DWT->CYCCNT; t6 = t6;

		// Check the precharge status
		float inverterVoltage = prechargeGetInverterVoltage ();
		bool prechargeComplete = prechargeCheck (packVoltage, inverterVoltage);
		peripheralsSetPrechargeComplete (prechargeComplete);

		volatile uint32_t t7 = DWT->CYCCNT; t7 = t7;

		// Commit the measured state
		peripheralsCommitState ();

		volatile uint32_t t8 = DWT->CYCCNT; t8 = t8;

		// Transmit the CAN messages.
		transmitBmsMessages (THREAD_PERIOD);

		volatile uint32_t t9 = DWT->CYCCNT; t9 = t9;

		// Sleep until the next loop
		chThdSleepUntilWindowed (timeCurrent, chTimeAddX (timeCurrent, THREAD_PERIOD));
		timePrevious = timeCurrent;
		timeCurrent = chVTGetSystemTimeX ();

		volatile uint32_t t10 = DWT->CYCCNT; t10 = t10;
		volatile uint32_t t11 = DWT->CYCCNT; t11 = t11;
	}
}

// Functions ------------------------------------------------------------------------------------------------------------------

void vehicleThreadStart (tprio_t priority)
{
	chThdCreateStatic (vehicleThreadWa, sizeof (vehicleThreadWa), priority, vehicleThread, NULL);
}