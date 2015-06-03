/*
	run_avr.c

	Copyright 2008, 2010 Michel Pollet <buserror@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <signal.h>
#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "sim_hex.h"
#include "avr_eeprom.h"
#include "avr_ioport.h"
#include "sim_vcd_file.h"
#include "emscripten.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#include "sim_core_decl.h"

char* loaded_chunk[4096];
int number_of_chunks = 0;

void display_usage(char * app)
{
	printf("Usage: %s [-t] [-g] [-v] [-m <device>] [-f <frequency>] firmware\n", app);
	printf("       -t: Run full scale decoder trace\n"
		   "       -g: Listen for gdb connection on port 1234\n"
		   "       -ff: Load next .hex file as flash\n"
		   "       -ee: Load next .hex file as eeprom\n"
		   "       -v: Raise verbosity level (can be passed more than once)\n"
		   "   Supported AVR cores:\n");
	for (int i = 0; avr_kind[i]; i++) {
		printf("       ");
		for (int ti = 0; ti < 4 && avr_kind[i]->names[ti]; ti++)
			printf("%s ", avr_kind[i]->names[ti]);
		printf("\n");
	}
	exit(1);
}

avr_t * avr = NULL;

void
sig_int(
		int sign)
{
	printf("signal caught, simavr terminating\n");
	if (avr)
		avr_terminate(avr);
	exit(0);
}

int main(int argc, char *argv[])
{
	elf_firmware_t f = {{0}};
	long f_cpu = 0;
	int trace = 0;
	int gdb = 0;
	int log = 1;
	char name[16] = "";
	uint32_t loadBase = AVR_SEGMENT_OFFSET_FLASH;
	int trace_vectors[8] = {0};
	int trace_vectors_count = 0;

	if (argc == 1)
		display_usage(basename(argv[0]));

	for (int pi = 1; pi < argc; pi++) {
		if (!strcmp(argv[pi], "-h") || !strcmp(argv[pi], "-help")) {
			display_usage(basename(argv[0]));
		} else if (!strcmp(argv[pi], "-m") || !strcmp(argv[pi], "-mcu")) {
			if (pi < argc-1)
				strcpy(name, argv[++pi]);
			else
				display_usage(basename(argv[0]));
		} else if (!strcmp(argv[pi], "-f") || !strcmp(argv[pi], "-freq")) {
			if (pi < argc-1)
				f_cpu = atoi(argv[++pi]);
			else
				display_usage(basename(argv[0]));
		} else if (!strcmp(argv[pi], "-t") || !strcmp(argv[pi], "-trace")) {
			trace++;
		} else if (!strcmp(argv[pi], "-ti")) {
			if (pi < argc-1)
				trace_vectors[trace_vectors_count++] = atoi(argv[++pi]);
		} else if (!strcmp(argv[pi], "-g") || !strcmp(argv[pi], "-gdb")) {
			gdb++;
		} else if (!strcmp(argv[pi], "-v")) {
			log++;
		} else if (!strcmp(argv[pi], "-ee")) {
			loadBase = AVR_SEGMENT_OFFSET_EEPROM;
		} else if (!strcmp(argv[pi], "-ff")) {
			loadBase = AVR_SEGMENT_OFFSET_FLASH;
		} else if (argv[pi][0] != '-') {
			char * filename = argv[pi];
			char * suffix = strrchr(filename, '.');
			if (suffix && !strcasecmp(suffix, ".hex")) {
				if (!name[0] || !f_cpu) {
					fprintf(stderr, "%s: -mcu and -freq are mandatory to load .hex files\n", argv[0]);
					exit(1);
				}
				ihex_chunk_p chunk = NULL;
				int cnt = read_ihex_chunks(&chunk);
				if (cnt <= 0) {
					fprintf(stderr, "%s: Unable to load IHEX file %s\n", argv[0], argv[pi]);
					exit(1);
				}
				printf("Loaded %d section of ihex\n", cnt);
				for (int ci = 0; ci < cnt; ci++) {
					if (chunk[ci].baseaddr < (1*1024*1024)) {
						f.flash = chunk[ci].data;
						f.flashsize = chunk[ci].size;
						f.flashbase = chunk[ci].baseaddr;
						printf("Load HEX flash %08x, %d\n", f.flashbase, f.flashsize);
					} else if (chunk[ci].baseaddr >= AVR_SEGMENT_OFFSET_EEPROM ||
							chunk[ci].baseaddr + loadBase >= AVR_SEGMENT_OFFSET_EEPROM) {
						// eeprom!
						f.eeprom = chunk[ci].data;
						f.eesize = chunk[ci].size;
						printf("Load HEX eeprom %08x, %d\n", chunk[ci].baseaddr, f.eesize);
					}
				}
			}
		}
	}

	if (strlen(name))
		strcpy(f.mmcu, name);
	if (f_cpu)
		f.frequency = f_cpu;

	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &f);
	if (f.flashbase) {
		printf("Attempted to load a bootloader at %04x\n", f.flashbase);
		avr->pc = f.flashbase;
	}
	avr->log = (log > LOG_TRACE ? LOG_TRACE : log);
	avr->trace = trace;
	for (int ti = 0; ti < trace_vectors_count; ti++) {
		for (int vi = 0; vi < avr->interrupts.vector_count; vi++)
			if (avr->interrupts.vector[vi]->vector == trace_vectors[ti])
				avr->interrupts.vector[vi]->trace = 1;
	}

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (gdb) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	signal(SIGINT, sig_int);
	signal(SIGTERM, sig_int);
}

int32_t getValueFromHex(uint8_t* buffer, int32_t size)
{
	int32_t value = 0;
	int32_t cursor = 0;
	while(size--)
	{
		int32_t shift = (1 << size*4);
		if(buffer[cursor] < ':')
		{
			value += (buffer[cursor++] - '0')*shift;
		}
		else
		{
			value += (buffer[cursor++] - 0x37)*shift;
		}
	}

	return value;
}

void loadPartialProgram(uint8_t* binary)
{
	int32_t lineCursor = 0;
	assert(binary[lineCursor++] == ':');
	int32_t byteCount = getValueFromHex(&binary[lineCursor], 2)*2;
	loaded_chunk[number_of_chunks] = (char*)malloc(byteCount+12);
	memset(loaded_chunk[number_of_chunks], '\0', byteCount+12);
	memcpy(loaded_chunk[number_of_chunks++], binary, byteCount+11);
}

void engineInit()
{
	char* args[] = {"node", "run_avr.js", "-f", "16000000", "-m", "atmega328", "a_Hello.cpp.hex"};
	main(7, args);
}

int32_t fetchN(int32_t n)
{
	int state = cpu_Limbo;
	for (int i = 0; i < n; i++) {
		state = avr_run(avr);
		if ( state == cpu_Done || state == cpu_Crashed)
                {
			avr_terminate(avr);
			break;
                }
	}
	EM_ASM("refreshUI()");
	return state;
}

void avr_load_firmware(avr_t * avr, elf_firmware_t * firmware)
{
	if (firmware->frequency)
		avr->frequency = firmware->frequency;
	if (firmware->vcc)
		avr->vcc = firmware->vcc;
	if (firmware->avcc)
		avr->avcc = firmware->avcc;
	if (firmware->aref)
		avr->aref = firmware->aref;
#if CONFIG_SIMAVR_TRACE && ELF_SYMBOLS
	int scount = firmware->flashsize >> 1;
	avr->trace_data->codeline = malloc(scount * sizeof(avr_symbol_t*));
	memset(avr->trace_data->codeline, 0, scount * sizeof(avr_symbol_t*));

	for (int i = 0; i < firmware->symbolcount; i++)
		if (firmware->symbol[i]->addr < firmware->flashsize)	// code address
			avr->trace_data->codeline[firmware->symbol[i]->addr >> 1] =
				firmware->symbol[i];
	// "spread" the pointers for known symbols forward
	avr_symbol_t * last = NULL;
	for (int i = 0; i < scount; i++) {
		if (!avr->trace_data->codeline[i])
			avr->trace_data->codeline[i] = last;
		else
			last = avr->trace_data->codeline[i];
	}
#endif

	avr_loadcode(avr, firmware->flash, firmware->flashsize, firmware->flashbase);
	avr->codeend = firmware->flashsize + firmware->flashbase - firmware->datasize;
	if (firmware->eeprom && firmware->eesize) {
		avr_eeprom_desc_t d = { .ee = firmware->eeprom, .offset = 0, .size = firmware->eesize };
		avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &d);
	}
	// load the default pull up/down values for ports
	for (int i = 0; i < 8; i++)
		if (firmware->external_state[i].port == 0)
			break;
		else {
			avr_ioport_external_t e = {
				.name = firmware->external_state[i].port,
				.mask = firmware->external_state[i].mask,
				.value = firmware->external_state[i].value,
			};
			avr_ioctl(avr, AVR_IOCTL_IOPORT_SET_EXTERNAL(e.name), &e);
		}
	avr_set_command_register(avr, firmware->command_register_addr);
	avr_set_console_register(avr, firmware->console_register_addr);

	// rest is initialization of the VCD file

	if (firmware->tracecount == 0)
		return;
	avr->vcd = malloc(sizeof(*avr->vcd));
	memset(avr->vcd, 0, sizeof(*avr->vcd));

	AVR_LOG(avr, LOG_TRACE, "Creating VCD trace file '%s'\n", avr->vcd->filename);
	for (int ti = 0; ti < firmware->tracecount; ti++) {
		if (firmware->trace[ti].mask == 0xff || firmware->trace[ti].mask == 0) {
			// easy one
			avr_irq_t * all = avr_iomem_getirq(avr,
					firmware->trace[ti].addr,
					firmware->trace[ti].name,
					AVR_IOMEM_IRQ_ALL);
			if (!all) {
				AVR_LOG(avr, LOG_ERROR, "ELF: %s: unable to attach trace to address %04x\n",
					__FUNCTION__, firmware->trace[ti].addr);
			}
		} else {
			int count = 0;
			for (int bi = 0; bi < 8; bi++)
				if (firmware->trace[ti].mask & (1 << bi))
					count++;
			for (int bi = 0; bi < 8; bi++)
				if (firmware->trace[ti].mask & (1 << bi)) {
					avr_irq_t * bit = avr_iomem_getirq(avr,
							firmware->trace[ti].addr,
							firmware->trace[ti].name,
							bi);
					if (!bit) {
						AVR_LOG(avr, LOG_ERROR, "ELF: %s: unable to attach trace to address %04x\n",
							__FUNCTION__, firmware->trace[ti].addr);
						break;
					}

					if (count == 1) {
						break;
					}
					char comp[128];
					sprintf(comp, "%s.%d", firmware->trace[ti].name, bi);
				}
		}
	}
}
