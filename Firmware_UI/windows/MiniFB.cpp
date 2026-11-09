#include "MiniFB.h"
#include <stdarg.h>

//#define kUseBilinearInterpolation

#if defined(kUseBilinearInterpolation)
//-------------------------------------
static uint32_t
interpolate(uint32_t *srcImage, uint32_t x, uint32_t y, uint32_t srcOffsetX, uint32_t srcOffsetY, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcPitch) {
    uint32_t incX = x + 1 < srcWidth ? 1 : 0;
    uint32_t incY = y + 1 < srcHeight ? srcPitch : 0;
    uint8_t *p00 = (uint8_t *) &srcImage[(srcOffsetX >> 16)];
    uint8_t *p01 = (uint8_t *) &srcImage[(srcOffsetX >> 16) + incX];
    uint8_t *p10 = (uint8_t *) &srcImage[(srcOffsetX >> 16) + incY];
    uint8_t *p11 = (uint8_t *) &srcImage[(srcOffsetX >> 16) + incY + incX];

    uint32_t wx2 = srcOffsetX & 0xffff;
    uint32_t wy2 = srcOffsetY & 0xffff;
    uint32_t wx1 = 0x10000 - wx2;
    uint32_t wy1 = 0x10000 - wy2;

    uint32_t w1 = ((uint64_t) wx1 * wy1) >> 16;
    uint32_t w2 = ((uint64_t) wx2 * wy1) >> 16;
    uint32_t w3 = ((uint64_t) wx1 * wy2) >> 16;
    uint32_t w4 = ((uint64_t) wx2 * wy2) >> 16;

    // If you don't have uint64_t
    //uint32_t b = (((p00[0] * wx1 + p01[0] * wx2) >> 16) * wy1 + ((p10[0] * wx1 + p11[0] * wx2) >> 16) * wy2) >> 16;
    //uint32_t g = (((p00[1] * wx1 + p01[1] * wx2) >> 16) * wy1 + ((p10[1] * wx1 + p11[1] * wx2) >> 16) * wy2) >> 16;
    //uint32_t r = (((p00[2] * wx1 + p01[2] * wx2) >> 16) * wy1 + ((p10[2] * wx1 + p11[2] * wx2) >> 16) * wy2) >> 16;
    //uint32_t a = (((p00[3] * wx1 + p01[3] * wx2) >> 16) * wy1 + ((p10[3] * wx1 + p11[3] * wx2) >> 16) * wy2) >> 16;

    uint32_t b = ((p00[0] * w1 + p01[0] * w2) + (p10[0] * w3 + p11[0] * w4)) >> 16;
    uint32_t g = ((p00[1] * w1 + p01[1] * w2) + (p10[1] * w3 + p11[1] * w4)) >> 16;
    uint32_t r = ((p00[2] * w1 + p01[2] * w2) + (p10[2] * w3 + p11[2] * w4)) >> 16;
    uint32_t a = ((p00[3] * w1 + p01[3] * w2) + (p10[3] * w3 + p11[3] * w4)) >> 16;

    return (a << 24) + (r << 16) + (g << 8) + b;
}
#endif

// Only for 32 bits images
//-------------------------------------
void stretch_image(uint32_t *srcImage, uint32_t srcX, uint32_t srcY, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcPitch,
    uint32_t *dstImage, uint32_t dstX, uint32_t dstY, uint32_t dstWidth, uint32_t dstHeight, uint32_t dstPitch) 
{
    uint32_t    x, y;
    uint32_t    srcOffsetX, srcOffsetY;

    if(srcImage == 0x0 || dstImage == 0x0) return;

    srcImage += srcX + srcY * srcPitch;
    dstImage += dstX + dstY * dstPitch;

    const uint32_t deltaX = (srcWidth  << 16) / dstWidth;
    const uint32_t deltaY = (srcHeight << 16) / dstHeight;

    srcOffsetY = 0;
    for(y=0; y<dstHeight; ++y) {
        srcOffsetX = 0;
        for(x=0; x<dstWidth; ++x) {
#if defined(kUseBilinearInterpolation)
            dstImage[x] = interpolate(srcImage, x+srcX, y+srcY, srcOffsetX, srcOffsetY, srcWidth, srcHeight, srcPitch);
#else
            dstImage[x] = srcImage[srcOffsetX >> 16];
#endif
            srcOffsetX += deltaX;
        }

        srcOffsetY += deltaY;
        if(srcOffsetY >= 0x10000) { srcImage += (srcOffsetY >> 16) * srcPitch; srcOffsetY &= 0xffff; }
        dstImage += dstPitch;
    }
}
#include "MiniFB.h"
#include <stdio.h>
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t translate_mod();

