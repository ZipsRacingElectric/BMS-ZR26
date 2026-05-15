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

/// @brief The period of the charger thread.
#define THREAD_PERIOD TIME_MS2I (250)

/// @brief The modulus for sampling cell voltages when the BMS is balancing. For example:
/// If THREAD_PERIOD = 250ms and BALANCING_SAMPLE_MODULUS = 64, cell voltages are sampled every 16s.
#define BALANCING_SAMPLE_MODULUS 64

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
	systime_t timeCellVoltagePrevious = timePrevious;
	chThdSleep (THREAD_PERIOD);
	systime_t timeCurrent = chVTGetSystemTimeX ();

	chargingThreadMode_t mode = MODE_CHARGING;
	uint16_t index = 0;
	while (true)
	{
		// Reset the watchdog.
		watchdogReset ();

		// Start the LTC transaction
		chMtxLock (&peripheralMutex);
		ltc6813Start (ltcBottom);
		ltc6813WakeupSleep (ltcBottom);

		// If charging, or if balancing and this is the N'th iteration, sample the cell voltages and board peripherals
		bool updateBalancing = false;
		if (mode == MODE_CHARGING || (mode == MODE_BALANCING && index >= BALANCING_SAMPLE_MODULUS))
		{
			// Reset the index
			index = 0;
			updateBalancing = true;

			// Sample the cell voltages and board peripherals
			ltc6813SampleCells (ltcBottom);
			peripheralsSample (chTimeDiffX (timeCellVoltagePrevious, timeCurrent));
			timeCellVoltagePrevious = timeCurrent;
		}
		else
			++index;

		// Sample the temperature sensors
		ltc6813SampleGpio (ltcBottom);
		ltc6813SampleStatus (ltcBottom);

		// Update cell balancing
		if (updateBalancing)
		{
			balancing = physicalEepromMap->balancingEnabled && mode == MODE_BALANCING;
			if (positiveIrEnabled && !bmsFault && balancing)
				cellBalancingUpdate ();
			else
				cellBalancingDisable ();

			ltc6813WriteConfig (ltcBottom);
		}

		// Finish the LTC transaction
		ltc6813Stop (ltcBottom);
		chMtxUnlock (&peripheralMutex);

		// Check faults and update the global peripheral state.
		peripheralsCheckState (chTimeDiffX (timePrevious, timeCurrent));

		// If charging is complete (an overvoltage fault was asserted), stop charging and switch to balancing.
		// NOTE(Barach): Be very careful when touching this logic, as misuse can overcharge cells.
		if (mode == MODE_CHARGING && overvoltageFault)
		{
			// Reset the fault and switch to balancing
			peripheralsResetOvervoltageFault ();
			mode = MODE_BALANCING;

			// We want to sample on the very next iteration, so start at the modulus itself.
			index = BALANCING_SAMPLE_MODULUS;
		}

		// If high voltage is disabled while balancing, switch back to charging.
		if (mode == MODE_BALANCING && !positiveIrEnabled)
			mode = MODE_CHARGING;

		// Update charging
		charging = physicalEepromMap->chargingEnabled && mode == MODE_CHARGING;
		if (positiveIrEnabled && !bmsFault && charging)
			chargingUpdate ();
		else
			chargingDisable ();

		// Check the precharge status
		float chargerVoltage = prechargeGetChargerVoltage ();
		bool prechargeComplete = prechargeCheck (packVoltage, chargerVoltage);
		peripheralsSetPrechargeComplete (prechargeComplete);

		// Commit the measured state
		peripheralsCommitState ();

		// Transmit the CAN messages.
		transmitBmsMessages (THREAD_PERIOD);

		// Sleep until the next loop
		chThdSleepUntilWindowed (timeCurrent, chTimeAddX (timeCurrent, THREAD_PERIOD));
		timePrevious = timeCurrent;
		timeCurrent = chVTGetSystemTimeX ();
	}
}

// Functions ------------------------------------------------------------------------------------------------------------------

void chargerThreadStart (tprio_t priority)
{
	chThdCreateStatic (chargerThreadWa, sizeof (chargerThreadWa), priority, chargerThread, NULL);
}