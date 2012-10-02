//!
//! @file 		UartComms.c
//! @author 	Geoffrey Hunter (gbmhunter@gmail.com)
//! @date 		12/09/2012
//! @brief 		Used for receiving/sending comms messages across the dedicated UART
//! @details
//!		<b>Last Modified:			</b> 27/09/2012					\n
//!		<b>Version:					</b> v1.0.0						\n
//!		<b>Company:					</b> CladLabs					\n
//!		<b>Project:					</b> Free Code Modules			\n
//!		<b>Language:				</b> C							\n
//!		<b>Compiler:				</b> GCC						\n
//! 	<b>uC Model:				</b> PSoC5						\n
//!		<b>Computer Architecture:	</b> ARM						\n
//! 	<b>Operating System:		</b> FreeRTOS v7.2.0			\n
//!		<b>Documentation Format:	</b> Doxygen					\n
//!		<b>License:					</b> GPLv3						\n
//!	
//!		Used for comms uart communication in an RTOS
//!		environment. Uses queues to allow multiple
//!		calls to the uart at once.
//!

//===============================================================================================//
//========================================= INCLUDES ============================================//
//===============================================================================================//

// Standard PSoC includes
#include <device.h>

// FreeRTOS includes
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// User includes
#include "PublicDefinesAndTypeDefs.h"
#include "Config.h"
#include "UartComms.h"
#include "UartDebug.h"

//===============================================================================================//
//================================== PRECOMPILER CHECKS =========================================//
//===============================================================================================//

#ifndef configENABLE_TASK_UART_COMMS
	#error Please define the switch configENABLE_TASK_UART_COMMS
#endif

#ifndef configPRINT_DEBUG_UART_COMMS
	#error Please define the switch configPRINT_DEBUG_UART_COMMS
#endif

#ifndef configALLOW_SLEEP_UART_COMMS
	#error Please define the switch configALLOW_SLEEP_UART_COMMS
#endif

//===============================================================================================//
//==================================== PRIVATE DEFINES ==========================================//
//===============================================================================================//


#define TX_QUEUE_SIZE 							(1)		//!< Queue size is 1, messages are sent byte by byte across the queue.
#define RX_QUEUE_SIZE 							(1)		//!< Queue size is 1, messages are sent byte by byte across the queue.
#define TX_QUEUE_MAX_WAIT_TIME_MS 				(1000)	//!< Max time (in ms) to wait for putting character onto tx queue before error occurs
//! Max time (in ms) to wait for another task to finish putting a string onto the tx queue
//! Since only the DEBUG task is using this UART, the semaphore should never have to be
//! waited on.
#define	TX_SEMAPHORE_MAX_WAIT_TIME_MS 			(1000)
//! Time to wait for another char to arrive on tx queue before the UART module is slept.
#define TIME_TO_WAIT_FOR_ANOTHER_CHAR_BEFORE_SLEEPING_MS (5)

//===============================================================================================//
//=================================== PRIVATE TYPEDEF's =========================================//
//===============================================================================================//

// none

//===============================================================================================//
//============================= PRIVATE VARIABLES/STRUCTURES ====================================//
//===============================================================================================//

static xTaskHandle _txTaskHandle = 0;			//!< Handle for the TX task
static xSemaphoreHandle _xTxMutexSemaphore = 0;	//!< Mutex semaphore for allowing only one task to write to the tx buffer at once
static uint8 _uartDebugSleepLockCount = 0;				//! Used by the functions UartComms_SleepLock() and UartComms_SleepUnlock() to keep track of how many time the uart has been locked from sleeping.

//! RX queue. Uart interrupt places characters on this queue as soon as they are received.
static xQueueHandle _xRxQueue;

//! Tx queue. Place characters on here to send them to the DEBUG module.
static xQueueHandle _xTxQueue;

//! Variable is TRUE if uart is asleep, otherwise FALSE.
static bool_t _isAsleep = FALSE;

//! Holds configurable UART parameters
struct{
	bool_t allowUartSleep;
} _uartCommsParameters =
{
	.allowUartSleep = configALLOW_SLEEP_UART_COMMS
};

//===============================================================================================//
//================================== PRIVATE FUNCTION PROTOTYPES ================================//
//===============================================================================================//

// General functions
void vUartComms_TxTask(void *pvParameters);

// ISR's
CY_ISR_PROTO(UartComms_UartRxIsr);

//===============================================================================================//
//===================================== PUBLIC FUNCTIONS ========================================//
//===============================================================================================//

