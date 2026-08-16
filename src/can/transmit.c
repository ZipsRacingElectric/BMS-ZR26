// Header
#include "transmit.h"

// Conversions -----------------------------------------------------------------------------------------------------------------

// Cell Voltage Values (V)
#define CELL_VOLTAGE_INVERSE_FACTOR			(10000.0f / 64.0f)
#define CELL_VOLTAGE_TO_WORD(voltage)		(uint16_t) ((voltage) * CELL_VOLTAGE_INVERSE_FACTOR)
#define CELL_VOLTAGE_MIN					0
#define CELL_VOLTAGE_MAX					(((1 << 10) - 1) / CELL_VOLTAGE_INVERSE_FACTOR)

#define CELL_DELTA_INVERSE_FACTOR			(10000.0f / 16.0f)
#define CELL_DELTA_TO_WORD(delta)			(uint16_t) ((delta) * CELL_DELTA_INVERSE_FACTOR)
#define CELL_DELTA_MIN						0
#define CELL_DELTA_MAX						(((1 << 12) - 1) / CELL_DELTA_INVERSE_FACTOR)

// Cell Temperature Values (C)
#define CELL_TEMP_INVERSE_FACTOR			(4096.0f / 256.0f)
#define CELL_TEMP_OFFSET					-106.0f
#define CELL_TEMP_TO_WORD(temperature)		(uint16_t) ((temperature - CELL_TEMP_OFFSET) * CELL_TEMP_INVERSE_FACTOR)

// LTC Temperature Values (C)
#define LTC_TEMP_INVERSE_FACTOR				(1024.0f / 128.0f)
#define LTC_TEMP_OFFSET 					-28.0f
#define LTC_TEMP_TO_WORD(temperature)		(uint16_t) ((temperature - LTC_TEMP_OFFSET) * LTC_TEMP_INVERSE_FACTOR)

// Pack Voltage (V)
#define PACK_VOLTAGE_INVERSE_FACTOR			(65536.0f / 819.2f)
#define PACK_VOLTAGE_TO_WORD(voltage)		(uint16_t) ((voltage) * PACK_VOLTAGE_INVERSE_FACTOR)

// Pack Current (A)
#define PACK_CURRENT_INVERSE_FACTOR			(32768.0f / 625.0f)
#define PACK_CURRENT_TO_WORD(current)		((int16_t) ((current) * PACK_CURRENT_INVERSE_FACTOR))

// Power (kW)
#define POWER_INVERSE_FACTOR				(1 / 0.004f)
#define POWER_TO_WORD(power)				((int16_t) ((power) * POWER_INVERSE_FACTOR))

// Energy (kWh)
#define ENERGY_INVERSE_FACTOR				(1 / 0.0005)
#define ENERGY_TO_WORD(energy)				((int16_t) ((energy) * ENERGY_INVERSE_FACTOR))

// Message IDs ----------------------------------------------------------------------------------------------------------------

#define STATUS_MESSAGE_ID					0x101
#define POWER_MESSAGE_ID					0x102
#define STAT_MESSAGE_ID						0x103
#define TEMP_POWER_MESSAGE_ID				0x104
#define VOLTAGE_MESSAGE_BASE_ID				0x700
#define TEMPERATURE_MESSAGE_BASE_ID			0x71E
#define SENSE_LINE_STATUS_BASE_ID			0x728
#define BALANCING_MESSAGE_BASE_ID			0x72B
#define LTC_TEMPERATURE_MESSAGE_BASE_ID		0x72E

// Functions ------------------------------------------------------------------------------------------------------------------

