typedef unsigned int DWORD;
typedef int BOOL;
typedef void* HWND;
typedef void* HDC;
typedef void* HMENU;
typedef void* HINSTANCE;
typedef unsigned int UINT;
typedef unsigned long long WPARAM;
typedef long long LPARAM;
typedef long long LRESULT;

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} RECT;

typedef struct {
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    char rgbReserved[32];
} PAINTSTRUCT;

typedef struct {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    int pt_x;
    int pt_y;
} MSG;

__declspec(dllimport) void ExitProcess(UINT uExitCode);
__declspec(dllimport) HWND CreateWindowExA(DWORD dwExStyle, const char* lpClassName, const char* lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, void* lpParam);
__declspec(dllimport) BOOL ShowWindow(HWND hWnd, int nCmdShow);
__declspec(dllimport) BOOL UpdateWindow(HWND hWnd);
__declspec(dllimport) HDC BeginPaint(HWND hWnd, PAINTSTRUCT* lpPaint);
__declspec(dllimport) BOOL EndPaint(HWND hWnd, const PAINTSTRUCT* lpPaint);
__declspec(dllimport) BOOL TextOutA(HDC hdc, int x, int y, const char* lpString, int c);
__declspec(dllimport) int MessageBoxA(HWND hWnd, const char* lpText, const char* lpCaption, UINT uType);
__declspec(dllimport) void PostQuitMessage(int nExitCode);

void mainCRTStartup(void) {
    MessageBoxA((HWND)0, "Welcome to ZweiOS Native Win32 Desktop GUI!", "ZweiOS Window Manager", 0);

    HWND hWnd = CreateWindowExA(0, "ZweiWindowClass", "Native Win32 App - ZweiOS", 0, 150, 120, 480, 320, (HWND)0, (HMENU)0, (HINSTANCE)0, (void*)0);
    if (hWnd) {
        ShowWindow(hWnd, 1);
        UpdateWindow(hWnd);

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        if (hdc) {
            TextOutA(hdc, 20, 20, "ZweiOS GUI Subsystem Active (USER32 / GDI32)", 44);
            TextOutA(hdc, 20, 50, "Direct Win32 Window Rendering on Bare Metal", 43);
            TextOutA(hdc, 20, 80, "Author: made by toiabzahoor", 27);
            EndPaint(hWnd, &ps);
        }
    }

    ExitProcess(0);
}
