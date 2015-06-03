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

avr_kind_t mega328 = {
	.names = { "atmega32u4" },
	.make = make
};

