#include "sound.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>

void debug_log(const char *format, ...) {
    va_list argptr;
    va_start(argptr, format);
    char str[1024];
    vsprintf_s(str, sizeof(str), format, argptr);
    va_end(argptr);
    OutputDebugString(str);
}

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef float    r32;
typedef double   r64;

typedef u32      b32;

#define ASSERT(expression) if(!(expression)) *(u8 *)0 = 0;

#define TARGET_FPS 60.0f
#define MAX_DT     (1.0f / TARGET_FPS)

#define TIMER_HZ 60
#define CYCLES_PER_TIMER 9
#define CYCLE_HZ (TIMER_HZ * CYCLES_PER_TIMER)
#define CYCLE_INTERVAL (1.0f / CYCLE_HZ)

#define SCREEN_WIDTH  64
#define SCREEN_HEIGHT 32
#define SCREEN_BYTES (SCREEN_HEIGHT * SCREEN_WIDTH)

#define BACKBUFFER_BYTES (SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u32))

static u8 screen[SCREEN_HEIGHT][SCREEN_WIDTH];

#define CHIP8_MEMORY_SIZE    4096
#define CHIP8_PROGRAM_OFFSET 512
#define CHIP8_NUM_REGISTERS  16
#define CHIP8_STACK_SIZE     16
#define CHIP8_NUM_KEYS       16

struct Backbuffer {
    u32 mem[BACKBUFFER_BYTES];
    HDC deviceContext;
    u32 windowWidth;
    u32 windowHeight;
    BITMAPINFO *bitmapInfo;
};

static u8 mem[CHIP8_MEMORY_SIZE];
static u16 PC; // program counter
static u8 V[CHIP8_NUM_REGISTERS]; // registers
static u16 I;
static u8 delayTimer;
static u8 soundTimer;
static u8 prevSoundTimer;
static u16 stack[CHIP8_STACK_SIZE];
static u8 SP; // stack pointer
static u32 cycleCounter;

void displayBackbuffer(Backbuffer *bb) {
    StretchDIBits(bb->deviceContext,
        0, 0, bb->windowWidth, bb->windowHeight,
        0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
        bb->mem, bb->bitmapInfo,
        DIB_RGB_COLORS, SRCCOPY);
}

void readFile(const char *path, u8 **content, u32 *size) {
    BOOL success;

    HANDLE fileHandle = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    ASSERT(fileHandle != INVALID_HANDLE_VALUE);

    LARGE_INTEGER fileSize;
    success = GetFileSizeEx(fileHandle, &fileSize);
    ASSERT(success);

    *size = fileSize.LowPart;
    *content = (u8 *)malloc(*size);

    DWORD numBytesRead;
    success = ReadFile(fileHandle, *content, *size, &numBytesRead, NULL);
    ASSERT(success);
    ASSERT(numBytesRead == *size);

    success = CloseHandle(fileHandle);
    ASSERT(success);
}

void chip8_init(const u8 *program, u32 program_size) {
    // load font data into memory
    u8 *p = mem;
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

    memcpy(mem + CHIP8_PROGRAM_OFFSET, program, program_size);

    PC = CHIP8_PROGRAM_OFFSET;
    cycleCounter = CYCLES_PER_TIMER;
}

