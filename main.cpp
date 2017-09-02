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

void read_file(const char *path, uint8_t **content, uint32_t *size) {
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

#define BACKBUFFER_BYTES (CHIP8_SCR_W * CHIP8_SCR_H * sizeof(uint32_t))

static int window_width, window_height;
static int dst_x, dst_y, dst_w, dst_h;
static uint32_t backbuffer[BACKBUFFER_BYTES];
static BITMAPINFO bmp_info;

static bool running = true;
static bool keys[CHIP8_NUM_KEYS];

LRESULT CALLBACK wnd_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_SIZE: {
            window_width = LOWORD(lparam);
            window_height = HIWORD(lparam);
            const float window_aspect = (float)window_width / window_height;
            if (window_aspect < CHIP8_ASPECT) {
                dst_x = 0;
                dst_w = window_width;
                dst_h = window_width / CHIP8_ASPECT;
                dst_y = (window_height - dst_h) / 2;
            }
            else {
                dst_y = 0;
                dst_h = window_height;
                dst_w = window_height * CHIP8_ASPECT;
                dst_x = (window_width - dst_w) / 2;
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(wnd, &ps);
            FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(DKGRAY_BRUSH));
            StretchDIBits(hdc, dst_x, dst_y, dst_w, dst_h, 0, 0, CHIP8_SCR_W, CHIP8_SCR_H, backbuffer, &bmp_info, DIB_RGB_COLORS, SRCCOPY);
            EndPaint(wnd, &ps);
            break;
        }
        case WM_KEYDOWN:
        case WM_KEYUP: {
            bool is_down = ((lparam & (1 << 31)) == 0);
            //debug_log("key %x, is down %x\n", wparam, is_down);
            switch (wparam) {
                case VK_ESCAPE: running = false; break;
                case '1': keys[0x1] = is_down; break;
                case '2': keys[0x2] = is_down; break;
                case '3': keys[0x3] = is_down; break;
                case '4': keys[0xc] = is_down; break;
                case 'Q': keys[0x4] = is_down; break;
                case 'W': keys[0x5] = is_down; break;
                case 'E': keys[0x6] = is_down; break;
                case 'R': keys[0xd] = is_down; break;
                case 'A': keys[0x7] = is_down; break;
                case 'S': keys[0x8] = is_down; break;
                case 'D': keys[0x9] = is_down; break;
                case 'F': keys[0xe] = is_down; break;
                case 'Z': keys[0xa] = is_down; break;
                case 'X': keys[0x0] = is_down; break;
                case 'C': keys[0xb] = is_down; break;
                case 'V': keys[0xf] = is_down; break;
            }
            break;
        }
        default:
            return DefWindowProc(wnd, msg, wparam, lparam);
    }
    return 0;
}

int CALLBACK WinMain(HINSTANCE inst, HINSTANCE prev_inst, LPSTR cmd_line, int cmd_show) {
    WNDCLASS wnd_class = { 0 };
    wnd_class.style = CS_HREDRAW | CS_VREDRAW;
    wnd_class.lpfnWndProc = wnd_proc;
    wnd_class.hInstance = inst;
    wnd_class.hCursor = LoadCursor(0, IDC_ARROW);
    wnd_class.lpszClassName = "CHIP-8";
    RegisterClass(&wnd_class);

    const int window_scale = 14;
    window_width = CHIP8_SCR_W * window_scale;
    window_height = CHIP8_SCR_H * window_scale;

    RECT crect = { 0 };
    crect.right = window_width;
    crect.bottom = window_height;

    DWORD wnd_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    AdjustWindowRect(&crect, wnd_style, 0);

    HWND wnd = CreateWindowEx(0, wnd_class.lpszClassName, "CHIP-8", wnd_style, 0, 0,
        crect.right - crect.left, crect.bottom - crect.top,
        0, 0, inst, 0);
    ShowWindow(wnd, cmd_show);
    UpdateWindow(wnd);

    HDC hdc = GetDC(wnd);

    bmp_info.bmiHeader.biSize = sizeof(bmp_info.bmiHeader);
    bmp_info.bmiHeader.biWidth = CHIP8_SCR_W;
    bmp_info.bmiHeader.biHeight = -CHIP8_SCR_H;
    bmp_info.bmiHeader.biPlanes = 1;
    bmp_info.bmiHeader.biBitCount = 32;
    bmp_info.bmiHeader.biCompression = BI_RGB;

    sound_init();

    {
        uint8_t *program = NULL;
        uint32_t program_size = 0;
        read_file("../data/CHIP8/GAMES/PONG2", &program, &program_size);
        chip8_init(program, program_size);
        free(program);
    }

    uint8_t prev_sound_timer = 0;
    static uint8_t screen[CHIP8_SCR_H][CHIP8_SCR_W];

    while (running) {
        MSG msg;
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) running = false;
            continue;
        }

        chip8_do_cycle(screen, keys);

        uint8_t sound_timer = chip8_get_sound_timer();
        if (prev_sound_timer == 0 && sound_timer > 0) {
            sound_start();
        }
        else if (prev_sound_timer > 0 && sound_timer == 0) {
            sound_stop();
        }
        sound_update();
        prev_sound_timer = sound_timer;

        for (uint32_t row = 0; row < CHIP8_SCR_H; ++row)
            for (uint32_t col = 0; col < CHIP8_SCR_W; ++col)
                backbuffer[row*CHIP8_SCR_W + col] = screen[row][col] ? 0xffffffff : 0xff000000;

        StretchDIBits(hdc, dst_x, dst_y, dst_w, dst_h, 0, 0, CHIP8_SCR_W, CHIP8_SCR_H, backbuffer, &bmp_info, DIB_RGB_COLORS, SRCCOPY);
        Sleep(CHIP8_CYCLE_INTERVAL * 1000);
    }
}