// Copied (and modified) from Windows Kit 10 to avoid setting _WIN32_WINNT to a higher version
typedef enum mfb_PROCESS_DPI_AWARENESS {
    mfb_PROCESS_DPI_UNAWARE           = 0,
    mfb_PROCESS_SYSTEM_DPI_AWARE      = 1,
    mfb_PROCESS_PER_MONITOR_DPI_AWARE = 2
} mfb_PROCESS_DPI_AWARENESS;

typedef enum mfb_MONITOR_DPI_TYPE {
    mfb_MDT_EFFECTIVE_DPI             = 0,
    mfb_MDT_ANGULAR_DPI               = 1,
    mfb_MDT_RAW_DPI                   = 2,
    mfb_MDT_DEFAULT                   = mfb_MDT_EFFECTIVE_DPI
} mfb_MONITOR_DPI_TYPE;

#define mfb_DPI_AWARENESS_CONTEXT_UNAWARE               ((HANDLE) -1)
#define mfb_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE          ((HANDLE) -2)
#define mfb_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE     ((HANDLE) -3)
#define mfb_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2  ((HANDLE) -4)
#define mfb_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED     ((HANDLE) -5)

// user32.dll
typedef BOOL(WINAPI *PFN_SetProcessDPIAware)(void);
typedef BOOL(WINAPI *PFN_SetProcessDpiAwarenessContext)(HANDLE);
typedef UINT(WINAPI *PFN_GetDpiForWindow)(HWND);
typedef BOOL(WINAPI *PFN_EnableNonClientDpiScaling)(HWND);

HMODULE                           mfb_user32_dll                    = 0x0;
PFN_SetProcessDPIAware            mfb_SetProcessDPIAware            = 0x0;
PFN_SetProcessDpiAwarenessContext mfb_SetProcessDpiAwarenessContext = 0x0;
PFN_GetDpiForWindow               mfb_GetDpiForWindow               = 0x0;
PFN_EnableNonClientDpiScaling     mfb_EnableNonClientDpiScaling     = 0x0;

// shcore.dll
typedef HRESULT(WINAPI *PFN_SetProcessDpiAwareness)(mfb_PROCESS_DPI_AWARENESS);
typedef HRESULT(WINAPI *PFN_GetDpiForMonitor)(HMONITOR, mfb_MONITOR_DPI_TYPE, UINT *, UINT *);

HMODULE                           mfb_shcore_dll                    = 0x0;
PFN_SetProcessDpiAwareness        mfb_SetProcessDpiAwareness        = 0x0;
PFN_GetDpiForMonitor              mfb_GetDpiForMonitor              = 0x0;

//--
void load_functions() 
{
    if(mfb_user32_dll == 0x0) {
        mfb_user32_dll = LoadLibraryA("user32.dll");
        if (mfb_user32_dll != 0x0) {
            mfb_SetProcessDPIAware = (PFN_SetProcessDPIAware) GetProcAddress(mfb_user32_dll, "SetProcessDPIAware");
            mfb_SetProcessDpiAwarenessContext = (PFN_SetProcessDpiAwarenessContext) GetProcAddress(mfb_user32_dll, "SetProcessDpiAwarenessContext");
            mfb_GetDpiForWindow = (PFN_GetDpiForWindow) GetProcAddress(mfb_user32_dll, "GetDpiForWindow");
            mfb_EnableNonClientDpiScaling = (PFN_EnableNonClientDpiScaling) GetProcAddress(mfb_user32_dll, "EnableNonClientDpiScaling");
        }
    }

    if(mfb_shcore_dll == 0x0) {
        mfb_shcore_dll = LoadLibraryA("shcore.dll");
        if (mfb_shcore_dll != 0x0) {
            mfb_SetProcessDpiAwareness = (PFN_SetProcessDpiAwareness) GetProcAddress(mfb_shcore_dll, "SetProcessDpiAwareness");
            mfb_GetDpiForMonitor = (PFN_GetDpiForMonitor) GetProcAddress(mfb_shcore_dll, "GetDpiForMonitor");
        }
    }
}

//--
// NOT Thread safe. Just convenient (Don't do this at home guys)
char * GetErrorMessage() {
    static char buffer[256];

    buffer[0] = 0;
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,  // Not used with FORMAT_MESSAGE_FROM_SYSTEM
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer,
        sizeof(buffer),
        NULL);

    return buffer;
}

