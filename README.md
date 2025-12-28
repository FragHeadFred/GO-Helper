# GO-Helper (v0.125)

**GO-Helper** is a high-performance, lightweight Win32 utility designed specifically for the Lenovo Legion Go. It replaces heavy OEM software with a surgical, low-level C++ tool that provides essential hardware controls, controller-to-mouse emulation, and system monitoring—all within a **2.3MB memory footprint**.

---

## 🚀 Full Feature List

### 🛠 Hardware & System Control
* **WMI SKU/Model Detection:** Automatically identifies your specific Legion Go model (e.g., "Legion Go 8APU1") and BIOS SKU for accurate hardware identification in the header.
* **Thermal Mode Control:** Direct WMI injection to toggle between **Quiet (Blue)**, **Balanced (White)**, and **Performance (Red)** modes.
* **MS-Gamebar Fix:** Integrated registry patcher to disable the "Press Win+G to open Game Bar" pop-up that disrupts handheld gaming.
* **Real-time Battery Status:** Tracks percentage and AC/DC power state (Plugged In vs. Discharging).
* **CPU Temperature Monitoring:** Real-time polling of MSAcpi thermal zones, displaying values in both **Celsius and Fahrenheit**.

### 🎮 Input & Navigation
* **Controller-to-Mouse Emulation:** * **RS (Right Stick):** Smooth mouse cursor movement.
    * **RB (Right Bumper):** Left Click.
    * **RT (Right Trigger):** Right Click.
* **Dynamic Sensitivity:** A custom-rendered slider allows for 1%–100% sensitivity adjustments with a real-time aura-glow thumb tracker.
* **Hardware Button Intercept:** Low-level keyboard hook to capture the **Legion L + X** hardware button combo to summon the app.
* **Global Hotkey:** `Ctrl + G` support for standard keyboard summoning.

### 🖥 Window & UI Management
* **Topmost Focus Logic:** When summoned, the app uses `SetForegroundWindow` and `SetFocus` to ensure it breaks through full-screen games and takes immediate input.
* **Bottom-Right Docking:** Automatically calculates screen resolution to anchor the window to the bottom-right corner.
* **System Tray Integration:** * **Muted by Default:** Application audio (thermal switch beeps) is muted by default on startup.
    * **Context Menu:** Quick access to Show/Hide, Mute Toggle, Gamebar Fix, and Exit.
* **Ultra-Lightweight UI:** Built with pure Win32 GDI and DWM (Desktop Window Manager) attributes for rounded corners and border coloring without GPU overhead.

---

## 🎮 Input Mapping

| Input | Action |
| :--- | :--- |
| **Right Stick (RS)** | Mouse Cursor Movement |
| **Right Bumper (RB)** | Left Mouse Click |
| **Right Trigger (RT)** | Right Mouse Click |
| **Legion L + X** | Toggle GO-Helper Menu |
| **Ctrl + G** | Toggle GO-Helper Menu (Keyboard) |

---

## 🔧 Installation & Usage

1. **Download:** Grab the latest `GO-Helper.exe` from the Releases section.
2. **Permissions:** Run as **Administrator**. This is required to access the WMI `ROOT\WMI` namespace for thermal control and to modify the Registry for the Gamebar fix.
3. **Startup:** The app starts minimized in the System Tray. 
4. **Summoning:** Press the **Legion L + X Button** or **Ctrl + G** to bring up the control panel.
5. **Audio:** By default, beeps for thermal mode switching are muted. Right-click the Tray Icon and uncheck "Mute Sounds" if you prefer audible feedback.

---

## 🛠 Technical Specifications

* **Language:** C++ / Win32 API
* **Memory Footprint:** ~2.3 MB
* **Binary Size:** Optimized < 500 KB
* **OS Support:** Windows 10 / 11
* **Hardware Support:** Lenovo Legion Go (Handheld)

---

## 📜 Version History (v0.125)
* **v0.125:** Added **Legion L + X** hardware intercept. Forced `TOPMOST` focus logic.
* **v0.122:** Added WMI SKU/Model detection and dual-unit (C/F) temperature tracking.
* **v0.115:** Implemented muted-by-default logic and system tray menu.
* **v0.100:** Initial core release with XInput mouse bridge and thermal controls.

---

## ⚖️ License
Distributed under the MIT License.