void transmitBmsMessages (sysinterval_t timeout)
{
	// Status message
	transmitStatusMessage (&CAND1, timeout);

	// Power message
	transmitPowerMessage (&CAND1, timeout);

	// Cell stats message
	transmitCellStatsMessage (&CAND1, timeout);

	// Temp stats and power message
	transmitTempStatsPowerMessage (&CAND1, timeout);

	// Cell voltage messages
	for (uint16_t index = 0; index < VOLTAGE_MESSAGE_COUNT; ++index)
		transmitVoltageMessage (&CAND1, timeout, index);

	// Sense line temperature messages
	for (uint16_t index = 0; index < TEMPERATURE_MESSAGE_COUNT; ++index)
		transmitTemperatureMessage (&CAND1, timeout, index);

	// Sense line status messages
	for (uint16_t index = 0; index < SENSE_LINE_STATUS_MESSAGE_COUNT; ++index)
		transmitSenseLineStatusMessage (&CAND1, timeout, index);

	// Cell balancing messages
	for (uint16_t index = 0; index < BALANCING_MESSAGE_COUNT; ++index)
		transmitBalancingMessage (&CAND1, timeout, index);

	// LTC temperature messages
	for (uint16_t index = 0; index < LTC_TEMPERATURE_MESSAGE_COUNT; ++index)
		transmitLtcTemperatureMessage (&CAND1, timeout, index);
}

msg_t transmitStatusMessage (CANDriver* driver, sysinterval_t timeout)
{
	CANTxFrame frame =
	{
		.DLC	= 8,
		.IDE	= CAN_IDE_STD,
		.SID	= STATUS_MESSAGE_ID,
		.data8	=
		{
			isospiFault |
			(selfTestFault << 1) |
			(senseLineFault << 2) |
			(undervoltageFault << 3) |
			(overvoltageFault << 4) |
			(undertemperatureFault << 5) |
			(overtemperatureFault << 6) |
			(bmsFault << 7),
			imdFault |
			(charging << 1) |
			(balancing << 2) |
			(shutdownVehicleClosed << 3) |
			(shutdownImdClosed << 4) |
			(shutdownBmsClosed << 5) |
			(shutdownMsdTsmsClosed << 6) |
			(shutdownLoopBlip << 7),
			negativeIrEnabled |
			(positiveIrEnabled << 1) |
			(physicalEeprom.state << 2)
		}
	};

	// IsoSPI faults
	for (uint8_t index = 0; index < LTC_COUNT; ++index)
		frame.data16 [2] |= (ltcs [index].state == LTC681X_STATE_FAILED || ltcs [index].state == LTC681X_STATE_PEC_ERROR) << index;

	// Self test faults
	for (uint8_t index = 0; index < LTC_COUNT; ++index)
		frame.data16 [3] |= (ltcs [index].state == LTC681X_STATE_SELF_TEST_FAULT) << index;

	// Limp Mode
	frame.data8 [5] |= (limpMode << 2);

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}

msg_t transmitPowerMessage (CANDriver* driver, sysinterval_t timeout)
{
	float powerkW = packVoltage * currentSensor.value * 1e-3;

	CANTxFrame frame =
	{
		.DLC	= 8,
		.IDE	= CAN_IDE_STD,
		.SID	= POWER_MESSAGE_ID,
		.data16	=
		{
			PACK_VOLTAGE_TO_WORD (packVoltage),
			PACK_CURRENT_TO_WORD (currentSensor.value),
			POWER_TO_WORD (powerkW),
			ENERGY_TO_WORD (energyDelivered)
		}
	};

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}

msg_t transmitCellStatsMessage (CANDriver* driver, sysinterval_t timeout)
{
	uint16_t voltages [5] =
	{
		CELL_DELTA_TO_WORD (cellVoltageMin),
		CELL_DELTA_TO_WORD (cellVoltageMax),
		CELL_DELTA_TO_WORD (cellVoltageAverage),
		CELL_DELTA_TO_WORD (cellDeltaMax),
		CELL_DELTA_TO_WORD (cellDeltaAverage)
	};

	CANTxFrame frame =
	{
		.DLC	= 8,
		.IDE	= CAN_IDE_STD,
		.SID	= STAT_MESSAGE_ID,
		.data8	=
		{
			voltages [0],
			(voltages [1] << 4) | ((voltages [0] >> 8) & 0b1111),
			voltages [1] >> 4,
			voltages [2],
			(voltages [3] << 4) | ((voltages [2] >> 8) & 0b1111),
			voltages [3] >> 4,
			voltages [4],
			(voltages [4] >> 8) & 0b1111
		}
	};

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}

