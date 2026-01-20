// Header
#include "transmit.h"

// Conversions -----------------------------------------------------------------------------------------------------------------

// Cell Voltage Values (V)
#define CELL_VOLTAGE_INVERSE_FACTOR			(1024.0f / 8.0f)
#define CELL_VOLTAGE_TO_WORD(voltage)		(uint16_t) ((voltage) * CELL_VOLTAGE_INVERSE_FACTOR)

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
#define VOLTAGE_MESSAGE_BASE_ID				0x700
#define TEMPERATURE_MESSAGE_BASE_ID			0x718
#define SENSE_LINE_STATUS_BASE_ID			0x724
#define POWER_MESSAGE_ID					0x102
#define BALANCING_MESSAGE_BASE_ID			0x729
#define LTC_TEMPERATURE_MESSAGE_BASE_ID		0x754

// Functions ------------------------------------------------------------------------------------------------------------------

void transmitBmsMessages (sysinterval_t timeout)
{
	// Status message
	systime_t timeCurrent = chVTGetSystemTimeX ();
	systime_t timeDeadline = chTimeAddX (timeCurrent, timeout);
	transmitStatusMessage (&CAND1, timeout);

	// Power message
	timeCurrent = chVTGetSystemTimeX ();
	timeout = chTimeDiffX (timeCurrent, timeDeadline);

	// Cell voltage messages
	for (uint16_t index = 0; index < VOLTAGE_MESSAGE_COUNT; ++index)
	{
		timeCurrent = chVTGetSystemTimeX ();
		timeout = chTimeDiffX (timeCurrent, timeDeadline);
		transmitVoltageMessage (&CAND1, timeout, index);
	}

	// Sense line temperature messages
	for (uint16_t index = 0; index < TEMPERATURE_MESSAGE_COUNT; ++index)
	{
		timeCurrent = chVTGetSystemTimeX ();
		timeout = chTimeDiffX (timeCurrent, timeDeadline);
		transmitTemperatureMessage (&CAND1, timeout, index);
	}

	// Sense line status messages
	for (uint16_t index = 0; index < SENSE_LINE_STATUS_MESSAGE_COUNT; ++index)
	{
		timeCurrent = chVTGetSystemTimeX ();
		timeout = chTimeDiffX (timeCurrent, timeDeadline);
		transmitSenseLineStatusMessage (&CAND1, timeout, index);
	}

	// Cell balancing messages
	for (uint16_t index = 0; index < BALANCING_MESSAGE_COUNT; ++index)
	{
		timeCurrent = chVTGetSystemTimeX ();
		timeout = chTimeDiffX (timeCurrent, timeDeadline);
		transmitBalancingMessage (&CAND1, timeout, index);
	}

	transmitPowerMessage (&CAND1, timeout);

	// LTC temperature messages
	for (uint16_t index = 0; index < LTC_TEMPERATURE_MESSAGE_COUNT; ++index)
	{
		timeCurrent = chVTGetSystemTimeX ();
		timeout = chTimeDiffX (timeCurrent, timeDeadline);
		transmitLtcTemperatureMessage (&CAND1, timeout, index);
	}
}

msg_t transmitStatusMessage (CANDriver* driver, sysinterval_t timeout)
{
	CANTxFrame frame =
	{
		.DLC	= 6,
		.IDE	= CAN_IDE_STD,
		.SID	= STATUS_MESSAGE_ID,
		.data8	=
		{
			undervoltageFault |
			(overvoltageFault << 1) |
			(undertemperatureFault << 2) |
			(overtemperatureFault << 3) |
			(senseLineFault << 4) |
			(isospiFault << 5) |
			(selfTestFault << 6) |
			(charging << 7),
			balancing |
			(shutdownLoopClosed << 1) |
			(prechargeComplete << 2) |
			(shutdownLoopBlip << 3) |
			(bmsFaultRelay << 4) |
			(imdFaultRelay << 5) |
			(bmsFault << 6)
		}
	};

	// IsoSPI faults
	for (uint8_t index = 0; index < LTC_COUNT; ++index)
		frame.data16 [1] |= (ltcs [index].state == LTC6811_STATE_FAILED || ltcs [index].state == LTC6811_STATE_PEC_ERROR) << index;

	// Self test faults
	for (uint8_t index = 0; index < LTC_COUNT; ++index)
		frame.data16 [2] |= (ltcs [index].state == LTC6811_STATE_SELF_TEST_FAULT) << index;

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}

