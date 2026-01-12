// Header
#include "monitor_thread.h"

// Includes
#include "peripherals.h"
#include "can/transmit.h"
#include "watchdog.h"
#include "controls/rolling_average.h"

// Constants ------------------------------------------------------------------------------------------------------------------

// TODO(Barach): Validate this is being achieved.
#define BMS_THREAD_PERIOD TIME_MS2I (250)

// Globals --------------------------------------------------------------------------------------------------------------------

float powerHistory [15] = { 0.0f };
#define POWER_ROLLING_AVERAGE_MAX_COUNT (sizeof (powerHistory) / sizeof (float)) + 1

uint16_t powerRollingAverageCount = 1;

// Threads --------------------------------------------------------------------------------------------------------------------

static THD_WORKING_AREA (monitorThreadWa, 512);
void monitorThread (void* arg)
{
	(void) arg;

	systime_t timePrevious = chVTGetSystemTimeX ();
	while (true)
	{
		// Reset the watchdog.
		watchdogReset ();

		chMtxLock (&peripheralMutex);

		// Sample the LTCs
		ltc6811ClearState (ltcBottom);
		ltc6811SampleCells (ltcBottom);
		ltc6811SampleStatus (ltcBottom);
		ltc6811SampleCellVoltageFaults (ltcBottom);
		ltc6811SampleGpio (ltcBottom);

		// TODO(Barach): Manage balancing.
		ltc6811OpenWireTest (ltcBottom);
		ltc6811WriteConfig (ltcBottom);

		// Update the global state

		packVoltage = 0.0f;
		for (uint16_t index = 0; index < LTC_COUNT; ++index)
			packVoltage += ltcs [index].cellVoltageSum;

		undervoltageFault = ltc6811UndervoltageFault (ltcBottom);
		overvoltageFault = ltc6811OvervoltageFault (ltcBottom);
		isospiFault = ltc6811IsospiFault (ltcBottom);
		senseLineFault = ltc6811OpenWireFault (ltcBottom);
		selfTestFault = ltc6811SelfTestFault (ltcBottom);

		undertemperatureFault = false;
		for (uint16_t ltcIndex = 0; ltcIndex < LTC_COUNT; ++ltcIndex)
			for (uint16_t thermistorIndex = 0; thermistorIndex < LTC6811_GPIO_COUNT; ++thermistorIndex)
				undertemperatureFault |= thermistors [ltcIndex][thermistorIndex].undertemperatureFault;

		overtemperatureFault = false;
		for (uint16_t ltcIndex = 0; ltcIndex < LTC_COUNT; ++ltcIndex)
		{
			for (uint16_t thermistorIndex = 0; thermistorIndex < LTC6811_GPIO_COUNT; ++thermistorIndex)
				overtemperatureFault |= thermistors [ltcIndex][thermistorIndex].overtemperatureFault;

			overvoltageFault |= ltcs [ltcIndex].dieTemperature > physicalEepromMap->ltcTemperatureMax;
		}

		bmsFault = undervoltageFault || overvoltageFault || isospiFault || senseLineFault || selfTestFault
			|| undertemperatureFault || overtemperatureFault;

		shutdownLoopClosed = !palReadLine (LINE_SHUTDOWN_STATUS);
		prechargeComplete = !palReadLine (LINE_PRECHARGE_STATUS);
		bmsFaultRelay = !palReadLine (LINE_BMS_FLTDD);
		imdFaultRelay = !palReadLine (LINE_IMD_FLT);

		// Sample the current sensor
		stmAdcSample (&adc);

		float power = packVoltage * currentSensor.value;
		powerRollingAverage = rollingAverageCalculate (power, powerHistory, powerRollingAverageCount);
		energyDelivered += power * TIME_I2US (BMS_THREAD_PERIOD) * (1e-9f / 3600.0f);

		chMtxUnlock (&peripheralMutex);

		// If a fault is present, open the shutdown loop.
		bool fltLine = !bmsFault;
		palWriteLine (LINE_BMS_FLT, fltLine);

		// Transmit the CAN messages.
		transmitBmsMessages (BMS_THREAD_PERIOD);

		// Reset the blip status
		if (shutdownLoopBlip && chTimeDiffX (shutdownLoopBlipTime, chVTGetSystemTimeX ()) < TIME_MS2I (1000))
			shutdownLoopBlip = false;

		// Sleep until the next loop
		chThdSleepUntilWindowed (timePrevious, chTimeAddX (timePrevious, BMS_THREAD_PERIOD));
		timePrevious = chVTGetSystemTimeX ();
	}
}

// Functions ------------------------------------------------------------------------------------------------------------------

void monitorThreadStart (tprio_t priority)
{
	chThdCreateStatic (monitorThreadWa, sizeof (monitorThreadWa), priority, monitorThread, NULL);
}

void monitorThreadSetRollingAverageCount (uint16_t count)
{
	if (count > POWER_ROLLING_AVERAGE_MAX_COUNT)
		count = POWER_ROLLING_AVERAGE_MAX_COUNT;

	powerRollingAverageCount = count;
}