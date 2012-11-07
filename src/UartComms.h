//!
//! @file 		UartComms.h
//! @author 	Geoffrey Hunter <gbmhunter@gmail.com> (www.cladlab.com)
//! @date 		12/09/2012
//! @brief 		Used for receiving/sending comms messages across the dedicated UART
//! @details
//!		<b>Last Modified:			</b> 07/11/2012					\n
//!		<b>Version:					</b> v1.0.2						\n
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
//!		calls to the UART at once. Doesn't support calls from
//!		an ISR. Designed for the PSoC architecture.
//!
//! 	CHANGELOG:
//!			v1.0.1 -> Removed '_' prefix from header guard constant.
//!				Added C++ header guard. Added warning not to call
//!				Uart_PutString() from an ISR. Moved documentation into
//!				.h file.
//!			v1.0.2 -> Fixed comments (see .c to .h)
//!		

//===============================================================================================//
//============================================ GUARDS ===========================================//
//===============================================================================================//

#ifndef UART_COMMS_H
#define UART_COMMS_H

#ifdef __cplusplus
	extern "C" {
#endif

//===============================================================================================//
//==================================== PUBLIC DEFINES ===========================================//
//===============================================================================================//

// none

//===============================================================================================//
//====================================== PUBLIC TYPEDEFS ========================================//
//===============================================================================================//

// none

//===============================================================================================//
//=================================== PUBLIC FUNCTION PROTOTYPES ================================//
//===============================================================================================//

// See the function definitions in UartComms.c for more information

//! @brief		Start-up function. Call from main() before starting scheduler
//! @note		Not thread-safe. Do not call from any task!
//! @sa			main()
//! @public
void 		UartComms_Start(uint32 txTaskStackSize, uint8 txTaskPriority);

//! @brief		Puts null-terminated string onto tx queue
//! @details	This is a blocking function which will not return until the entire string has been
//!				put onto the queue. It will block if another task is currently putting stuff on the
//!				queue (and hence the semaphore taken), or the tx queue is full (hence the UART is busy).
//! @warning	Do not call from an ISR!
//! @note		Thread-safe
//! @public
bool_t 		UartComms_PutString(const char* string);

//! @brief		
//! @details	Blocks until character is received
//! @note		Not-thread safe.
//! @public
void 		UartComms_GetChar(char* singleChar);

//! @brief		Used to prevent the DEBUG UART from sleeping
//! @details	Can be called from any task, up to 255 times. The UART will be prevented from sleeping
//! 			until a similar number of UartComms_SleepUnlock() calls have been made.
//! @note		Thread-safe
//! @sa			UartComms_SleepUnlock()
//! @public
void 		UartComms_SleepLock(void);

//! @brief		Used to allow the DEBUG UART to sleep
//! @details	Can be called from any task, up to 255 times. This function needs to be called
//!				as many times as UartComms_SleepLock was called before the UART will be allowed
//!				to sleep.
//! @note		Thread-safe
//! @sa			UartComms_SleepLock()
//! @public
void 		UartComms_SleepUnlock(void);

//! @brief 		Returns sleep state of the UART
//! @public
bool_t 		UartComms_IsAsleep(void);

//! @brief		Returns the handle for the TX task
//! @returns	Handle of the TX task
//! @note		Thread-safe
//! @public
xTaskHandle UartComms_ReturnTxTaskHandle(void);

//===============================================================================================//
//=================================== PUBLIC VARIABLES/STRUCTURES ===============================//
//===============================================================================================//

// none

#ifdef __cplusplus
	} // extern "C" {
#endif

#endif // #ifndef UART_COMMS_H

// EOF
