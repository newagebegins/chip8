#include "chip8.h"

#include <memory.h>
#include <assert.h>
#include <stdlib.h>

#define MEMORY_SIZE 4096
#define PROGRAM_OFFSET 512
#define NUM_REGISTERS 16
#define STACK_SIZE 16
#define SCREEN_BYTES (CHIP8_SCREEN_HEIGHT * CHIP8_SCREEN_WIDTH)
#define FONT_HEIGHT 5

static uint8_t M[MEMORY_SIZE] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80, // F
};
static uint16_t PC; // program counter
static uint8_t V[NUM_REGISTERS]; // registers
static uint16_t I;
static uint8_t delay_timer;
static uint8_t sound_timer;
static uint16_t stack[STACK_SIZE];
static uint8_t SP; // stack pointer
static uint32_t cycle_counter;

void chip8_init(const uint8_t *program, uint32_t program_size) {
    memcpy(M + PROGRAM_OFFSET, program, program_size);
    PC = PROGRAM_OFFSET;
    cycle_counter = CHIP8_CYCLES_PER_TIMER;
}

void chip8_do_cycle(uint8_t screen[CHIP8_SCREEN_HEIGHT][CHIP8_SCREEN_WIDTH], const bool keys[CHIP8_NUM_KEYS]) {
    switch (M[PC] >> 4) {
        case 0x0: {
            assert(M[PC] == 0);
            switch (M[PC + 1]) {
                case 0xe0:
                    memset(screen, 0, SCREEN_BYTES);
                    PC += 2;
                    break;
                case 0xee:
                    assert(SP > 0);
                    PC = stack[--SP];
                    break;
                default: assert(!"Unknown instruction");
            }
            break;
        }
        case 0x1: {
            uint16_t addr = ((M[PC] & 0xF) << 8) | M[PC + 1];
            PC = addr;
            break;
        }
        case 0x2: {
            uint16_t addr = ((M[PC] & 0xF) << 8) | M[PC + 1];
            PC += 2;
            assert(SP < STACK_SIZE);
            stack[SP++] = PC;
            PC = addr;
            break;
        }
        case 0x3: {
            uint8_t x = M[PC] & 0xF;
            uint8_t k = M[PC + 1];
            PC += 2;
            if (V[x] == k) PC += 2;
            break;
        }
        case 0x4: {
            uint8_t x = M[PC] & 0xF;
            uint8_t k = M[PC + 1];
            PC += 2;
            if (V[x] != k) PC += 2;
            break;
        }
        case 0x5: {
            uint8_t x = M[PC] & 0xF;
            uint8_t y = M[PC + 1] >> 4;
            PC += 2;
            if (V[x] == V[y]) PC += 2;
            break;
        }
        case 0x6: {
            uint8_t x = M[PC] & 0xF;
            uint8_t k = M[PC + 1];
            V[x] = k;
            PC += 2;
            break;
        }
        case 0x7: {
            uint8_t x = M[PC] & 0xF;
            uint8_t k = M[PC + 1];
            V[x] += k;
            PC += 2;
            break;
        }
        case 0x8: {
            uint8_t x = M[PC] & 0xF;
            uint8_t y = M[PC + 1] >> 4;
            switch (M[PC + 1] & 0xF) {
                case 0x0: {
                    V[x] = V[y];
                    PC += 2;
                    break;
                }
                case 0x1: {
                    V[x] = V[x] | V[y];
                    PC += 2;
                    break;
                }
                case 0x2: {
                    V[x] = V[x] & V[y];
                    PC += 2;
                    break;
                }
                case 0x3: {
                    V[x] = V[x] ^ V[y];
                    PC += 2;
                    break;
                }
                case 0x4: {
                    uint32_t result = V[x] + V[y];
                    if (result > 0xFF) V[0xF] = 1;
                    V[x] = result & 0xFF;
                    PC += 2;
                    break;
                }
                case 0x5: {
                    V[0xF] = V[x] > V[y];
                    V[x] = V[x] - V[y];
                    PC += 2;
                    break;
                }
                case 0x6: {
                    V[0xF] = V[x] & 0x1;
                    V[x] >>= 1;
                    PC += 2;
                    break;
                }
                case 0x7: {
                    V[0xF] = V[y] > V[x];
                    V[x] = V[y] - V[x];
                    PC += 2;
                    break;
                }
                case 0xe: {
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
            uint8_t x = M[PC] & 0xF;
            uint8_t y = M[PC + 1] >> 4;
            PC += 2;
            if (V[x] != V[y]) {
                PC += 2;
            }
            break;
        }
        case 0xa: {
            uint16_t addr = ((M[PC] & 0xF) << 8) | M[PC + 1];
            I = addr;
            PC += 2;
            break;
        }
        case 0xc: {
            uint8_t reg = M[PC] & 0xF;
            uint8_t mask = M[PC + 1];
            uint8_t val = (rand() % 0x100) & mask;
            V[reg] = val;
            PC += 2;
            break;
        }
        case 0xd: {
            uint8_t xReg = M[PC] & 0xF;
            uint8_t yReg = M[PC + 1] >> 4;
            uint8_t height = M[PC + 1] & 0xF;
            uint8_t collision = 0;
            for (uint8_t row = 0; row < height; ++row) {
                uint8_t curY = V[yReg] + row;
                if (curY >= CHIP8_SCREEN_HEIGHT) break;
                uint8_t spriteRow = M[I + row];
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
            switch (M[PC + 1]) {
                case 0x9e: {
                    uint8_t reg = M[PC] & 0xF;
                    uint8_t key = V[reg];
                    assert(key <= 0xF);
                    PC += 2;
                    if (keys[key]) PC += 2;
                    break;
                }
                case 0xa1: {
                    uint8_t reg = M[PC] & 0xF;
                    uint8_t key = V[reg];
                    assert(key <= 0xF);
                    PC += 2;
                    if (!keys[key]) PC += 2;
                    break;
                }
                default: assert(!"Unknown instruction");
            }
            break;
        }
        case 0xf: {
            switch (M[PC + 1]) {
                case 0x07: {
                    uint8_t x = M[PC] & 0xF;
                    V[x] = delay_timer;
                    PC += 2;
                    break;
                }
                case 0x0a: {
                    uint8_t x = M[PC] & 0xF;
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
                    uint8_t x = M[PC] & 0xF;
                    delay_timer = V[x];
                    PC += 2;
                    break;
                }
                case 0x18: {
                    uint8_t x = M[PC] & 0xF;
                    sound_timer = V[x];
                    PC += 2;
                    break;
                }
                case 0x1e: {
                    uint8_t x = M[PC] & 0xF;
                    I += V[x];
                    PC += 2;
                    break;
                }
                case 0x29: {
                    uint8_t x = M[PC] & 0xF;
                    assert(V[x] <= 0xF);
                    I = FONT_HEIGHT * V[x];
                    PC += 2;
                    break;
                }
                case 0x33: {
                    uint8_t x = M[PC] & 0xF;
                    uint8_t hundreds = V[x] / 100;
                    uint8_t tens = (V[x] % 100) / 10;
                    uint8_t ones = V[x] % 10;
                    M[I + 0] = hundreds;
                    M[I + 1] = tens;
                    M[I + 2] = ones;
                    PC += 2;
                    break;
                }
                case 0x55: {
                    uint8_t end_reg = M[PC] & 0xF;
                    for (uint8_t i = 0; i <= end_reg; ++i) M[I + i] = V[i];
                    PC += 2;
                    break;
                }
                case 0x65: {
                    uint8_t end_reg = M[PC] & 0xF;
                    for (uint8_t i = 0; i <= end_reg; ++i) V[i] = M[I + i];
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
