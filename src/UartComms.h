//!
//! @file 		UartComms.h
//! @author 	Geoffrey Hunter (gbmhunter@gmail.com)
//! @date 		12/09/2012
//! @brief 		Header file for UartComms.c
//! @details
//!		<b>Last Modified:			</b> 27/09/2012					\n
//!		<b>Version:					</b> v1.0.0						\n
//!		<b>Company:					</b> CladLabs					\n
//!		<b>Project:					</b> Template Code				\n
//!		<b>Computer Architecture:	</b> ARM						\n
//!		<b>Compiler:				</b> GCC						\n
//! 	<b>uC Model:				</b> PSoC5						\n
//! 	<b>Operating System:		</b> FreeRTOS v7.2.0			\n
//!		<b>Documentation Format:	</b> Doxygen					\n
//!

//===============================================================================================//
//======================================== HEADER GUARD =========================================//
//===============================================================================================//

#ifndef _UART_COMMS_H
#define _UART_COMMS_H

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

void 		UartComms_Start(uint32 txTaskStackSize, uint8 txTaskPriority);
bool_t 		UartComms_PutString(char* string);
void 		UartComms_GetChar(char* singleChar);
void 		UartComms_SleepLock(void);
void 		UartComms_SleepUnlock(void);
bool_t 		UartComms_IsAsleep(void);
xTaskHandle UartComms_ReturnTxTaskHandle(void);




//===============================================================================================//
//=================================== PUBLIC VARIABLES/STRUCTURES ===============================//
//===============================================================================================//

// none

// End of header guard
#endif

// EOF