msg_t transmitPowerMessage (CANDriver* driver, sysinterval_t timeout)
{
	float power_kW = powerRollingAverage * 1e-3;

	CANTxFrame frame =
	{
		.DLC	= 8,
		.IDE	= CAN_IDE_STD,
		.SID	= POWER_MESSAGE_ID,
		.data16	=
		{
			PACK_VOLTAGE_TO_WORD (packVoltage),
			PACK_CURRENT_TO_WORD (currentSensor.value),
			POWER_TO_WORD (power_kW),
			ENERGY_TO_WORD (energyDelivered)
		}
	};

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}

msg_t transmitVoltageMessage (CANDriver* driver, sysinterval_t timeout, uint16_t index)
{
	uint16_t ltcIndex = index / 2;
	uint8_t voltOffset = (index % 2) * 6;

	uint16_t voltages [6];
	for (uint8_t voltIndex = 0; voltIndex < 6; ++voltIndex)
		voltages [voltIndex] = CELL_VOLTAGE_TO_WORD (ltcs [ltcIndex].cellVoltages [voltOffset + voltIndex]);

	CANTxFrame frame =
	{
		.DLC	= 8,
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
		.DLC	= 8,
		.IDE	= CAN_IDE_STD,
		.SID	= SENSE_LINE_STATUS_BASE_ID + index,
	};

	for (uint8_t bit = 0; bit < LTC6811_CELL_COUNT + 1; ++bit)
		frame.data16 [0] |= ltcs [index].openWireFaults [bit] << bit;

	if (LTC_COUNT > ltcIndex + 1)
		for (uint8_t bit = 0; bit < LTC6811_CELL_COUNT + 1; ++bit)
			frame.data16 [1] |= ltcs [index + 1].openWireFaults [bit] << bit;

	if (LTC_COUNT > ltcIndex + 2)
		for (uint8_t bit = 0; bit < LTC6811_CELL_COUNT + 1; ++bit)
			frame.data16 [2] |= ltcs [index + 2].openWireFaults [bit] << bit;

	if (LTC_COUNT > ltcIndex + 3)
		for (uint8_t bit = 0; bit < LTC6811_CELL_COUNT + 1; ++bit)
			frame.data16 [3] |= ltcs [index + 3].openWireFaults [bit] << bit;

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}

msg_t transmitBalancingMessage (CANDriver* driver, sysinterval_t timeout, uint16_t index)
{
	uint16_t ltcIndex = index * 4;

	CANTxFrame frame =
	{
		.DLC	= 8,
		.IDE	= CAN_IDE_STD,
		.SID	= BALANCING_MESSAGE_BASE_ID + index,
	};

	for (uint8_t bit = 0; bit < LTC6811_CELL_COUNT; ++bit)
		frame.data16 [0] |= ltcs [index].cellsDischarging [bit] << bit;

	if (LTC_COUNT > ltcIndex + 1)
		for (uint8_t bit = 0; bit < LTC6811_CELL_COUNT; ++bit)
			frame.data16 [1] |= ltcs [index + 1].cellsDischarging [bit] << bit;

	if (LTC_COUNT > ltcIndex + 2)
		for (uint8_t bit = 0; bit < LTC6811_CELL_COUNT; ++bit)
			frame.data16 [2] |= ltcs [index + 2].cellsDischarging [bit] << bit;

	if (LTC_COUNT > ltcIndex + 3)
		for (uint8_t bit = 0; bit < LTC6811_CELL_COUNT; ++bit)
			frame.data16 [3] |= ltcs [index + 3].cellsDischarging [bit] << bit;

	return canTransmitTimeout (driver, CAN_ANY_MAILBOX, &frame, timeout);
}

msg_t transmitLtcTemperatureMessage (CANDriver* driver, sysinterval_t timeout, uint16_t index)
{
	uint16_t temperatures [6] = {};
	uint16_t ltcBase = index * 6;
	for (uint16_t ltcOffset = 0; ltcOffset < 6; ++ltcOffset)
	{
		if (ltcOffset + ltcBase >= LTC_COUNT)
			break;

		temperatures [ltcOffset] = LTC_TEMP_TO_WORD (ltcs [ltcBase + ltcOffset].dieTemperature);
	}

	CANTxFrame frame =
	{
		.DLC	= 8,
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