void chip8_do_cycle(const b32 *keys) {
    switch (mem[PC] >> 4) {
        case 0x0: {
            ASSERT(mem[PC] == 0);
            switch (mem[PC + 1]) {
                case 0xe0: {
                    memset(screen, 0, SCREEN_BYTES);
                    PC += 2;
                    break;
                }
                case 0xee: {
                    ASSERT(SP > 0);
                    PC = stack[--SP];
                    break;
                }
                default: ASSERT(!"Unknown instruction");
            }
            break;
        }
        case 0x1: {
            u16 addr = ((mem[PC] & 0xF) << 8) | mem[PC + 1];
            PC = addr;
            break;
        }
        case 0x2: {
            u16 addr = ((mem[PC] & 0xF) << 8) | mem[PC + 1];
            PC += 2;
            ASSERT(SP < CHIP8_STACK_SIZE);
            stack[SP++] = PC;
            PC = addr;
            break;
        }
        case 0x3: {
            u8 reg = mem[PC] & 0xF;
            u8 n = mem[PC + 1];
            PC += 2;
            if (V[reg] == n) {
                PC += 2;
            }
            break;
        }
        case 0x4: {
            u8 reg = mem[PC] & 0xF;
            u8 n = mem[PC + 1];
            PC += 2;
            if (V[reg] != n) {
                PC += 2;
            }
            break;
        }
        case 0x5: {
            u8 x = mem[PC] & 0xF;
            u8 y = mem[PC + 1] >> 4;
            PC += 2;
            if (V[x] == V[y]) {
                PC += 2;
            }
            break;
        }
        case 0x6: {
            u8 reg = mem[PC] & 0xF;
            u8 val = mem[PC + 1];
            V[reg] = val;
            PC += 2;
            break;
        }
        case 0x7: {
            u8 reg = mem[PC] & 0xF;
            u8 val = mem[PC + 1];
            V[reg] += val;
            PC += 2;
            break;
        }
        case 0x8: {
            switch (mem[PC + 1] & 0xF) {
                case 0x0: {
                    u8 regX = mem[PC] & 0xF;
                    u8 regY = mem[PC + 1] >> 4;
                    V[regX] = V[regY];
                    PC += 2;
                    break;
                }
                case 0x1: {
                    u8 regX = mem[PC] & 0xF;
                    u8 regY = mem[PC + 1] >> 4;
                    V[regX] = V[regX] | V[regY];
                    PC += 2;
                    break;
                }
                case 0x2: {
                    u8 regX = mem[PC] & 0xF;
                    u8 regY = mem[PC + 1] >> 4;
                    V[regX] = V[regX] & V[regY];
                    PC += 2;
                    break;
                }
                case 0x3: {
                    u8 regX = mem[PC] & 0xF;
                    u8 regY = mem[PC + 1] >> 4;
                    V[regX] = V[regX] ^ V[regY];
                    PC += 2;
                    break;
                }
                case 0x4: {
                    u8 regX = mem[PC] & 0xF;
                    u8 regY = mem[PC + 1] >> 4;
                    u32 result = V[regX] + V[regY];
                    if (result > 0xFF) V[0xF] = 1;
                    V[regX] = result & 0xFF;
                    PC += 2;
                    break;
                }
                case 0x5: {
                    u8 regX = mem[PC] & 0xF;
                    u8 regY = mem[PC + 1] >> 4;
                    V[0xF] = V[regX] > V[regY];
                    V[regX] = V[regX] - V[regY];
                    PC += 2;
                    break;
                }
                case 0x6: {
                    u8 x = mem[PC] & 0xF;
                    V[0xF] = V[x] & 0x1;
                    V[x] >>= 1;
                    PC += 2;
                    break;
                }
                case 0x7: {
                    u8 x = mem[PC] & 0xF;
                    u8 y = mem[PC + 1] >> 4;
                    V[0xF] = V[y] > V[x];
                    V[x] = V[y] - V[x];
                    PC += 2;
                    break;
                }
                case 0xe: {
                    u8 x = mem[PC] & 0xF;
                    V[0xF] = V[x] & 0x80;
                    V[x] <<= 1;
                    PC += 2;
                    break;
                }
                default: ASSERT(!"Unknown instruction");
            }
            break;
        }
        case 0x9: {
            u8 x = mem[PC] & 0xF;
            u8 y = mem[PC + 1] >> 4;
            PC += 2;
            if (V[x] != V[y]) {
                PC += 2;
            }
            break;
        }
        case 0xa: {
            u16 addr = ((mem[PC] & 0xF) << 8) | mem[PC + 1];
            I = addr;
            PC += 2;
            break;
        }
        case 0xc: {
            u8 reg = mem[PC] & 0xF;
            u8 mask = mem[PC + 1];
            u8 val = (rand() % 0x100) & mask;
            V[reg] = val;
            PC += 2;
            break;
        }
        case 0xd: {
            u8 xReg = mem[PC] & 0xF;
            u8 yReg = mem[PC + 1] >> 4;
            u8 height = mem[PC + 1] & 0xF;
            u8 collision = 0;
            for (u8 row = 0; row < height; ++row) {
                u8 curY = V[yReg] + row;
                if (curY >= SCREEN_HEIGHT) break;
                u8 spriteRow = mem[I + row];
                for (u8 col = 0; col < 8; ++col) {
                    u8 curX = V[xReg] + col;
                    if (curX >= SCREEN_WIDTH) break;
                    u8 spriteBit = (spriteRow >> (7 - col)) & 1;
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
                    u8 reg = mem[PC] & 0xF;
                    u8 key = V[reg];
                    ASSERT(key <= 0xF);
                    PC += 2;
                    if (keys[key]) {
                        PC += 2;
                    }
                    break;
                }
                case 0xa1: {
                    u8 reg = mem[PC] & 0xF;
                    u8 key = V[reg];
                    ASSERT(key <= 0xF);
                    PC += 2;
                    if (!keys[key]) {
                        PC += 2;
                    }
                    break;
                }
                default: ASSERT(!"Unknown instruction");
            }
            break;
        }
        case 0xf: {
            switch (mem[PC + 1]) {
                case 0x07: {
                    u8 reg = mem[PC] & 0xF;
                    V[reg] = delayTimer;
                    PC += 2;
                    break;
                }
                case 0x0a: {
                    u8 x = mem[PC] & 0xF;
                    for (u8 key = 0; key < CHIP8_NUM_KEYS; ++key) {
                        if (keys[key]) {
                            V[x] = key;
                            PC += 2;
                            break;
                        }
                    }
                    break;
                }
                case 0x15: {
                    u8 reg = mem[PC] & 0xF;
                    u8 val = V[reg];
                    delayTimer = val;
                    PC += 2;
                    break;
                }
                case 0x18: {
                    u8 reg = mem[PC] & 0xF;
                    u8 val = V[reg];
                    soundTimer = val;
                    PC += 2;
                    break;
                }
                case 0x1e: {
                    u8 x = mem[PC] & 0xF;
                    I += V[x];
                    PC += 2;
                    break;
                }
                case 0x29: {
                    u8 reg = mem[PC] & 0xF;
                    u8 val = V[reg];
                    ASSERT(val <= 0xF);
                    I = 5 * val;
                    PC += 2;
                    break;
                }
                case 0x33: {
                    u8 reg = mem[PC] & 0xF;
                    u8 val = V[reg];
                    u8 hundreds = val / 100;
                    u8 tens = (val % 100) / 10;
                    u8 ones = val % 10;
                    mem[I + 0] = hundreds;
                    mem[I + 1] = tens;
                    mem[I + 2] = ones;
                    PC += 2;
                    break;
                }
                case 0x55: {
                    u8 endReg = mem[PC] & 0xF;
                    for (u8 i = 0; i <= endReg; ++i) {
                        mem[I + i] = V[i];
                    }
                    PC += 2;
                    break;
                }
                case 0x65: {
                    u8 endReg = mem[PC] & 0xF;
                    for (u8 i = 0; i <= endReg; ++i) {
                        V[i] = mem[I + i];
                    }
                    PC += 2;
                    break;
                }
                default: ASSERT(!"Unknown instruction");
            }
            break;
        }
        default: ASSERT(!"Unknown instruction");
    }

    cycleCounter--;
    if (cycleCounter == 0) {
        cycleCounter = CYCLES_PER_TIMER;
        if (delayTimer > 0) delayTimer--;
        if (soundTimer > 0) soundTimer--;

        if (prevSoundTimer == 0 && soundTimer > 0) {
            sound_start();
        }
        else if (prevSoundTimer > 0 && soundTimer == 0) {
            sound_stop();
        }
        sound_update();
        prevSoundTimer = soundTimer;
    }
}

