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

#include "sim_board_micro.h"

#ifdef EMSCRIPTEN
#include "emscripten.h"
#else
uint8_t bState = 0x0;
uint8_t cState = 0x0;
uint8_t dState = 0x0;
uint8_t eState = 0x0;
uint8_t fState = 0x0;
#endif

#include "sim_core_decl.h"

char* loaded_chunk[4096];
int number_of_chunks = 0;

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

#if defined(EMSCRIPTEN) || defined(ANDROID)
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
#else

void (*writeSPI)(struct spiWrite call) = NULL;

void setSPICallback(void (*callback)(struct spiWrite call))
{
	writeSPI = callback;
}

void (*SharpSPI)(uint8_t, uint8_t) = NULL;

static void flatten(struct spiWrite call)
{
	SharpSPI(call.ports[2], call.spi);
}

void SharpCallback(void (*callback)(uint8_t, uint8_t))
{
	writeSPI = flatten;
	SharpSPI = callback;
}

void nativeWriteSPI(uint8_t value)
{
	struct spiWrite call;
	call.ports[0] = bState;
	call.ports[1] = cState;
	call.ports[2] = dState;
	call.ports[3] = eState;
	call.ports[4] = fState;
	call.spi = value;
	if(writeSPI != NULL)
	{
		writeSPI(call);
	}
}

#endif
