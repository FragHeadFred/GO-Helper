/* GO-Helper Application 
   Version: 0.131.2026.01.02
   Update: Threaded Input, Centered About, Visual Fixes
   Features: 
   - Threading: Controller input runs in a separate thread (Fixes Tray Lag).
   - About Window: Pops up in center screen, fixed button corners.
   - Slider: Instant repainting to prevent artifacts.
   - Touchpad: Exclusive Mode with Tap-to-Click.
   - Controller Mode: Analog / Touchpad / Mouse Off.
   - UI: "Controller Mode:" label with dynamic status text.
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

#define APP_VERSION L"0.131.2026.01.02" // Internal app version tracking
#define _WIN32_WINNT 0x0601             // Target Windows 7 SP1 or newer
#define _WIN32_DCOM                     // Enable DCOM for WMI initialization

#include <windows.h>                    // Main Windows API
#include <comdef.h>                     // COM standard types
#include <WbemIdl.h>                    // WMI Interfaces
#include <string>                       // Standard string manipulation
#include <shellapi.h>                   // Shell functionality (Tray/Elevation/Links)
#include <vector>                       // Container support
#include <thread>                       // Multi-threading for Input/Listeners
#include <atomic>                       // Thread-safe variable access
#include <cmath>                        // Mathematical functions
#include <chrono>                       // High-resolution timing
#include <Xinput.h>                     // Controller input API
#include <commctrl.h>                   // Common controls (Trackbar)
#include <uxtheme.h>                    // Themeing and visual styles
#include <dwmapi.h>                     // DWM window management
#include "resource.h"                   // Resource IDs (Icons)

// --- LIBRARIES ---
#pragma comment(lib, "wbemuuid.lib")    // Link WMI
#pragma comment(lib, "user32.lib")      // Link User Interface
#pragma comment(lib, "shell32.lib")     // Link Shell Operations
#pragma comment(lib, "gdi32.lib")       // Link Graphics Interface
#pragma comment(lib, "ole32.lib")       // Link COM Core
#pragma comment(lib, "oleaut32.lib")    // Link OLE Automation
#pragma comment(lib, "advapi32.lib")    // Link Registry/Security
#pragma comment(lib, "Xinput.lib")      // Link Controller Support
#pragma comment(lib, "comctl32.lib")    // Link Common Controls
#pragma comment(lib, "uxtheme.lib")     // Link UI Themeing
#pragma comment(lib, "dwmapi.lib")      // Link DWM Features

// --- CONSTANTS ---
#define BTN_QUIET 101                   // Quiet Mode Button ID
#define BTN_BALANCED 102                // Balanced Mode Button ID
#define BTN_PERFORMANCE 103             // Performance Mode Button ID
#define BTN_MOUSE_TOGGLE 104            // Controller Mode Cycle ID
#define SLIDER_SENSE 105                // Sensitivity Trackbar ID
#define BTN_CLOSE 107                   // Close Button ID
#define BTN_ABOUT_CLOSE_BOTTOM 109      // About Window Close Button ID
#define WM_TRAYICON (WM_USER + 1)       // Custom Tray Message
#define ID_TRAY_ABOUT 200               // Menu About ID
#define ID_TRAY_EXIT 201                // Menu Exit ID
#define ID_TRAY_TOGGLE 202              // Menu Show/Hide ID
#define ID_TRAY_DISABLE_GB 203          // Menu Gamebar Fix ID
#define ID_TRAY_MUTE_APP 204            // Menu Mute Toggle ID
#define ID_TRAY_START_WITH_WIN 205      // Menu Auto-start ID

// --- UI THEME COLORS ---
#define CLR_BACK      RGB(20, 20, 20)   // Dark background
#define CLR_CARD      RGB(45, 45, 45)   // Button base color
#define CLR_TEXT      RGB(240, 240, 240)// White text
#define CLR_QUIET     RGB(0, 102, 204)  // Blue for Quiet/Touchpad
#define CLR_BAL       RGB(255, 255, 255)// White for Balanced
#define CLR_PERF      RGB(178, 34, 34)  // Red for Performance
#define CLR_RED       RGB(255, 0, 0)    // Pure red for warnings/close
#define CLR_AURA      RGB(40, 40, 40)   // Aura effect color
#define CLR_ACCENT    RGB(0, 180, 90)   // Green for success/Active
#define CLR_VERSION   RGB(160, 160, 160)// Muted gray for meta info
#define CLR_DISABLED  RGB(80, 80, 80)   // Inactive gray
#define CLR_LINK      RGB(80, 180, 255) // Light blue for hyperlinks

// --- ENUMS & GLOBALS ---
enum ControllerMode { MODE_ANALOG = 0, MODE_TOUCHPAD = 1, MODE_OFF = 2 }; // Mode enumeration

HWND g_hwnd = NULL;                     // Main window handle
HWND g_aboutHwnd = NULL;                // About window handle
HHOOK g_hHook = NULL;                   // Global keyboard hook
NOTIFYICONDATAW g_nid = { 0 };          // Tray data structure
HICON g_hMainIcon = NULL;               // Application icon handle
HBRUSH g_hBackBrush = NULL;             // Main BG brush
bool g_appMuted = true;                 // Audible feedback state

std::atomic<int> g_controllerMode(MODE_ANALOG); // Thread-safe mode state
std::atomic<bool> g_running(true);      // Application execution flag

int g_currentSenseVal = 5;              // Sensitivity integer (1-50)
float g_mouseSensitivity = 5 * 0.0005f; // Actual mouse multiplier

const int WIN_WIDTH = 350;              // App width
const int WIN_HEIGHT = 255;             // App height

// --- FORWARD DECLARATIONS ---
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam); // Hook handler
LRESULT CALLBACK AboutProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam); // About window handler
LRESULT CALLBACK SliderSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData); // Slider painter
void ToggleVisibility(HWND hwnd);       // Visibility toggle logic
void DrawGButton(HDC hdc, RECT rc, LPCWSTR text, COLORREF color, bool pressed); // GDI Button painter

// --- CONTROLLER THREAD FUNCTION ---
void ControllerThread() {
    XINPUT_STATE state;                 // Store controller data
    bool rbPressed = false;             // Track bumper state
    bool rtPressed = false;             // Track trigger state

    while (g_running) {                 // Run while app is alive
        if (g_controllerMode.load() == MODE_ANALOG) { // Only run if in Analog Mode
            if (XInputGetState(0, &state) == ERROR_SUCCESS) { // Poll first controller
                short rx = state.Gamepad.sThumbRX; // Get right stick X
                short ry = state.Gamepad.sThumbRY; // Get right stick Y
                const int deadzone = 8000;         // Prevent stick drift
                int magX = (abs(rx) < deadzone) ? 0 : (rx > 0 ? rx - deadzone : rx + deadzone); // Calculate X magnitude
                int magY = (abs(ry) < deadzone) ? 0 : (ry > 0 ? ry - deadzone : ry + deadzone); // Calculate Y magnitude
                
                if (magX != 0 || magY != 0) {   // If stick is moved
                    INPUT move = { 0 };         // Create mouse input
                    move.type = INPUT_MOUSE;    // Mouse type
                    move.mi.dwFlags = MOUSEEVENTF_MOVE; // Movement flag
                    move.mi.dx = static_cast<long>(magX * g_mouseSensitivity); // X movement
                    move.mi.dy = static_cast<long>(-magY * g_mouseSensitivity); // Y movement (Inverted)
                    SendInput(1, &move, sizeof(INPUT)); // Inject movement
                }

                bool rbNow = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0; // Check RB state
                if (rbNow != rbPressed) {       // On state change
                    INPUT click = { 0 }; click.type = INPUT_MOUSE; // Create mouse input
                    click.mi.dwFlags = rbNow ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; // Set click action
                    SendInput(1, &click, sizeof(INPUT)); rbPressed = rbNow; // Inject click
                }

                bool rtNow = (state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD); // Check RT state
                if (rtNow != rtPressed) {       // On state change
                    INPUT click = { 0 }; click.type = INPUT_MOUSE; // Create mouse input
                    click.mi.dwFlags = rtNow ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP; // Set click action
                    SendInput(1, &click, sizeof(INPUT)); rtPressed = rtNow; // Inject click
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Throttle loop to 200Hz
    }
}

// --- LEGION TRIGGER CLASS ---
class LegionTrigger {
public:
    static void Start(HWND targetWindow) {
        std::thread([targetWindow]() { MonitorController(targetWindow); }).detach(); // Detach button listener
    }
private:
    static void MonitorController(HWND targetWindow) {
        std::string devicePath = "\\\\?\\hid#vid_17ef&pid_61eb&mi_02#8&ece5261&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}"; // Vendor path
        HANDLE hDevice = CreateFileA(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL); // Open device
        if (hDevice == INVALID_HANDLE_VALUE) return; // Exit if no device

        unsigned char buffer[64];       // Input report buffer
        DWORD bytesRead = 0;            // Status holder
        bool wasPressed = false;        // state flag

        while (g_running) {
            if (ReadFile(hDevice, buffer, 64, &bytesRead, NULL)) { // Read report
                bool isPressed = (buffer[18] & 0x40) == 0x40; // Bit check for Legion R
                if (isPressed && !wasPressed) {
                    PostMessage(targetWindow, WM_COMMAND, ID_TRAY_TOGGLE, 0); // Toggle UI
                    wasPressed = true;
                } else if (!isPressed) {
                    wasPressed = false;
                }
            } else break; 
        }
        CloseHandle(hDevice); // Clean up
    }
};

// --- LEGION PAD CLASS ---
class LegionPad {
public:
    static void Start() {
        std::thread(MonitorTouchpad).detach(); // Detach touchpad thread
    }
private:
    static void MonitorTouchpad() {
        const char* devicePath = "\\\\?\\hid#vid_17ef&pid_61eb&mi_02#8&ece5261&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}"; // Vendor path
        const int MIN_X = 50, MAX_X = 950, MIN_Y = 50, MAX_Y = 950; // Touchpad range
        const double SMOOTHING = 0.5;   // Input filter
        const int TAP_TIMEOUT_MS = 200; // Tap duration limit
        const int TAP_DRIFT_TOL = 20;   // Tap movement tolerance
        const int MIDDLE_X_LIMIT = 500; // Left/Right tap divisor

        HANDLE hDevice = CreateFileA(devicePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL); // Open device
        if (hDevice == INVALID_HANDLE_VALUE) {
             hDevice = CreateFileA(devicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        }
        if (hDevice == INVALID_HANDLE_VALUE) return; // Exit if access failed

        unsigned char buffer[64];       // Report buffer
        DWORD bytesRead = 0;            // Status holder
        double lastX = -1.0, lastY = -1.0, remX = 0.0, remY = 0.0; // Movement variables
        bool wasDown = false;           // Touchdown flag
        std::chrono::steady_clock::time_point tapStartTime; // Timer initialization
        int tapStartX = 0; int tapStartY = 0; bool possibleTap = false; // Click logic flags

        while (g_running) {
            if (ReadFile(hDevice, buffer, 64, &bytesRead, NULL)) { // Read report
                if (g_controllerMode.load() != MODE_TOUCHPAD) {   // Verify mode
                    lastX = -1.0; wasDown = false; possibleTap = false; continue; 
                }
                int rawX = (buffer[26] << 8) | buffer[27]; // Parse X
                int rawY = (buffer[28] << 8) | buffer[29]; // Parse Y
                bool isDown = (rawX != 0 || rawY != 0);    // Check touch

                if (isDown) { // If finger is touching
                    if (rawX < MIN_X) rawX = MIN_X; if (rawX > MAX_X) rawX = MAX_X; // Clamp X
                    if (rawY < MIN_Y) rawY = MIN_Y; if (rawY > MAX_Y) rawY = MAX_Y; // Clamp Y
                    double normX = static_cast<double>(rawX); // Convert to double
                    double normY = static_cast<double>(rawY); // Convert to double

                    if (wasDown && lastX != -1.0) { // If finger is moving
                        double scale = (g_currentSenseVal * 0.3); // Set scaling factor
                        double deltaX = (normX - lastX) * scale; // Calculate X change
                        double deltaY = (normY - lastY) * scale; // Calculate Y change
                        if (possibleTap) { // Invalidate tap if finger moves too much
                            int totalDrift = std::abs(rawX - tapStartX) + std::abs(rawY - tapStartY);
                            if (totalDrift > TAP_DRIFT_TOL) possibleTap = false; 
                        }
                        if (std::abs(deltaX) < 1.0) deltaX = 0.0; // Noise reduction
                        if (std::abs(deltaY) < 1.0) deltaY = 0.0; // Noise reduction
                        remX = (SMOOTHING * deltaX) + ((1.0 - SMOOTHING) * remX); // Apply smoothing
                        remY = (SMOOTHING * deltaY) + ((1.0 - SMOOTHING) * remY); // Apply smoothing
                        int moveX = static_cast<int>(remX); // Cast to pixel move
                        int moveY = static_cast<int>(remY); // Cast to pixel move
                        if (moveX != 0 || moveY != 0) {
                            mouse_event(MOUSEEVENTF_MOVE, moveX, moveY, 0, 0); // Inject movement
                            remX -= moveX; remY -= moveY;
                        }
                    } else { // On initial contact
                        remX = 0.0; remY = 0.0;
                        tapStartTime = std::chrono::steady_clock::now(); // Start tap timer
                        tapStartX = rawX; tapStartY = rawY; possibleTap = true;
                    }
                    lastX = normX; lastY = normY; wasDown = true;
                } else { // If finger is lifted
                    if (wasDown && possibleTap) { // Check for click validation
                        auto endTime = std::chrono::steady_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - tapStartTime).count();
                        if (duration < TAP_TIMEOUT_MS) { // If lift happened fast enough
                            if (tapStartX < MIDDLE_X_LIMIT) { // Left half click
                                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                            } else { // Right half click
                                mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
                                mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
                            }
                        }
                    }
                    wasDown = false; lastX = -1.0; possibleTap = false;
                }
            } else break;
        }
        CloseHandle(hDevice); // Clean up
    }
};

// --- LOGIC: SYSTEM ---
bool IsRunAsAdmin() { // Check process security token
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup)) {
        CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin);
        FreeSid(pAdministratorsGroup);
    }
    return fIsRunAsAdmin;
}

void ElevateNow() { // Restart as administrator
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas"; sei.lpFile = szPath; sei.hwnd = NULL; sei.nShow = SW_NORMAL;
        if (!ShellExecuteExW(&sei)) return; else exit(0);
    }
}

bool IsAutoStartEnabled() { // Check startup registry
    HKEY hKey; bool enabled = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"GO-Helper", NULL, NULL, NULL, NULL) == ERROR_SUCCESS) enabled = true;
        RegCloseKey(hKey);
    }
    return enabled;
}

void SetAutoStart(bool enable) { // Write app path to run key
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t path[MAX_PATH]; GetModuleFileNameW(NULL, path, MAX_PATH);
            RegSetValueExW(hKey, L"GO-Helper", 0, REG_SZ, (const BYTE*)path, static_cast<DWORD>((wcslen(path) + 1) * sizeof(wchar_t)));
        } else RegDeleteValueW(hKey, L"GO-Helper");
        RegCloseKey(hKey);
    }
}

// --- FULL WMI IMPLEMENTATION ---
std::wstring GetCPUTempString() { // Fetch CPU temp via WMI
    long tempDK = 0;
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
    double celsius = (tempDK / 10.0) - 273.15;
    double fahrenheit = (celsius * 9.0 / 5.0) + 32.0;
    wchar_t buf[64]; swprintf_s(buf, L"CPU: %.0f°C / %.0f°F", celsius, fahrenheit);
    return std::wstring(buf);
}

void DisableGameBarRegistry() { // Disable GameDVR capture
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD dwVal = 0; RegSetValueExW(hKey, L"AppCaptureEnabled", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(dwVal)); RegCloseKey(hKey);
    }
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"System\\GameConfigStore", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD dwVal = 0; RegSetValueExW(hKey, L"GameDVR_Enabled", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(dwVal)); RegCloseKey(hKey);
    }
    MessageBoxW(NULL, L"Game Bar features disabled. Restart recommended.", L"GO-Helper", MB_OK | MB_ICONINFORMATION);
}

std::wstring GetThermalModeString() { // Get mode from Lenovo WMI
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
    if (mode == 1) return L"Quiet";
    if (mode == 2) return L"Balanced";
    if (mode == 3) return L"Performance";
    return L"Unknown";
}

std::wstring GetBatteryStatusString() { // Fetch percentage and AC status
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) return L"Battery: Unknown";
    std::wstring status = L"Battery: ";
    status += (sps.ACLineStatus == 1) ? L"Plugged In" : L"Discharging";
    status += L" @ " + std::to_wstring((int)sps.BatteryLifePercent) + L"%";
    return status;
}

bool SetThermalMode(int value) { // Write fan mode via WMI
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
    if (success && !g_appMuted) Beep(800 + (value * 100), 100);
    return success;
}

std::wstring GetSystemSKU() { // Fetch SKU and Model from CIMV2
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
void RepositionToBottomRight(HWND hwnd) { // Dock UI above tray
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
        int x = mi.rcWork.right - WIN_WIDTH - 20;
        int y = mi.rcWork.bottom - WIN_HEIGHT - 20;
        SetWindowPos(hwnd, HWND_TOPMOST, x, y, WIN_WIDTH, WIN_HEIGHT, SWP_SHOWWINDOW);
    }
}

void ToggleVisibility(HWND hwnd) { // UI Show/Hide with focus grab
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

void DrawGButton(HDC hdc, RECT rc, LPCWSTR text, COLORREF color, bool pressed) { // Paint rounded component
    HBRUSH hBackBr = CreateSolidBrush(CLR_BACK); FillRect(hdc, &rc, hBackBr); DeleteObject(hBackBr);
    HBRUSH hBr = CreateSolidBrush(pressed ? color : CLR_CARD); HPEN hPen = CreatePen(PS_SOLID, 1, pressed ? RGB(200, 200, 200) : RGB(80, 80, 80));
    SelectObject(hdc, hPen); SelectObject(hdc, hBr); RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 12, 12);
    SetTextColor(hdc, CLR_TEXT); SetBkMode(hdc, TRANSPARENT); DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hBr); DeleteObject(hPen);
}

// --- SLIDER PAINT ---
LRESULT CALLBACK SliderSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) { // Custom trackbar painter
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

// --- ABOUT WINDOW ---
void ShowAboutWindow(HINSTANCE hI, HWND parent) { // Centered popup initialization
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

LRESULT CALLBACK AboutProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { // About proc logic
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
        RECT r1 = {20, 50, 380, 75}; DrawTextW(hdc, L"Created and Programmed by FragHeadFred", -1, &r1, DT_LEFT);
        SetTextColor(hdc, CLR_LINK);
        RECT r2 = {20, 85, 380, 110}; DrawTextW(hdc, L"https://github.com/FragHeadFred/GO-Helper", -1, &r2, DT_LEFT);
        RECT r3 = {20, 120, 380, 170}; DrawTextW(hdc, L"https://www.paypal.com/donate/?hosted_button_id=PA5MTBGWQMUP4", -1, &r3, DT_LEFT | DT_WORDBREAK);
        EndPaint(hwnd, &ps);
    } break;
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT p = (LPDRAWITEMSTRUCT)lParam;
        if (p->CtlID == BTN_CLOSE) {
            HBRUSH hBack = CreateSolidBrush(CLR_BACK); FillRect(p->hDC, &p->rcItem, hBack); DeleteObject(hBack);
            HBRUSH hbb = CreateSolidBrush(RGB(0, 0, 0)); HPEN hrb = CreatePen(PS_SOLID, 1, CLR_RED);
            SelectObject(p->hDC, hbb); SelectObject(p->hDC, hrb); RoundRect(p->hDC, p->rcItem.left, p->rcItem.top, p->rcItem.right, p->rcItem.bottom, 8, 8);
            SetTextColor(p->hDC, RGB(255, 255, 255)); SetBkMode(p->hDC, TRANSPARENT); DrawTextW(p->hDC, L"✕", -1, &p->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DeleteObject(hbb); DeleteObject(hrb);
        } else if (p->CtlID == BTN_ABOUT_CLOSE_BOTTOM) {
            DrawGButton(p->hDC, p->rcItem, L"Close", CLR_CARD, p->itemState & ODS_SELECTED);
        }
        return TRUE;
    }
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        if (y > 80 && y < 110) ShellExecuteW(0, L"open", L"https://github.com/FragHeadFred/GO-Helper", 0, 0, 1);
        if (y > 115 && y < 170) ShellExecuteW(0, L"open", L"https://www.paypal.com/donate/?hosted_button_id=PA5MTBGWQMUP4", 0, 0, 1);
    } break;
    case WM_COMMAND: 
        if(LOWORD(wParam) == BTN_CLOSE || LOWORD(wParam) == BTN_ABOUT_CLOSE_BOTTOM) DestroyWindow(hwnd); 
        break;
    case WM_DESTROY: g_aboutHwnd = NULL; break;
    default: return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    } return 0;
}

// --- MAIN WINDOW PROC ---
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
        LegionPad::Start(); 

        refreshTimer = SetTimer(hwnd, 1, 3000, NULL);
        COLORREF auraColor = CLR_AURA; DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &auraColor, sizeof(auraColor));
        DWM_WINDOW_CORNER_PREFERENCE cp = DWMWCP_ROUND; DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cp, sizeof(cp));
    }
    break;

    case WM_TIMER: if (wParam == 1 && IsWindowVisible(hwnd)) InvalidateRect(hwnd, NULL, FALSE); break;

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HDC memDC = CreateCompatibleDC(hdc); HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        SelectObject(memDC, memBM);
        FillRect(memDC, &rc, g_hBackBrush);
        if (g_hMainIcon) DrawIconEx(memDC, 20, 15, g_hMainIcon, 18, 18, 0, NULL, DI_NORMAL);
        SelectObject(memDC, hFontBold); SetTextColor(memDC, CLR_TEXT); SetBkMode(memDC, TRANSPARENT);
        std::wstring title = L"GO-Helper"; TextOutW(memDC, 45, 17, title.c_str(), (int)title.length());
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
        SetTextColor(memDC, CLR_TEXT);
        std::wstring cLabel = L"Controller Mode: ";
        TextOutW(memDC, 20, 125, cLabel.c_str(), (int)cLabel.length());
        SIZE sizeLab; GetTextExtentPoint32W(memDC, cLabel.c_str(), (int)cLabel.length(), &sizeLab);
        int cm = g_controllerMode.load();
        std::wstring cModeStr = L"Analog";
        COLORREF cModeClr = CLR_ACCENT;
        if (cm == MODE_TOUCHPAD) { cModeStr = L"Touchpad"; cModeClr = CLR_QUIET; } 
        else if (cm == MODE_OFF) { cModeStr = L"Mouse Off"; cModeClr = CLR_DISABLED; }
        SetTextColor(memDC, cModeClr);
        TextOutW(memDC, 20 + sizeLab.cx, 125, cModeStr.c_str(), (int)cModeStr.length());
        SetTextColor(memDC, (cm != MODE_OFF) ? CLR_TEXT : CLR_DISABLED);
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
            SetTextColor(pdis->hDC, RGB(255, 255, 255)); SetBkMode(pdis->hDC, TRANSPARENT);
            DrawTextW(pdis->hDC, L"✕", -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(pdis->hDC, oldBrush); SelectObject(pdis->hDC, oldPen);
            DeleteObject(hBlackBg); DeleteObject(hRedBorder);
        }
        else {
            if (pdis->CtlID == BTN_QUIET) DrawGButton(pdis->hDC, pdis->rcItem, L"Quiet", CLR_QUIET, prs);
            else if (pdis->CtlID == BTN_BALANCED) DrawGButton(pdis->hDC, pdis->rcItem, L"Balanced", CLR_BAL, prs);
            else if (pdis->CtlID == BTN_PERFORMANCE) DrawGButton(pdis->hDC, pdis->rcItem, L"Performance", CLR_PERF, prs);
            else if (pdis->CtlID == BTN_MOUSE_TOGGLE) {
                int mode = g_controllerMode.load();
                LPCWSTR txt = L"Mode"; 
                COLORREF color = CLR_CARD; 
                if (mode == MODE_ANALOG) color = CLR_QUIET; 
                else if (mode == MODE_TOUCHPAD) color = CLR_CARD; 
                else if (mode == MODE_OFF) color = CLR_ACCENT; 
                DrawGButton(pdis->hDC, pdis->rcItem, txt, color, prs);
            }
        }
        return TRUE;
    }

    case WM_HSCROLL:
        if ((HWND)lParam == hSlider) {
            g_currentSenseVal = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
            g_mouseSensitivity = g_currentSenseVal * 0.0005f; 
            InvalidateRect(hwnd, NULL, FALSE);
            InvalidateRect(hSlider, NULL, FALSE); 
        }
        break;

    case WM_COMMAND:
        switch(LOWORD(wParam)) {
            case BTN_QUIET: SetThermalMode(1); InvalidateRect(hwnd, NULL, FALSE); break;
            case BTN_BALANCED: SetThermalMode(2); InvalidateRect(hwnd, NULL, FALSE); break;
            case BTN_PERFORMANCE: SetThermalMode(3); InvalidateRect(hwnd, NULL, FALSE); break;
            case BTN_MOUSE_TOGGLE: 
                g_controllerMode.store((g_controllerMode.load() + 1) % 3);
                EnableWindow(hSlider, (g_controllerMode.load() != MODE_OFF)); 
                InvalidateRect(hwnd, NULL, TRUE); 
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
        } else if (lParam == WM_LBUTTONUP) ToggleVisibility(hwnd);
        break;

    case WM_DESTROY: 
        g_running = false; 
        KillTimer(hwnd, refreshTimer); 
        Shell_NotifyIconW(NIM_DELETE, &g_nid); 
        if (g_hBackBrush) DeleteObject(g_hBackBrush);
        PostQuitMessage(0); 
        break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) { // Global hook
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

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE, PWSTR, int) { // Entry point
    if (!IsRunAsAdmin()) { ElevateNow(); return 0; }

    INITCOMMONCONTROLSEX ic = { sizeof(ic), ICC_BAR_CLASSES }; InitCommonControlsEx(&ic);
    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = WindowProc; wc.hInstance = hI; wc.lpszClassName = L"GOHCLASS";
    wc.hIcon = LoadIconW(hI, MAKEINTRESOURCEW(IDI_ICON1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, L"GOHCLASS", L"GO-Helper", WS_POPUP | WS_CLIPCHILDREN, 0, 0, WIN_WIDTH, WIN_HEIGHT, NULL, NULL, hI, NULL);
    SetLayeredWindowAttributes(g_hwnd, 0, 250, LWA_ALPHA);

    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hI, 0);

    std::thread inputThread(ControllerThread); // Start input thread
    inputThread.detach();

    ShowWindow(g_hwnd, SW_HIDE);
    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) { // Message loop
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    if (g_hHook) UnhookWindowsHookEx(g_hHook);
    return 0;
}