msg_t transmitTempStatsPowerMessage (CANDriver* driver, sysinterval_t timeout)
{
	uint16_t temps [2] =
	{
		CELL_TEMP_TO_WORD (senseLineTempMax),
		CELL_TEMP_TO_WORD (senseLineTempAverage)
	};

	int16_t powerWord = POWER_TO_WORD (powerRollingAverage * 1e-3);

	CANTxFrame frame =
	{
		.DLC	= 5,
		.IDE	= CAN_IDE_STD,
		.SID	= TEMP_POWER_MESSAGE_ID,
		.data8	=
		{
			temps [0],
			(temps [1] << 4) | ((temps [0] >> 8) & 0b1111),
			temps [1] >> 4,
			powerWord,
			powerWord >> 8
		}
	};

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}

msg_t transmitVoltageMessage (CANDriver* driver, sysinterval_t timeout, uint16_t index)
{
	uint16_t ltcIndex = index / 3;
	uint8_t voltOffset = (index % 3) * 6;
	uint8_t dlc = 8;
	if (index % 3 == 2)
		dlc = 3;

	uint16_t voltages [6];
	for (uint8_t voltIndex = 0; voltIndex < 6; ++voltIndex)
	{
		float voltage = ltcs [ltcIndex].cellVoltages [voltOffset + voltIndex];
		if (voltage < CELL_VOLTAGE_MIN)
			voltage = CELL_VOLTAGE_MIN;
		else if (voltage > CELL_VOLTAGE_MAX)
			voltage = CELL_VOLTAGE_MAX;

		voltages [voltIndex] = CELL_VOLTAGE_TO_WORD (voltage);
	}

	CANTxFrame frame =
	{
		.DLC	= dlc,
		.IDE	= CAN_IDE_STD,
		.SID	= VOLTAGE_MESSAGE_BASE_ID + index,
		.data8	=
		{
			voltages [0],
			(voltages [1] << 2) | ((voltages [0] >> 8) & 0b11),
			(voltages [2] << 4) | ((voltages [1] >> 6) & 0b1111),
			(voltages [3] << 6) | ((voltages [2] >> 4) & 0b111111),
			voltages [3] >> 2,
			voltages [4],
			(voltages [5] << 2) | ((voltages [4] >> 8) & 0b11),
			((voltages [5] >> 6) & 0b1111)
		}
	};

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}

msg_t transmitTemperatureMessage (CANDriver* driver, sysinterval_t timeout, uint16_t index)
{
	uint16_t temperatures [5];

	for (uint8_t tempIndex = 0; tempIndex < 5; ++tempIndex)
		temperatures [tempIndex] = CELL_TEMP_TO_WORD (thermistors [index][tempIndex].temperature);

	CANTxFrame frame =
	{
		.DLC	= 8,
		.IDE	= CAN_IDE_STD,
		.SID	= TEMPERATURE_MESSAGE_BASE_ID + index,
		.data8	=
		{
			temperatures [0],
			(temperatures [1] << 4) | ((temperatures [0] >> 8) & 0b1111),
			temperatures [1] >> 4,
			temperatures [2],
			(temperatures [3] << 4) | ((temperatures [2] >> 8) & 0b1111),
			temperatures [3] >> 4,
			temperatures [4],
			((temperatures [4] >> 8) & 0b1111)
		}
	};

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}

