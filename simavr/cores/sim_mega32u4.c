/* simavr compatibility only, prevents build errors, values from 328p  */
#define SELFPRGEN 0
#define PCIE1 1
#define PCIF1 1
#define PCINT1_vect_num   4
#define PCINT1_vect       _VECTOR(4)   /* Pin Change Interrupt Request 0 */
#define PCIE2 2
#define PCIF2 2
#define PCINT2_vect_num   5
#define PCINT2_vect       _VECTOR(5)   /* Pin Change Interrupt Request 1 */
#define PRR _SFR_MEM8(0x64)
#define UDR0 _SFR_MEM8(0xCE)
#define UCSR0B _SFR_MEM8(0xC9)
#define TXEN0 3
#define RXEN0 4
#define UCSR0C _SFR_MEM8(0xCA)
#define USBS0 3
#define UCSZ00 1
#define UCSZ02 2
#define UCSR0A _SFR_MEM8(0xC8)
#define UBRR0L _SFR_MEM8(0xCC)
#define UBRR0H _SFR_MEM8(0xCD)
#define RXCIE0 7
#define RXC0 7
#define USART_RX_vect_num 18
#define USART_RX_vect     _VECTOR(18)  /* USART Rx Complete */
#define TXCIE0 6
#define TXC0 6
#define USART_TX_vect_num 20
#define USART_TX_vect     _VECTOR(20)  /* USART Tx Complete */
#define UDRIE0 5
#define UDRE0 5
#define USART_UDRE_vect_num   19
#define USART_UDRE_vect   _VECTOR(19)  /* USART, Data Register Empty */
#define ASSR _SFR_MEM8(0xB6)
#define AS2 5
#define TIMER2_OVF_vect_num   9
#define TIMER2_OVF_vect   _VECTOR(9)   /* Timer/Counter2 Overflow */
#define TIMER2_COMPA_vect_num 7
#define TIMER2_COMPA_vect _VECTOR(7)   /* Timer/Counter2 Compare Match A */
#define TIMER2_COMPB_vect_num 8
#define TIMER2_COMPB_vect _VECTOR(8)   /* Timer/Counter2 Compare Match A */

#include "sim_avr.h"

#define SIM_VECTOR_SIZE	4
#define SIM_MMCU		"atmega32u4"
#define SIM_CORENAME	mcu_mega32u4

#define _AVR_IO_H_
#define __ASSEMBLER__
#include "avr/iom32u4.h"
// instantiate the new core
#include "sim_megax8.h"

static avr_t * make()
{
	return avr_core_allocate(&SIM_CORENAME.core, sizeof(struct mcu_t));
}

avr_kind_t mega32u4 = {
	.names = { "atmega32u4" },
	.make = make
};

