#include "chip8.h"

#include <memory.h>
#include <assert.h>
#include <stdlib.h>

#define MEMORY_SIZE 4096
#define PROGRAM_OFFSET 512
#define NUM_REGISTERS 16
#define STACK_SIZE 16
#define SCREEN_BYTES (CHIP8_SCREEN_HEIGHT * CHIP8_SCREEN_WIDTH)

static uint8_t mem[MEMORY_SIZE];
static uint16_t PC; // program counter
static uint8_t V[NUM_REGISTERS]; // registers
static uint16_t I;
static uint8_t delay_timer;
static uint8_t sound_timer;
static uint16_t stack[STACK_SIZE];
static uint8_t SP; // stack pointer
static uint32_t cycle_counter;

void chip8_init(const uint8_t *program, uint32_t program_size) {
    // load font data into memory
    uint8_t *p = mem;
    *(p++) = 0xF0; *(p++) = 0x90; *(p++) = 0x90; *(p++) = 0x90; *(p++) = 0xF0; // 0
    *(p++) = 0x20; *(p++) = 0x60; *(p++) = 0x20; *(p++) = 0x20; *(p++) = 0x70; // 1
    *(p++) = 0xF0; *(p++) = 0x10; *(p++) = 0xF0; *(p++) = 0x80; *(p++) = 0xF0; // 2
    *(p++) = 0xF0; *(p++) = 0x10; *(p++) = 0xF0; *(p++) = 0x10; *(p++) = 0xF0; // 3
    *(p++) = 0x90; *(p++) = 0x90; *(p++) = 0xF0; *(p++) = 0x10; *(p++) = 0x10; // 4
    *(p++) = 0xF0; *(p++) = 0x80; *(p++) = 0xF0; *(p++) = 0x10; *(p++) = 0xF0; // 5
    *(p++) = 0xF0; *(p++) = 0x80; *(p++) = 0xF0; *(p++) = 0x90; *(p++) = 0xF0; // 6
    *(p++) = 0xF0; *(p++) = 0x10; *(p++) = 0x20; *(p++) = 0x40; *(p++) = 0x40; // 7
    *(p++) = 0xF0; *(p++) = 0x90; *(p++) = 0xF0; *(p++) = 0x90; *(p++) = 0xF0; // 8
    *(p++) = 0xF0; *(p++) = 0x90; *(p++) = 0xF0; *(p++) = 0x10; *(p++) = 0xF0; // 9
    *(p++) = 0xF0; *(p++) = 0x90; *(p++) = 0xF0; *(p++) = 0x90; *(p++) = 0x90; // A
    *(p++) = 0xE0; *(p++) = 0x90; *(p++) = 0xE0; *(p++) = 0x90; *(p++) = 0xE0; // B
    *(p++) = 0xF0; *(p++) = 0x80; *(p++) = 0x80; *(p++) = 0x80; *(p++) = 0xF0; // C
    *(p++) = 0xE0; *(p++) = 0x90; *(p++) = 0x90; *(p++) = 0x90; *(p++) = 0xE0; // D
    *(p++) = 0xF0; *(p++) = 0x80; *(p++) = 0xF0; *(p++) = 0x80; *(p++) = 0xF0; // E
    *(p++) = 0xF0; *(p++) = 0x80; *(p++) = 0xF0; *(p++) = 0x80; *(p++) = 0x80; // F

    memcpy(mem + PROGRAM_OFFSET, program, program_size);

    PC = PROGRAM_OFFSET;
    cycle_counter = CHIP8_CYCLES_PER_TIMER;
}

