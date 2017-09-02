#include "chip8.h"
#include "sound.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

void debug_log(const char *format, ...) {
    va_list argptr;
    va_start(argptr, format);
    char str[1024];
    vsprintf_s(str, sizeof(str), format, argptr);
    va_end(argptr);
    OutputDebugString(str);
}

#define TARGET_FPS 60.0f
#define MAX_DT (1.0f / TARGET_FPS)
#define BACKBUFFER_BYTES (CHIP8_SCREEN_WIDTH * CHIP8_SCREEN_HEIGHT * sizeof(uint32_t))

struct Backbuffer {
    uint32_t mem[BACKBUFFER_BYTES];
    HDC deviceContext;
    uint32_t windowWidth;
    uint32_t windowHeight;
    BITMAPINFO *bitmapInfo;
};

void displayBackbuffer(Backbuffer *bb) {
    StretchDIBits(bb->deviceContext,
        0, 0, bb->windowWidth, bb->windowHeight,
        0, 0, CHIP8_SCREEN_WIDTH, CHIP8_SCREEN_HEIGHT,
        bb->mem, bb->bitmapInfo,
        DIB_RGB_COLORS, SRCCOPY);
}

void readFile(const char *path, uint8_t **content, uint32_t *size) {
    BOOL success;

    HANDLE fileHandle = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    assert(fileHandle != INVALID_HANDLE_VALUE);

    LARGE_INTEGER fileSize;
    success = GetFileSizeEx(fileHandle, &fileSize);
    assert(success);

    *size = fileSize.LowPart;
    *content = (uint8_t *)malloc(*size);

    DWORD numBytesRead;
    success = ReadFile(fileHandle, *content, *size, &numBytesRead, NULL);
    assert(success);
    assert(numBytesRead == *size);

    success = CloseHandle(fileHandle);
    assert(success);
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

    uint32_t windowScale = 14;
    bb->windowWidth = CHIP8_SCREEN_WIDTH * windowScale;
    bb->windowHeight = CHIP8_SCREEN_HEIGHT * windowScale;

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
    bb->bitmapInfo->bmiHeader.biWidth = CHIP8_SCREEN_WIDTH;
    bb->bitmapInfo->bmiHeader.biHeight = -CHIP8_SCREEN_HEIGHT;
    bb->bitmapInfo->bmiHeader.biPlanes = 1;
    bb->bitmapInfo->bmiHeader.biBitCount = 32;
    bb->bitmapInfo->bmiHeader.biCompression = BI_RGB;

    sound_init();

    float dt = 0.0f;
    LARGE_INTEGER perfcFreq = { 0 };
    LARGE_INTEGER perfc = { 0 };
    LARGE_INTEGER perfcPrev = { 0 };

    QueryPerformanceFrequency(&perfcFreq);
    QueryPerformanceCounter(&perfc);

    uint8_t *fileContent = NULL;
    uint32_t fileSize = 0;
    readFile("../data/CHIP8/GAMES/PONG2", &fileContent, &fileSize);
    chip8_init(fileContent, fileSize);
    free(fileContent);

    bool keys[CHIP8_NUM_KEYS] = { 0 };
    float cycleTimer = CHIP8_CYCLE_INTERVAL;
    uint8_t prev_sound_timer = 0;

    bool running = true;
    static uint8_t screen[CHIP8_SCREEN_HEIGHT][CHIP8_SCREEN_WIDTH];

    while (running) {
        perfcPrev = perfc;
        QueryPerformanceCounter(&perfc);
        dt = (float)(perfc.QuadPart - perfcPrev.QuadPart) / (float)perfcFreq.QuadPart;
        if (dt > MAX_DT) dt = MAX_DT;

        MSG msg;
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            switch (msg.message) {
                case WM_QUIT:
                    running = false;
                    break;

                case WM_KEYDOWN:
                case WM_KEYUP: {
                    bool isDown = ((msg.lParam & (1 << 31)) == 0);
                    //debugLog("key %x, is down %x\n", msg.wParam, isDown);
                    switch (msg.wParam) {
                        case VK_ESCAPE: running = false; break;

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
            cycleTimer += CHIP8_CYCLE_INTERVAL;
            chip8_do_cycle(screen, keys);
        }

        if (prev_sound_timer == 0 && chip8_get_sound_timer() > 0) {
            sound_start();
        }
        else if (prev_sound_timer > 0 && chip8_get_sound_timer() == 0) {
            sound_stop();
        }
        sound_update();
        prev_sound_timer = chip8_get_sound_timer();

        for (uint32_t row = 0; row < CHIP8_SCREEN_HEIGHT; ++row)
            for (uint32_t col = 0; col < CHIP8_SCREEN_WIDTH; ++col)
                bb->mem[row*CHIP8_SCREEN_WIDTH + col] = screen[row][col] ? 0xffffffff : 0xff000000;
        displayBackbuffer(bb);
    }
}