//! @brief		Start-up function. Call from main() before starting scheduler
//! @note		Not thread-safe. Do not call from any task!
//! @sa			main()
//! @public
void UartComms_Start(uint32 txTaskStackSize, uint8 txTaskPriority)
{
	#if(configENABLE_TASK_UART_COMMS == 1)
		// Create the tx task
		xTaskCreate(	&vUartComms_TxTask,
						(signed portCHAR *) "Comms Uart TX Task",
						txTaskStackSize,
						NULL,
						txTaskPriority,
						&_txTaskHandle);
	#endif
					
	// Create TX Queue
	_xTxQueue = xQueueCreate(configUART_COMMS_TX_QUEUE_LENGTH, TX_QUEUE_SIZE);
	
	// Create RX Queue
	_xRxQueue = xQueueCreate(configUART_COMMS_RX_QUEUE_LENGTH, RX_QUEUE_SIZE);
	
	// Create TX mutex semaphore
	_xTxMutexSemaphore = xSemaphoreCreateMutex();
	
	// Start the debug UART. This is a Cpyress API call.
	UartCpComms_Start();
	
}

//! @brief		Returns the handle for the TX task
//! @returns	Handle of the TX task
//! @note		Thread-safe
//! @public
xTaskHandle UartComms_ReturnTxTaskHandle(void)
{
	return _txTaskHandle;
}

//! @brief		Puts null-terminated string onto tx queue
//! @details	This is a blocking function which will not return until the entire string has been
//!				put onto the queue. It will block if another task is currently putting stuff on the
//!				queue (and hence the semaphore taken), or the tx queue is full (hence the UART is busy).
//! @note		Thread-safe
//! @public
bool_t UartComms_PutString(char* string)
{
	// Take semaphore to allow placing things on queue
	if(xSemaphoreTake(_xTxMutexSemaphore, TX_SEMAPHORE_MAX_WAIT_TIME_MS/portTICK_RATE_MS) == pdFAIL)
	{
		#if(configPRINT_DEBUG_UART_COMMS == 1)
			static char *msgTimeoutWaitingForTxQueueSemaphore = "UART_COMMS: Timeout waiting for tx queue semaphore.\r\n";
			UartComms_PutString(msgTimeoutWaitingForTxQueueSemaphore);	
		#endif
		return FALSE;
	}
	
	//! @todo Add error handling
	
	// Put characters onto tx queue one-by-one
	while(*string != '\0')
	{
		// Put char on back of queue one-by-one
		xQueueSendToBack(_xTxQueue, string, TX_QUEUE_MAX_WAIT_TIME_MS/portTICK_RATE_MS);
		// Increment string
		string++;
		//! @todo Add error handling if queue fail
	}
	
	// Return semaphore
	xSemaphoreGive(_xTxMutexSemaphore);
	
	return TRUE;
}

//! @brief		
//! @details	Blocks until character is received
//! @note		Not-thread safe.
//! @public
void UartComms_GetChar(char* singleChar)
{
	xQueueReceive(_xRxQueue, singleChar, portMAX_DELAY);
}

//! @brief 		Returns sleep state of the UART
//! @public
bool_t UartComms_IsAsleep(void)
{
	if(_isAsleep == TRUE)
		return TRUE;
	else
		return FALSE;
}

//! @brief		Used to prevent the DEBUG UART from sleeping
//! @details	Can be called from any task, up to 255 times. The UART will be prevented from sleeping
//! 			until a similar number of UartComms_SleepUnlock() calls have been made.
//! @note		Thread-safe
//! @sa			UartComms_SleepUnlock()
//! @public
void UartComms_SleepLock(void)
{
	// Stop context switch since UART_COMMS_Wakeup() is not thread-safe
	taskENTER_CRITICAL();
	//vTaskSuspendAll();
	// Wakeup UART if sleep lock was on 0 (a hence sleeping) as long as sleep is allowed
	if((_isAsleep == TRUE) && (_uartCommsParameters.allowUartSleep == TRUE))
	{
		// Set flag to false to prevent multiple wake-ups
		_isAsleep = FALSE;
		// Cypress APU call
		UartCpComms_Wakeup();
		#if(configPRINT_DEBUG_UART_COMMS == 1)
			static char *msgWakingUartComms = "UART_COMMS: Woke up comms UART.\r\n";
			UartComms_PutString(msgWakingUartComms);	
		#endif
		
	}
		
	// Increment sleep lock count. If statement should never be false, but added just as a
	// precaution.
	if(_uartDebugSleepLockCount != 255)
		_uartDebugSleepLockCount++;
	
	// Allow context switch again
	//xTaskResumeAll(); 
	taskEXIT_CRITICAL();
}