//--
void CFBWindow::get_monitor_scale(float &scale_x, float &scale_y) 
{
    UINT    x, y;
    if(mfb_GetDpiForMonitor != 0x0) 
    {
        HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        mfb_GetDpiForMonitor(monitor, mfb_MDT_EFFECTIVE_DPI, &x, &y);
    }
    else 
    {
        const HDC dc = GetDC(hWnd);
        x = GetDeviceCaps(dc, LOGPIXELSX);
        y = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(NULL, dc);
    }
    scale_x = x / (float) USER_DEFAULT_SCREEN_DPI;
    if(scale_x == 0) scale_x = 1;
    scale_y = y / (float) USER_DEFAULT_SCREEN_DPI;
    if (scale_y == 0) scale_y = 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void timerCb(void *p, int id)
{
    printf("Timer\r\n");
    ((CFBWindow*)p)->update_display();
}

void CFBWindow::setFps(int fps)
{
    this->fps= fps;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
    LRESULT res = 0;
    CFBWindow *fb= (CFBWindow*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (message)
    {
    case WM_NCCREATE:
        if(mfb_EnableNonClientDpiScaling) mfb_EnableNonClientDpiScaling(hWnd);
        return DefWindowProc(hWnd, message, wParam, lParam);

        // TODO
        //case 0x02E4://WM_GETDPISCALEDSIZE:
        //{
        //    SIZE* size = (SIZE*) lParam;
        //    WORD dpi = LOWORD(wParam);
        //    return true;
        //    break;
        //}

        // TODO
        //case WM_DPICHANGED:
        //{
        //    const float xscale = HIWORD(wParam);
        //    const float yscale = LOWORD(wParam);
        //    break;
        //}

    case WM_PAINT:
        fb->bitmapInfo.bmiHeader.biWidth       = fb->w;
        fb->bitmapInfo.bmiHeader.biHeight      = -fb->h;
        StretchDIBits(fb->hdc, fb->dst_offset_x, fb->dst_offset_y, fb->dst_width, fb->dst_height, 0, 0, fb->w, fb->h, fb->fb, &fb->bitmapInfo, DIB_RGB_COLORS, SRCCOPY);

        //fb->loadFont(L"Pixeleum-48", 11, 11);
        //fb->loadFont(L"Retro Gaming", 11, 11);
        fb->loadFont(L"technofosiano", 11, 11);

        ValidateRect(hWnd, 0x0);
        break;
    case WM_CLOSE:
        if (fb->close()) { fb->closed = true; DestroyWindow(fb->hWnd); } // Obtain a confirmation of close
        break;

    case WM_DESTROY: fb->closed= true; break;

    case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_KEYUP: case WM_SYSKEYUP:
        fb->keyboard((lParam&0x7fffffff)>>16, translate_mod(), !((lParam >> 31) & 1));
        break;

    case WM_CHAR: case WM_SYSCHAR:
    {
        static WCHAR highSurrogate = 0;
        if (wParam >= 0xd800 && wParam <= 0xdbff) highSurrogate = (WCHAR) wParam;
        else {
            unsigned int codepoint = 0;
            if (wParam >= 0xdc00 && wParam <= 0xdfff) {
                if (highSurrogate != 0) {
                    codepoint += (highSurrogate - 0xd800) << 10;
                    codepoint += (WCHAR) wParam - 0xdc00;
                    codepoint += 0x10000;
                }
            }
            else codepoint = (WCHAR) wParam;
            highSurrogate = 0;
            fb->char_input(codepoint);
        }
        break;
    }

    case WM_UNICHAR:
    {
        if (wParam == UNICODE_NOCHAR) {
            // WM_UNICHAR is not sent by Windows, but is sent by some third-party input method engine
            // Returning TRUE here announces support for this message
            return TRUE;
        }

        fb->char_input(uint32_t(wParam));
        break;
    }

    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_XBUTTONUP: case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
    {
        int button = 0;
        bool is_pressed= 0;
        switch(message) {
        case WM_LBUTTONDOWN: is_pressed = 1;
        case WM_LBUTTONUP: button = 1; break;
        case WM_RBUTTONDOWN: is_pressed = 1;
        case WM_RBUTTONUP: button = 2; break;
        case WM_MBUTTONDOWN: is_pressed = 1;
        case WM_MBUTTONUP: button = 3; break;
        default:
            button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? 5 : 5);
            if (message == WM_XBUTTONDOWN) is_pressed = 1;
        }
        fb->mouse_button(button, translate_mod(), is_pressed);
        break;
    }

    case WM_MOUSEWHEEL:
        fb->mouse_wheel(HIWORD(wParam) / WHEEL_DELTA, 0, translate_mod());
        break;

    case WM_MOUSEHWHEEL:
        fb->mouse_wheel(0, HIWORD(wParam) / WHEEL_DELTA, translate_mod());
        break;

    case WM_MOUSEMOVE:
        if (fb->mouse_inside == false) 
        {
            fb->mouse_inside = true;
            TRACKMOUSEEVENT tme;
            ZeroMemory(&tme, sizeof(tme));
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);
        }
        {
            float scale_x, scale_y;
            fb->get_monitor_scale(scale_x, scale_y);
            int x = int(LOWORD(lParam)/scale_x), y = int(HIWORD(lParam)/scale_y);
            fb->mouse_move(x, y);
        }
        break;

    case WM_MOUSELEAVE: fb->mouse_inside = false; break;

    case WM_SIZE:
    {
        float scale_x, scale_y;
        if (wParam == SIZE_MINIMIZED) return res;
        fb->get_monitor_scale(scale_x, scale_y);
        fb->window_width= LOWORD(lParam), fb->window_height= HIWORD(lParam);
        fb->defaultViewport();
        fb->resize(LOWORD(lParam), HIWORD(lParam));
        BitBlt(fb->hdc, 0, 0, fb->window_width, fb->window_height, 0, 0, 0, BLACKNESS);
    }
    break;

    case WM_SETFOCUS: fb->is_active = true; fb->active(true); break;

    case WM_KILLFOCUS: fb->is_active = false; fb->active(false); break;

    default: res = DefWindowProc(hWnd, message, wParam, lParam); break;
    }
    return res;
}


