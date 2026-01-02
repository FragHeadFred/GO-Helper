/* GO-Helper Application 
   Version: 0.129.2026.01.02
   Update: Updated memory footprint reference to 2.6MB.
   Features: 
   - Battery / Charge Status: Tracking percentage and AC/DC power state.
   - 2.6MB Memory Usage: Optimized Win32 footprint for background operation.
   - Close Button: Rounded, Black BG, Red Border, White X for quick UI dismissal.
   - Legion R Listener: Raw HID implementation monitoring Byte 18, Bit 6.
   - Admin Check: Auto-elevates process to ensure successful HID, WMI, and Registry access.
   - WMI SKU/Model Detection: Precise hardware identification querying BIOS strings.
   - Thermal Mode Control: Direct WMI method calls for Quiet, Balanced, and Performance.
   - Controller-to-Mouse Emulation: XInput polling for RS (Move), RB (Left Click), and RT (Right Click).
   - CPU Temp Monitoring: Live polling of ACPI thermal zones in Celsius and Fahrenheit.
   - Global Hotkey: Ctrl + G active for keyboard-based summoning.
   - Tray Menu: Includes Mute, Gamebar Fix, Start with Windows, and Exit.
*/

#define APP_VERSION L"0.129.2026.01.02" // Application version for UI and logic
#define _WIN32_WINNT 0x0601             // Target Windows 7 and above
#define _WIN32_DCOM                     // Enable Distributed COM for WMI connectivity

#include <windows.h>                    // Main Windows API header
#include <comdef.h>                     // COM standard types
#include <WbemIdl.h>                    // WMI interface definitions
#include <string>                       // C++ Standard string library
#include <shellapi.h>                   // Shell functionality (Tray/Elevation)
#include <vector>                       // Standard vector container
#include <thread>                       // Multi-threading support for HID polling
#include <Xinput.h>                     // Xbox controller input API
#include <commctrl.h>                   // Windows Common Controls
#include <uxtheme.h>                    // Windows Themeing API
#include <dwmapi.h>                     // Desktop Window Manager (UI Effects)
#include "resource.h"                   // Resource IDs for icons and strings

// --- LIBRARIES ---
#pragma comment(lib, "wbemuuid.lib")    // Link WMI library
#pragma comment(lib, "user32.lib")      // User interface link
#pragma comment(lib, "shell32.lib")     // Shell operations link
#pragma comment(lib, "gdi32.lib")       // Graphics link
#pragma comment(lib, "ole32.lib")       // OLE/COM link
#pragma comment(lib, "oleaut32.lib")    // Automation link
#pragma comment(lib, "advapi32.lib")    // Registry/Security link
#pragma comment(lib, "Xinput.lib")      // Controller input link
#pragma comment(lib, "comctl32.lib")    // Common controls link
#pragma comment(lib, "uxtheme.lib")     // UI theme link
#pragma comment(lib, "dwmapi.lib")      // DWM link

// --- CONSTANTS ---
#define BTN_QUIET 101                   // Quiet Mode Button ID
#define BTN_BALANCED 102                // Balanced Mode Button ID
#define BTN_PERFORMANCE 103             // Performance Mode Button ID
#define BTN_MOUSE_TOGGLE 104            // Mouse Emulation Toggle ID
#define SLIDER_SENSE 105                // Sensitivity Slider ID
#define BTN_CLOSE 107                   // Close (Hide) Button ID
#define WM_TRAYICON (WM_USER + 1)       // Custom Tray Message ID
#define ID_TRAY_EXIT 201                // Exit Menu ID
#define ID_TRAY_TOGGLE 202              // Show/Hide Menu ID
#define ID_TRAY_DISABLE_GB 203          // Gamebar Fix ID
#define ID_TRAY_MUTE_APP 204            // Mute App ID
#define ID_TRAY_START_WITH_WIN 205      // Auto-start Toggle ID