void chip8_do_cycle(uint8_t screen[CHIP8_SCREEN_HEIGHT][CHIP8_SCREEN_WIDTH], const bool keys[CHIP8_NUM_KEYS]) {
    switch (mem[PC] >> 4) {
        case 0x0: {
            assert(mem[PC] == 0);
            switch (mem[PC + 1]) {
                case 0xe0: {
                    memset(screen, 0, SCREEN_BYTES);
                    PC += 2;
                    break;
                }
                case 0xee: {
                    assert(SP > 0);
                    PC = stack[--SP];
                    break;
                }
                default: assert(!"Unknown instruction");
            }
            break;
        }
        case 0x1: {
            uint16_t addr = ((mem[PC] & 0xF) << 8) | mem[PC + 1];
            PC = addr;
            break;
        }
        case 0x2: {
            uint16_t addr = ((mem[PC] & 0xF) << 8) | mem[PC + 1];
            PC += 2;
            assert(SP < STACK_SIZE);
            stack[SP++] = PC;
            PC = addr;
            break;
        }
        case 0x3: {
            uint8_t reg = mem[PC] & 0xF;
            uint8_t n = mem[PC + 1];
            PC += 2;
            if (V[reg] == n) {
                PC += 2;
            }
            break;
        }
        case 0x4: {
            uint8_t reg = mem[PC] & 0xF;
            uint8_t n = mem[PC + 1];
            PC += 2;
            if (V[reg] != n) {
                PC += 2;
            }
            break;
        }
        case 0x5: {
            uint8_t x = mem[PC] & 0xF;
            uint8_t y = mem[PC + 1] >> 4;
            PC += 2;
            if (V[x] == V[y]) {
                PC += 2;
            }
            break;
        }
        case 0x6: {
            uint8_t reg = mem[PC] & 0xF;
            uint8_t val = mem[PC + 1];
            V[reg] = val;
            PC += 2;
            break;
        }
        case 0x7: {
            uint8_t reg = mem[PC] & 0xF;
            uint8_t val = mem[PC + 1];
            V[reg] += val;
            PC += 2;
            break;
        }
        case 0x8: {
            switch (mem[PC + 1] & 0xF) {
                case 0x0: {
                    uint8_t regX = mem[PC] & 0xF;
                    uint8_t regY = mem[PC + 1] >> 4;
                    V[regX] = V[regY];
                    PC += 2;
                    break;
                }
                case 0x1: {
                    uint8_t regX = mem[PC] & 0xF;
                    uint8_t regY = mem[PC + 1] >> 4;
                    V[regX] = V[regX] | V[regY];
                    PC += 2;
                    break;
                }
                case 0x2: {
                    uint8_t regX = mem[PC] & 0xF;
                    uint8_t regY = mem[PC + 1] >> 4;
                    V[regX] = V[regX] & V[regY];
                    PC += 2;
                    break;
                }
                case 0x3: {
                    uint8_t regX = mem[PC] & 0xF;
                    uint8_t regY = mem[PC + 1] >> 4;
                    V[regX] = V[regX] ^ V[regY];
                    PC += 2;
                    break;
                }
                case 0x4: {
                    uint8_t regX = mem[PC] & 0xF;
                    uint8_t regY = mem[PC + 1] >> 4;
                    uint32_t result = V[regX] + V[regY];
                    if (result > 0xFF) V[0xF] = 1;
                    V[regX] = result & 0xFF;
                    PC += 2;
                    break;
                }
                case 0x5: {
                    uint8_t regX = mem[PC] & 0xF;
                    uint8_t regY = mem[PC + 1] >> 4;
                    V[0xF] = V[regX] > V[regY];
                    V[regX] = V[regX] - V[regY];
                    PC += 2;
                    break;
                }
                case 0x6: {
                    uint8_t x = mem[PC] & 0xF;
                    V[0xF] = V[x] & 0x1;
                    V[x] >>= 1;
                    PC += 2;
                    break;
                }
                case 0x7: {
                    uint8_t x = mem[PC] & 0xF;
                    uint8_t y = mem[PC + 1] >> 4;
                    V[0xF] = V[y] > V[x];
                    V[x] = V[y] - V[x];
                    PC += 2;
                    break;
                }
                case 0xe: {
                    uint8_t x = mem[PC] & 0xF;
                    V[0xF] = V[x] & 0x80;
                    V[x] <<= 1;
                    PC += 2;
                    break;
                }
                default: assert(!"Unknown instruction");
            }
            break;
        }
        case 0x9: {
            uint8_t x = mem[PC] & 0xF;
            uint8_t y = mem[PC + 1] >> 4;
            PC += 2;
            if (V[x] != V[y]) {
                PC += 2;
            }
            break;
        }
        case 0xa: {
            uint16_t addr = ((mem[PC] & 0xF) << 8) | mem[PC + 1];
            I = addr;
            PC += 2;
            break;
        }
        case 0xc: {
            uint8_t reg = mem[PC] & 0xF;
            uint8_t mask = mem[PC + 1];
            uint8_t val = (rand() % 0x100) & mask;
            V[reg] = val;
            PC += 2;
            break;
        }
        case 0xd: {
            uint8_t xReg = mem[PC] & 0xF;
            uint8_t yReg = mem[PC + 1] >> 4;
            uint8_t height = mem[PC + 1] & 0xF;
            uint8_t collision = 0;
            for (uint8_t row = 0; row < height; ++row) {
                uint8_t curY = V[yReg] + row;
                if (curY >= CHIP8_SCREEN_HEIGHT) break;
                uint8_t spriteRow = mem[I + row];
                for (uint8_t col = 0; col < 8; ++col) {
                    uint8_t curX = V[xReg] + col;
                    if (curX >= CHIP8_SCREEN_WIDTH) break;
                    uint8_t spriteBit = (spriteRow >> (7 - col)) & 1;
                    if (collision == 0) collision = screen[curY][curX] & spriteBit;
                    screen[curY][curX] ^= spriteBit;
                }
            }
            V[0xF] = collision;
            PC += 2;
            break;
        }
        case 0xe: {
            switch (mem[PC + 1]) {
                case 0x9e: {
                    uint8_t reg = mem[PC] & 0xF;
                    uint8_t key = V[reg];
                    assert(key <= 0xF);
                    PC += 2;
                    if (keys[key]) {
                        PC += 2;
                    }
                    break;
                }
                case 0xa1: {
                    uint8_t reg = mem[PC] & 0xF;
                    uint8_t key = V[reg];
                    assert(key <= 0xF);
                    PC += 2;
                    if (!keys[key]) {
                        PC += 2;
                    }
                    break;
                }
                default: assert(!"Unknown instruction");
            }
            break;
        }
        case 0xf: {
            switch (mem[PC + 1]) {
                case 0x07: {
                    uint8_t reg = mem[PC] & 0xF;
                    V[reg] = delay_timer;
                    PC += 2;
                    break;
                }
                case 0x0a: {
                    uint8_t x = mem[PC] & 0xF;
                    for (uint8_t key = 0; key < CHIP8_NUM_KEYS; ++key) {
                        if (keys[key]) {
                            V[x] = key;
                            PC += 2;
                            break;
                        }
                    }
                    break;
                }
                case 0x15: {
                    uint8_t reg = mem[PC] & 0xF;
                    uint8_t val = V[reg];
                    delay_timer = val;
                    PC += 2;
                    break;
                }
                case 0x18: {
                    uint8_t reg = mem[PC] & 0xF;
                    uint8_t val = V[reg];
                    sound_timer = val;
                    PC += 2;
                    break;
                }
                case 0x1e: {
                    uint8_t x = mem[PC] & 0xF;
                    I += V[x];
                    PC += 2;
                    break;
                }
                case 0x29: {
                    uint8_t reg = mem[PC] & 0xF;
                    uint8_t val = V[reg];
                    assert(val <= 0xF);
                    I = 5 * val;
                    PC += 2;
                    break;
                }
                case 0x33: {
                    uint8_t reg = mem[PC] & 0xF;
                    uint8_t val = V[reg];
                    uint8_t hundreds = val / 100;
                    uint8_t tens = (val % 100) / 10;
                    uint8_t ones = val % 10;
                    mem[I + 0] = hundreds;
                    mem[I + 1] = tens;
                    mem[I + 2] = ones;
                    PC += 2;
                    break;
                }
                case 0x55: {
                    uint8_t endReg = mem[PC] & 0xF;
                    for (uint8_t i = 0; i <= endReg; ++i) {
                        mem[I + i] = V[i];
                    }
                    PC += 2;
                    break;
                }
                case 0x65: {
                    uint8_t endReg = mem[PC] & 0xF;
                    for (uint8_t i = 0; i <= endReg; ++i) {
                        V[i] = mem[I + i];
                    }
                    PC += 2;
                    break;
                }
                default: assert(!"Unknown instruction");
            }
            break;
        }
        default: assert(!"Unknown instruction");
    }

    cycle_counter--;
    if (cycle_counter == 0) {
        cycle_counter = CHIP8_CYCLES_PER_TIMER;
        if (delay_timer > 0) delay_timer--;
        if (sound_timer > 0) sound_timer--;
    }
}

uint8_t chip8_get_sound_timer() {
    return sound_timer;
}
