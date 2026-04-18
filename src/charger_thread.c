// Header
#include "charger_thread.h"

// Includes
#include "peripherals.h"
#include "can/transmit.h"
#include "watchdog.h"
#include "peripherals/cell_balancing.h"
#include "peripherals/charging.h"
#include "peripherals/precharge.h"

// Constants ------------------------------------------------------------------------------------------------------------------

#define THREAD_CHARING_PERIOD		TIME_MS2I (250)
#define THREAD_BALANCING_PERIOD		TIME_MS2I (3000)

// Datatypes ------------------------------------------------------------------------------------------------------------------

typedef enum
{
	MODE_CHARGING,
	MODE_BALANCING
} chargingThreadMode_t;

// Threads --------------------------------------------------------------------------------------------------------------------

static THD_WORKING_AREA (chargerThreadWa, 512);
static void chargerThread (void* arg)
{
	(void) arg;
	chRegSetThreadName ("charger");

	systime_t timePrevious = chVTGetSystemTimeX ();
	chargingThreadMode_t mode = MODE_CHARGING;
	while (true)
	{
		// Reset the watchdog.
		// watchdogReset ();

		// Start the LTC transaction
		chMtxLock (&peripheralMutex);
		ltc6813Start (ltcBottom);
		ltc6813WakeupSleep (ltcBottom);

		// TODO(Barach): Open wire and LTC status?

		// Sample the cell voltages and board peripherals
		ltc6813SampleCells (ltcBottom);
		peripheralsSample (mode == MODE_CHARGING ? THREAD_CHARING_PERIOD : THREAD_BALANCING_PERIOD);

		// Sample the temperature sensors
		ltc6813SampleGpio (ltcBottom);
		ltc6813SampleStatus (ltcBottom);

		// Finish the LTC transaction
		ltc6813Stop (ltcBottom);
		chMtxUnlock (&peripheralMutex);

		// Check faults and update the global peripheral state.
		peripheralsCheckState ();

		// TODO(Barach): Need to work out thread period.

		// // Cell balancing
		// balancing = physicalEepromMap->balancingEnabled;
		// if (positiveIrEnabled && !bmsFault && balancing)
		// 	cellBalancingUpdate ();
		// else
		// 	cellBalancingDisable ();

		// Charging
		charging = physicalEepromMap->chargingEnabled;
		if (positiveIrEnabled && !bmsFault && charging)
			chargingUpdate ();
		else
			chargingDisable ();

		// Check the precharge status
		float chargerVoltage = prechargeGetChargerVoltage ();
		bool prechargeComplete = prechargeCheck (packVoltage, chargerVoltage);
		peripheralsSetPrechargeComplete (prechargeComplete);

		// TODO(Barach): Order of operations?
		peripheralsCommitState ();

		// Transmit the CAN messages.
		transmitBmsMessages (mode == MODE_CHARGING ? THREAD_CHARING_PERIOD : THREAD_BALANCING_PERIOD);

		// Sleep until the next loop
		chThdSleepUntilWindowed (timePrevious, chTimeAddX (timePrevious, mode == MODE_CHARGING ? THREAD_CHARING_PERIOD : THREAD_BALANCING_PERIOD));
		timePrevious = chVTGetSystemTimeX ();
	}
}

// Functions ------------------------------------------------------------------------------------------------------------------

void chargerThreadStart (tprio_t priority)
{
	chThdCreateStatic (chargerThreadWa, sizeof (chargerThreadWa), priority, chargerThread, NULL);
}