LRESULT CALLBACK wndProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(wnd, msg, wparam, lparam);
    }
    return 0;
}

int CALLBACK WinMain(HINSTANCE inst, HINSTANCE prevInst, LPSTR cmdLine, int cmdShow) {
    WNDCLASS wndClass = { 0 };
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = wndProc;
    wndClass.hInstance = inst;
    wndClass.hCursor = LoadCursor(0, IDC_ARROW);
    wndClass.lpszClassName = "CHIP-8";
    RegisterClass(&wndClass);

    Backbuffer *bb = (Backbuffer *)calloc(sizeof(Backbuffer), 1);

    u32 windowScale = 14;
    bb->windowWidth = SCREEN_WIDTH * windowScale;
    bb->windowHeight = SCREEN_HEIGHT * windowScale;

    RECT crect = { 0 };
    crect.right = bb->windowWidth;
    crect.bottom = bb->windowHeight;

    DWORD wndStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    AdjustWindowRect(&crect, wndStyle, 0);

    HWND wnd = CreateWindowEx(0, wndClass.lpszClassName, "CHIP-8", wndStyle, 0, 0,
        crect.right - crect.left, crect.bottom - crect.top,
        0, 0, inst, 0);
    ShowWindow(wnd, cmdShow);
    UpdateWindow(wnd);

    bb->deviceContext = GetDC(wnd);

    bb->bitmapInfo = (BITMAPINFO *)calloc(sizeof(BITMAPINFOHEADER), 1);
    bb->bitmapInfo->bmiHeader.biSize = sizeof(bb->bitmapInfo->bmiHeader);
    bb->bitmapInfo->bmiHeader.biWidth = SCREEN_WIDTH;
    bb->bitmapInfo->bmiHeader.biHeight = -SCREEN_HEIGHT;
    bb->bitmapInfo->bmiHeader.biPlanes = 1;
    bb->bitmapInfo->bmiHeader.biBitCount = 32;
    bb->bitmapInfo->bmiHeader.biCompression = BI_RGB;

    sound_init();

    r32 dt = 0.0f;
    LARGE_INTEGER perfcFreq = { 0 };
    LARGE_INTEGER perfc = { 0 };
    LARGE_INTEGER perfcPrev = { 0 };

    QueryPerformanceFrequency(&perfcFreq);
    QueryPerformanceCounter(&perfc);

    u8 *fileContent = NULL;
    u32 fileSize = 0;
    readFile("../data/CHIP8/GAMES/PONG2", &fileContent, &fileSize);
    chip8_init(fileContent, fileSize);
    free(fileContent);

    b32 keys[CHIP8_NUM_KEYS] = { 0 };
    r32 cycleTimer = CYCLE_INTERVAL;

    b32 running = TRUE;

    while (running) {
        perfcPrev = perfc;
        QueryPerformanceCounter(&perfc);
        dt = (r32)(perfc.QuadPart - perfcPrev.QuadPart) / (r32)perfcFreq.QuadPart;
        if (dt > MAX_DT) dt = MAX_DT;

        MSG msg;
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            switch (msg.message) {
                case WM_QUIT:
                    running = FALSE;
                    break;

                case WM_KEYDOWN:
                case WM_KEYUP: {
                    b32 isDown = ((msg.lParam & (1 << 31)) == 0);
                    //debugLog("key %x, is down %x\n", msg.wParam, isDown);
                    switch (msg.wParam) {
                        case VK_ESCAPE: running = FALSE; break;

                        case VK_DECIMAL:  keys[0x0] = isDown; break;
                        case VK_NUMPAD7:  keys[0x1] = isDown; break;
                        case VK_NUMPAD8:  keys[0x2] = isDown; break;
                        case VK_NUMPAD9:  keys[0x3] = isDown; break;
                        case VK_NUMPAD4:  keys[0x4] = isDown; break;
                        case VK_NUMPAD5:  keys[0x5] = isDown; break;
                        case VK_NUMPAD6:  keys[0x6] = isDown; break;
                        case VK_NUMPAD1:  keys[0x7] = isDown; break;
                        case VK_NUMPAD2:  keys[0x8] = isDown; break;
                        case VK_NUMPAD3:  keys[0x9] = isDown; break;
                        case VK_NUMPAD0:  keys[0xa] = isDown; break;
                        case VK_RETURN:   keys[0xb] = isDown; break;
                        case VK_DIVIDE:   keys[0xc] = isDown; break;
                        case VK_MULTIPLY: keys[0xd] = isDown; break;
                        case VK_SUBTRACT: keys[0xe] = isDown; break;
                        case VK_ADD:      keys[0xf] = isDown; break;
                    }
                    break;
                }

                default:
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                    break;
            }
        }

        cycleTimer -= dt;
        if (cycleTimer <= 0) {
            cycleTimer += CYCLE_INTERVAL;
            chip8_do_cycle(keys);
        }

        for (u32 row = 0; row < SCREEN_HEIGHT; ++row)
            for (u32 col = 0; col < SCREEN_WIDTH; ++col)
                bb->mem[row*SCREEN_WIDTH + col] = screen[row][col] ? 0xffffffff : 0xff000000;
        displayBackbuffer(bb);
    }
}