//! @brief		Used to allow the DEBUG UART to sleep
//! @details	Can be called from any task, up to 255 times. This function needs to be called
//!				as many times as UartComms_SleepLock was called before the UART will be allowed
//!				to sleep.
//! @note		Thread-safe
//! @sa			UartComms_SleepLock()
//! @public
void UartComms_SleepUnlock(void)
{
	// Prevent contect switch since UART_COMMS_Sleep() is not thread-safe
	taskENTER_CRITICAL();
	//vTaskSuspendAll();
	// Decrement sleep lock count. If statement should never be false, but added just as a
	// precaution
	if(_uartDebugSleepLockCount != 0)
		_uartDebugSleepLockCount--;
		
	// Sleep UART if sleepLockCount has reached 0
	if((_uartDebugSleepLockCount == 0) && (_isAsleep == FALSE))
	{
		if(_uartCommsParameters.allowUartSleep == TRUE)
		{
			/*
			#if(configPRINT_DEBUG_UART_COMMS == 1)
				static char *msgSleepingUartComms = "UART_COMMS: Sleeping UART DEBUG.\r\n";
				UartComms_PutString(msgSleepingUartComms);	
				// Wait for message to complete since sleeping itself
				while(!(UART_COMMS_ReadTxStatus() & UART_COMMS_TX_STS_COMPLETE));
			#endif
			*/
			
			// Sleep UART. Cypress API call.
			UartCpComms_Sleep();
			// Set flag to true so UartComms_SleepLock() knows to wake up device
			_isAsleep = TRUE;
		}
		else
		{
			/* @debug Repeatedly prints itself
			#if(configPRINT_DEBUG_UART_COMMS == 1)
				static char *msgUartCommsSleepDisabled = "UART_COMMS: UART DEBUG sleep disabled. Keeping awake.\r\n";
				UartComms_PutString(msgUartCommsSleepDisabled);	
			#endif
			*/
		}
	}
	
	// Allow context switch again
	//xTaskResumeAll(); 
	taskEXIT_CRITICAL();
}

//===============================================================================================//
//==================================== PRIVATE FUNCTIONS ========================================//
//===============================================================================================//

//======================================== TASK FUNCTIONS =======================================//

//! @brief 		DEBUG UART TX task
//! @param		*pvParameters Void pointer (not used)
//! @note		Not thread-safe. Do not call from any task, this function is a task that
//!				is called by the FreeRTOS kernel
//! @private
void vUartComms_TxTask(void *pvParameters)
{
	#if(configPRINT_DEBUG_UART_COMMS == 1)
		static char* msgUartCommsTxTaskStarted = "UART_COMMS: Comms Uart TX task started.\r\n";
		UartDebug_PutString(msgUartCommsTxTaskStarted);	
	#endif

	// Start USART RX interrupt, store in vector table the function address of UartComms_UartRxISR()
	// This must be done in the task, since the interrupt calls xQueueSendToBackFromISR() which 
	// must not be called before the scheduler starts (freeRTOS restriction).
	IsrCpUartCommsRx_StartEx(UartComms_UartRxIsr);	
	
	typedef enum
	{
		ST_INIT,	//!< Initial state
		ST_IDLE,	//!< Idle state. UART could be asleep in this state
		ST_SENDING	//!< Sending state. UART is prevented from sleeping in this state until timeout
	}txTaskState_t;
	
	txTaskState_t txTaskState = ST_INIT;

	// Infinite task loop
	for(;;)
	{
		//! Holds received character from the #xUartCommsTxQueue
		char singleChar;
		
		// State machine
		switch(txTaskState)
		{
			case ST_INIT:
			{
				// Allow UART to initially sleep if allowed to
				//UartComms_SleepLock();
				UartComms_SleepUnlock();
				// Go to idle state
				txTaskState = ST_IDLE;
				break;
			}
			case ST_IDLE:
			{
				// Now UART is asleep, wait indefinetly for next char
				xQueueReceive(_xTxQueue, &singleChar, portMAX_DELAY);

				// Prevent UART from sleeping and wake-up if neccessary
				UartComms_SleepLock();
				// Goto sending state
				txTaskState = ST_SENDING;
				break;
			}
			case ST_SENDING:
			{
				// Send char using Cypress API.
				// This function will not return untill there is room to put character on buffer
				//! @todo Implement this in a blocking fashion?
				UartCpComms_PutChar(singleChar);
				
				if(xQueueReceive(_xTxQueue, &singleChar, TIME_TO_WAIT_FOR_ANOTHER_CHAR_BEFORE_SLEEPING_MS/portTICK_RATE_MS) == pdFAIL)
				{
					// Wait until UART has completely finished sending the message
					// (both the hardware buffer and the byte sent flag are set)
					//while(!(UART_COMMS_ReadTxStatus() & (UART_COMMS_TX_STS_FIFO_EMPTY | UART_COMMS_TX_STS_COMPLETE)));	//(software wait)
					while(!(UartCpComms_ReadTxStatus() & UartCpComms_TX_STS_COMPLETE));
					//CyDelay(95);
					// Now it is safe to unlock the UART to allow for sleeping
					UartComms_SleepUnlock();
					// Go back to idle state
					txTaskState = ST_IDLE;
				}
				break;
			}
		}
		// Finished, now loop for next message
	}
}

