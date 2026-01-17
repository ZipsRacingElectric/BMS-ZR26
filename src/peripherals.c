// Header
#include "peripherals.h"

// Includes
#include "peripherals/adc/stm_adc.h"
#include "controls/rolling_average.h"

// TODO(Barach): This is pretty messy, whole lot of hard-coded values and copy-paste code.

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

// Global Peripherals ---------------------------------------------------------------------------------------------------------

// Public
mutex_t					peripheralMutex;
stmAdc_t				adc;
mc24lc32_t				physicalEeprom;
virtualEeprom_t			virtualEeprom;
ltc6811_t				ltcs [LTC_COUNT];
ltc6811_t*				ltcBottom;
thermistorPulldown_t	thermistors [LTC_COUNT][LTC6811_GPIO_COUNT];
dhabS124_t				currentSensor;

// Private
static eeprom_t			readonlyWriteonlyEeprom;

#define POWER_ROLLING_AVERAGE_MAX_COUNT 256
float powerHistory [POWER_ROLLING_AVERAGE_MAX_COUNT - 1] = { 0.0f };

uint16_t powerRollingAverageCount = 1;

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
static const ltc6811Config_t DAISY_CHAIN_CONFIG =
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
	.cellAdcMode			= LTC6811_ADC_422HZ,				// 422 Hz ADC sampling for cell voltages.
	.gpioAdcMode			= LTC6811_ADC_27KHZ,				// 422 Hz ADC sampling for the thermistors.
	.dischargeAllowed		= true,								// Allow cell discharging.
	.dischargeTimeout		= LTC6811_DISCHARGE_TIMEOUT_30_S,	// Timeout cell discharging after 30s of no command.
	.openWireTestIterations	= 3,								// Perform 3 pull-up / pull-down commands before measuring.
	.pollTolerance			= TIME_MS2I (1),					// Allow 1ms of play in each operation's execution time.
	.gpioSensors =												// Thermistor references. Note this must match the daisy chain
																// ordering.
	{
		{
			(analogSensor_t*) &thermistors [1][4],
			(analogSensor_t*) &thermistors [1][3],
			(analogSensor_t*) &thermistors [1][2],
			(analogSensor_t*) &thermistors [1][1],
			(analogSensor_t*) &thermistors [1][0],
		},
		{
			(analogSensor_t*) &thermistors [0][4],
			(analogSensor_t*) &thermistors [0][3],
			(analogSensor_t*) &thermistors [0][2],
			(analogSensor_t*) &thermistors [0][1],
			(analogSensor_t*) &thermistors [0][0],
		},
		{
			(analogSensor_t*) &thermistors [3][4],
			(analogSensor_t*) &thermistors [3][3],
			(analogSensor_t*) &thermistors [3][2],
			(analogSensor_t*) &thermistors [3][1],
			(analogSensor_t*) &thermistors [3][0],
		},
		{
			(analogSensor_t*) &thermistors [2][4],
			(analogSensor_t*) &thermistors [2][3],
			(analogSensor_t*) &thermistors [2][2],
			(analogSensor_t*) &thermistors [2][1],
			(analogSensor_t*) &thermistors [2][0],
		},
		{
			(analogSensor_t*) &thermistors [5][4],
			(analogSensor_t*) &thermistors [5][3],
			(analogSensor_t*) &thermistors [5][2],
			(analogSensor_t*) &thermistors [5][1],
			(analogSensor_t*) &thermistors [5][0],
		},
		{
			(analogSensor_t*) &thermistors [4][4],
			(analogSensor_t*) &thermistors [4][3],
			(analogSensor_t*) &thermistors [4][2],
			(analogSensor_t*) &thermistors [4][1],
			(analogSensor_t*) &thermistors [4][0],
		},
		{
			(analogSensor_t*) &thermistors [7][4],
			(analogSensor_t*) &thermistors [7][3],
			(analogSensor_t*) &thermistors [7][2],
			(analogSensor_t*) &thermistors [7][1],
			(analogSensor_t*) &thermistors [7][0],
		},
		{
			(analogSensor_t*) &thermistors [6][4],
			(analogSensor_t*) &thermistors [6][3],
			(analogSensor_t*) &thermistors [6][2],
			(analogSensor_t*) &thermistors [6][1],
			(analogSensor_t*) &thermistors [6][0],
		},
		{
			(analogSensor_t*) &thermistors [9][4],
			(analogSensor_t*) &thermistors [9][3],
			(analogSensor_t*) &thermistors [9][2],
			(analogSensor_t*) &thermistors [9][1],
			(analogSensor_t*) &thermistors [9][0],
		},
		{
			(analogSensor_t*) &thermistors [8][4],
			(analogSensor_t*) &thermistors [8][3],
			(analogSensor_t*) &thermistors [8][2],
			(analogSensor_t*) &thermistors [8][1],
			(analogSensor_t*) &thermistors [8][0],
		},
		{
			(analogSensor_t*) &thermistors [11][4],
			(analogSensor_t*) &thermistors [11][3],
			(analogSensor_t*) &thermistors [11][2],
			(analogSensor_t*) &thermistors [11][1],
			(analogSensor_t*) &thermistors [11][0],
		},
		{
			(analogSensor_t*) &thermistors [10][4],
			(analogSensor_t*) &thermistors [10][3],
			(analogSensor_t*) &thermistors [10][2],
			(analogSensor_t*) &thermistors [10][1],
			(analogSensor_t*) &thermistors [10][0],
		}
	},
};

