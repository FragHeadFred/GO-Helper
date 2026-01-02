# GO-Helper (v0.129)

**GO-Helper** is a high-performance, lightweight Win32 utility designed specifically for the Lenovo Legion Go. It replaces heavy OEM software with a native C++ tool providing essential hardware controls, controller-to-mouse emulation, and system monitoring all within an ultra-lean **2.6MB memory footprint**.

---

## [:floppy_disk: Download](https://github.com/FragHeadFred/GO-Helper/releases/download/v.0.129.2026.01.02-alpha/GO-Helper.exe)

## 🚀 Full Feature List

### 🛠  Hardware & System Control
* **Legion R Listener:** High-speed raw HID implementation monitoring Byte 18, Bit 6. Pressing the Legion R button instantly toggles UI visibility.
* **WMI SKU/Model Detection:** Dynamically identifies specific Legion Go model variants and BIOS SKUs for accurate hardware communication.
* **Thermal Mode Control:** Direct WMI method injection for **Quiet (Blue)**, **Balanced (White)**, and **Performance (Red)** fan profiles.
* **MS-Gamebar Fix:** Integrated registry patcher to suppress intrusive Windows Game Bar pop-ups during gameplay.
* **Admin Check:** Integrated security token verification; the app auto-elevates on startup to ensure WMI, Registry, and HID access.

### 🎮 Input & Navigation
* **Controller-to-Mouse Emulation:**
    * **RS (Right Stick):** High-polling mouse cursor movement.
    * **RB (Right Bumper):** Left Mouse Click.
    * **RT (Right Trigger):** Right Mouse Click.
* **Dynamic Sensitivity:** Custom-rendered "Red Ball" slider for 1%â€“100% sensitivity adjustments with a real-time aura-glow tracker.
* **Global Hotkey:** Full support for `Ctrl + G` summoning via low-level keyboard hook.

### 📟 Real-time Monitoring
* **Battery / Charge Status:** Live tracking of battery percentage and power state (Plugged In / Discharging).
* **CPU Temperature Tracking:** Continuous polling of ACPI thermal zones displaying values in both **Celsius and Fahrenheit**.

### 🖥 Window & UI Management
* **Topmost Focus Logic:** Uses `AttachThreadInput` and `SetForegroundWindow` to ensure the app restores and focuses over any active full-screen application or game.
* **Close Button:** A specialized rounded component (Black BG, Red Border, White X) in the top-right corner for rapid UI dismissal.
* **System Tray Integration:**
    * **Muted by Default:** Audio feedback for thermal changes is disabled at startup (Toggleable).
    * **Persistence:** "Start with Windows" tray option via Registry `Run` key integration.
* **Zero Overhead UI:** Built with pure native Win32/C++ and GDI for minimal RAM usage and zero GPU impact.

---

## 🎮 Input Mapping

| Input | Action |
| :--- | :--- |
| **Legion R Button** | Summon / Hide GO-Helper |
| **Right Stick (RS)** | Mouse Cursor Movement |
| **Right Bumper (RB)** | Left Mouse Click |
| **Right Trigger (RT)** | Right Mouse Click |
| **Ctrl + G** | Toggle GO-Helper Menu (Keyboard) |
| **Close Button (âœ•)** | Hide UI to Tray |

---

## 🔧 Installation & Usage

1.  **Download:** Download the latest `GO-Helper.exe`.
2.  **Permissions:** Run the application. It will automatically request **Administrator** privileges (Required for HID Listener and WMI calls).
3.  **Summoning:** Press the hardware **Legion R** button or use the `Ctrl + G` hotkey.
4.  **Auto-Start:** To ensure GO-Helper is always ready, right-click the tray icon and select **"Start with Windows"**.

---

## 🛠  Technical Specifications

* **Language:** C++ / Win32 API
* **Memory Footprint:** ~2.6 MB
* **OS Support:** Windows 10 / 11
* **Hardware Requirement:** Lenovo Legion Go

---

## 📜 Version History (v0.129.2026.01.02)
* **v0.129:** Implemented Close/Hide UI button and real-time Battery/Charge tracking.
* **v0.128:** Added Hardware-level Raw HID listener for Legion R button support.
* **v0.127:** Integrated Admin Check and advanced Topmost focus-stealing logic.
* **v0.126:** Added "Start with Windows" persistence logic.

---

## ⚖️ Disclaimers
* THE SOFTWARE IS PROVIDED “AS IS” AND WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. MISUSE OF THIS SOFTWARE COULD CAUSE SYSTEM INSTABILITY OR MALFUNCTION.
