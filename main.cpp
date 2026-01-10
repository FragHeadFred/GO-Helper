/* GO-Helper Application
   Version: 0.140.2026.01.10
   Update: GitHub Release / Layout Adjustments.
   Features:
   - Fix: Force repaint of child buttons using RedrawWindow + RDW_ALLCHILDREN for flicker-free UI.
   - Hz Toggle: Instant visual flip, 600Hz tone feedback, and background hardware refresh rate switching.
   - UI: "DONATE" (Caps) link, bolded section headers, and perfect horizontal button/label alignment.
   - TDP Control: Custom range 9W-30W with WMI-based hardware injection.
   - Brightness: Integrated WMI monitor brightness slider control.
   - Scaling: Scaled dimensions (+2%) for improved handheld visibility.
   - Threading: Separated Controller/Mouse input thread to ensure tray menu responsiveness.
   - About Window: Centered popup with program meta-info and support links.
   - Touchpad: Exclusive HID polling with Left/Right tap-to-click logic.
   - Legion R Listener: Byte 18, Bit 6 monitoring for hardware-triggered UI summon.
   - Admin Check: Automatic elevation check to guarantee Registry and WMI access.
   - Global Hotkey: Ctrl + G active for keyboard users.
*/

#define APP_VERSION L"0.140.2026.01.10" // App version for UI
#define _WIN32_WINNT 0x0601             // Target Windows 7 SP1+
#define _WIN32_DCOM                     // Enable DCOM for WMI interfaces

#include <windows.h>                    // Core Windows API
#include <comdef.h>                     // COM Types
#include <WbemIdl.h>                    // WMI definitions
#include <string>                       // String manipulation
#include <shellapi.h>                   // Shell/Tray API
#include <vector>                       // Container library
#include <thread>                       // Multi-threading
#include <atomic>                       // Thread-safe variables
#include <cmath>                        // Math utilities
#include <chrono>                       // High-res timing
#include <Xinput.h>                     // Controller API
#include <commctrl.h>                   // Common Controls (Slider)
#include <uxtheme.h>                    // Themeing engine
#include <dwmapi.h>                     // DWM window effects
#include "resource.h"                   // Resource IDs (Icons)

// --- LIBRARIES ---
#pragma comment(lib, "wbemuuid.lib")    // Link WMI
#pragma comment(lib, "user32.lib")      // Link UI
#pragma comment(lib, "shell32.lib")     // Link Shell
#pragma comment(lib, "gdi32.lib")       // Link Graphics
#pragma comment(lib, "ole32.lib")       // Link COM
#pragma comment(lib, "oleaut32.lib")    // Link Automation
#pragma comment(lib, "advapi32.lib")    // Link Security
#pragma comment(lib, "Xinput.lib")      // Link XInput
#pragma comment(lib, "comctl32.lib")    // Link Common Controls
#pragma comment(lib, "uxtheme.lib")     // Link Themes
#pragma comment(lib, "dwmapi.lib")      // Link DWM

// --- CONSTANTS ---
#define BTN_QUIET 101                   // Quiet mode ID
#define BTN_BALANCED 102                // Balanced mode ID
#define BTN_PERFORMANCE 103             // Performance mode ID
#define BTN_MOUSE_TOGGLE 104            // Mode cycle ID
#define SLIDER_SENSE 105                // Sensitivity ID
#define BTN_CUSTOM 106                  // Custom mode ID
#define BTN_CLOSE 107                   // Close button ID
#define SLIDER_TDP 108                  // TDP Slider ID
#define BTN_ABOUT_CLOSE_BOTTOM 109      // About close ID
#define SLIDER_BRIGHTNESS 110           // Brightness ID
#define BTN_REFRESH_TOGGLE 111          // Hz Toggle ID
#define WM_TRAYICON (WM_USER + 1)       // Tray callback
#define WM_REFRESH_AFTER_HZ (WM_USER + 2) // Post-Hz refresh message

#define ID_TRAY_ABOUT 200               // About menu ID
#define ID_TRAY_EXIT 201                // Exit menu ID
#define ID_TRAY_TOGGLE 202              // Show/Hide ID
#define ID_TRAY_DISABLE_GB 203          // Gamebar fix ID
#define ID_TRAY_MUTE_APP 204            // Mute toggle ID
#define ID_TRAY_START_WITH_WIN 205      // Auto-start ID

// --- UI THEME COLORS ---
#define CLR_BACK      RGB(20, 20, 20)   // Primary background
#define CLR_CARD      RGB(45, 45, 45)   // Element background
#define CLR_TEXT      RGB(240, 240, 240)// Main text
#define CLR_QUIET     RGB(0, 102, 204)  // Blue
#define CLR_BAL       RGB(255, 255, 255)// White
#define CLR_PERF      RGB(178, 34, 34)  // Red
#define CLR_CUSTOM    RGB(200, 0, 255)  // Purple
#define CLR_RED       RGB(255, 0, 0)    // Warning/Glow red
#define CLR_AURA      RGB(40, 40, 40)   // Halo color
#define CLR_ACCENT    RGB(0, 180, 90)   // Success Green
#define CLR_VERSION   RGB(160, 160, 160)// Meta gray
#define CLR_DISABLED  RGB(80, 80, 80)   // Inactive gray
#define CLR_LINK      RGB(80, 180, 255) // Link blue

// --- ENUMS & GLOBALS ---
enum ControllerMode { MODE_ANALOG = 0, MODE_TOUCHPAD = 1, MODE_OFF = 2 };

HWND g_hwnd = NULL;                     // Global Window handle
HWND g_aboutHwnd = NULL;                // Global About window handle
HHOOK g_hHook = NULL;                   // Global Keyboard hook
NOTIFYICONDATAW g_nid = { 0 };          // Tray icon data
HICON g_hMainIcon = NULL;               // App icon handle
HBRUSH g_hBackBrush = NULL;             // BG brush handle
bool g_appMuted = true;                 // App mute state

std::atomic<int> g_controllerMode(MODE_ANALOG); // Thread-safe mode
std::atomic<bool> g_running(true);      // App exit flag
ULONGLONG g_lastHzChangeTick = 0;       // Hz switch debounce

int g_currentSenseVal = 5;              // Sense setting
float g_mouseSensitivity = 5 * 0.0005f; // Real sensitivity
int g_currentTDP = 9;                   // TDP setting
int g_currentBrightness = 50;           // Brightness level
int g_currentHz = 60;                   // Refresh rate level

// SCALED DIMENSIONS
const int WIN_WIDTH = 408;              // 2% scaled width
const int WIN_HEIGHT = 390;             // 2% scaled height

// --- FORWARD DECLARATIONS ---
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK AboutProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SliderSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
void ProcessControllerMouse();
void ToggleVisibility(HWND hwnd);
void DrawGButton(HDC hdc, RECT rc, LPCWSTR text, COLORREF color, bool pressed);