void CFBWindow::loadFont(wchar_t const *n, int s1, int s2)
{return;
    int y= 100;
    for (int h=s1; h<=s2; h++)
    {
        HFONT f= CreateFont(-h, 0, 0, 0, 0, false, false, false, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, n);
        SelectObject(hdc, f);
        //Sets the coordinates for the rectangle in which the text is to be formatted.
        RECT rect; SetRect(&rect, 0,y,700,y+100);
        SetTextColor(hdc, RGB(0,0,0));
        wchar_t t[97]; for (int i=' '; i<=128; i++) t[i-' ']= i;  t[96]= 0;
        y+= DrawText(hdc, t, -1, &rect, DT_NOCLIP);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
THPList<CFBWindow*> CFBWindow::windows;
void CFBWindow::run()
{
    int64_t frequency; QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
    while (windows.Size)
    {
        uint64_t now; QueryPerformanceCounter((LARGE_INTEGER*)&now); now= now * 1000 / frequency;
        uint64_t next= -1;
        for (int i=windows.Size; --i>=0;) 
        {
            if (windows[i]->nextFrame<=now) { windows[i]->update_display(); windows[i]->nextFrame= now+1000/windows[i]->fps; }
            if (windows[i]->nextFrame<next) next= windows[i]->nextFrame;
        }
        Sleep(DWORD(next-now));
    }
}


CFBWindow::CFBWindow(const wchar_t *title, int width, int height, unsigned flags, int fbw, int fbh): CSimpleFrameBuffer(fbw!=0 ? fbw:width, fbh!=0 ? fbh:height), execThread(this)
{
    windows.add(this);
    RECT rect = { 0 };
    int  x = 0, y = 0;

    load_functions();
    if (mfb_SetProcessDpiAwarenessContext != 0x0) {
        if(mfb_SetProcessDpiAwarenessContext(mfb_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) == false) {
            uint32_t error = GetLastError();
            if(error == ERROR_INVALID_PARAMETER) {
                error = NO_ERROR;
                if(mfb_SetProcessDpiAwarenessContext(mfb_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE) == false) {
                    error = GetLastError();
                }
            }
            if(error != NO_ERROR) {
                fprintf(stderr, "Error (SetProcessDpiAwarenessContext): %s\n", GetErrorMessage());
            }
        }
    }
    else if (mfb_SetProcessDpiAwareness != 0x0) {
        if(mfb_SetProcessDpiAwareness(mfb_PROCESS_PER_MONITOR_DPI_AWARE) != S_OK) {
            fprintf(stderr, "Error (SetProcessDpiAwareness): %s\n", GetErrorMessage());
        }
    }
    else if (mfb_SetProcessDPIAware != 0x0) {
        if(mfb_SetProcessDPIAware() == false) {
            fprintf(stderr, "Error (SetProcessDPIAware): %s\n", GetErrorMessage());
        }
    }

    s_window_style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
    if (flags & WF_FULLSCREEN) 
    {
        flags = WF_FULLSCREEN;  // Remove all other flags
        rect.right  = GetSystemMetrics(SM_CXSCREEN);
        rect.bottom = GetSystemMetrics(SM_CYSCREEN);
        s_window_style = WS_POPUP & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);

        DEVMODE settings = { 0 };
        EnumDisplaySettings(0, 0, &settings);
        settings.dmPelsWidth  = GetSystemMetrics(SM_CXSCREEN);
        settings.dmPelsHeight = GetSystemMetrics(SM_CYSCREEN);
        settings.dmBitsPerPel = 32;
        settings.dmFields     = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

        if (ChangeDisplaySettings(&settings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) flags = WF_FULLSCREEN_DESKTOP;
    }

    if (flags & WF_BORDERLESS) s_window_style = WS_POPUP;
    if (flags & WF_RESIZABLE) s_window_style |= WS_MAXIMIZEBOX | WS_SIZEBOX;

    if (flags & WF_FULLSCREEN_DESKTOP) 
    {
        s_window_style = WS_OVERLAPPEDWINDOW;
        width  = GetSystemMetrics(SM_CXFULLSCREEN);
        height = GetSystemMetrics(SM_CYFULLSCREEN);
        rect.right  = width;
        rect.bottom = height;
        AdjustWindowRect(&rect, s_window_style, 0);
        if (rect.left < 0) { width += rect.left * 2; rect.right += rect.left; rect.left = 0; }
        if (rect.bottom > (LONG) height) { height -= (rect.bottom - height); rect.bottom += (rect.bottom - height); rect.top = 0; }
    }
    else if (!(flags & WF_FULLSCREEN)) 
    {
        float scale_x, scale_y;
        get_monitor_scale(scale_x, scale_y);
        rect.right  = (LONG) (width  * scale_x);
        rect.bottom = (LONG) (height * scale_y);
        AdjustWindowRect(&rect, s_window_style, 0);
        rect.right  -= rect.left;
        rect.bottom -= rect.top;
        x = (GetSystemMetrics(SM_CXSCREEN) - rect.right) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - rect.bottom + rect.top) / 2;
    }

    wc.style         = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hCursor       = LoadCursor(0, IDC_ARROW);
    wc.lpszClassName = title;
    RegisterClass(&wc);

    //calc_dst_factor(width, height);

    window_width  = rect.right;
    window_height = rect.bottom;
    defaultViewport();

    hWnd = CreateWindowEx(0, title, title, s_window_style, x, y, window_width, window_height, 0, 0, 0, 0);if (!hWnd) return;

    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) this);

    if (flags & WF_ALWAYS_ON_TOP) SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(hWnd, SW_NORMAL);
    hdc= GetDC(hWnd);

    bitmapInfo.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biPlanes      = 1;
    bitmapInfo.bmiHeader.biBitCount    = 32;
    bitmapInfo.bmiHeader.biCompression = BI_BITFIELDS;
    bitmapInfo.bmiColors[0].rgbRed     = 0xff;
    bitmapInfo.bmiColors[1].rgbGreen   = 0xff;
    bitmapInfo.bmiColors[2].rgbBlue    = 0xff;
    execThread.resume();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFBWindow::update_display() 
{
    if (hWnd == 0x0) return false;
    if (closed) { destroy_window_data(); return false; }
    InvalidateRect(hWnd, 0x0, TRUE);
    SendMessage(hWnd, WM_PAINT, 0, 0);
    MSG msg; while (closed == false && PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))  { TranslateMessage(&msg); DispatchMessage(&msg); }
    frameCount++;
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFBWindow::update_events() 
{
    if (hWnd == 0x0) return false;
    if (closed) { destroy_window_data(); return false; }
    MSG msg; while (closed==false && PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))  { TranslateMessage(&msg); DispatchMessage(&msg); }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFBWindow::wait_sync() 
{
    if (closed) { destroy_window_data(); return false; }
    int f= frameCount; while (f!=frameCount) Sleep(1);
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CFBWindow::destroy_window_data() 
{
    if (hWnd!=0 && hdc!=0) { ReleaseDC(hWnd, hdc); DestroyWindow(hWnd); }
    hWnd = 0; hdc = 0;
    closed = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t translate_mod() 
{
    uint32_t mods = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) mods |= KB_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= KB_MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000) mods |= KB_MOD_ALT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mods |= KB_MOD_SUPER;
    if (GetKeyState(VK_CAPITAL) & 1) mods |= KB_MOD_CAPS_LOCK;
    if (GetKeyState(VK_NUMLOCK) & 1) mods |= KB_MOD_NUM_LOCK;
    return mods;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//void CFBWindow::set_viewport(int offset_x, int offset_y, int width, int height) 
//{
//    //float scale_x= scale, scale_y= scale;
//    //if (scale==-1) get_monitor_scale(scale_x, scale_y);
//    dst_offset_x = (uint32_t) (offset_x ); //* scale_x);
//    dst_offset_y = (uint32_t) (offset_y ); //* scale_y);
//    dst_width    = (uint32_t) (width    ); //* scale_x);
//    dst_height   = (uint32_t) (height   ); //* scale_y);
//    BitBlt(hdc, 0, 0, window_width, window_height, 0, 0, 0, BLACKNESS);
//}


unsigned long long const CSimpleFrameBuffer::font8[]= // This is the 256 character, 6*8 font data. Each long long represents 1 character, with 1 bit per pixel and 8 bits per line
{ 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000000000000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0101000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 
0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x80C0E1F1E1C08000ULL, 0xF1F1F1F1F1F1F100ULL, 0x0000000000515100ULL, 0x0000000000515100ULL, 
0x0000000000000000ULL, 0x4040404040004000ULL, 0xA0A0A00000000000ULL, 0xA0A0F1A0F1A0A000ULL, 0x40E150E041F04000ULL, 0x3031804020918100ULL, 0x2050502051906100ULL, 0x4040400000000000ULL, 0x8040202020408000ULL, 0x2040808080402000ULL, 0x00A040F140A00000ULL, 0x004040F140400000ULL, 0x0000000060604020ULL, 0x000000F100000000ULL, 0x0000000000606000ULL, 0x0001804020100000ULL, 
0xE01191513111E000ULL, 0x406040404040E000ULL, 0xE01101C02010F100ULL, 0xE01101E00111E000ULL, 0x80C0A090F1808000ULL, 0xF110F0010111E000ULL, 0xC02010F01111E000ULL, 0xF101804020202000ULL, 0xE01111E01111E000ULL, 0xE01111E101806000ULL, 0x0060600060600000ULL, 0x0060600060604020ULL, 0x8040201020408000ULL, 0x0000F100F1000000ULL, 0x1020408040201000ULL, 0xE011018040004000ULL, 
0xE01151D15010E100ULL, 0xE01111F111111100ULL, 0xF01111F01111F000ULL, 0xE01110101011E000ULL, 0x7090111111907000ULL, 0xF11010F01010F100ULL, 0xF11010F010101000ULL, 0xE01110109111E100ULL, 0x111111F111111100ULL, 0xE04040404040E000ULL, 0x010101011111E000ULL, 0x1190503050901100ULL, 0x101010101010F100ULL, 0x11B1515111111100ULL, 0x1111315191111100ULL, 0xE01111111111E000ULL, 
0xF01111F010101000ULL, 0xE011111151906100ULL, 0xF01111F050901100ULL, 0xE01110E00111E000ULL, 0xF140404040404000ULL, 0x111111111111E000ULL, 0x111111A0A0404000ULL, 0x1111115151B11100ULL, 0x1111A040A0111100ULL, 0x1111A04040404000ULL, 0xF10180402010F100ULL, 0xE02020202020E000ULL, 0x0010204080010000ULL, 0xE08080808080E000ULL, 0x40A0110000000000ULL, 0x000000000000F100ULL, 
0x2020400000000000ULL, 0x0000E001E111E100ULL, 0x1010F0111111F000ULL, 0x0000E1101010E100ULL, 0x0101E1111111E100ULL, 0x0000E011F110E000ULL, 0x40A0207020202000ULL, 0x0000E01111E101E0ULL, 0x1010F01111111100ULL, 0x400060404040E000ULL, 0x8000C08080809060ULL, 0x1010905030509000ULL, 0x604040404040E000ULL, 0x0000B05151511100ULL, 0x0000F01111111100ULL, 0x0000E0111111E000ULL, 
0x0000F01111F01010ULL, 0x0000E11111E10101ULL, 0x0000D13010101000ULL, 0x0000E110E001F000ULL, 0x2020702020A04000ULL, 0x000011111111E100ULL, 0x0000111111A04000ULL, 0x000011115151A000ULL, 0x000011A040A01100ULL, 0x0000111111E101E0ULL, 0x0000F1804020F100ULL, 0xC02020102020C000ULL, 0x4040404040404000ULL, 0x6080800180806000ULL, 0x0000205180000000ULL, 0x51A051A051A05100ULL, 
0x000180406090F100ULL, 0xF10011A040A01100ULL, 0x00F111A0A0400000ULL, 0xC140404050604000ULL, 0x8041404040502000ULL, 0xF12140804021F100ULL, 0x3070F0F1F0703000ULL, 0x0000F1A0A0A0A000ULL, 0x204080E11111E000ULL, 0x01804020F100F100ULL, 0x10204080F100F100ULL, 0x0080F140F1200000ULL, 0x0000006190906100ULL, 0x004080F180400000ULL, 0x004020F120400000ULL, 0x4040404051E04000ULL, 
0x40E0514040404000ULL, 0x0000215180808000ULL, 0x402040E090906000ULL, 0x0000E010F010E000ULL, 0x0000A05141410101ULL, 0x609090F090906000ULL, 0x0010102040A01100ULL, 0x00C02121E0202010ULL, 0x0000E19090906000ULL, 0x0000E15040418000ULL, 0x000090115151A000ULL, 0x000040A011F10000ULL, 0xF1A0A0A0A0A0A000ULL, 0xE011111111A0B100ULL, 0x0000E0E0E0000000ULL, 0x0000A05151A00000ULL, 
0xC02170207021C000ULL, 0x4000404040404000ULL, 0x0040E15050E14000ULL, 0xC02120702020F100ULL, 0x11E0111111E01100ULL, 0x1111A0F140F14000ULL, 0x4040400040404000ULL, 0xC020E011E0806000ULL, 0xA000000000000000ULL, 0xE01171317111E000ULL, 0x6080E09060F00000ULL, 0x0041A050A0410000ULL, 0x000000F080000000ULL, 0x000000F000000000ULL, 0xE0117171B111E000ULL, 0xF100000000000000ULL, 
0xE0A0E00000000000ULL, 0x004040F14040F100ULL, 0xE080E020E0000000ULL, 0xE080E080E0000000ULL, 0x8040000000000000ULL, 0x0000009090907110ULL, 0xE171716141416100ULL, 0x0000006060000000ULL, 0x0000000000408060ULL, 0x604040E000000000ULL, 0xE01111E000F10000ULL, 0x0050A041A0500000ULL, 0x1090502051C10100ULL, 0x109050A111808100ULL, 0x3021B06071C10100ULL, 0x400040201011E000ULL, 
0x2040E011F1111100ULL, 0x8040E011F1111100ULL, 0x40A0E011F1111100ULL, 0xA050E011F1111100ULL, 0xA000E011F1111100ULL, 0xE0A0E011F1111100ULL, 0xA15050F15050D100ULL, 0xE011101011E08060ULL, 0x2040F110F010F100ULL, 0x8040F110F010F100ULL, 0x40A0F110F010F100ULL, 0xA000F110F010F100ULL, 0x2040E0404040E000ULL, 0x8040E0404040E000ULL, 0x40A0E0404040E000ULL, 0xA000E0404040E000ULL, 
0x60A0217121A06000ULL, 0x41A0113151911100ULL, 0x2040E0111111E000ULL, 0x8040E0111111E000ULL, 0x40A0E0111111E000ULL, 0xA050E0111111E000ULL, 0xA000E0111111E000ULL, 0x0011A040A0110000ULL, 0x01E0915131E01000ULL, 0x204011111111E000ULL, 0x804011111111E000ULL, 0x40A000111111E000ULL, 0xA00011111111E000ULL, 0x804011A040404000ULL, 0x7020E021E0207000ULL, 0xE011F01111F01010ULL, 
0x2040E001E111E100ULL, 0x8040E001E111E100ULL, 0x40A0E001E111E100ULL, 0xA050E001E111E100ULL, 0xA000E001E111E100ULL, 0xE0A0E001E111E100ULL, 0x0000B141F150F100ULL, 0x0000E11010E18060ULL, 0x2040E011F110E000ULL, 0x8040E011F110E000ULL, 0x40A0E011F110E000ULL, 0xA000E011F110E000ULL, 0x204000604040E000ULL, 0x804000604040E000ULL, 0x40A000604040E000ULL, 0xA00000604040E000ULL, 
0x80C180E090906000ULL, 0x41A000F011111100ULL, 0x204000E01111E000ULL, 0x804000E01111E000ULL, 0x40A000E01111E000ULL, 0x41A000E01111E000ULL, 0xA00000E01111E000ULL, 0x004000F100400000ULL, 0x000061905121D000ULL, 0x204000111111E100ULL, 0x804000111111E100ULL, 0x40A000111111E100ULL, 0xA00000111111E100ULL, 0x8040001111E101E0ULL, 0x0010709090701010ULL, 0xA000001111E101E0ULL };

static char const s16[] = "0123456789ABCDEF"; 
template <typename T>
static int base(T n, char *b, int base)
{
    int neg= 0; if (n<0) { *b++= '-'; n= 0-n; neg= 1; }
    int size= 0; do { b[size++]= s16[n%base]; n/=base; } while (n!=0);
    for (int i=0; i<size/2; i++) { char t= b[i]; b[i]= b[size-i-1]; b[size-i-1]= t; };
    return size+neg;
}
static char *NumberToHex(char *b, uint8_t n) { *b++= s16[n>>4]; *b++= s16[n&15]; return b; }
static char *NumberToHex(char *b, uint16_t n) { return NumberToHex(NumberToHex(b, (uint8_t)(n>>8)), (uint8_t)n); }
static char *NumberToHex(char *b, uint32_t n) { return NumberToHex(NumberToHex(b, (uint16_t)(n>>16)), (uint16_t)n); }
static char *NumberToHex(char *b, uint64_t n) { return NumberToHex(NumberToHex(b, (uint32_t)(n>>32)), (uint32_t)n); }
char *CSimpleFrameBuffer::print(char const *format, ...)
{
    va_list argp;
    va_start(argp, format);
    char *t= print(format, argp);
    va_end(argp);
    return t;
}
char *hpsprintf2(char *buf, int bufsize, char const *format, va_list argp)
{
    char *printbuf= buf;
    while (*format != L'\0' && bufsize > 0)
    {
        if (*format != L'%') { *buf = *format; buf++; format++; bufsize--; continue; }
        if (format[1] == L'%') { *buf = '%'; buf++; format += 2; bufsize--; continue; }
        if (format[1] == 'c') { char c= (char)va_arg(argp, int); *buf= c; buf++; format += 2; bufsize--; continue; }
        if (format[1] == 'd') { int i = va_arg(argp, int); int l= base(i, buf, 10); buf+= l; bufsize-= l; format += 2; continue; }
        if (format[1] == 'u') { unsigned int i = va_arg(argp, unsigned int); int l= base(i, buf, 10); buf+= l; bufsize-= l; format += 2; continue; }
        if (format[1] == 'x') { unsigned int i = va_arg(argp, unsigned int); int l= base(i, buf, 16); buf+= l; bufsize-= l; format += 2; continue; }
        if (format[1] >= '0' && format[1] <= '8' && format[2] >= 'x') { unsigned int i = va_arg(argp, unsigned int); i &= ~((-1) << (4 * (format[1] - '0'))); int l= base(i, buf, 16); buf+= l; bufsize-= l; format += 3; continue; }
        if (format[1] == '0' && format[2] > '0' && format[2] <= '8' && format[3] >= 'x')
        {
            unsigned int i = va_arg(argp, unsigned int); i &= ~((-1) << (4 * (format[2] - '0'))); 
            int l= base(i, buf, 16); 
            int sze = format[2]-'0'-l; bufsize-= sze+l;
            while (--sze>=0) *buf++= '0';
            l= base(i, buf, 16);
            buf+= l; format += 4; continue; 
        }
        if (format[1] == 's') { char *b = va_arg(argp, char*); if (b!=NULL) { while (*b && --bufsize >= 0) *buf++ = *b++; } format += 2; continue; }
        if (format[1] == 'D')
        {
            unsigned char *b = va_arg(argp, unsigned char*); int size = va_arg(argp, int);
            if (b==NULL) { strcpy_s(buf, 100, "NULL"); bufsize-= 4, buf+= 4; format += 2; continue; }
            while (--size >= 0 && bufsize > 2) { buf= NumberToHex(buf, *b++); bufsize -= 2; }
            format += 2; continue;
        }
        if (format[1] == 'b')
        {
            format+= 2; int bytes= 1; if (*format==L'1') bytes= 1, format++; else if (*format==L'2') bytes= 2, format++; else if (*format==L'4') bytes= 4, format++; else if (*format==L'8') bytes= 8, format++;
            unsigned char *b = va_arg(argp, uint8_t*); int size = va_arg(argp, int);
            if (b==NULL) { *buf++= 'N'; *buf++= 'U'; *buf++= 'L'; *buf++= 'L'; bufsize-= 4; continue; }
            while (--size>=0 && bufsize>2*bytes+1) 
            { 
                if (bytes==1) buf= NumberToHex(buf, *b++); 
                else if (bytes==2) { uint16_t d; memcpy(&d, b, bytes); b+= bytes; buf= NumberToHex(buf, d); }
                else if (bytes==4) { uint32_t d; memcpy(&d, b, bytes); b+= bytes; buf= NumberToHex(buf, d); }
                else if (bytes==8) { uint64_t d; memcpy(&d, b, bytes); b+= bytes; buf= NumberToHex(buf, d); }
                *buf++= L' ';
                bufsize -= 2*bytes+1; }
            continue;
        }
        *buf = *format; buf++, format++; bufsize--; continue;
    }
    *buf = L'\0';
    va_end(argp);
    return printbuf;
}
char *hpsprintf(char *buf, int bufsize, char const *format, ...)
{
    va_list argp;
    va_start(argp, format);
    char *t= hpsprintf2(buf, bufsize, format, argp);
    va_end(argp);
    return t;
}
char *CSimpleFrameBuffer::print(char const *format, va_list argp)
{
    if (printbuf==NULL) printbuf= (char*)malloc(1024);
    if (printbuf==NULL) return NULL;
    return hpsprintf2(printbuf, 1024-10, format, argp);
}

void CSimpleFrameBuffer::log2(char const *s)
{
    int cols= w/6+10;
    int rows= h/8;
    if (logstrs==NULL) { logstrs= (char*)malloc(rows*cols); memset(logstrs, 0, rows*cols); }
    if (logstrs==NULL) return;
    memmove(logstrs+cols, logstrs, (rows-1)*cols);
    memcpy(logstrs, s, cols-1); logstrs[cols-1]= 0;
    for (int l= rows; --l>=0;) text(0, l*8, w, logstrs+(l*cols));
}
void CSimpleFrameBuffer::log(char const *format, ...)
{
    va_list argp; va_start(argp, format);
    log2(print(format, argp));
    va_end(argp);
}
void CSimpleFrameBuffer::line(int x1, int y1, int x2, int y2, TSFBColor color)
{
    if (x2<x1) { int t= x1; x1= x2; x2= t; t= y1; y1= y2; y2= t; } // always draw form left to right
    int DX= x2-x1, DY= y2-y1, pos= 1; // calculate delta x and y
    if (DY<0) pos=-1, DY= -DY;

    if (DX>=DY) // shallow, non-decreasing Y (e.g., 4 o'clock)
    { int er= -DX/2;
    while (x1 <= x2) { pixel(x1, y1, color); x1++; er+= DY; if (er<0) continue; er-= DX; y1+= pos; } 
    } else { // mostly vertical lines...
        int er= -DY/2;
        if (pos==1)
            while (y1<=y2) { pixel(x1, y1, color); y1++; er+= DX; if (er<0) continue; er-= DY; x1++; } 
        else
            while (y1>=y2) { pixel(x1, y1, color); y1--; er+= DX; if (er<0) continue; er-= DY; x1++; } 
    }
}
