// Header
#include "peripherals.h"

// Includes
#include "peripherals/adc/stm_adc.h"
#include "controls/rolling_average.h"

// Global State ---------------------------------------------------------------------------------------------------------------

float packVoltage = 0.0f;
float powerRollingAverage = 0.0f;
float energyDelivered = 0.0f;
bool bmsFault = true;
bool undervoltageFault = false;
bool overvoltageFault = false;
bool undertemperatureFault = false;
bool overtemperatureFault = false;
bool senseLineFault = false;
bool isospiFault = false;
bool selfTestFault = false;
bool charging = false;
bool balancing = false;
bool shutdownLoopClosed = false;
bool prechargeComplete = false;
bool shutdownLoopBlip = false;
systime_t shutdownLoopBlipTime = 0;
bool bmsFaultRelay = true;
bool imdFaultRelay = true;

// Global State (Private) -----------------------------------------------------------------------------------------------------

/// @brief Power history used for the power rolling average.
static float powerHistory [POWER_ROLLING_AVERAGE_MAX_COUNT - 1] = { 0.0f };

/// @brief Number of samples to use in the power rolling average.
static uint16_t powerRollingAverageCount = 1;

/// @brief Number of continuous cell voltage faults tripped by each cell.
static uint16_t cellVoltageFaultCounters [LTC_COUNT][LTC6813_CELL_COUNT] = { 0 };

/// @brief Number of continuous temperature faults tripped by each thermistor.
static uint16_t temperatureFaultCounters [LTC_COUNT][LTC6813_GPIO_COUNT] = { 0 };

// Global Peripherals ---------------------------------------------------------------------------------------------------------

// Public
mutex_t					peripheralMutex;
stmAdc_t				adc;
mc24lc32_t				physicalEeprom;
virtualEeprom_t			virtualEeprom;
ltc6813_t				ltcs [LTC_COUNT];
ltc6813_t*				ltcBottom;
thermistorPulldown_t	thermistors [LTC_COUNT][LTC6813_GPIO_COUNT];
dhabS124_t				currentSensor;

// Private
static eeprom_t			readonlyWriteonlyEeprom;

// Configuration --------------------------------------------------------------------------------------------------------------

/// @brief Configuration for the I2C 1 bus.
static const I2CConfig I2C1_CONFIG =
{
	.op_mode		= OPMODE_I2C,
	.clock_speed	= 400000,
	.duty_cycle		= FAST_DUTY_CYCLE_2
};

/// @brief Configuration for the ADC 1 peripheral.
static const stmAdcConfig_t ADC_CONFIG =
{
	.driver			= &ADCD1,
	.channels		=
	{
		ADC_CHANNEL_IN0,
		ADC_CHANNEL_IN1
	},
	.channelCount	= 2,
	.sensors		=
	{
		(analogSensor_t*) &currentSensor.channel1,
		(analogSensor_t*) &currentSensor.channel2
	}
};

/// @brief Configuration for the on-board EEPROM.
static const mc24lc32Config_t PHYSICAL_EEPROM_CONFIG =
{
	.addr			= 0x50,
	.i2c			= &I2CD1,
	.timeout		= TIME_MS2I (500),
	.magicString	= EEPROM_MAP_STRING,
	.dirtyHook		= peripheralsReconfigure
};

/// @brief Configuration for the BMS's virtual EEPROM.
static const virtualEepromConfig_t VIRTUAL_EEPROM_CONFIG =
{
	.count		= 2,
	.entries	=
	{
		{
			.eeprom	= (eeprom_t*) &physicalEeprom,
			.addr	= 0x0000,
			.size	= 0x1000,
		},
		{
			.eeprom = &readonlyWriteonlyEeprom,
			.addr	= 0x1000,
			.size	= 0x1000
		}
	}
};

/// @brief Configuration for the LTC daisy chain.
static const ltc6813Config_t LTC_CONFIG =
{
	.spiDriver				= &SPID1,
	.spiConfig 				=
	{
		.circular			= false,							// Linear buffer.
		.slave				= false,							// Device is in master mode.
		.cr1				= 0									// 2-line unidirectional, no CRC, MSB first, master mode, clock
																// idles high, data capture on first clock transition.
							| 0b110 << SPI_CR1_BR_Pos,			// Baudrate 656250 bps.
		.cr2				= 0,								// Default CR2 config.
		.data_cb			= NULL,								// No callbacks.
		.error_cb			= NULL,								//
		.ssport				= PAL_PORT (LINE_CS_ISOSPI),		// IsoSPI transceiver CS pin.
		.sspad				= PAL_PAD (LINE_CS_ISOSPI)			//
	},
	.readAttemptCount		= 5,								// Fail after 5 invalid read attempts.
	.cellAdcMode			= LTC681X_ADC_422HZ,				// 422 Hz ADC sampling for cell voltages.
	.gpioAdcMode			= LTC681X_ADC_27KHZ,				// 422 Hz ADC sampling for the thermistors.
	.dischargeTimeout		= LTC681X_DISCHARGE_TIMEOUT_30_S,	// Timeout cell discharging after 30s of no command.
	.openWireTestIterations	= 3,								// Perform 3 pull-up / pull-down commands before measuring.
	.pollTolerance			= TIME_MS2I (1),					// Allow 1ms of play in each operation's execution time.
};