//===============================================================================================//
//============================================ ISR's ============================================//
//===============================================================================================//

//! @brief 		ISR called when UART rx buffer has new character
//! @public
CY_ISR(UartComms_UartRxIsr)
{
	// Check to see if we just slept, if so, wake up peripherals
	//! @todo Get rid of this
	//if(PowerMgmt_AreWeSleeping() == TRUE)
	//	PowerMgmt_WakeUp();
	
	static portBASE_TYPE xHigherPriorityTaskWoken;
	// Set to false on interrupt entry
	xHigherPriorityTaskWoken = FALSE;

	// Get received byte (lower 8-bits) and error info from UART (higher 8-bits) (total 16-bits)
	do
	{
		uint16_t byte = UartCpComms_GetByte();
		
		// Mask error info
		uint8_t status = (byte >> 8);
		
		// Check for error
		if(status == (UartCpComms_RX_STS_BREAK | UartCpComms_RX_STS_PAR_ERROR | UartCpComms_RX_STS_STOP_ERROR 
			| UartCpComms_RX_STS_OVERRUN | UartCpComms_RX_STS_SOFT_BUFF_OVER))
		{
			// UART error has occured
			//Main_SetErrorLed();
			#if(configPRINT_DEBUG_UartCpComms == 1)
				if(status == UartCpComms_RX_STS_MRKSPC)
				{
					static char* msgErrorMarkOrSpaceWasReceivedInParityBit = "DEBUG_RX_INT: Error: Mark or space was received in parity bit.\r\n";
					UartDebug_PutString(msgErrorMarkOrSpaceWasReceivedInParityBit);	
				}
				else if(status == UartCpComms_RX_STS_BREAK)
				{
					static char* msgBreakWasDetected = "DEBUG_RX_INT: Error: Break was detected.\r\n";
					UartDebug_PutString(msgBreakWasDetected);	
				}
				else if(status == UartCpComms_RX_STS_PAR_ERROR)
				{
					static char* msgErorrParity = "DEBUG_RX_INT: Error: Parity error was detected.\r\n";
					UartDebug_PutString(msgErorrParity);	
				}
				else if(status == UartCpComms_RX_STS_STOP_ERROR)
				{
					static char* msgErorrStop = "DEBUG_RX_INT: Error: Stop error was detected.\r\n";
					UartDebug_PutString(msgErorrStop);	
				}
				else if(status == UartCpComms_RX_STS_OVERRUN)
				{
					static char* msgErrorFifoRxBufferOverrun = "DEBUG_RX_INT: Error: FIFO RX buffer was overrun.\r\n";
					UartDebug_PutString(msgErrorFifoRxBufferOverrun);	
				}
				else if(status == UartCpComms_RX_STS_FIFO_NOTEMPTY)
				{
					static char* msgErrorRxBufferNotEmpty = "DEBUG_RX_INT: Error: RX buffer not empty.\r\n";
					UartDebug_PutString(msgErrorRxBufferNotEmpty);	
				}
				else if(status == UartCpComms_RX_STS_ADDR_MATCH)
				{
					static char* msgErrorAddressMatch = "DEBUG_RX_INT: Error: Address match.\r\n";
					UartDebug_PutString(msgErrorAddressMatch);	
				}
				else if(status == UartCpComms_RX_STS_SOFT_BUFF_OVER)
				{
					static char* msgErrorSoftwareBufferOverflowed = "DEBUG_RX_INT: Error: RX software buffer ovverflowed.\r\n";
					UartDebug_PutString(msgErrorSoftwareBufferOverflowed);	
				}
			#endif
		}
		else
		{
			// Put byte in queue (ISR safe function)
			xQueueSendToBackFromISR(_xRxQueue, &byte, &xHigherPriorityTaskWoken);
		}
	}
	while((UartCpComms_ReadRxStatus() & UartCpComms_RX_STS_FIFO_NOTEMPTY) != 0x00);
	
	// Force a context swicth if interrupt unblocked a task with a higher or equal priority
	// to the currently running task
	portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}

//===============================================================================================//
//========================================= GRAVEYARD ===========================================//
//===============================================================================================//


// EOF