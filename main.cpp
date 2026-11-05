#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include "app.h"
#include "gfx.h"

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// 全局画刷资源
HBRUSH g_whiteBrush = nullptr;

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    CoInitialize(nullptr);
    
    g_whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"ToDoWellWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_whiteBrush;
    RegisterClassExW(&wc);

    int ballSize = 60;
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"ToDoWell",
        WS_POPUP,
        100, 100, ballSize, ballSize,
        nullptr, nullptr, hInst, nullptr
    );

    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    g_gfx.create(hwnd);
    g_app = new App();
    g_app->create(hwnd);

    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_app->destroy();
    delete g_app;
    g_app = nullptr;
    g_gfx.destroy();
    
    if (g_whiteBrush) {
        DeleteObject(g_whiteBrush);
        g_whiteBrush = nullptr;
    }
    
    CoUninitialize();

    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static UINT_PTR timerId = 0;
    static LARGE_INTEGER lastTime = {};
    static LARGE_INTEGER freq = {};

    switch (msg) {
        case WM_CREATE:
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&lastTime);
            timerId = SetTimer(hwnd, 1, 16, nullptr);
            return 0;

        case WM_TIMER:
            if (wp == 1 && g_app) {
                LARGE_INTEGER now;
                QueryPerformanceCounter(&now);
                float dt = (float)(now.QuadPart - lastTime.QuadPart) / (float)freq.QuadPart;
                lastTime = now;
                g_app->tick(dt);
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            if (g_app) g_app->render();
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
            if (g_app) g_app->onSize();
            return 0;

        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hwnd, msg, wp, lp);
            if (g_app) {
                POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                LRESULT custom = g_app->onNcHitTest(pt);
                if (custom != HTCLIENT) return custom;
            }
            return hit;
        }

        case WM_MOUSEMOVE: {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (g_app) g_app->onMouseMove(g_app->toDip(pt.x), g_app->toDip(pt.y));
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (g_app) g_app->onLButtonDown(g_app->toDip(pt.x), g_app->toDip(pt.y));
            return 0;
        }

        case WM_LBUTTONUP: {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (g_app) g_app->onLButtonUp(g_app->toDip(pt.x), g_app->toDip(pt.y));
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (g_app) g_app->onLButtonDblClk(g_app->toDip(pt.x), g_app->toDip(pt.y));
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            if (g_app) g_app->onWheel(delta);
            return 0;
        }

        case WM_CHAR:
            if (g_app) g_app->onChar((wchar_t)wp);
            return 0;

        case WM_KEYDOWN:
            if (g_app) g_app->onKeyDown((int)wp);
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (timerId) KillTimer(hwnd, timerId);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}
