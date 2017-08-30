#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

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

typedef u32 b32;
#define TRUE  1
#define FALSE 0

void debugLog(char *format, ...) {
  va_list argptr;
  va_start(argptr, format);
  char str[1024];
  vsprintf_s(str, sizeof(str), format, argptr);
  va_end(argptr);
  OutputDebugString(str);
}

#define TARGET_FPS 60.0f
#define MAX_DT     (1.0f / TARGET_FPS)

#define BACKBUFFER_WIDTH  64
#define BACKBUFFER_HEIGHT 32
#define BACKBUFFER_STRIDE (BACKBUFFER_WIDTH / 8)
#define BACKBUFFER_BYTES  (BACKBUFFER_STRIDE * BACKBUFFER_HEIGHT)

typedef struct {
  u8 *data;
  u32 size;
} File;

File readFile(char *path) {
  File file;
  BOOL success;

  HANDLE fileHandle = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  assert(fileHandle != INVALID_HANDLE_VALUE);

  LARGE_INTEGER fileSize;
  success = GetFileSizeEx(fileHandle, &fileSize);
  assert(success);

  file.size = fileSize.LowPart;
  file.data = malloc(file.size);

  DWORD numBytesRead;
  success = ReadFile(fileHandle, file.data, file.size, &numBytesRead, NULL);
  assert(success);
  assert(numBytesRead == file.size);

  success = CloseHandle(fileHandle);
  assert(success);

  return file;
}

#define CHIP8_MEMORY_SIZE    4096
#define CHIP8_PROGRAM_OFFSET 512
#define CHIP8_NUM_REGISTERS  16
#define CHIP8_STACK_SIZE     16

typedef struct {
  u8  mem[CHIP8_MEMORY_SIZE];
  u16 PC;                      // program counter
  u8  V[CHIP8_NUM_REGISTERS];  // registers
  u16 I;
  u8  delayTimer;
  u8  soundTimer;

  u16 stack[CHIP8_STACK_SIZE];
  u8  SP;                      // stack pointer
} Chip8;

Chip8* chip8Create(char *programPath) {
  Chip8 *chip8 = calloc(sizeof(Chip8), 1);

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

  File file = readFile(programPath);
  memcpy(chip8->mem + CHIP8_PROGRAM_OFFSET, file.data, file.size);
  free(file.data);

  chip8->PC = CHIP8_PROGRAM_OFFSET;

  return chip8;
}

#define INTERVAL_60HZ (1.0f/60.0f)
r32 timer60hz = INTERVAL_60HZ;

#define NUM_KEYS 16
static b32 keyDown[NUM_KEYS];