// --- LEGION SCREEN CLASS ---
class LegionScreen {
public:
    static int GetRefreshRate() { // Read refresh rate via GDI
        DEVMODE dm = { 0 }; dm.dmSize = sizeof(DEVMODE);
        if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm)) return dm.dmDisplayFrequency;
        return 60;
    }
    static void SetRefreshRate(int hz) { // Write refresh rate via GDI
        DEVMODE dm = { 0 }; dm.dmSize = sizeof(DEVMODE);
        if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
            dm.dmDisplayFrequency = hz; dm.dmFields = DM_DISPLAYFREQUENCY;
            ChangeDisplaySettings(&dm, CDS_UPDATEREGISTRY);
        }
    }
    static int GetBrightness() { // Query brightness via WMI
        int brightness = 50;
        HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
        IWbemLocator* pLoc = NULL;
        if (SUCCEEDED(CoCreateInstance(__uuidof(WbemLocator), 0, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID*)&pLoc))) {
            IWbemServices* pSvc = NULL;
            if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc))) {
                CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
                IEnumWbemClassObject* pEnum = NULL;
                if (SUCCEEDED(pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT CurrentBrightness FROM WmiMonitorBrightness"), WBEM_FLAG_FORWARD_ONLY, NULL, &pEnum))) {
                    IWbemClassObject* pObj = NULL; ULONG u = 0;
                    if (pEnum->Next(WBEM_INFINITE, 1, &pObj, &u) == WBEM_S_NO_ERROR) {
                        VARIANT v;
                        if (SUCCEEDED(pObj->Get(L"CurrentBrightness", 0, &v, 0, 0))) { brightness = v.uiVal; VariantClear(&v); }
                        pObj->Release();
                    }
                    pEnum->Release();
                }
                pSvc->Release();
            }
            pLoc->Release();
        }
        return brightness;
    }
    static void SetBrightness(int percent) { // Set brightness via WMI methods
        if (percent < 0) percent = 0; if (percent > 100) percent = 100;
        HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
        IWbemLocator* pLoc = NULL;
        if (SUCCEEDED(CoCreateInstance(__uuidof(WbemLocator), 0, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID*)&pLoc))) {
            IWbemServices* pSvc = NULL;
            if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc))) {
                CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
                IWbemClassObject* pClass = NULL;
                if (SUCCEEDED(pSvc->GetObject(_bstr_t(L"WmiMonitorBrightnessMethods"), 0, NULL, &pClass, NULL))) {
                    IWbemClassObject* pInParamsDef = NULL;
                    if (SUCCEEDED(pClass->GetMethod(_bstr_t(L"WmiSetBrightness"), 0, &pInParamsDef, NULL))) {
                        IEnumWbemClassObject* pEnum = NULL;
                        if (SUCCEEDED(pSvc->CreateInstanceEnum(_bstr_t(L"WmiMonitorBrightnessMethods"), 0, NULL, &pEnum))) {
                            IWbemClassObject* pInst = NULL; ULONG u = 0;
                            while (pEnum->Next(WBEM_INFINITE, 1, &pInst, &u) == WBEM_S_NO_ERROR) {
                                VARIANT path;
                                if (SUCCEEDED(pInst->Get(L"__PATH", 0, &path, NULL, NULL))) {
                                    IWbemClassObject* pParams = NULL;
                                    if (SUCCEEDED(pInParamsDef->SpawnInstance(0, &pParams))) {
                                        VARIANT vT; vT.vt = VT_I4; vT.lVal = 1;
                                        VARIANT vB; vB.vt = VT_UI1; vB.bVal = (BYTE)percent;
                                        pParams->Put(L"Timeout", 0, &vT, 0); pParams->Put(L"Brightness", 0, &vB, 0);
                                        pSvc->ExecMethod(path.bstrVal, _bstr_t(L"WmiSetBrightness"), 0, NULL, pParams, NULL, NULL);
                                        pParams->Release();
                                    }
                                    VariantClear(&path);
                                }
                                pInst->Release();
                            }
                            pEnum->Release();
                        }
                        pInParamsDef->Release();
                    }
                    pClass->Release();
                }
                pSvc->Release();
            }
            pLoc->Release();
        }
        CoUninitialize();
    }
};

// --- LEGION POWER CLASS ---
class LegionPower {
public:
    static void SetTDP(int watts) { // Inject watts into BIOS registers via WMI
        const int ID_SUSTAINED = 16973568; const int ID_FAST = 16908032;
        HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return;
        IWbemLocator* pLoc = NULL;
        if (SUCCEEDED(CoCreateInstance(__uuidof(WbemLocator), 0, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID*)&pLoc))) {
            IWbemServices* pSvc = NULL;
            if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc))) {
                CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
                ExecuteSimpleMethod(pSvc, L"LENOVO_GAMEZONE_DATA", L"SetDDSControlOwner", 1);
                ExecuteSimpleMethod(pSvc, L"LENOVO_GAMEZONE_DATA", L"SetSmartFanMode", 255);
                ExecuteSimpleMethod(pSvc, L"LENOVO_GAMEZONE_DATA", L"SetIntelligentSubMode", 255);
                int currentVal = GetFeatureValue(pSvc, ID_SUSTAINED);
                int multiplier = (currentVal > 1000) ? 1000 : 1;
                int valueToSend = watts * multiplier;
                SetFeatureValue(pSvc, ID_SUSTAINED, valueToSend);
                SetFeatureValue(pSvc, ID_FAST, valueToSend);
                pSvc->Release();
            }
            pLoc->Release();
        }
        CoUninitialize();
    }