// --- UI THEME COLORS ---
#define CLR_BACK      RGB(20, 20, 20)   // Background Black
#define CLR_CARD      RGB(45, 45, 45)   // Component Gray
#define CLR_TEXT      RGB(240, 240, 240)// Off-white Text
#define CLR_QUIET     RGB(0, 102, 204)  // Lenovo Quiet Blue
#define CLR_BAL       RGB(255, 255, 255)// Lenovo Balanced White
#define CLR_PERF      RGB(178, 34, 34)  // Lenovo Performance Red
#define CLR_RED       RGB(255, 0, 0)    // Border/Slider Red
#define CLR_AURA      RGB(40, 40, 40)   // UI Aura Gray
#define CLR_ACCENT    RGB(0, 180, 90)   // Active Success Green
#define CLR_VERSION   RGB(160, 160, 160)// Muted Version Gray
#define CLR_DISABLED  RGB(80, 80, 80)   // Disabled Element Gray

// --- GLOBALS ---
HWND g_hwnd = NULL;                     // Global Window Handle
HHOOK g_hHook = NULL;                   // Global Keyboard Hook Handle
NOTIFYICONDATAW g_nid = { 0 };          // Tray Icon Data Structure
HICON g_hMainIcon = NULL;               // Application Icon Handle
HBRUSH g_hBackBrush = NULL;             // Main Background Brush
bool g_appMuted = true;                 // Mute audio by default
bool g_mouseEnabled = true;             // Controller-to-mouse active state
int g_currentSenseVal = 5;              // Mouse sensitivity slider pos
float g_mouseSensitivity = 5 * 0.0005f; // Calculated movement speed

const int WIN_WIDTH = 350;              // App Width
const int WIN_HEIGHT = 255;             // App Height

// --- LEGION TRIGGER CLASS (HID LISTENER) ---
// Directly monitors hardware for the Legion R button press event
class LegionTrigger {
public:
    static void Start(HWND targetWindow) {
        // Run polling in a detached worker thread for Byte 18 monitor
        std::thread([targetWindow]() {
            MonitorController(targetWindow);
        }).detach();
    }

private:
    static void MonitorController(HWND targetWindow) {
        // Raw hardware path for the Legion Go Controller interface
        std::string devicePath = "\\\\?\\hid#vid_17ef&pid_61eb&mi_02#8&ece5261&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}";
        // Open file handle to the HID device for raw bytes reading
        HANDLE hDevice = CreateFileA(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

        if (hDevice == INVALID_HANDLE_VALUE) {
            OutputDebugStringA("Failed to open HID device.\n");
            return;
        }

        unsigned char buffer[64];
        DWORD bytesRead = 0;
        bool wasPressed = false;

        while (true) {
            // Read hardware input report
            if (ReadFile(hDevice, buffer, 64, &bytesRead, NULL)) {
                // Byte 18, Bit 6 contains the Legion R state (0x40)
                bool isPressed = (buffer[18] & 0x40) == 0x40;
                if (isPressed && !wasPressed) {
                    // Command UI thread to toggle visibility
                    PostMessage(targetWindow, WM_COMMAND, ID_TRAY_TOGGLE, 0);
                    wasPressed = true;
                } else if (!isPressed) {
                    wasPressed = false;
                }
            } else break; // Device disconnected or error
        }
        CloseHandle(hDevice); // Handle cleanup
    }
};

// --- LOGIC: ELEVATION ---
// Check if the application is running with admin tokens
bool IsRunAsAdmin() {
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    // Build Administrator group SID
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup)) {
        CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin);
        FreeSid(pAdministratorsGroup);
    }
    return fIsRunAsAdmin;
}

// Relaunch app with 'runas' verb to trigger UAC
void ElevateNow() {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas"; sei.lpFile = szPath; sei.hwnd = NULL; sei.nShow = SW_NORMAL;
        if (!ShellExecuteExW(&sei)) return; else exit(0); // Exit non-admin instance
    }
}

// --- LOGIC: AUTO-START ---
// Check registry if app is in the 'Run' key
bool IsAutoStartEnabled() {
    HKEY hKey; bool enabled = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"GO-Helper", NULL, NULL, NULL, NULL) == ERROR_SUCCESS) enabled = true;
        RegCloseKey(hKey);
    }
    return enabled;
}