/// @brief The LTC IsoSPI daisy chain, used to accomodate for changes to the IsoSPI wiring.
static ltc6811_t* const DAISY_CHAIN [] =
{
	&ltcs [1],
	&ltcs [0],
	&ltcs [3],
	&ltcs [2],
	&ltcs [5],
	&ltcs [4],
	&ltcs [7],
	&ltcs [6],
	&ltcs [9],
	&ltcs [8],
	&ltcs [11],
	&ltcs [10]
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

	// Reconfigurable peripheral initializations. Note this must occur before the LTC initialization as the LTCs are dependent
	// on the thermistor peripherals.
	peripheralsReconfigure (NULL);

	// LTC daisy chain initialization
	ltc6811Init (DAISY_CHAIN, LTC_COUNT, &DAISY_CHAIN_CONFIG);
	ltcBottom = DAISY_CHAIN [0];

	// Set the on shutdown loop open callback
	palEnableLineEvent (LINE_SHUTDOWN_STATUS, PAL_EVENT_MODE_RISING_EDGE);
	palSetLineCallback (LINE_SHUTDOWN_STATUS, onShutdownLoopOpen, NULL);

	// Test the LTC sense lines
	ltc6811Start (ltcBottom);
	ltc6811WakeupSleep (ltcBottom);
	ltc6811OpenWireTest (ltcBottom);
	ltc6811Stop (ltcBottom);

	return true;
}

void peripheralsReconfigure (void* caller)
{
	(void) caller;

	chMtxLock (&peripheralMutex);

	// Thermistor initialization
	for (uint16_t deviceIndex = 0; deviceIndex < LTC_COUNT; ++deviceIndex)
		for (uint16_t gpioIndex = 0; gpioIndex < LTC6811_GPIO_COUNT; ++gpioIndex)
			thermistorPulldownInit (&thermistors [deviceIndex][gpioIndex], &physicalEepromMap->thermistorConfig);

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
		for (uint16_t cellIndex = 0; cellIndex < LTC6811_CELL_COUNT; ++cellIndex)
			packVoltage += ltcs [ltcIndex].cellVoltages [cellIndex];

	// Calculate the power, power rolling average, and energy delivered
	float power = packVoltage * currentSensor.value;
	powerRollingAverage = rollingAverageCalculate (power, powerHistory, powerRollingAverageCount);
	energyDelivered += power * TIME_I2US (period) * (1e-9f / 3600.0f);
}

void peripheralsCheckState ()
{
	// LTC-specific faults

	for (uint16_t ltcIndex = 0; ltcIndex < LTC_COUNT; ++ltcIndex)
	{
		for (uint16_t cellIndex = 0; cellIndex < LTC6811_CELL_COUNT; ++cellIndex)
		{
			undervoltageFault |= ltcs [ltcIndex].cellVoltages [cellIndex] < physicalEepromMap->cellVoltageMin;
			overvoltageFault |= ltcs [ltcIndex].cellVoltages [cellIndex] > physicalEepromMap->cellVoltageMax;
		}

		for (uint16_t senseLineIndex = 0; senseLineIndex < LTC6811_WIRE_COUNT; ++senseLineIndex)
			senseLineFault |= ltcs [ltcIndex].openWireFaults [senseLineIndex];
	}

	isospiFault = ltc6811IsospiFault (ltcBottom);
	selfTestFault = ltc6811SelfTestFault (ltcBottom);

	// Temperature faults

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