private:
    static void ExecuteSimpleMethod(IWbemServices* pSvc, LPCWSTR className, LPCWSTR methodName, int dataVal) {
        IWbemClassObject* pClass = NULL;
        if (SUCCEEDED(pSvc->GetObject(_bstr_t(className), 0, NULL, &pClass, NULL))) {
            IWbemClassObject* pInDef = NULL;
            if (SUCCEEDED(pClass->GetMethod(_bstr_t(methodName), 0, &pInDef, NULL)) && pInDef) {
                IWbemClassObject* pInInst = NULL; pInDef->SpawnInstance(0, &pInInst);
                VARIANT v; v.vt = VT_I4; v.lVal = dataVal; pInInst->Put(L"Data", 0, &v, 0);
                IEnumWbemClassObject* pEnum = NULL;
                if (SUCCEEDED(pSvc->CreateInstanceEnum(_bstr_t(className), 0, NULL, &pEnum))) {
                    IWbemClassObject* pInst = NULL; ULONG u = 0;
                    if (pEnum->Next(WBEM_INFINITE, 1, &pInst, &u) == WBEM_S_NO_ERROR) {
                        VARIANT path;
                        if (SUCCEEDED(pInst->Get(L"__PATH", 0, &path, NULL, NULL))) {
                            pSvc->ExecMethod(path.bstrVal, _bstr_t(methodName), 0, NULL, pInInst, NULL, NULL);
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
    }
    static int GetFeatureValue(IWbemServices* pSvc, int id) {
        int result = 0; IWbemClassObject* pClass = NULL;
        if (SUCCEEDED(pSvc->GetObject(_bstr_t(L"LENOVO_OTHER_METHOD"), 0, NULL, &pClass, NULL))) {
            IWbemClassObject* pInDef = NULL;
            if (SUCCEEDED(pClass->GetMethod(L"GetFeatureValue", 0, &pInDef, NULL))) {
                IWbemClassObject* pInInst = NULL; pInDef->SpawnInstance(0, &pInInst);
                VARIANT v; v.vt = VT_I4; v.lVal = id; pInInst->Put(L"IDs", 0, &v, 0);
                IEnumWbemClassObject* pEnum = NULL;
                if (SUCCEEDED(pSvc->CreateInstanceEnum(_bstr_t(L"LENOVO_OTHER_METHOD"), 0, NULL, &pEnum))) {
                    IWbemClassObject* pInst = NULL; ULONG u = 0;
                    if (pEnum->Next(WBEM_INFINITE, 1, &pInst, &u) == WBEM_S_NO_ERROR) {
                        VARIANT path; pInst->Get(L"__PATH", 0, &path, NULL, NULL);
                        IWbemClassObject* pOut = NULL;
                        if (SUCCEEDED(pSvc->ExecMethod(path.bstrVal, _bstr_t(L"GetFeatureValue"), 0, NULL, pInInst, &pOut, NULL)) && pOut) {
                            VARIANT ret; if (SUCCEEDED(pOut->Get(L"Value", 0, &ret, NULL, NULL))) { result = ret.lVal; VariantClear(&ret); }
                            pOut->Release();
                        }
                        VariantClear(&path); pInst->Release();
                    }
                    pEnum->Release();
                }
                pInInst->Release(); pInDef->Release();
            }
            pClass->Release();
        }
        return result;
    }
    static void SetFeatureValue(IWbemServices* pSvc, int id, int value) {
        IWbemClassObject* pClass = NULL;
        if (SUCCEEDED(pSvc->GetObject(_bstr_t(L"LENOVO_OTHER_METHOD"), 0, NULL, &pClass, NULL))) {
            IWbemClassObject* pInDef = NULL;
            if (SUCCEEDED(pClass->GetMethod(L"SetFeatureValue", 0, &pInDef, NULL))) {
                IWbemClassObject* pInInst = NULL; pInDef->SpawnInstance(0, &pInInst);
                VARIANT vId; vId.vt = VT_I4; vId.lVal = id; pInInst->Put(L"IDs", 0, &vId, 0);
                VARIANT vVal; vVal.vt = VT_I4; vVal.lVal = value; pInInst->Put(L"Value", 0, &vVal, 0);
                IEnumWbemClassObject* pEnum = NULL;
                if (SUCCEEDED(pSvc->CreateInstanceEnum(_bstr_t(L"LENOVO_OTHER_METHOD"), 0, NULL, &pEnum))) {
                    IWbemClassObject* pInst = NULL; ULONG u = 0;
                    if (pEnum->Next(WBEM_INFINITE, 1, &pInst, &u) == WBEM_S_NO_ERROR) {
                        VARIANT path; pInst->Get(L"__PATH", 0, &path, NULL, NULL);
                        pSvc->ExecMethod(path.bstrVal, _bstr_t(L"SetFeatureValue"), 0, NULL, pInInst, NULL, NULL);
                        VariantClear(&path); pInst->Release();
                    }
                    pEnum->Release();
                }
                pInInst->Release(); pInDef->Release();
            }
            pClass->Release();
        }
    }
};

// --- LEGION TRIGGER CLASS ---
class LegionTrigger {
public:
    static void Start(HWND targetWindow) { // Polling thread for hardware button
        std::thread([targetWindow]() { MonitorController(targetWindow); }).detach();
    }
private:
    static void MonitorController(HWND targetWindow) {
        std::string devicePath = "\\\\?\\hid#vid_17ef&pid_61eb&mi_02#8&ece5261&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}";
        unsigned char buffer[64];
        DWORD bytesRead = 0;
        bool wasPressed = false;
        while (g_running) {
            HANDLE hDevice = CreateFileA(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (hDevice == INVALID_HANDLE_VALUE) { std::this_thread::sleep_for(std::chrono::milliseconds(1000)); continue; }
            while (g_running) {
                if (ReadFile(hDevice, buffer, 64, &bytesRead, NULL)) {
                    bool isPressed = (buffer[18] & 0x40) == 0x40;
                    if (isPressed && !wasPressed) { PostMessage(targetWindow, WM_COMMAND, ID_TRAY_TOGGLE, 0); wasPressed = true; }
                    else if (!isPressed) { wasPressed = false; }
                }
                else break;
            }
            CloseHandle(hDevice);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
};

// --- LEGION PAD CLASS ---
class LegionPad {
public:
    static void Start() { // Polling thread for raw touchpad HID data
        std::thread(MonitorTouchpad).detach();
    }
private:
    static void MonitorTouchpad() {
        const char* devicePath = "\\\\?\\hid#vid_17ef&pid_61eb&mi_02#8&ece5261&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}";
        const int MIN_X = 50, MAX_X = 950, MIN_Y = 50, MAX_Y = 950;
        const double SMOOTHING = 0.5;
        const int TAP_TIMEOUT_MS = 200;
        const int TAP_DRIFT_TOL = 20;
        const int MIDDLE_X_LIMIT = 500;
        unsigned char buffer[64];
        DWORD bytesRead = 0;
        double lastX = -1.0, lastY = -1.0, remX = 0.0, remY = 0.0;
        bool wasDown = false;
        std::chrono::steady_clock::time_point tapStartTime;
        int tapStartX = 0; int tapStartY = 0; bool possibleTap = false;
        while (g_running) {
            HANDLE hDevice = CreateFileA(devicePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (hDevice == INVALID_HANDLE_VALUE) { hDevice = CreateFileA(devicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL); }
            if (hDevice == INVALID_HANDLE_VALUE) { std::this_thread::sleep_for(std::chrono::milliseconds(1000)); continue; }
            while (g_running) {
                if (ReadFile(hDevice, buffer, 64, &bytesRead, NULL)) {
                    if (g_controllerMode.load() != MODE_TOUCHPAD) { lastX = -1.0; wasDown = false; possibleTap = false; continue; }
                    int rawX = (buffer[26] << 8) | buffer[27]; int rawY = (buffer[28] << 8) | buffer[29];
                    bool isDown = (rawX != 0 || rawY != 0);
                    if (isDown) {
                        if (rawX < MIN_X) rawX = MIN_X; if (rawX > MAX_X) rawX = MAX_X;
                        if (rawY < MIN_Y) rawY = MIN_Y; if (rawY > MAX_Y) rawY = MAX_Y;
                        double normX = static_cast<double>(rawX); double normY = static_cast<double>(rawY);
                        if (wasDown && lastX != -1.0) {
                            double scale = (g_currentSenseVal * 0.3);
                            double deltaX = (normX - lastX) * scale; double deltaY = (normY - lastY) * scale;
                            if (possibleTap) { int totalDrift = std::abs(rawX - tapStartX) + std::abs(rawY - tapStartY); if (totalDrift > TAP_DRIFT_TOL) possibleTap = false; }
                            remX = (SMOOTHING * deltaX) + ((1.0 - SMOOTHING) * remX); remY = (SMOOTHING * deltaY) + ((1.0 - SMOOTHING) * remY);
                            int moveX = static_cast<int>(remX); int moveY = static_cast<int>(remY);
                            if (moveX != 0 || moveY != 0) { mouse_event(MOUSEEVENTF_MOVE, moveX, moveY, 0, 0); remX -= moveX; remY -= moveY; }
                        }
                        else {
                            remX = 0.0; remY = 0.0; tapStartTime = std::chrono::steady_clock::now();
                            tapStartX = rawX; tapStartY = rawY; possibleTap = true;
                        }
                        lastX = normX; lastY = normY; wasDown = true;
                    }
                    else {
                        if (wasDown && possibleTap) {
                            auto endTime = std::chrono::steady_clock::now();
                            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - tapStartTime).count();
                            if (duration < TAP_TIMEOUT_MS) {
                                if (tapStartX < MIDDLE_X_LIMIT) { mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0); }
                                else { mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0); mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0); }
                            }
                        }
                        wasDown = false; lastX = -1.0; possibleTap = false;
                    }
                }
                else break;
            }
            CloseHandle(hDevice);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
};

// --- LOGIC: SYSTEM ---
bool IsRunAsAdmin() { // Security token verification
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup)) {
        CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin);
        FreeSid(pAdministratorsGroup);
    }
    return fIsRunAsAdmin;
}
void ElevateNow() { // Restart executable as Administrator
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas"; sei.lpFile = szPath; sei.hwnd = NULL; sei.nShow = SW_NORMAL;
        if (!ShellExecuteExW(&sei)) return; else exit(0);
    }
}
bool IsAutoStartEnabled() { // Check Registry Run key
    HKEY hKey; bool enabled = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"GO-Helper", NULL, NULL, NULL, NULL) == ERROR_SUCCESS) enabled = true;
        RegCloseKey(hKey);
    }
    return enabled;
}
void SetAutoStart(bool enable) { // Update Registry Run key
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t path[MAX_PATH]; GetModuleFileNameW(NULL, path, MAX_PATH);
            RegSetValueExW(hKey, L"GO-Helper", 0, REG_SZ, (const BYTE*)path, static_cast<DWORD>((wcslen(path) + 1) * sizeof(wchar_t)));
        }
        else RegDeleteValueW(hKey, L"GO-Helper");
        RegCloseKey(hKey);
    }
}