// Add or remove app from Windows startup via Registry
void SetAutoStart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t path[MAX_PATH]; GetModuleFileNameW(NULL, path, MAX_PATH);
            RegSetValueExW(hKey, L"GO-Helper", 0, REG_SZ, (const BYTE*)path, (DWORD)((wcslen(path) + 1) * sizeof(wchar_t)));
        } else RegDeleteValueW(hKey, L"GO-Helper");
        RegCloseKey(hKey);
    }
}

// --- LOGIC: SYSTEM POLLS ---
// Poll MSAcpi_ThermalZoneTemperature via WMI for CPU thermals
std::wstring GetCPUTempString() {
    long tempDK = 0; HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return L"CPU: --";
    IWbemLocator* pLoc = NULL;
    if (SUCCEEDED(CoCreateInstance(__uuidof(WbemLocator), 0, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID*)&pLoc))) {
        IWbemServices* pSvc = NULL;
        if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc))) {
            CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
            IEnumWbemClassObject* pEnum = NULL;
            if (SUCCEEDED(pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"), WBEM_FLAG_FORWARD_ONLY, NULL, &pEnum))) {
                IWbemClassObject* pObj = NULL; ULONG uRet = 0;
                if (SUCCEEDED(pEnum->Next(WBEM_INFINITE, 1, &pObj, &uRet)) && uRet > 0) {
                    VARIANT vt; if (SUCCEEDED(pObj->Get(L"CurrentTemperature", 0, &vt, 0, 0))) { tempDK = vt.lVal; VariantClear(&vt); }
                    pObj->Release();
                }
                pEnum->Release();
            }
            pSvc->Release();
        }
        pLoc->Release();
    }
    CoUninitialize();
    if (tempDK <= 0) return L"CPU: --";
    // Convert 10ths of Kelvin to C and F
    double celsius = (tempDK / 10.0) - 273.15;
    double fahrenheit = (celsius * 9.0 / 5.0) + 32.0;
    wchar_t buf[64]; swprintf_s(buf, L"CPU: %.0f°C / %.0f°F", celsius, fahrenheit);
    return std::wstring(buf);
}

// Kill GameBar recording and app capturing popups via Registry
void DisableGameBarRegistry() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD dwVal = 0; RegSetValueExW(hKey, L"AppCaptureEnabled", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(dwVal)); RegCloseKey(hKey);
    }
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"System\\GameConfigStore", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD dwVal = 0; RegSetValueExW(hKey, L"GameDVR_Enabled", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(dwVal)); RegCloseKey(hKey);
    }
    MessageBoxW(NULL, L"Game Bar features disabled. Restart recommended.", L"GO-Helper", MB_OK | MB_ICONINFORMATION);
}

// Get Lenovo Smart Fan mode from WMI
std::wstring GetThermalModeString() {
    int mode = 0; HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return L"Unknown";
    IWbemLocator* pLoc = NULL;
    if (SUCCEEDED(CoCreateInstance(__uuidof(WbemLocator), 0, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID*)&pLoc))) {
        IWbemServices* pSvc = NULL;
        if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc))) {
            CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
            _bstr_t className(L"LENOVO_GAMEZONE_DATA"); IEnumWbemClassObject* pEnum = NULL;
            if (SUCCEEDED(pSvc->CreateInstanceEnum(className, 0, NULL, &pEnum))) {
                IWbemClassObject* pInst = NULL; ULONG uRet = 0;
                if (pEnum->Next(WBEM_INFINITE, 1, &pInst, &uRet) == WBEM_S_NO_ERROR) {
                    VARIANT vtPath;
                    if (SUCCEEDED(pInst->Get(L"__PATH", 0, &vtPath, NULL, NULL))) {
                        IWbemClassObject* pOut = NULL;
                        // Call Lenovo specific fan mode method
                        if (SUCCEEDED(pSvc->ExecMethod(vtPath.bstrVal, _bstr_t(L"GetSmartFanMode"), 0, NULL, NULL, &pOut, NULL))) {
                            VARIANT vtRes; if (SUCCEEDED(pOut->Get(L"Data", 0, &vtRes, NULL, NULL))) { mode = vtRes.lVal; VariantClear(&vtRes); }
                            pOut->Release();
                        }
                        VariantClear(&vtPath);
                    }
                    pInst->Release();
                }
                pEnum->Release();
            }
            pSvc->Release();
        }
        pLoc->Release();
    }
    CoUninitialize();
    if (mode == 1) return L"Quiet"; if (mode == 2) return L"Balanced"; if (mode == 3) return L"Performance";
    return L"Unknown";
}