msg_t transmitSenseLineStatusMessage (CANDriver* driver, sysinterval_t timeout, uint16_t index)
{
	uint16_t ltcIndex = index * 4;

	CANTxFrame frame =
	{
		.DLC	= 2,
		.IDE	= CAN_IDE_STD,
		.SID	= SENSE_LINE_STATUS_BASE_ID + index,
	};

	for (uint8_t bit = 0; bit < WIRES_PER_LTC; ++bit)
		frame.data16 [0] |= ltcs [ltcIndex].openWireFaults [bit] << bit;

	if (ltcIndex + 1 < LTC_COUNT)
	{
		for (uint8_t bit = 0; bit < WIRES_PER_LTC; ++bit)
			frame.data16 [1] |= ltcs [ltcIndex + 1].openWireFaults [bit] << bit;
		frame.DLC += 2;
	}

	if (ltcIndex + 2 < LTC_COUNT)
	{
		for (uint8_t bit = 0; bit < WIRES_PER_LTC; ++bit)
			frame.data16 [2] |= ltcs [ltcIndex + 2].openWireFaults [bit] << bit;
		frame.DLC += 2;
	}

	if (ltcIndex + 3 < LTC_COUNT)
	{
		for (uint8_t bit = 0; bit < WIRES_PER_LTC; ++bit)
			frame.data16 [3] |= ltcs [ltcIndex + 3].openWireFaults [bit] << bit;
		frame.DLC += 2;
	}

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}

msg_t transmitBalancingMessage (CANDriver* driver, sysinterval_t timeout, uint16_t index)
{
	uint16_t ltcIndex = index * 4;

	CANTxFrame frame =
	{
		.DLC	= 2,
		.IDE	= CAN_IDE_STD,
		.SID	= BALANCING_MESSAGE_BASE_ID + index,
	};

	for (uint8_t bit = 0; bit < CELLS_PER_LTC; ++bit)
		frame.data16 [0] |= ltcs [ltcIndex].cellsDischarging [bit] << bit;

	if (ltcIndex + 1 < LTC_COUNT)
	{
		for (uint8_t bit = 0; bit < CELLS_PER_LTC; ++bit)
			frame.data16 [1] |= ltcs [ltcIndex + 1].cellsDischarging [bit] << bit;
		frame.DLC += 2;
	}

	if (ltcIndex + 2 < LTC_COUNT)
	{
		for (uint8_t bit = 0; bit < CELLS_PER_LTC; ++bit)
			frame.data16 [2] |= ltcs [ltcIndex + 2].cellsDischarging [bit] << bit;
		frame.DLC += 2;
	}

	if (ltcIndex + 3 < LTC_COUNT)
	{
		for (uint8_t bit = 0; bit < CELLS_PER_LTC; ++bit)
			frame.data16 [3] |= ltcs [ltcIndex + 3].cellsDischarging [bit] << bit;
		frame.DLC += 2;
	}

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}

msg_t transmitLtcTemperatureMessage (CANDriver* driver, sysinterval_t timeout, uint16_t index)
{
	uint16_t temperatures [6] = {};
	uint16_t ltcBase = index * 6;
	uint8_t dlc = 0;
	for (uint16_t ltcOffset = 0; ltcOffset < 6; ++ltcOffset)
	{
		if (ltcOffset + ltcBase >= LTC_COUNT)
			break;

		if (ltcOffset % 4 == 0)
			dlc += 2;
		else
			dlc += 1;

		temperatures [ltcOffset] = LTC_TEMP_TO_WORD (ltcs [ltcBase + ltcOffset].dieTemperature);
	}

	CANTxFrame frame =
	{
		.DLC	= dlc,
		.IDE	= CAN_IDE_STD,
		.SID	= LTC_TEMPERATURE_MESSAGE_BASE_ID + index,
		.data8	=
		{
			temperatures [0],
			(temperatures [1] << 2) | ((temperatures [0] >> 8) & 0b11),
			(temperatures [2] << 4) | ((temperatures [1] >> 6) & 0b1111),
			(temperatures [3] << 6) | ((temperatures [2] >> 4) & 0b111111),
			temperatures [3] >> 2,
			temperatures [4],
			(temperatures [5] << 2) | ((temperatures [4] >> 8) & 0b11),
			(temperatures [5] >> 6) & 0b1111
		}
	};

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}