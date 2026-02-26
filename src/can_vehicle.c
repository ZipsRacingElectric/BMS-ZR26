// Header
#include "can_vehicle.h"

// Includes
#include "can/can_thread.h"
#include "can/receive.h"

// Threads --------------------------------------------------------------------------------------------------------------------

static CAN_THREAD_WORKING_AREA (can1RxThreadWa);

// Global Nodes ---------------------------------------------------------------------------------------------------------------

amkInverter_t amks [AMK_COUNT];

#define NODE_COUNT (sizeof (nodes) / sizeof (canNode_t*))
canNode_t* nodes [] =
{
	(canNode_t*) &amkRl, (canNode_t*) &amkRr, (canNode_t*) &amkFl, (canNode_t*) &amkFr
};

// Configuration --------------------------------------------------------------------------------------------------------------

/**
 * @brief Configuration of the CAN 1 peripheral.
 * @note See section 32.9 of the STM32F405 Reference Manual for more details.
 * @note Also see: https://www.canbusdebugger.com/can-bit-timing-calculator.
 */
static const CANConfig CAN1_CONFIG =
{
	.mcr = 	CAN_MCR_ABOM |		// Automatic bus-off management.
			CAN_MCR_AWUM |		// Automatic wakeup mode.
			CAN_MCR_TXFP,		// Chronologic FIFI priority.
	.btr =	CAN_BTR_SJW (1) |	// Max 2 TQ resynchronization jump.
			CAN_BTR_TS2 (1) |	// 2 TQ for time segment 2
			CAN_BTR_TS1 (10) |	// 11 TQ for time segment 1
			CAN_BTR_BRP (2)		// Baudrate divisor of 3 (1 Mbps)
};

static const canThreadConfig_t CAN1_RX_THREAD_CONFIG =
{
	.name			= "can1_rx",
	.driver			= &CAND1,
	.period			= TIME_MS2I (10),
	.nodes			= nodes,
	.nodeCount		= NODE_COUNT,
	.rxHandler		= &receiveBmsMessage,
	.bridgeDriver	= NULL
};

static const amkInverterConfig_t AMK_CONFIGS [AMK_COUNT] =
{
	// RL
	{
		.mainDriver		= &CAND1,
		.baseId			= 0x200,
		.timeoutPeriod	= TIME_MS2I (100),
	},
	// RR
	{
		.mainDriver		= &CAND1,
		.baseId			= 0x201,
		.timeoutPeriod	= TIME_MS2I (100),
	},
	// FL
	{
		.mainDriver		= &CAND1,
		.baseId			= 0x202,
		.timeoutPeriod	= TIME_MS2I (100),
	},
	// FR
	{
		.mainDriver		= &CAND1,
		.baseId			= 0x203,
		.timeoutPeriod	= TIME_MS2I (100),
	}
};

// Functions ------------------------------------------------------------------------------------------------------------------

bool canVehicleInit (tprio_t priority)
{
	// CAN 1 driver initialization
	if (canStart (&CAND1, &CAN1_CONFIG) != MSG_OK)
		return false;

	// Leave standby mode
	palClearLine (LINE_CAN1_STBY);

	// Initialize the CAN nodes
	for (uint8_t index = 0; index < AMK_COUNT; ++index)
		amkInit (amks + index, AMK_CONFIGS + index);

	// Create the CAN RX thread
	canThreadStart (can1RxThreadWa, sizeof (can1RxThreadWa), priority, &CAN1_RX_THREAD_CONFIG);

	return true;
}