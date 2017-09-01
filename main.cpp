#include "sound.h"
#include "debug.h"

#include <windows.h>
#include <stdint.h>

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

struct Chip8 {
    u8  mem[CHIP8_MEMORY_SIZE];
    u16 PC;                      // program counter
    u8  V[CHIP8_NUM_REGISTERS];  // registers
    u16 I;
    u8  delayTimer;
    u8  soundTimer;
    u8  prevSoundTimer;

    u16 stack[CHIP8_STACK_SIZE];
    u8  SP;                      // stack pointer

    u32 cycleCounter;
};

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

Chip8* chip8Create(char *programPath) {
    Chip8 *chip8 = (Chip8 *)calloc(sizeof(Chip8), 1);

    // load font data into memory
    u8 *p = chip8->mem;
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

    u8 *fileContent = NULL;
    u32 fileSize = 0;
    readFile(programPath, &fileContent, &fileSize);
    memcpy(chip8->mem + CHIP8_PROGRAM_OFFSET, fileContent, fileSize);
    free(fileContent);

    chip8->PC = CHIP8_PROGRAM_OFFSET;
    chip8->cycleCounter = CYCLES_PER_TIMER;

    return chip8;
}

void chip8DoCycle(Chip8 *chip8, const b32 *keys) {
    switch (chip8->mem[chip8->PC] >> 4) {
        case 0x0: {
            ASSERT(chip8->mem[chip8->PC] == 0);
            switch (chip8->mem[chip8->PC + 1]) {
                case 0xe0: {
                    memset(screen, 0, SCREEN_BYTES);
                    chip8->PC += 2;
                    break;
                }
                case 0xee: {
                    ASSERT(chip8->SP > 0);
                    chip8->PC = chip8->stack[--chip8->SP];
                    break;
                }
                default: ASSERT(!"Unknown instruction");
            }
            break;
        }
        case 0x1: {
            u16 addr = ((chip8->mem[chip8->PC] & 0xF) << 8) | chip8->mem[chip8->PC + 1];
            chip8->PC = addr;
            break;
        }
        case 0x2: {
            u16 addr = ((chip8->mem[chip8->PC] & 0xF) << 8) | chip8->mem[chip8->PC + 1];
            chip8->PC += 2;
            ASSERT(chip8->SP < CHIP8_STACK_SIZE);
            chip8->stack[chip8->SP++] = chip8->PC;
            chip8->PC = addr;
            break;
        }
        case 0x3: {
            u8 reg = chip8->mem[chip8->PC] & 0xF;
            u8 n = chip8->mem[chip8->PC + 1];
            chip8->PC += 2;
            if (chip8->V[reg] == n) {
                chip8->PC += 2;
            }
            break;
        }
        case 0x4: {
            u8 reg = chip8->mem[chip8->PC] & 0xF;
            u8 n = chip8->mem[chip8->PC + 1];
            chip8->PC += 2;
            if (chip8->V[reg] != n) {
                chip8->PC += 2;
            }
            break;
        }
        case 0x6: {
            u8 reg = chip8->mem[chip8->PC] & 0xF;
            u8 val = chip8->mem[chip8->PC + 1];
            chip8->V[reg] = val;
            chip8->PC += 2;
            break;
        }
        case 0x7: {
            u8 reg = chip8->mem[chip8->PC] & 0xF;
            u8 val = chip8->mem[chip8->PC + 1];
            chip8->V[reg] += val;
            chip8->PC += 2;
            break;
        }
        case 0x8: {
            switch (chip8->mem[chip8->PC + 1] & 0xF) {
                case 0x0: {
                    u8 regX = chip8->mem[chip8->PC] & 0xF;
                    u8 regY = chip8->mem[chip8->PC + 1] >> 4;
                    chip8->V[regX] = chip8->V[regY];
                    chip8->PC += 2;
                    break;
                }
                case 0x1: {
                    u8 regX = chip8->mem[chip8->PC] & 0xF;
                    u8 regY = chip8->mem[chip8->PC + 1] >> 4;
                    chip8->V[regX] = chip8->V[regX] | chip8->V[regY];
                    chip8->PC += 2;
                    break;
                }
                case 0x2: {
                    u8 regX = chip8->mem[chip8->PC] & 0xF;
                    u8 regY = chip8->mem[chip8->PC + 1] >> 4;
                    chip8->V[regX] = chip8->V[regX] & chip8->V[regY];
                    chip8->PC += 2;
                    break;
                }
                case 0x3: {
                    u8 regX = chip8->mem[chip8->PC] & 0xF;
                    u8 regY = chip8->mem[chip8->PC + 1] >> 4;
                    chip8->V[regX] = chip8->V[regX] ^ chip8->V[regY];
                    chip8->PC += 2;
                    break;
                }
                case 0x4: {
                    u8 regX = chip8->mem[chip8->PC] & 0xF;
                    u8 regY = chip8->mem[chip8->PC + 1] >> 4;
                    u32 result = chip8->V[regX] + chip8->V[regY];
                    if (result > 0xFF) chip8->V[0xF] = 1;
                    chip8->V[regX] = result & 0xFF;
                    chip8->PC += 2;
                    break;
                }
                case 0x5: {
                    u8 regX = chip8->mem[chip8->PC] & 0xF;
                    u8 regY = chip8->mem[chip8->PC + 1] >> 4;
                    chip8->V[0xF] = chip8->V[regX] > chip8->V[regY];
                    chip8->V[regX] = chip8->V[regX] - chip8->V[regY];
                    chip8->PC += 2;
                    break;
                }
                case 0x6: {
                    u8 x = chip8->mem[chip8->PC] & 0xF;
                    chip8->V[0xF] = chip8->V[x] & 0x1;
                    chip8->V[x] >>= 1;
                    chip8->PC += 2;
                    break;
                }
                case 0xe: {
                    u8 x = chip8->mem[chip8->PC] & 0xF;
                    chip8->V[0xF] = chip8->V[x] & 0x80;
                    chip8->V[x] <<= 1;
                    chip8->PC += 2;
                    break;
                }
                default: ASSERT(!"Unknown instruction");
            }
            break;
        }
        case 0x9: {
            u8 x = chip8->mem[chip8->PC] & 0xF;
            u8 y = chip8->mem[chip8->PC + 1] >> 4;
            chip8->PC += 2;
            if (chip8->V[x] != chip8->V[y]) {
                chip8->PC += 2;
            }
            break;
        }
        case 0xa: {
            u16 addr = ((chip8->mem[chip8->PC] & 0xF) << 8) | chip8->mem[chip8->PC + 1];
            chip8->I = addr;
            chip8->PC += 2;
            break;
        }
        case 0xc: {
            u8 reg = chip8->mem[chip8->PC] & 0xF;
            u8 mask = chip8->mem[chip8->PC + 1];
            u8 val = (rand() % 0x100) & mask;
            chip8->V[reg] = val;
            chip8->PC += 2;
            break;
        }
        case 0xd: {
            u8 xReg = chip8->mem[chip8->PC] & 0xF;
            u8 yReg = chip8->mem[chip8->PC + 1] >> 4;
            u8 height = chip8->mem[chip8->PC + 1] & 0xF;
            u8 collision = 0;
            for (u8 row = 0; row < height; ++row) {
                u8 curY = chip8->V[yReg] + row;
                if (curY >= SCREEN_HEIGHT) break;
                u8 spriteRow = chip8->mem[chip8->I + row];
                for (u8 col = 0; col < 8; ++col) {
                    u8 curX = chip8->V[xReg] + col;
                    if (curX >= SCREEN_WIDTH) break;
                    u8 spriteBit = (spriteRow >> (7 - col)) & 1;
                    if (collision == 0) collision = screen[curY][curX] & spriteBit;
                    screen[curY][curX] ^= spriteBit;
                }
            }
            chip8->V[0xF] = collision;
            chip8->PC += 2;
            break;
        }
        case 0xe: {
            switch (chip8->mem[chip8->PC + 1]) {
                case 0x9e: {
                    u8 reg = chip8->mem[chip8->PC] & 0xF;
                    u8 key = chip8->V[reg];
                    ASSERT(key <= 0xF);
                    chip8->PC += 2;
                    if (keys[key]) {
                        chip8->PC += 2;
                    }
                    break;
                }
                case 0xa1: {
                    u8 reg = chip8->mem[chip8->PC] & 0xF;
                    u8 key = chip8->V[reg];
                    ASSERT(key <= 0xF);
                    chip8->PC += 2;
                    if (!keys[key]) {
                        chip8->PC += 2;
                    }
                    break;
                }
                default: ASSERT(!"Unknown instruction");
            }
            break;
        }
        case 0xf: {
            switch (chip8->mem[chip8->PC + 1]) {
                case 0x07: {
                    u8 reg = chip8->mem[chip8->PC] & 0xF;
                    chip8->V[reg] = chip8->delayTimer;
                    chip8->PC += 2;
                    break;
                }
                case 0x0a: {
                    u8 x = chip8->mem[chip8->PC] & 0xF;
                    for (u8 key = 0; key < CHIP8_NUM_KEYS; ++key) {
                        if (keys[key]) {
                            chip8->V[x] = key;
                            chip8->PC += 2;
                            break;
                        }
                    }
                    break;
                }
                case 0x15: {
                    u8 reg = chip8->mem[chip8->PC] & 0xF;
                    u8 val = chip8->V[reg];
                    chip8->delayTimer = val;
                    chip8->PC += 2;
                    break;
                }
                case 0x18: {
                    u8 reg = chip8->mem[chip8->PC] & 0xF;
                    u8 val = chip8->V[reg];
                    chip8->soundTimer = val;
                    chip8->PC += 2;
                    break;
                }
                case 0x1e: {
                    u8 x = chip8->mem[chip8->PC] & 0xF;
                    chip8->I += chip8->V[x];
                    chip8->PC += 2;
                    break;
                }
                case 0x29: {
                    u8 reg = chip8->mem[chip8->PC] & 0xF;
                    u8 val = chip8->V[reg];
                    ASSERT(val <= 0xF);
                    chip8->I = 5 * val;
                    chip8->PC += 2;
                    break;
                }
                case 0x33: {
                    u8 reg = chip8->mem[chip8->PC] & 0xF;
                    u8 val = chip8->V[reg];
                    u8 hundreds = val / 100;
                    u8 tens = (val % 100) / 10;
                    u8 ones = val % 10;
                    chip8->mem[chip8->I + 0] = hundreds;
                    chip8->mem[chip8->I + 1] = tens;
                    chip8->mem[chip8->I + 2] = ones;
                    chip8->PC += 2;
                    break;
                }
                case 0x55: {
                    u8 endReg = chip8->mem[chip8->PC] & 0xF;
                    for (u8 i = 0; i <= endReg; ++i) {
                        chip8->mem[chip8->I + i] = chip8->V[i];
                    }
                    chip8->PC += 2;
                    break;
                }
                case 0x65: {
                    u8 endReg = chip8->mem[chip8->PC] & 0xF;
                    for (u8 i = 0; i <= endReg; ++i) {
                        chip8->V[i] = chip8->mem[chip8->I + i];
                    }
                    chip8->PC += 2;
                    break;
                }
                default: ASSERT(!"Unknown instruction");
            }
            break;
        }
        default: ASSERT(!"Unknown instruction");
    }

    chip8->cycleCounter--;
    if (chip8->cycleCounter == 0) {
        chip8->cycleCounter = CYCLES_PER_TIMER;
        if (chip8->delayTimer > 0) chip8->delayTimer--;
        if (chip8->soundTimer > 0) chip8->soundTimer--;

        if (chip8->prevSoundTimer == 0 && chip8->soundTimer > 0) {
            sound_start();
        }
        else if (chip8->prevSoundTimer > 0 && chip8->soundTimer == 0) {
            sound_stop();
        }
        else if (chip8->soundTimer > 0) {
            sound_update();
        }
        chip8->prevSoundTimer = chip8->soundTimer;
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
    UNREFERENCED_PARAMETER(prevInst);

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

    Chip8 *chip8 = chip8Create(cmdLine);
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
            chip8DoCycle(chip8, keys);
        }

        for (u32 row = 0; row < SCREEN_HEIGHT; ++row)
            for (u32 col = 0; col < SCREEN_WIDTH; ++col)
                bb->mem[row*SCREEN_WIDTH + col] = screen[row][col] ? 0xffffffff : 0xff000000;
        displayBackbuffer(bb);
    }
}
