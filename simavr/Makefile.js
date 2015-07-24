SRC := sim/run_avr.c \
       sim/sim_avr.c \
       sim/sim_hex.c \
       sim/sim_core.c \
       sim/sim_cycle_timers.c \
       sim/sim_gdb.c \
       sim/sim_interrupts.c \
       sim/avr_adc.c \
       sim/avr_bitbang.c \
       sim/avr_eeprom.c \
       sim/avr_extint.c \
       sim/avr_flash.c \
       sim/avr_ioport.c \
       sim/avr_lin.c \
       sim/avr_spi.c \
       sim/avr_timer.c \
       sim/avr_twi.c \
       sim/avr_uart.c \
       sim/avr_usb.c \
       sim/avr_watchdog.c \
       cores/sim_tinyx5.c \
       cores/sim_tiny85.c \
       cores/sim_mega328.c \
       cores/sim_mega32u4.c \
       cores/sim_megax8.c \
       sim/sim_io.c \
       sim/sim_irq.c \

run_avr.js:
	emcc -O3 -s ASM_JS=1 $(SRC) -o run_avr.js -I. -Isim -s EXPORTED_FUNCTIONS="['_loadPartialProgram','_engineInit','_fetchN', '_buttonHit']"
