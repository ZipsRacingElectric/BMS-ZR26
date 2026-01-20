#ifndef PERIPHERALS_H
#define PERIPHERALS_H

// Peripherals ----------------------------------------------------------------------------------------------------------------
//
// Author: Cole Barach
// Date Created: 2025.02.20
//
// Description: Global objects representing the low-voltage hardware of the BMS.

// Includes -------------------------------------------------------------------------------------------------------------------

// Includes
#include "peripherals/eeprom_map.h"

#include "peripherals/adc/dhab_s124.h"
#include "peripherals/adc/stm_adc.h"
#include "peripherals/adc/thermistor_pulldown.h"

#include "peripherals/i2c/mc24lc32.h"
#include "peripherals/spi/ltc6811.h"

// Constants ------------------------------------------------------------------------------------------------------------------

/// @brief The number of LTC BMS ICs in the daisy chain. Note this must be even.
#define LTC_COUNT 12

/// @brief The number of cells in the accumulator.
#define CELL_COUNT (LTC_COUNT * LTC6811_CELL_COUNT)

/// @brief The number of sense lines in the accumulator.
#define WIRE_COUNT (LTC_COUNT * (LTC6811_CELL_COUNT + 1))

/// @brief The number of temperature sensors in the accumulator.
#define TEMP_COUNT (LTC_COUNT * LTC6811_GPIO_COUNT)

/// @brief The maximum number of sample to include in the @c powerRollingAverage statistic. Note this isn't the actual number
/// of samples used, that is defined in the @c physicalEepromMap .
#define POWER_ROLLING_AVERAGE_MAX_COUNT 256

// Global State ---------------------------------------------------------------------------------------------------------------

/// @brief The voltage of the entire pack, as measured by the LTCs.
extern float packVoltage;

/// @brief The rolling average of the instantaneous power being delivered by the pack, in Watts.
extern float powerRollingAverage;

/// @brief The total energy delivered by the pack since power up, in kilowatt hours.
extern float energyDelivered;

/// @brief Indicates whether any faults are present.
extern bool bmsFault;

/// @brief Indicates an undervoltage fault is present.
extern bool undervoltageFault;

/// @brief Indicates an overvoltage fault is present.
extern bool overvoltageFault;

/// @brief Indicates an undertemperature fault is present.
extern bool undertemperatureFault;

/// @brief Indicates an overtemperature fault is present.
extern bool overtemperatureFault;

/// @brief Indicates a sense-line fault is present.
extern bool senseLineFault;

/// @brief Indicates an IsoSPI fault is present.
extern bool isospiFault;

/// @brief Indicates an LTC self-test fault is present.
extern bool selfTestFault;

/// @brief Indicates the BMS is in charging mode and the charger is powered.
extern bool charging;

/// @brief Indicates the BMS is balancing cell voltages.
extern bool balancing;

/// @brief Indicates the shutdown loop is closed (up to precharge circuit).
extern bool shutdownLoopClosed;

/// @brief Indicates the shutdown loop is closed and precharge is complete.
extern bool prechargeComplete;

/// @brief Indicates the shutdown loop opened briefly when it was previously read to be closed.
extern bool shutdownLoopBlip;
extern systime_t shutdownLoopBlipTime;

/// @brief Indicates the status of the BMS fault relay. This is a latched version of @c bmsFault . True means the relay is open
/// (faulted), false means closed (okay).
extern bool bmsFaultRelay;

/// @brief Indicates the status of the IMD fault relay. True means the relay is open (faulted), false means closed (okay).
extern bool imdFaultRelay;

// Global Peripherals ---------------------------------------------------------------------------------------------------------

/// @brief Mutex guarding access to the global peripherals.
extern mutex_t peripheralMutex;

/// @brief The STM's on-board ADC.
extern stmAdc_t adc;

/// @brief The BMS's physical (on-board) EEPROM. This is responsible for storing all non-volatile variables.
extern mc24lc32_t physicalEeprom;

/// @brief Structure mapping the physical EEPROM's contents to C datatypes.
static eepromMap_t* const physicalEepromMap = (eepromMap_t*) physicalEeprom.cache;

/// @brief The BMS's virtual memory map. This aggregates all externally accessible memory into a single map for CAN-bus access.
extern virtualEeprom_t virtualEeprom;

/// @brief The BMS's sense-board ICs. Indexed from negative-most potential LTC to positive-most potential LTC.
/// @note The indexing of this is not the same as the daisy chain's indexing. This is referred to as the 'logical' indexing.
extern ltc6811_t ltcs [LTC_COUNT];

/// @brief The first LTC in the IsoSPI daisy chain. Used as the operand in all LTC operations.
extern ltc6811_t* ltcBottom;

/// @brief The BMS's sense-board thermistors. Indexed from negative-most potential to positive-most potential, then by
/// thermistor index (not necessarily the same at the LTC's GPIO index).
extern thermistorPulldown_t thermistors [LTC_COUNT][LTC6811_GPIO_COUNT];

/// @brief The BMS's pack current sensor.
extern dhabS124_t currentSensor;

// Functions ------------------------------------------------------------------------------------------------------------------

/**
 * @brief Initializes the BMS's peripherals.
 * @return False if a fatal peripheral failed to initialize, true otherwise.
 */
bool peripheralsInit (void);

/**
 * @brief Re-initializes the BMS's peripherals after a change has been made to the on-board EEPROM.
 * @param caller Ignored. Used to make function signature compatible with EEPROM dirty hook.
 */
void peripheralsReconfigure (void* caller);

/**
 * @brief Samples the value of all peripherals, excluding the LTCs. The LTC cell voltages should be sampled before calling this
 * function.
 * @param period The amount of time that has passed since the last call to this function.
 */
void peripheralsSample (sysinterval_t period);

/**
 * @brief Computes the global state of all peripherals. This checks fault conditions and hardware state.
 */
void peripheralsCheckState (void);

#endif // PERIPHERALS_H