// --- FULL WMI IMPLEMENTATION ---
std::wstring GetCPUTempString() { // Polling ACPI thermal zones
    long maxTempDK = 0;
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return L"CPU: --";
    IWbemLocator* pLoc = NULL;
    if (SUCCEEDED(CoCreateInstance(__uuidof(WbemLocator), 0, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID*)&pLoc))) {
        IWbemServices* pSvc = NULL;
        if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc))) {
            CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
            IEnumWbemClassObject* pEnum = NULL;
            if (SUCCEEDED(pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"), WBEM_FLAG_FORWARD_ONLY, NULL, &pEnum))) {
                IWbemClassObject* pObj = NULL; ULONG uRet = 0;
                while (pEnum->Next(WBEM_INFINITE, 1, &pObj, &uRet) == WBEM_S_NO_ERROR) {
                    VARIANT vt;
                    if (SUCCEEDED(pObj->Get(L"CurrentTemperature", 0, &vt, 0, 0))) {
                        long t = vt.lVal;
                        if (t > maxTempDK && t < 4000) maxTempDK = t;
                        VariantClear(&vt);
                    }
                    pObj->Release();
                }
                pEnum->Release();
            }
            pSvc->Release();
        }
        pLoc->Release();
    }
    CoUninitialize();
    if (maxTempDK <= 0) return L"CPU: --";
    double celsius = (maxTempDK / 10.0) - 273.15;
    double fahrenheit = (celsius * 9.0 / 5.0) + 32.0;
    wchar_t buf[64]; swprintf_s(buf, L"CPU: %.1f°C / %.0f°F", celsius, fahrenheit);
    return std::wstring(buf);
}

void DisableGameBarRegistry() { // Kill system app capturing popups
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD dwVal = 0; RegSetValueExW(hKey, L"AppCaptureEnabled", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(dwVal)); RegCloseKey(hKey);
    }
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"System\\GameConfigStore", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD dwVal = 0; RegSetValueExW(hKey, L"GameDVR_Enabled", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(dwVal)); RegCloseKey(hKey);
    }
    MessageBoxW(NULL, L"Game Bar features disabled. Restart recommended.", L"GO-Helper", MB_OK | MB_ICONINFORMATION);
}