// Fetch battery percentage and status from System Power
std::wstring GetBatteryStatusString() {
    SYSTEM_POWER_STATUS sps; if (!GetSystemPowerStatus(&sps)) return L"Battery: Unknown";
    std::wstring status = L"Battery: ";
    status += (sps.ACLineStatus == 1) ? L"Plugged In" : L"Discharging";
    status += L" @ " + std::to_wstring((int)sps.BatteryLifePercent) + L"%";
    return status;
}

// Write new Smart Fan value to BIOS through WMI
bool SetThermalMode(int value) {
    bool success = false; HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
    IWbemLocator* pLoc = NULL;
    if (SUCCEEDED(CoCreateInstance(__uuidof(WbemLocator), 0, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID*)&pLoc))) {
        IWbemServices* pSvc = NULL;
        if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc))) {
            CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
            _bstr_t className(L"LENOVO_GAMEZONE_DATA"); IWbemClassObject* pClass = NULL;
            if (SUCCEEDED(pSvc->GetObject(className, 0, NULL, &pClass, NULL))) {
                IWbemClassObject* pInDef = NULL;
                if (SUCCEEDED(pClass->GetMethod(_bstr_t(L"SetSmartFanMode"), 0, &pInDef, NULL)) && pInDef) {
                    IWbemClassObject* pInInst = NULL; pInDef->SpawnInstance(0, &pInInst);
                    VARIANT v; v.vt = VT_I4; v.lVal = value; pInInst->Put(L"Data", 0, &v, 0);
                    IEnumWbemClassObject* pEnum = NULL;
                    if (SUCCEEDED(pSvc->CreateInstanceEnum(className, 0, NULL, &pEnum))) {
                        IWbemClassObject* pInst = NULL; ULONG uRet = 0;
                        if (pEnum->Next(WBEM_INFINITE, 1, &pInst, &uRet) == WBEM_S_NO_ERROR) {
                            VARIANT path;
                            if (SUCCEEDED(pInst->Get(L"__PATH", 0, &path, NULL, NULL))) {
                                // Execute the mode set command
                                if (SUCCEEDED(pSvc->ExecMethod(path.bstrVal, _bstr_t(L"SetSmartFanMode"), 0, NULL, pInInst, NULL, NULL))) success = true;
                                VariantClear(&path);
                            }
                            pInst->Release();
                        }
                        pEnum->Release();
                    }
                    pInInst->Release(); pInDef->Release();
                }
                pClass->Release();
            }
            pSvc->Release();
        }
        pLoc->Release();
    }
    CoUninitialize();
    if (success && !g_appMuted) Beep(800 + (value * 100), 100);
    return success;
}

// Fetch model and SKU for UI title display
std::wstring GetSystemSKU() {
    std::wstring biosModel = L""; std::wstring biosSKU = L"";
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return L"";
    IWbemLocator* pLoc = NULL;
    if (SUCCEEDED(CoCreateInstance(__uuidof(WbemLocator), 0, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID*)&pLoc))) {
        IWbemServices* pSvc = NULL;
        if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc))) {
            IEnumWbemClassObject* pEnum = NULL;
            if (SUCCEEDED(pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT Name, SKUNumber FROM Win32_ComputerSystemProduct"), WBEM_FLAG_FORWARD_ONLY, NULL, &pEnum))) {
                IWbemClassObject* pObj = NULL; ULONG uRet = 0;
                if (SUCCEEDED(pEnum->Next(WBEM_INFINITE, 1, &pObj, &uRet)) && uRet > 0) {
                    VARIANT vt; if (SUCCEEDED(pObj->Get(L"Name", 0, &vt, 0, 0))) { biosModel = vt.bstrVal; VariantClear(&vt); }
                    if (SUCCEEDED(pObj->Get(L"SKUNumber", 0, &vt, 0, 0))) { biosSKU = vt.bstrVal; VariantClear(&vt); }
                    pObj->Release();
                }
                pEnum->Release();
            }
            pSvc->Release();
        }
        pLoc->Release();
    }
    CoUninitialize();
    if (biosModel.empty() || biosModel == L"Default string") biosModel = L"Legion Go";
    return (biosSKU.empty() || biosSKU == L"Default string") ? biosModel : biosModel + L" (" + biosSKU + L")";
}

