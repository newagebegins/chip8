#pragma once

#include <stdint.h>

#define CHIP8_TIMER_HZ 60
#define CHIP8_CYCLES_PER_TIMER 9
#define CHIP8_CYCLE_HZ (CHIP8_TIMER_HZ * CHIP8_CYCLES_PER_TIMER)
#define CHIP8_CYCLE_INTERVAL (1.0f / CHIP8_CYCLE_HZ)

#define CHIP8_SCR_W  64
#define CHIP8_SCR_H 32
#define CHIP8_ASPECT ((float)CHIP8_SCR_W / CHIP8_SCR_H)
#define CHIP8_NUM_KEYS 16

void chip8_init(const uint8_t *program, uint32_t program_size);
void chip8_do_cycle(uint8_t screen[CHIP8_SCR_H][CHIP8_SCR_W], const bool keys[CHIP8_NUM_KEYS]);
uint8_t chip8_get_sound_timer();