std::wstring GetThermalModeString() { // Read current hardware profile
    int mode = 0;
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return L"Unknown";
    IWbemLocator* pLoc = NULL;
    if (SUCCEEDED(CoCreateInstance(__uuidof(WbemLocator), 0, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID*)&pLoc))) {
        IWbemServices* pSvc = NULL;
        if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc))) {
            CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
            _bstr_t className(L"LENOVO_GAMEZONE_DATA");
            IEnumWbemClassObject* pEnum = NULL;
            if (SUCCEEDED(pSvc->CreateInstanceEnum(className, 0, NULL, &pEnum))) {
                IWbemClassObject* pInst = NULL; ULONG uRet = 0;
                if (pEnum->Next(WBEM_INFINITE, 1, &pInst, &uRet) == WBEM_S_NO_ERROR) {
                    VARIANT vtPath;
                    if (SUCCEEDED(pInst->Get(L"__PATH", 0, &vtPath, NULL, NULL))) {
                        IWbemClassObject* pOut = NULL;
                        if (SUCCEEDED(pSvc->ExecMethod(vtPath.bstrVal, _bstr_t(L"GetSmartFanMode"), 0, NULL, NULL, &pOut, NULL))) {
                            VARIANT vtRes;
                            if (SUCCEEDED(pOut->Get(L"Data", 0, &vtRes, NULL, NULL))) { mode = vtRes.lVal; VariantClear(&vtRes); }
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
    if (mode == 1) return L"Quiet 9W";
    if (mode == 2) return L"Balanced 15W";
    if (mode == 3) return L"Performance 20W";
    if (mode == 255) return L"Custom";
    return L"Unknown";
}

std::wstring GetBatteryStatusString() { // Fetch system power stats
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) return L"Battery: Unknown";
    std::wstring status = L"Battery: ";
    status += (sps.ACLineStatus == 1) ? L"Plugged In" : L"Discharging";
    status += L" @ " + std::to_wstring((int)sps.BatteryLifePercent) + L"%";
    return status;
}

bool SetThermalMode(int value) { // Push hardware profile
    bool success = false;
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
    IWbemLocator* pLoc = NULL;
    if (SUCCEEDED(CoCreateInstance(__uuidof(WbemLocator), 0, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID*)&pLoc))) {
        IWbemServices* pSvc = NULL;
        if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc))) {
            CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
            _bstr_t className(L"LENOVO_GAMEZONE_DATA");
            IWbemClassObject* pClass = NULL;
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
    if (success && !g_appMuted) {
        int freq = 800;
        switch (value) {
        case 1: freq = 800; break; case 2: freq = 900; break; case 3: freq = 1000; break; case 255: freq = 1100; break;
        }
        Beep(freq, 100);
    }
    return success;
}

std::wstring GetSystemSKU() { // Fetch BIOS meta-strings
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
                    VARIANT vt;
                    if (SUCCEEDED(pObj->Get(L"Name", 0, &vt, 0, 0)) && vt.vt == VT_BSTR) biosModel = vt.bstrVal;
                    VariantClear(&vt);
                    if (SUCCEEDED(pObj->Get(L"SKUNumber", 0, &vt, 0, 0)) && vt.vt == VT_BSTR) biosSKU = vt.bstrVal;
                    VariantClear(&vt);
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

// --- UI UTILS ---
void RepositionToBottomRight(HWND hwnd) { // Anchor logic
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
        int x = mi.rcWork.right - WIN_WIDTH - 20;
        int y = mi.rcWork.bottom - WIN_HEIGHT - 20;
        SetWindowPos(hwnd, HWND_TOPMOST, x, y, WIN_WIDTH, WIN_HEIGHT, SWP_SHOWWINDOW);
    }
}

void ToggleVisibility(HWND hwnd) { // UI Show/Hide with focus steal
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

void DrawGButton(HDC hdc, RECT rc, LPCWSTR text, COLORREF color, bool pressed) { // Button renderer
    HBRUSH hBackBr = CreateSolidBrush(CLR_BACK); FillRect(hdc, &rc, hBackBr); DeleteObject(hBackBr);
    HBRUSH hBr = CreateSolidBrush(pressed ? color : CLR_CARD); HPEN hPen = CreatePen(PS_SOLID, 1, pressed ? RGB(200, 200, 200) : RGB(80, 80, 80));
    SelectObject(hdc, hPen); SelectObject(hdc, hBr); RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 12, 12);
    SetTextColor(hdc, CLR_TEXT); SetBkMode(hdc, TRANSPARENT); DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hBr); DeleteObject(hPen);
}

// --- CONTROLLER MOUSE THREAD ---
void ProcessControllerMouse() { // Controller stick mapping
    if (g_controllerMode.load() != MODE_ANALOG) return;
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

// --- THREAD WRAPPER ---
void ControllerThreadWrapper() { // Isolated polling thread
    while (g_running) {
        ProcessControllerMouse();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

LRESULT CALLBACK SliderSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) { // Slider renderer
    if (uMsg == WM_ERASEBKGND) return 1;
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        HDC memDC = CreateCompatibleDC(hdc); HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        SelectObject(memDC, memBM);
        HBRUSH hBg = CreateSolidBrush(CLR_BACK); FillRect(memDC, &rc, hBg); DeleteObject(hBg);
        bool enabled = IsWindowEnabled(hWnd);
        HBRUSH hTrack = CreateSolidBrush(enabled ? CLR_RED : CLR_DISABLED);
        int trackHeight = 4; int centerV = (rc.bottom - rc.top) / 2;
        RECT chRc = { rc.left + 12, centerV - (trackHeight / 2), rc.right - 12, centerV + (trackHeight / 2) };
        FillRect(memDC, &chRc, hTrack); DeleteObject(hTrack);
        int curPos = (int)SendMessage(hWnd, TBM_GETPOS, 0, 0);
        int min = (int)SendMessage(hWnd, TBM_GETRANGEMIN, 0, 0);
        int max = (int)SendMessage(hWnd, TBM_GETRANGEMAX, 0, 0);
        float ratio = (float)(curPos - min) / (float)(max - min);
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

// --- ABOUT WINDOW ---
void ShowAboutWindow(HINSTANCE hI, HWND parent) { // popup init
    if (g_aboutHwnd && IsWindow(g_aboutHwnd)) { SetForegroundWindow(g_aboutHwnd); return; }
    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = AboutProc; wc.hInstance = hI; wc.lpszClassName = L"GOHABOUT";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); RegisterClassW(&wc);
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 400; int winH = 220;
    int x = (scrW - winW) / 2;
    int y = (scrH - winH) / 2;
    g_aboutHwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, L"GOHABOUT", L"About GO-Helper", WS_POPUP, x, y, winW, winH, parent, NULL, hI, NULL);
    SetLayeredWindowAttributes(g_aboutHwnd, 0, 250, LWA_ALPHA);
    ShowWindow(g_aboutHwnd, SW_SHOW);
}

LRESULT CALLBACK AboutProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { // popup handle
    static HFONT hF;
    switch (uMsg) {
    case WM_CREATE:
        hF = CreateFontW(16, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
        CreateWindowW(L"BUTTON", L"✕", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 400 - 35, 10, 25, 25, hwnd, (HMENU)BTN_CLOSE, NULL, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 150, 175, 100, 35, hwnd, (HMENU)BTN_ABOUT_CLOSE_BOTTOM, NULL, NULL);
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH hb = CreateSolidBrush(CLR_BACK); FillRect(hdc, &rc, hb); DeleteObject(hb);
        SelectObject(hdc, hF); SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT);
        RECT r1 = { 20, 50, 380, 75 }; DrawTextW(hdc, L"Created and Programmed by FragHeadFred", -1, &r1, DT_LEFT);
        SetTextColor(hdc, CLR_LINK);
        RECT r2 = { 20, 85, 380, 110 }; DrawTextW(hdc, L"https://github.com/FragHeadFred/GO-Helper", -1, &r2, DT_LEFT);
        RECT r3 = { 20, 120, 380, 170 }; DrawTextW(hdc, L"https://www.paypal.com/donate/?hosted_button_id=PA5MTBGWQMUP4", -1, &r3, DT_LEFT | DT_WORDBREAK);
        EndPaint(hwnd, &ps);
    } break;
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
        if (pdis->CtlID == BTN_CLOSE) {
            HBRUSH hBack = CreateSolidBrush(CLR_BACK); FillRect(pdis->hDC, &pdis->rcItem, hBack); DeleteObject(hBack);
            HBRUSH hBlackBg = CreateSolidBrush(RGB(0, 0, 0));
            HPEN hRedBorder = CreatePen(PS_SOLID, 1, CLR_RED);
            HGDIOBJ oldBrush = SelectObject(pdis->hDC, hBlackBg);
            HGDIOBJ oldPen = SelectObject(pdis->hDC, hRedBorder);
            RoundRect(pdis->hDC, pdis->rcItem.left, pdis->rcItem.top, pdis->rcItem.right, pdis->rcItem.bottom, 8, 8);
            SetTextColor(pdis->hDC, RGB(255, 255, 255)); SetBkMode(pdis->hDC, TRANSPARENT); DrawTextW(pdis->hDC, L"✕", -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(pdis->hDC, oldBrush); SelectObject(pdis->hDC, oldPen);
            DeleteObject(hBlackBg); DeleteObject(hRedBorder);
        }
        else if (pdis->CtlID == BTN_ABOUT_CLOSE_BOTTOM) {
            DrawGButton(pdis->hDC, pdis->rcItem, L"Close", CLR_CARD, pdis->itemState & ODS_SELECTED);
        }
        return TRUE;
    }
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        if (y > 80 && y < 110) ShellExecuteW(0, L"open", L"https://github.com/FragHeadFred/GO-Helper", 0, 0, 1);
        if (y > 115 && y < 170) ShellExecuteW(0, L"open", L"https://www.paypal.com/donate/?hosted_button_id=PA5MTBGWQMUP4", 0, 0, 1);
    } break;
    case WM_COMMAND:
        if (LOWORD(wParam) == BTN_CLOSE || LOWORD(wParam) == BTN_ABOUT_CLOSE_BOTTOM) DestroyWindow(hwnd);
        break;
    case WM_DESTROY: g_aboutHwnd = NULL; break;
    default: return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    } return 0;
}