// TODO(Barach): Replace with programmatic init
/// @brief The LTC IsoSPI daisy chain, used to accomodate for changes to the IsoSPI wiring.
static ltc6813_t* const LTC_DAISY_CHAIN [] =
{
	&ltcs [1],
	&ltcs [0],
};

// Callbacks ------------------------------------------------------------------------------------------------------------------

void onShutdownLoopOpen (void* arg)
{
	(void) arg;

	// If the shutdown loop was previously closed, record the blip.
	if (shutdownLoopClosed)
	{
		shutdownLoopBlip = true;
		shutdownLoopBlipTime = chVTGetSystemTimeX ();
	}
}

// Functions ------------------------------------------------------------------------------------------------------------------

bool peripheralsInit (void)
{
	chMtxObjectInit (&peripheralMutex);

	// ADC 1 initialization
	if (!stmAdcInit (&adc, &ADC_CONFIG))
		return false;

	// I2C 1 driver initialization
	if (i2cStart (&I2CD1, &I2C1_CONFIG) != MSG_OK)
		return false;

	// Physical EEPROM initialization (only exit early if a failure occurred).
	if (!mc24lc32Init (&physicalEeprom, &PHYSICAL_EEPROM_CONFIG) && physicalEeprom.state == MC24LC32_STATE_FAILED)
		return false;

	// Readonly / Writeonly EEPROM initialization
	eepromInit (&readonlyWriteonlyEeprom, eepromWriteonlyWrite, eepromReadonlyRead);

	// Virtual EEPROM initialization
	virtualEepromInit (&virtualEeprom, &VIRTUAL_EEPROM_CONFIG);

	// LTC daisy chain initialization
	// Note we are iterating by the daisy chain index, not the logical index.
	ltcBottom = LTC_DAISY_CHAIN [0];
	ltc6813StartChain (ltcBottom, &LTC_CONFIG);
	for (uint16_t ltcIndex = 1; ltcIndex < LTC_COUNT; ++ltcIndex)
		ltc6813AppendChain (ltcBottom, LTC_DAISY_CHAIN [ltcIndex]);
	ltc6813FinalizeChain (ltcBottom);

	// Reconfigurable peripheral initializations.
	peripheralsReconfigure (NULL);

	// Link each LTC GPIO to its associated thermistor.
	// Note we are iterating by the logical index, not the daisy chain index (hence why this is not done in the
	// initialization loop). Note this also must come after the thermistors are initialized in peripheralsReconfigure.
	for (uint16_t ltcIndex = 0; ltcIndex < LTC_COUNT; ++ltcIndex)
	{
		for (uint8_t gpioIndex = 0; gpioIndex < LTC6813_GPIO_COUNT; ++gpioIndex)
		{
			// TODO(Barach): Different
			// GPIO 0 => Thermistor 4
			// GPIO 1 => Thermistor 3
			// GPIO 2 => Thermistor 2
			// GPIO 3 => Thermistor 1
			// GPIO 4 => Thermistor 0
			analogSensor_t* sensor = (analogSensor_t*) &thermistors [ltcIndex] [LTC6813_GPIO_COUNT - gpioIndex - 1];
			ltc6813SetGpioSensor (&ltcs [ltcIndex], gpioIndex, sensor);
		}
	}

	// Set the on shutdown loop open callback
	palEnableLineEvent (LINE_SHUTDOWN_STATUS, PAL_EVENT_MODE_RISING_EDGE);
	palSetLineCallback (LINE_SHUTDOWN_STATUS, onShutdownLoopOpen, NULL);

	// TODO(Barach): Reimplement
	// Test the LTC sense lines
	// ltc6813Start (ltcBottom);
	// ltc6813WakeupSleep (ltcBottom);
	// ltc6813OpenWireTest (ltcBottom);
	// ltc6813Stop (ltcBottom);

	return true;
}

void peripheralsReconfigure (void* caller)
{
	(void) caller;

	chMtxLock (&peripheralMutex);

	// Thermistor initialization
	for (uint16_t ltcIndex = 0; ltcIndex < LTC_COUNT; ++ltcIndex)
		for (uint16_t thermistorIndex = 0; thermistorIndex < LTC6813_GPIO_COUNT; ++thermistorIndex)
			thermistorPulldownInit (&thermistors [ltcIndex][thermistorIndex], &physicalEepromMap->thermistorConfig);

	// Current sensor initialization
	dhabS124Init (&currentSensor, &physicalEepromMap->currentSensorConfig);

	// Power rolling average
	powerRollingAverageCount = physicalEepromMap->powerRollingAverageCount;
	if (powerRollingAverageCount > POWER_ROLLING_AVERAGE_MAX_COUNT)
		powerRollingAverageCount = POWER_ROLLING_AVERAGE_MAX_COUNT;

	chMtxUnlock (&peripheralMutex);
}

