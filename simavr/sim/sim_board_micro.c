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

#ifdef EMSCRIPTEN
#include "emscripten.h"
#endif

#include "sim_core_decl.h"

char* loaded_chunk[4096];
int number_of_chunks = 0;

extern avr_t * avr;
void sig_int(int sign);

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

#ifdef __cplusplus
extern "C" {
#endif

void engineInit(const char* m)
{
	elf_firmware_t f = {{0}};
	long f_cpu = 5000000;
	int log = 1;
	char name[16];
	strcpy(name, m);
	uint32_t loadBase = AVR_SEGMENT_OFFSET_FLASH;

	ihex_chunk_p chunk = NULL;
	int cnt = read_ihex_chunks(&chunk);
	if (cnt <= 0) {
		fprintf(stderr, "Unable to load IHEX file\n");
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

	if (strlen(name))
		strcpy(f.mmcu, name);
	if (f_cpu)
		f.frequency = f_cpu;

	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "AVR '%s' not known\n", f.mmcu);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &f);
	if (f.flashbase) {
		printf("Attempted to load a bootloader at %04x\n", f.flashbase);
		avr->pc = f.flashbase;
	}
	avr->log = (log > LOG_TRACE ? LOG_TRACE : log);

	signal(SIGINT, sig_int);
	signal(SIGTERM, sig_int);
}
#ifdef __cplusplus
}
#endif

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

	//PLL CSR initialization
	uint8_t v = avr_core_watch_read(avr, 0x49);
	v = 0 < (v & 0x2) ? v | 0x1 : v & 0xFE;
	avr_core_watch_write(avr, 0x49, v);
#ifdef EMSCRIPTEN
	EM_ASM("refreshUI()");
#endif
	avr_core_watch_write(avr, 0x23, 0xFF);
	avr_core_watch_write(avr, 0x26, 0xFF);
	avr_core_watch_write(avr, 0x29, 0xFF);
	avr_core_watch_write(avr, 0x2F, 0xFF);

	return state;
}

void buttonHit(int r, int v)
{
	avr_core_watch_write(avr, r, v);
}