// --- MAIN WINDOW PROC ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HFONT hFontBold, hFontSmall, hFontHeader, hFontSection;
    static std::wstring skuText; static HWND hSlider, hSliderTDP, hSliderBright;
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
        hFontHeader = CreateFontW(18, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
        hFontSection = CreateFontW(16, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");

        skuText = GetSystemSKU();

        g_nid.cbSize = sizeof(NOTIFYICONDATAW); g_nid.hWnd = hwnd; g_nid.uID = 1;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = g_hMainIcon; wcscpy_s(g_nid.szTip, L"GO-Helper");
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        int btnW = 87; int startX = 21; int gap = 5;
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, startX, 75, btnW, 36, hwnd, (HMENU)BTN_QUIET, NULL, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, startX + btnW + gap, 75, btnW, 36, hwnd, (HMENU)BTN_BALANCED, NULL, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, startX + (btnW + gap) * 2, 75, btnW, 36, hwnd, (HMENU)BTN_PERFORMANCE, NULL, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, startX + (btnW + gap) * 3, 75, btnW, 36, hwnd, (HMENU)BTN_CUSTOM, NULL, NULL);

        hSliderTDP = CreateWindowW(TRACKBAR_CLASSW, L"", WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_NOTICKS | WS_DISABLED, 135, 125, 250, 31, hwnd, (HMENU)SLIDER_TDP, NULL, NULL);
        SetWindowTheme(hSliderTDP, L"", L""); SetWindowSubclass(hSliderTDP, SliderSubclassProc, 0, 0);
        SendMessage(hSliderTDP, TBM_SETRANGE, TRUE, MAKELONG(9, 30));
        SendMessage(hSliderTDP, TBM_SETPOS, TRUE, g_currentTDP);

        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 21, 195, 87, 36, hwnd, (HMENU)BTN_MOUSE_TOGGLE, NULL, NULL);
        hSlider = CreateWindowW(TRACKBAR_CLASSW, L"", WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_NOTICKS, 135, 209, 250, 31, hwnd, (HMENU)SLIDER_SENSE, NULL, NULL);
        SetWindowTheme(hSlider, L"", L""); SetWindowSubclass(hSlider, SliderSubclassProc, 0, 0);
        SendMessage(hSlider, TBM_SETRANGE, TRUE, MAKELONG(1, 50));
        SendMessage(hSlider, TBM_SETPOS, TRUE, g_currentSenseVal);

        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 21, 275, 87, 36, hwnd, (HMENU)BTN_REFRESH_TOGGLE, NULL, NULL);
        hSliderBright = CreateWindowW(TRACKBAR_CLASSW, L"", WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_NOTICKS, 135, 289, 250, 31, hwnd, (HMENU)SLIDER_BRIGHTNESS, NULL, NULL);
        SetWindowTheme(hSliderBright, L"", L""); SetWindowSubclass(hSliderBright, SliderSubclassProc, 0, 0);
        SendMessage(hSliderBright, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        g_currentBrightness = LegionScreen::GetBrightness();
        SendMessage(hSliderBright, TBM_SETPOS, TRUE, g_currentBrightness);
        g_currentHz = LegionScreen::GetRefreshRate();

        CreateWindowW(L"BUTTON", L"✕", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, WIN_WIDTH - 35, 10, 26, 26, hwnd, (HMENU)BTN_CLOSE, NULL, NULL);

        SetThermalMode(2);
        LegionTrigger::Start(hwnd);
        LegionPad::Start();

        refreshTimer = SetTimer(hwnd, 1, 3000, NULL);
        COLORREF auraColor = CLR_AURA; DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &auraColor, sizeof(auraColor));
        DWM_WINDOW_CORNER_PREFERENCE cp = DWMWCP_ROUND; DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cp, sizeof(cp));
    }
    break;

    case WM_REFRESH_AFTER_HZ: { // Background thread hz query callback
        g_currentHz = LegionScreen::GetRefreshRate();
        InvalidateRect(hwnd, NULL, TRUE);
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    } break;

    case WM_TIMER: if (wParam == 1 && IsWindowVisible(hwnd)) {
        int newB = LegionScreen::GetBrightness();
        if (abs(newB - g_currentBrightness) > 5) {
            g_currentBrightness = newB;
            SendMessage(hSliderBright, TBM_SETPOS, TRUE, g_currentBrightness);
        }
        if ((GetTickCount64() - g_lastHzChangeTick) > 5000) {
            g_currentHz = LegionScreen::GetRefreshRate();
            InvalidateRect(hwnd, NULL, FALSE);
        }
    } break;

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HDC memDC = CreateCompatibleDC(hdc); HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        SelectObject(memDC, memBM);
        HBRUSH hBg = CreateSolidBrush(CLR_BACK); FillRect(memDC, &rc, hBg); DeleteObject(hBg);

        if (g_hMainIcon) DrawIconEx(memDC, 20, 15, g_hMainIcon, 18, 18, 0, NULL, DI_NORMAL);

        SelectObject(memDC, hFontBold); SetTextColor(memDC, CLR_TEXT); SetBkMode(memDC, TRANSPARENT);
        std::wstring title = L"GO-Helper"; TextOutW(memDC, 46, 18, title.c_str(), (int)title.length());
        SIZE sz; GetTextExtentPoint32W(memDC, title.c_str(), (int)title.length(), &sz);
        int dashX = 46 + sz.cx + 5; int dashY = 18 + (sz.cy / 2) + 1;
        HPEN hDashPen = CreatePen(PS_SOLID, 1, CLR_TEXT); SelectObject(memDC, hDashPen);
        MoveToEx(memDC, dashX, dashY, NULL); LineTo(memDC, dashX + 8, dashY); DeleteObject(hDashPen);
        TextOutW(memDC, dashX + 14, 18, skuText.c_str(), (int)skuText.length());

        SelectObject(memDC, hFontSection);
        std::wstring therm = L"Thermal Mode: " + GetThermalModeString();
        TextOutW(memDC, 21, 50, therm.c_str(), (int)therm.length());
        std::wstring cpuTemp = GetCPUTempString();
        SIZE cpSz; GetTextExtentPoint32W(memDC, cpuTemp.c_str(), (int)cpuTemp.length(), &cpSz);
        TextOutW(memDC, rc.right - cpSz.cx - 20, 50, cpuTemp.c_str(), (int)cpuTemp.length());

        SelectObject(memDC, hFontHeader);
        SetTextColor(memDC, IsWindowEnabled(hSliderTDP) ? CLR_CUSTOM : CLR_DISABLED);
        std::wstring tdpLabel = L"TDP: " + std::to_wstring(g_currentTDP) + L"W";
        SIZE tdpSz; GetTextExtentPoint32W(memDC, tdpLabel.c_str(), (int)tdpLabel.length(), &tdpSz);
        int tdpX = 64 - (tdpSz.cx / 2);
        TextOutW(memDC, tdpX, 130, tdpLabel.c_str(), (int)tdpLabel.length());

        SelectObject(memDC, hFontSection); SetTextColor(memDC, CLR_TEXT);
        std::wstring cLabel = L"Controller Mode: ";
        TextOutW(memDC, 21, 170, cLabel.c_str(), (int)cLabel.length());
        SIZE sizeLab; GetTextExtentPoint32W(memDC, cLabel.c_str(), (int)cLabel.length(), &sizeLab);
        int cm = g_controllerMode.load();
        std::wstring cModeStr = L"Analog"; COLORREF cModeClr = CLR_ACCENT;
        if (cm == MODE_TOUCHPAD) { cModeStr = L"Touchpad"; cModeClr = CLR_QUIET; }
        else if (cm == MODE_OFF) { cModeStr = L"Mouse Off"; cModeClr = CLR_DISABLED; }
        SetTextColor(memDC, cModeClr); TextOutW(memDC, 21 + sizeLab.cx, 170, cModeStr.c_str(), (int)cModeStr.length());

        SelectObject(memDC, hFontBold); SetTextColor(memDC, (cm != MODE_OFF) ? CLR_TEXT : CLR_DISABLED);
        std::wstring sStr = L"Sensitivity: " + std::to_wstring(g_currentSenseVal * 2) + L"%";
        SIZE sSz; GetTextExtentPoint32W(memDC, sStr.c_str(), (int)sStr.length(), &sSz);
        int sX = 135 + (250 - sSz.cx) / 2; TextOutW(memDC, sX, 194, sStr.c_str(), (int)sStr.length());

        SelectObject(memDC, hFontSection); SetTextColor(memDC, CLR_TEXT);
        std::wstring hzStr = L"Screen Mode: " + std::to_wstring(g_currentHz) + L"Hz";
        TextOutW(memDC, 21, 250, hzStr.c_str(), (int)hzStr.length());

        SelectObject(memDC, hFontBold);
        std::wstring bStr = L"Brightness: " + std::to_wstring(g_currentBrightness) + L"%";
        SIZE bSz; GetTextExtentPoint32W(memDC, bStr.c_str(), (int)bStr.length(), &bSz);
        int bX = 135 + (250 - bSz.cx) / 2; TextOutW(memDC, bX, 275, bStr.c_str(), (int)bStr.length());

        SelectObject(memDC, hFontSection); SetTextColor(memDC, CLR_TEXT);
        std::wstring bat = GetBatteryStatusString(); TextOutW(memDC, 21, 330, bat.c_str(), (int)bat.length());

        SetTextColor(memDC, CLR_LINK); std::wstring donStr = L"DONATE";
        SIZE dSz; GetTextExtentPoint32W(memDC, donStr.c_str(), (int)donStr.length(), &dSz);
        TextOutW(memDC, 21, rc.bottom - dSz.cy - 15, donStr.c_str(), (int)donStr.length());

        SetTextColor(memDC, CLR_VERSION); std::wstring vStr = L"Version: " + std::wstring(APP_VERSION);
        SIZE vS; GetTextExtentPoint32W(memDC, vStr.c_str(), (int)vStr.length(), &vS);
        TextOutW(memDC, rc.right - vS.cx - 20, rc.bottom - vS.cy - 15, vStr.c_str(), (int)vStr.length());

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        DeleteObject(memBM); DeleteDC(memDC); EndPaint(hwnd, &ps);
    }
                 break;

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam); int y = HIWORD(lParam);
        HDC hdc = GetDC(hwnd); SelectObject(hdc, hFontSection);
        std::wstring donStr = L"DONATE"; SIZE dSz; GetTextExtentPoint32W(hdc, donStr.c_str(), (int)donStr.length(), &dSz);
        ReleaseDC(hwnd, hdc); RECT rc; GetClientRect(hwnd, &rc);
        if (x >= 21 && x <= 21 + dSz.cx && y >= (rc.bottom - dSz.cy - 15) && y <= rc.bottom - 15) {
            ShellExecuteW(0, L"open", L"https://www.paypal.com/donate/?hosted_button_id=PA5MTBGWQMUP4", 0, 0, 1);
        }
    } break;

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
        bool prs = (pdis->itemState & ODS_SELECTED);
        SelectObject(pdis->hDC, hFontBold);
        if (pdis->CtlID == BTN_CLOSE) {
            HBRUSH hBack = CreateSolidBrush(CLR_BACK); FillRect(pdis->hDC, &pdis->rcItem, hBack); DeleteObject(hBack);
            HBRUSH hBlackBg = CreateSolidBrush(RGB(0, 0, 0)); HPEN hRedBorder = CreatePen(PS_SOLID, 1, CLR_RED);
            SelectObject(pdis->hDC, hBlackBg); SelectObject(pdis->hDC, hRedBorder);
            RoundRect(pdis->hDC, pdis->rcItem.left, pdis->rcItem.top, pdis->rcItem.right, pdis->rcItem.bottom, 8, 8);
            SetTextColor(pdis->hDC, RGB(255, 255, 255)); SetBkMode(pdis->hDC, TRANSPARENT);
            DrawTextW(pdis->hDC, L"✕", -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DeleteObject(hBlackBg); DeleteObject(hRedBorder);
        }
        else {
            if (pdis->CtlID == BTN_QUIET) DrawGButton(pdis->hDC, pdis->rcItem, L"Quiet", CLR_QUIET, prs);
            else if (pdis->CtlID == BTN_BALANCED) DrawGButton(pdis->hDC, pdis->rcItem, L"Balanced", CLR_BAL, prs);
            else if (pdis->CtlID == BTN_PERFORMANCE) DrawGButton(pdis->hDC, pdis->rcItem, L"Performance", CLR_PERF, prs);
            else if (pdis->CtlID == BTN_CUSTOM) DrawGButton(pdis->hDC, pdis->rcItem, L"Custom", CLR_CUSTOM, prs);
            else if (pdis->CtlID == BTN_MOUSE_TOGGLE) {
                int mode = g_controllerMode.load(); COLORREF color = CLR_CARD;
                if (mode == MODE_ANALOG) color = CLR_QUIET;
                else if (mode == MODE_TOUCHPAD) color = CLR_CARD;
                else if (mode == MODE_OFF) color = CLR_ACCENT;
                DrawGButton(pdis->hDC, pdis->rcItem, L"Mode", color, prs);
            }
            else if (pdis->CtlID == BTN_REFRESH_TOGGLE) {
                LPCWSTR txt = (g_currentHz > 100) ? L"60Hz" : L"144Hz";
                DrawGButton(pdis->hDC, pdis->rcItem, txt, CLR_CARD, prs);
            }
        }
        return TRUE;
    }

    case WM_HSCROLL:
        if ((HWND)lParam == hSlider) {
            g_currentSenseVal = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
            g_mouseSensitivity = g_currentSenseVal * 0.0005f; InvalidateRect(hwnd, NULL, FALSE);
        }
        else if ((HWND)lParam == hSliderTDP) {
            g_currentTDP = (int)SendMessage(hSliderTDP, TBM_GETPOS, 0, 0);
            LegionPower::SetTDP(g_currentTDP); InvalidateRect(hwnd, NULL, FALSE);
        }
        else if ((HWND)lParam == hSliderBright) {
            g_currentBrightness = (int)SendMessage(hSliderBright, TBM_GETPOS, 0, 0);
            LegionScreen::SetBrightness(g_currentBrightness); InvalidateRect(hwnd, NULL, FALSE);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case BTN_QUIET: SetThermalMode(1); EnableWindow(hSliderTDP, FALSE); break;
        case BTN_BALANCED: SetThermalMode(2); EnableWindow(hSliderTDP, FALSE); break;
        case BTN_PERFORMANCE: SetThermalMode(3); EnableWindow(hSliderTDP, FALSE); break;
        case BTN_CUSTOM: SetThermalMode(255); EnableWindow(hSliderTDP, TRUE); g_currentTDP = 9; SendMessage(hSliderTDP, TBM_SETPOS, TRUE, 9); LegionPower::SetTDP(9); break;
        case BTN_MOUSE_TOGGLE:
            g_controllerMode.store((g_controllerMode.load() + 1) % 3);
            EnableWindow(hSlider, (g_controllerMode.load() != MODE_OFF));
            InvalidateRect(hwnd, NULL, TRUE); if (!g_appMuted) Beep(700, 100);
            break;
        case BTN_REFRESH_TOGGLE:
        {
            int newHz = (g_currentHz > 100) ? 60 : 144;
            g_currentHz = newHz; g_lastHzChangeTick = GetTickCount64();
            if (!g_appMuted) Beep(600, 100);
            RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            std::thread([hwnd, newHz]() {
                LegionScreen::SetRefreshRate(newHz);
                std::this_thread::sleep_for(std::chrono::milliseconds(4000));
                PostMessage(hwnd, WM_REFRESH_AFTER_HZ, 0, 0);
                }).detach();
        }
        break;
        case BTN_CLOSE: ToggleVisibility(hwnd); break;
        case ID_TRAY_ABOUT: ShowAboutWindow(GetModuleHandle(NULL), hwnd); break;
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
            AppendMenuW(hM, MF_STRING, ID_TRAY_ABOUT, L"About GO-Helper");
            AppendMenuW(hM, MF_STRING, ID_TRAY_EXIT, L"Exit");
            POINT pt; GetCursorPos(&pt); SetForegroundWindow(hwnd);
            TrackPopupMenu(hM, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hM);
        }
        else if (lParam == WM_LBUTTONUP) ToggleVisibility(hwnd);
        break;

    case WM_DESTROY:
        g_running = false; KillTimer(hwnd, refreshTimer);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        if (g_hBackBrush) DeleteObject(g_hBackBrush);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// --- GLOBAL HOTKEY HOOK (CTRL + G) ---
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* pK = (KBDLLHOOKSTRUCT*)lParam;
        bool isCtrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
        if (isCtrlDown && pK->vkCode == 'G') { ToggleVisibility(g_hwnd); return 1; }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE, PWSTR, int) {
    if (!IsRunAsAdmin()) { ElevateNow(); return 0; }
    INITCOMMONCONTROLSEX ic = { sizeof(ic), ICC_BAR_CLASSES }; InitCommonControlsEx(&ic);
    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = WindowProc; wc.hInstance = hI; wc.lpszClassName = L"GOHCLASS";
    wc.hIcon = LoadIconW(hI, MAKEINTRESOURCEW(IDI_ICON1)); wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, L"GOHCLASS", L"GO-Helper", WS_POPUP | WS_CLIPCHILDREN, 0, 0, WIN_WIDTH, WIN_HEIGHT, NULL, NULL, hI, NULL);
    SetLayeredWindowAttributes(g_hwnd, 0, 250, LWA_ALPHA);
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hI, 0);
    std::thread inputThread(ControllerThreadWrapper);
    inputThread.detach();
    ShowWindow(g_hwnd, SW_HIDE);
    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) { TranslateMessage(&m); DispatchMessage(&m); }
    if (g_hHook) UnhookWindowsHookEx(g_hHook);
    return 0;
}