void peripheralsSample (sysinterval_t period)
{
	// Sample the current sensor
	stmAdcSample (&adc);

	// Calculate the pack voltage
	packVoltage = 0.0f;
	for (uint16_t ltcIndex = 0; ltcIndex < LTC_COUNT; ++ltcIndex)
		for (uint16_t cellIndex = 0; cellIndex < LTC6813_CELL_COUNT; ++cellIndex)
			packVoltage += ltcs [ltcIndex].cellVoltages [cellIndex];

	// Calculate the power, power rolling average, and energy delivered
	float power = packVoltage * currentSensor.value;
	powerRollingAverage = rollingAverageCalculate (power, powerHistory, powerRollingAverageCount);
	energyDelivered += power * TIME_I2US (period) * (1e-9f / 3600.0f);
}

void peripheralsCheckState ()
{
	// LTC-specific faults
	isospiFault = ltc6813IsospiFault (ltcBottom);
	selfTestFault = ltc6813SelfTestFault (ltcBottom);

	// Cell voltage / sense line faults
	for (uint16_t ltcIndex = 0; ltcIndex < LTC_COUNT; ++ltcIndex)
	{
		for (uint16_t cellIndex = 0; cellIndex < LTC6813_CELL_COUNT; ++cellIndex)
		{
			bool undervoltage = ltcs [ltcIndex].cellVoltages [cellIndex] < physicalEepromMap->cellVoltageMin;
			bool overvoltage = ltcs [ltcIndex].cellVoltages [cellIndex] > physicalEepromMap->cellVoltageMax;

			if (undervoltage || overvoltage)
			{
				// If a fault is present, increment the counter.
				++cellVoltageFaultCounters [ltcIndex][cellIndex];

				// If the fault threshold is exceeded, set the appropriate flag
				if (cellVoltageFaultCounters [ltcIndex][cellIndex] >= physicalEepromMap->cellVoltageFaultThreshold)
				{
					undervoltageFault |= undervoltage;
					overvoltageFault |= overvoltage;
				}
			}
			else
				// If no fault is present, reset the counter.
				cellVoltageFaultCounters [ltcIndex][cellIndex] = 0;

		}

		for (uint16_t senseLineIndex = 0; senseLineIndex < LTC6813_WIRE_COUNT; ++senseLineIndex)
			senseLineFault |= ltcs [ltcIndex].openWireFaults [senseLineIndex];
	}

	// Temperature faults
	for (uint16_t ltcIndex = 0; ltcIndex < LTC_COUNT; ++ltcIndex)
	{
		for (uint16_t thermistorIndex = 0; thermistorIndex < LTC6813_GPIO_COUNT; ++thermistorIndex)
		{
			bool undertemperature = thermistors [ltcIndex][thermistorIndex].undertemperatureFault;
			bool overtemperature = thermistors [ltcIndex][thermistorIndex].overtemperatureFault;

			if (undertemperature || overtemperature)
			{
				// If a fault is present, increment the counter.
				++temperatureFaultCounters [ltcIndex][thermistorIndex];

				// If the fault threshold is exceeded, set the appropriate flag
				if (temperatureFaultCounters [ltcIndex][thermistorIndex] >= physicalEepromMap->temperatureFaultThreshold)
				{
					undertemperatureFault |= undertemperature;
					overtemperatureFault |= overtemperature;
				}
			}
		}

		overtemperatureFault |= ltcs [ltcIndex].dieTemperature > physicalEepromMap->ltcTemperatureMax;
	}

	// If any fault is present, the BMS is faulted.
	bmsFault = undervoltageFault || overvoltageFault || isospiFault || senseLineFault || selfTestFault
		|| undertemperatureFault || overtemperatureFault;

	// General state
	shutdownLoopClosed = !palReadLine (LINE_SHUTDOWN_STATUS);
	prechargeComplete = !palReadLine (LINE_PRECHARGE_STATUS);
	bmsFaultRelay = !palReadLine (LINE_BMS_FLTDD);
	imdFaultRelay = !palReadLine (LINE_IMD_FLT);

	// Reset the shutdown loop blip status
	if (shutdownLoopBlip && chTimeDiffX (shutdownLoopBlipTime, chVTGetSystemTimeX ()) < TIME_MS2I (1000))
		shutdownLoopBlip = false;

	// If a fault is present, open the shutdown loop.
	bool fltLine = !bmsFault;
	palWriteLine (LINE_BMS_FLT, fltLine);
}