// --- UI UTILS & FOCUS ---
void RepositionToBottomRight(HWND hwnd) {
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
        int x = mi.rcWork.right - WIN_WIDTH - 20;
        int y = mi.rcWork.bottom - WIN_HEIGHT - 20;
        SetWindowPos(hwnd, HWND_TOPMOST, x, y, WIN_WIDTH, WIN_HEIGHT, SWP_SHOWWINDOW);
    }
}

void ToggleVisibility(HWND hwnd) {
    if (IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_HIDE);
    else {
        RepositionToBottomRight(hwnd);
        ShowWindow(hwnd, SW_SHOW); ShowWindow(hwnd, SW_RESTORE); 
        UpdateWindow(hwnd); SetForegroundWindow(hwnd); SetActiveWindow(hwnd); SetFocus(hwnd);
        DWORD dwCurThread = GetCurrentThreadId();
        DWORD dwFGThread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
        if (dwCurThread != dwFGThread) {
            AttachThreadInput(dwCurThread, dwFGThread, TRUE);
            SetForegroundWindow(hwnd);
            AttachThreadInput(dwCurThread, dwFGThread, FALSE);
        }
    }
}

// --- CONTROLLER MOUSE ---
void ProcessControllerMouse() {
    if (!g_mouseEnabled) return;
    XINPUT_STATE state;
    static bool rbPressed = false; static bool rtPressed = false;
    if (XInputGetState(0, &state) == ERROR_SUCCESS) {
        short rx = state.Gamepad.sThumbRX; short ry = state.Gamepad.sThumbRY;
        const int deadzone = 8000;
        int magX = (abs(rx) < deadzone) ? 0 : (rx > 0 ? rx - deadzone : rx + deadzone);
        int magY = (abs(ry) < deadzone) ? 0 : (ry > 0 ? ry - deadzone : ry + deadzone);
        if (magX != 0 || magY != 0) {
            INPUT move = { 0 }; move.type = INPUT_MOUSE; move.mi.dwFlags = MOUSEEVENTF_MOVE;
            move.mi.dx = static_cast<long>(magX * g_mouseSensitivity);
            move.mi.dy = static_cast<long>(-magY * g_mouseSensitivity);
            SendInput(1, &move, sizeof(INPUT));
        }
        bool rbNow = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
        if (rbNow != rbPressed) {
            INPUT click = { 0 }; click.type = INPUT_MOUSE;
            click.mi.dwFlags = rbNow ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
            SendInput(1, &click, sizeof(INPUT)); rbPressed = rbNow;
        }
        bool rtNow = (state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
        if (rtNow != rtPressed) {
            INPUT click = { 0 }; click.type = INPUT_MOUSE;
            click.mi.dwFlags = rtNow ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
            SendInput(1, &click, sizeof(INPUT)); rtPressed = rtNow;
        }
    }
}

// --- RENDERING ---
void DrawGButton(HDC hdc, RECT rc, LPCWSTR text, COLORREF color, bool pressed) {
    HBRUSH hBackBr = CreateSolidBrush(CLR_BACK); FillRect(hdc, &rc, hBackBr); DeleteObject(hBackBr);
    HBRUSH hBr = CreateSolidBrush(pressed ? color : CLR_CARD);
    HPEN hPen = CreatePen(PS_SOLID, 1, pressed ? RGB(200, 200, 200) : RGB(80, 80, 80));
    SelectObject(hdc, hPen); SelectObject(hdc, hBr);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 12, 12);
    SetTextColor(hdc, CLR_TEXT); SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hBr); DeleteObject(hPen);
}

