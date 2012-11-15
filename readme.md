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