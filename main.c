#include <windows.h>
#include <stdint.h>
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

#define BACKBUFFER_WIDTH 64
#define BACKBUFFER_HEIGHT 32
#define BACKBUFFER_BYTES (BACKBUFFER_WIDTH * BACKBUFFER_HEIGHT * sizeof(u32))

void setPixel(u32 *backbuffer, u32 x, u32 y, u32 color) {
  assert(x >= 0 && x < BACKBUFFER_WIDTH);
  assert(y >= 0 && y < BACKBUFFER_HEIGHT);
  backbuffer[y*BACKBUFFER_WIDTH + x] = color;
}

void drawFilledRect(u32 *backbuffer, u32 left, u32 top, u32 right, u32 bottom, u32 color) {
  for (u32 y = top; y <= bottom; ++y) {
    for (u32 x = left; x <= right; ++x) {
      setPixel(backbuffer, x, y, color);
    }
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

  //
  // Initialize window
  //

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

  //
  // Initialize backbuffer
  //

  HDC deviceContext = GetDC(wnd);
  u32 *backbuffer = malloc(BACKBUFFER_BYTES);

  BITMAPINFO bitmapInfo;
  bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
  bitmapInfo.bmiHeader.biWidth = BACKBUFFER_WIDTH;
  bitmapInfo.bmiHeader.biHeight = BACKBUFFER_HEIGHT;
  bitmapInfo.bmiHeader.biPlanes = 1;
  bitmapInfo.bmiHeader.biBitCount = 32;
  bitmapInfo.bmiHeader.biCompression = BI_RGB;

  //
  // Clock
  //

  r32 dt = 0.0f;
  r32 targetFps = 60.0f;
  r32 maxDt = 1.0f / targetFps;
  LARGE_INTEGER perfcFreq = {0};
  LARGE_INTEGER perfc = {0};
  LARGE_INTEGER perfcPrev = {0};

  QueryPerformanceFrequency(&perfcFreq);
  QueryPerformanceCounter(&perfc);

  //
  // Game loop
  //

  b32 gameIsRunning = TRUE;

  while (gameIsRunning) {
    perfcPrev = perfc;
    QueryPerformanceCounter(&perfc);
    dt = (r32)(perfc.QuadPart - perfcPrev.QuadPart) / (r32)perfcFreq.QuadPart;
    if (dt > maxDt) {
      dt = maxDt;
    }

    //
    // Message handling
    //

    MSG msg;
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
      switch (msg.message) {
        case WM_QUIT:
          gameIsRunning = FALSE;
          break;

        default:
          TranslateMessage(&msg);
          DispatchMessage(&msg);
          break;
      }
    }

    //
    // Rendering
    //

    drawFilledRect(backbuffer, 0, 0, BACKBUFFER_WIDTH-1, BACKBUFFER_HEIGHT-1, 0xFFFF0000);

    StretchDIBits(deviceContext,
                  0, 0, windowWidth, windowHeight,
                  0, 0, BACKBUFFER_WIDTH, BACKBUFFER_HEIGHT,
                  backbuffer, &bitmapInfo,
                  DIB_RGB_COLORS, SRCCOPY);
  }
}