void chip8Run(Chip8 *chip8, u8 *backbuffer, r32 dt) {
  switch (chip8->mem[chip8->PC] >> 4) {
    case 0x0: {
      assert(chip8->mem[chip8->PC] == 0);
      switch (chip8->mem[chip8->PC+1]) {
        case 0xee: {
          assert(chip8->SP > 0);
          chip8->PC = chip8->stack[--chip8->SP];
          break;
        }
        default: assert(!"Unknown instruction");
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
      assert(chip8->SP < CHIP8_STACK_SIZE);
      chip8->stack[chip8->SP++] = chip8->PC;
      chip8->PC = addr;
      break;
    }
    case 0x3: {
      u8 reg = chip8->mem[chip8->PC] & 0xF;
      u8 n = chip8->mem[chip8->PC+1];
      chip8->PC += 2;
      if (chip8->V[reg] == n) {
        chip8->PC += 2;
      }
      break;
    }
    case 0x4: {
      u8 reg = chip8->mem[chip8->PC] & 0xF;
      u8 n = chip8->mem[chip8->PC+1];
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
      switch (chip8->mem[chip8->PC+1] & 0xF) {
        case 0x0: {
          u8 regX = chip8->mem[chip8->PC] & 0xF;
          u8 regY = chip8->mem[chip8->PC+1] >> 4;
          chip8->V[regX] = chip8->V[regY];
          chip8->PC += 2;
          break;
        }
        case 0x2: {
          u8 regX = chip8->mem[chip8->PC] & 0xF;
          u8 regY = chip8->mem[chip8->PC+1] >> 4;
          chip8->V[regX] = chip8->V[regX] & chip8->V[regY];
          chip8->PC += 2;
          break;
        }
        case 0x4: {
          u8 regX = chip8->mem[chip8->PC] & 0xF;
          u8 regY = chip8->mem[chip8->PC+1] >> 4;
          u32 result = chip8->V[regX] + chip8->V[regY];
          if (result > 0xFF) chip8->V[0xF] = 1;
          chip8->V[regX] = result & 0xFF;
          chip8->PC += 2;
          break;
        }
        case 0x5: {
          u8 regX = chip8->mem[chip8->PC] & 0xF;
          u8 regY = chip8->mem[chip8->PC+1] >> 4;
          chip8->V[0xF] = chip8->V[regX] > chip8->V[regY];
          chip8->V[regX] = chip8->V[regX] - chip8->V[regY];
          chip8->PC += 2;
          break;
        }
        default: assert(!"Unknown instruction");
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
      u8 mask = chip8->mem[chip8->PC+1];
      u8 val = (rand() % 0x100) & mask;
      chip8->V[reg] = val;
      chip8->PC += 2;
      break;
    }
    case 0xd: {
      u8 xReg   = chip8->mem[chip8->PC] & 0xF;
      u8 yReg   = chip8->mem[chip8->PC+1] >> 4;
      u8 height = chip8->mem[chip8->PC+1] & 0xF;
      u8 x = chip8->V[xReg];
      u8 y = chip8->V[yReg];
      u8 collision = 0;
      for (u8 row = 0; row < height; ++row) {
        u8 spriteRow = chip8->mem[chip8->I + row];
        u8 xByteOffset = x%8;
        u32 bbOffset = (y + row) * BACKBUFFER_STRIDE + x/8;
        if (xByteOffset == 0) {
          assert(bbOffset < BACKBUFFER_BYTES);
          if (collision == 0) {
            collision = (backbuffer[bbOffset] & spriteRow) != 0;
          }
          backbuffer[bbOffset] ^= spriteRow;
        } else {
          assert(bbOffset   < BACKBUFFER_BYTES);
          assert(bbOffset+1 < BACKBUFFER_BYTES);
          u8 leftByte  = (spriteRow >> xByteOffset);
          u8 rightByte = (spriteRow << (8 - xByteOffset));
          if (collision == 0) {
            u8 leftByteCollision  = (backbuffer[bbOffset]   & leftByte)  != 0;
            u8 rightByteCollision = (backbuffer[bbOffset+1] & rightByte) != 0;
            collision = (leftByteCollision || rightByteCollision);
          }
          backbuffer[bbOffset]   ^= leftByte;
          backbuffer[bbOffset+1] ^= rightByte;
        }
      }
      chip8->V[0xF] = collision;
      chip8->PC += 2;
      break;
    }
    case 0xe: {
      switch (chip8->mem[chip8->PC+1]) {
        case 0xa1: {
          u8 reg = chip8->mem[chip8->PC] & 0xF;
          u8 key = chip8->V[reg];
          assert(key <= 0xF);
          chip8->PC += 2;
          if (!keyDown[key]) {
            chip8->PC += 2;
          }
          break;
        }
        default: assert(!"Unknown instruction");
      }
      break;
    }
    case 0xf: {
      switch (chip8->mem[chip8->PC+1]) {
        case 0x07: {
          u8 reg = chip8->mem[chip8->PC] & 0xF;
          chip8->V[reg] = chip8->delayTimer;
          chip8->PC += 2;
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
        case 0x29: {
          u8 reg = chip8->mem[chip8->PC] & 0xF;
          u8 val = chip8->V[reg];
          assert(val <= 0xF);
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
          chip8->mem[chip8->I+0] = hundreds;
          chip8->mem[chip8->I+1] = tens;
          chip8->mem[chip8->I+2] = ones;
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
        default: assert(!"Unknown instruction");
      }
      break;
    }
    default: assert(!"Unknown instruction");
  }

  timer60hz -= dt;
  if (timer60hz <= 0) {
    timer60hz += INTERVAL_60HZ;
    if (chip8->delayTimer > 0) chip8->delayTimer--;
    if (chip8->soundTimer > 0) chip8->soundTimer--;
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
  UNREFERENCED_PARAMETER(cmdLine);

  WNDCLASS wndClass = {0};
  wndClass.style = CS_HREDRAW | CS_VREDRAW;
  wndClass.lpfnWndProc = wndProc;
  wndClass.hInstance = inst;
  wndClass.hCursor = LoadCursor(0, IDC_ARROW);
  wndClass.lpszClassName = "CHIP-8";
  RegisterClass(&wndClass);

  u32 windowScale = 12;
  u32 windowWidth = BACKBUFFER_WIDTH * windowScale;
  u32 windowHeight = BACKBUFFER_HEIGHT * windowScale;

  RECT crect = {0};
  crect.right = windowWidth;
  crect.bottom = windowHeight;

  DWORD wndStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
  AdjustWindowRect(&crect, wndStyle, 0);

  HWND wnd = CreateWindowEx(0, wndClass.lpszClassName, "CHIP-8", wndStyle, 300, 100,
                            crect.right - crect.left, crect.bottom - crect.top,
                            0, 0, inst, 0);
  ShowWindow(wnd, cmdShow);
  UpdateWindow(wnd);

  HDC deviceContext = GetDC(wnd);
  u8 *backbuffer = calloc(BACKBUFFER_BYTES, 1);

  BITMAPINFO *bitmapInfo = calloc(sizeof(BITMAPINFOHEADER) + (2 * sizeof(RGBQUAD)), 1);
  bitmapInfo->bmiHeader.biSize        = sizeof(bitmapInfo->bmiHeader);
  bitmapInfo->bmiHeader.biWidth       = BACKBUFFER_WIDTH;
  bitmapInfo->bmiHeader.biHeight      = -BACKBUFFER_HEIGHT;
  bitmapInfo->bmiHeader.biPlanes      = 1;
  bitmapInfo->bmiHeader.biBitCount    = 1;
  bitmapInfo->bmiHeader.biCompression = BI_RGB;
  bitmapInfo->bmiHeader.biClrUsed     = 2;

  RGBQUAD black = {0x44, 0x44, 0x44, 0x00};
  RGBQUAD white = {0xFF, 0xFF, 0xFF, 0x00};

  bitmapInfo->bmiColors[0] = black;
  bitmapInfo->bmiColors[1] = white;

  r32 dt = 0.0f;
  LARGE_INTEGER perfcFreq = {0};
  LARGE_INTEGER perfc = {0};
  LARGE_INTEGER perfcPrev = {0};

  QueryPerformanceFrequency(&perfcFreq);
  QueryPerformanceCounter(&perfc);

  Chip8 *chip8 = chip8Create("../data/CHIP8/GAMES/PONG");

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
            case VK_DECIMAL:  keyDown[0x0] = isDown; break;
            case VK_NUMPAD7:  keyDown[0x1] = isDown; break;
            case VK_NUMPAD8:  keyDown[0x2] = isDown; break;
            case VK_NUMPAD9:  keyDown[0x3] = isDown; break;
            case VK_NUMPAD4:  keyDown[0x4] = isDown; break;
            case VK_NUMPAD5:  keyDown[0x5] = isDown; break;
            case VK_NUMPAD6:  keyDown[0x6] = isDown; break;
            case VK_NUMPAD1:  keyDown[0x7] = isDown; break;
            case VK_NUMPAD2:  keyDown[0x8] = isDown; break;
            case VK_NUMPAD3:  keyDown[0x9] = isDown; break;
            case VK_NUMPAD0:  keyDown[0xa] = isDown; break;
            case VK_RETURN:   keyDown[0xb] = isDown; break;
            case VK_DIVIDE:   keyDown[0xc] = isDown; break;
            case VK_MULTIPLY: keyDown[0xd] = isDown; break;
            case VK_SUBTRACT: keyDown[0xe] = isDown; break;
            case VK_ADD:      keyDown[0xf] = isDown; break;
          }
          break;
        }

        default:
          TranslateMessage(&msg);
          DispatchMessage(&msg);
          break;
      }
    }

    chip8Run(chip8, backbuffer, dt);

    StretchDIBits(deviceContext,
                  0, 0, windowWidth, windowHeight,
                  0, 0, BACKBUFFER_WIDTH, BACKBUFFER_HEIGHT,
                  backbuffer, bitmapInfo,
                  DIB_RGB_COLORS, SRCCOPY);
  }
}