LRESULT CALLBACK SliderSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_ERASEBKGND) return 1;
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        SelectObject(memDC, memBM);
        HBRUSH hBg = CreateSolidBrush(CLR_BACK); FillRect(memDC, &rc, hBg); DeleteObject(hBg);
        bool enabled = IsWindowEnabled(hWnd);
        HBRUSH hTrack = CreateSolidBrush(enabled ? CLR_RED : CLR_DISABLED);
        int trackHeight = 4; int centerV = (rc.bottom - rc.top) / 2;
        RECT chRc = { rc.left + 12, centerV - (trackHeight / 2), rc.right - 12, centerV + (trackHeight / 2) };
        FillRect(memDC, &chRc, hTrack); DeleteObject(hTrack);
        int curPos = (int)SendMessage(hWnd, TBM_GETPOS, 0, 0);
        float ratio = (float)(curPos - 1) / 49.0f;
        int thumbX = (int)(chRc.left + (ratio * (chRc.right - chRc.left)));
        if (enabled) {
            HBRUSH hAura = CreateSolidBrush(CLR_AURA); SelectObject(memDC, hAura); SelectObject(memDC, GetStockObject(NULL_PEN));
            Ellipse(memDC, thumbX - 9, centerV - 9, thumbX + 9, centerV + 9); DeleteObject(hAura);
        }
        HBRUSH hBall = CreateSolidBrush(enabled ? CLR_RED : CLR_DISABLED); SelectObject(memDC, hBall);
        Ellipse(memDC, thumbX - 7, centerV - 7, thumbX + 7, centerV + 7); DeleteObject(hBall);
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBM); DeleteDC(memDC);
        EndPaint(hWnd, &ps); return 0;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// --- WINDOW PROCEDURE ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HFONT hFontBold, hFontSmall; static std::wstring skuText; static HWND hSlider;
    static UINT_PTR refreshTimer = 0;

    switch (uMsg) {
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC: return (LRESULT)g_hBackBrush;

    case WM_CREATE:
    {
        g_hBackBrush = CreateSolidBrush(CLR_BACK);
        g_hMainIcon = (HICON)LoadImageW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, 32, 32, LR_SHARED);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hMainIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hMainIcon);

        hFontBold = CreateFontW(14, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
        hFontSmall = CreateFontW(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
        skuText = GetSystemSKU();

        g_nid.cbSize = sizeof(NOTIFYICONDATAW); g_nid.hWnd = hwnd; g_nid.uID = 1;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = g_hMainIcon; wcscpy_s(g_nid.szTip, L"GO-Helper");
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 20, 75, 100, 35, hwnd, (HMENU)BTN_QUIET, NULL, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 125, 75, 100, 35, hwnd, (HMENU)BTN_BALANCED, NULL, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 230, 75, 100, 35, hwnd, (HMENU)BTN_PERFORMANCE, NULL, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 20, 145, 100, 35, hwnd, (HMENU)BTN_MOUSE_TOGGLE, NULL, NULL);
        
        CreateWindowW(L"BUTTON", L"✕", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, WIN_WIDTH - 35, 10, 25, 25, hwnd, (HMENU)BTN_CLOSE, NULL, NULL);

        hSlider = CreateWindowW(TRACKBAR_CLASSW, L"", WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_NOTICKS, 135, 157, 206, 30, hwnd, (HMENU)SLIDER_SENSE, NULL, NULL);
        SetWindowTheme(hSlider, L"", L""); SetWindowSubclass(hSlider, SliderSubclassProc, 0, 0);
        SendMessage(hSlider, TBM_SETRANGE, TRUE, MAKELONG(1, 50));
        SendMessage(hSlider, TBM_SETPOS, TRUE, g_currentSenseVal);

        SetThermalMode(2); 
        LegionTrigger::Start(hwnd);

        refreshTimer = SetTimer(hwnd, 1, 3000, NULL);
        COLORREF auraColor = CLR_AURA; DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &auraColor, sizeof(auraColor));
        DWM_WINDOW_CORNER_PREFERENCE cp = DWMWCP_ROUND; DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cp, sizeof(cp));
    }
    break;

    case WM_TIMER: if (wParam == 1 && IsWindowVisible(hwnd)) InvalidateRect(hwnd, NULL, FALSE); break;

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        SelectObject(memDC, memBM);
        FillRect(memDC, &rc, g_hBackBrush);

        if (g_hMainIcon) DrawIconEx(memDC, 20, 15, g_hMainIcon, 18, 18, 0, NULL, DI_NORMAL);

        SelectObject(memDC, hFontBold); SetTextColor(memDC, CLR_TEXT); SetBkMode(memDC, TRANSPARENT);
        std::wstring title = L"GO-Helper";
        TextOutW(memDC, 45, 17, title.c_str(), (int)title.length());
        SIZE sz; GetTextExtentPoint32W(memDC, title.c_str(), (int)title.length(), &sz);
        int dashX = 45 + sz.cx + 5; int dashY = 17 + (sz.cy / 2) + 1; 
        HPEN hDashPen = CreatePen(PS_SOLID, 1, CLR_TEXT); SelectObject(memDC, hDashPen);
        MoveToEx(memDC, dashX, dashY, NULL); LineTo(memDC, dashX + 8, dashY); DeleteObject(hDashPen);
        TextOutW(memDC, dashX + 14, 17, skuText.c_str(), (int)skuText.length());
        
        std::wstring therm = L"Thermal Mode: " + GetThermalModeString();
        TextOutW(memDC, 20, 50, therm.c_str(), (int)therm.length());

        std::wstring cpuTemp = GetCPUTempString();
        SIZE cpSz; GetTextExtentPoint32W(memDC, cpuTemp.c_str(), (int)cpuTemp.length(), &cpSz);
        TextOutW(memDC, rc.right - cpSz.cx - 20, 50, cpuTemp.c_str(), (int)cpuTemp.length());

        SetTextColor(memDC, g_mouseEnabled ? CLR_TEXT : CLR_DISABLED);
        std::wstring sStr = L"Sensitivity: " + std::to_wstring(g_currentSenseVal * 2) + L"%";
        TextOutW(memDC, 195, 144, sStr.c_str(), (int)sStr.length());
        SetTextColor(memDC, CLR_TEXT);
        std::wstring bat = GetBatteryStatusString();
        TextOutW(memDC, 20, 205, bat.c_str(), (int)bat.length());

        SelectObject(memDC, hFontSmall); SetTextColor(memDC, CLR_VERSION);
        std::wstring vStr = L"Version: " + std::wstring(APP_VERSION);
        SIZE vS; GetTextExtentPoint32W(memDC, vStr.c_str(), (int)vStr.length(), &vS);
        TextOutW(memDC, rc.right - vS.cx - 20, rc.bottom - vS.cy - 15, vStr.c_str(), (int)vStr.length());

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBM); DeleteDC(memDC); EndPaint(hwnd, &ps);
    }
    break;

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
        bool prs = (pdis->itemState & ODS_SELECTED);
        SelectObject(pdis->hDC, hFontBold);
        
        if (pdis->CtlID == BTN_CLOSE) {
            HBRUSH hBlackBg = CreateSolidBrush(RGB(0, 0, 0));
            HPEN hRedBorder = CreatePen(PS_SOLID, 1, CLR_RED);
            HGDIOBJ oldBrush = SelectObject(pdis->hDC, hBlackBg);
            HGDIOBJ oldPen = SelectObject(pdis->hDC, hRedBorder);
            RoundRect(pdis->hDC, pdis->rcItem.left, pdis->rcItem.top, pdis->rcItem.right, pdis->rcItem.bottom, 8, 8);
            SetTextColor(pdis->hDC, RGB(255, 255, 255));
            SetBkMode(pdis->hDC, TRANSPARENT);
            DrawTextW(pdis->hDC, L"✕", -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(pdis->hDC, oldBrush); SelectObject(pdis->hDC, oldPen);
            DeleteObject(hBlackBg); DeleteObject(hRedBorder);
        }
        else {
            if (pdis->CtlID == BTN_QUIET) DrawGButton(pdis->hDC, pdis->rcItem, L"Quiet", CLR_QUIET, prs);
            else if (pdis->CtlID == BTN_BALANCED) DrawGButton(pdis->hDC, pdis->rcItem, L"Balanced", CLR_BAL, prs);
            else if (pdis->CtlID == BTN_PERFORMANCE) DrawGButton(pdis->hDC, pdis->rcItem, L"Performance", CLR_PERF, prs);
            else if (pdis->CtlID == BTN_MOUSE_TOGGLE) DrawGButton(pdis->hDC, pdis->rcItem, g_mouseEnabled ? L"Mouse" : L"Gamepad", g_mouseEnabled ? CLR_ACCENT : CLR_CARD, prs);
        }
        return TRUE;
    }

    case WM_HSCROLL:
        if ((HWND)lParam == hSlider) {
            g_currentSenseVal = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
            g_mouseSensitivity = g_currentSenseVal * 0.0005f; InvalidateRect(hwnd, NULL, FALSE);
        }
        break;

    case WM_COMMAND:
        switch(LOWORD(wParam)) {
            case BTN_QUIET: SetThermalMode(1); InvalidateRect(hwnd, NULL, FALSE); break;
            case BTN_BALANCED: SetThermalMode(2); InvalidateRect(hwnd, NULL, FALSE); break;
            case BTN_PERFORMANCE: SetThermalMode(3); InvalidateRect(hwnd, NULL, FALSE); break;
            case BTN_MOUSE_TOGGLE: g_mouseEnabled = !g_mouseEnabled; EnableWindow(hSlider, g_mouseEnabled); InvalidateRect(hwnd, NULL, TRUE); break;
            case BTN_CLOSE: ToggleVisibility(hwnd); break; 
            case ID_TRAY_MUTE_APP: g_appMuted = !g_appMuted; break;
            case ID_TRAY_DISABLE_GB: DisableGameBarRegistry(); break;
            case ID_TRAY_START_WITH_WIN: SetAutoStart(!IsAutoStartEnabled()); break;
            case ID_TRAY_EXIT: DestroyWindow(hwnd); break;
            case ID_TRAY_TOGGLE: ToggleVisibility(hwnd); break;
        }
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            HMENU hM = CreatePopupMenu();
            AppendMenuW(hM, MF_STRING, ID_TRAY_TOGGLE, L"Show Menu");
            AppendMenuW(hM, MF_STRING | (g_appMuted ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_MUTE_APP, L"Mute Sounds");
            AppendMenuW(hM, MF_STRING | (IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_START_WITH_WIN, L"Start with Windows");
            AppendMenuW(hM, MF_STRING, ID_TRAY_DISABLE_GB, L"Disable Game Bar");
            AppendMenuW(hM, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hM, MF_STRING, ID_TRAY_EXIT, L"Exit");
            POINT pt; GetCursorPos(&pt); SetForegroundWindow(hwnd);
            TrackPopupMenu(hM, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hM);
        } else if (lParam == WM_LBUTTONUP) ToggleVisibility(hwnd);
        break;

    case WM_DESTROY: 
        KillTimer(hwnd, refreshTimer); 
        Shell_NotifyIconW(NIM_DELETE, &g_nid); 
        if (g_hBackBrush) DeleteObject(g_hBackBrush);
        PostQuitMessage(0); 
        break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* pK = (KBDLLHOOKSTRUCT*)lParam;
        bool isCtrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
        if (isCtrlDown && pK->vkCode == 'G') {
            ToggleVisibility(g_hwnd);
            return 1;
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE, PWSTR, int) {
    if (!IsRunAsAdmin()) { ElevateNow(); return 0; }

    INITCOMMONCONTROLSEX ic = { sizeof(ic), ICC_BAR_CLASSES }; InitCommonControlsEx(&ic);
    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = WindowProc; wc.hInstance = hI; wc.lpszClassName = L"GOHCLASS";
    wc.hIcon = LoadIconW(hI, MAKEINTRESOURCEW(IDI_ICON1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, L"GOHCLASS", L"GO-Helper", WS_POPUP | WS_CLIPCHILDREN, 0, 0, WIN_WIDTH, WIN_HEIGHT, NULL, NULL, hI, NULL);
    SetLayeredWindowAttributes(g_hwnd, 0, 250, LWA_ALPHA);

    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hI, 0);

    ShowWindow(g_hwnd, SW_HIDE);
    MSG m;
    while (true) {
        if (PeekMessage(&m, NULL, 0, 0, PM_REMOVE)) { if (m.message == WM_QUIT) break; TranslateMessage(&m); DispatchMessage(&m); }
        ProcessControllerMouse(); Sleep(5); 
    }
    if (g_hHook) UnhookWindowsHookEx(g_hHook);
    return 0;
}
