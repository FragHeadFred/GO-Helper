# GO-Helper (v0.160)

**GO-Helper** is a high-performance, lightweight Win32 utility designed specifically for the Lenovo Legion Go. It replaces heavy OEM software with a surgical, native C++ tool providing essential hardware controls, controller-to-mouse emulation, and system monitoring—all within an ultra-lean footprint.

---

## [:floppy_disk: Download Latest Release](https://github.com/FragHeadFred/GO-Helper/releases)

## ☕ Support the Project

If you find **GO-Helper** useful and would like to support its continued development and maintenance, consider making a donation.

[**Donate via PayPal**](https://www.paypal.com/donate/?hosted_button_id=PA5MTBGWQMUP4)

---

## 🚀 Full Feature List

### 🛠 Hardware & System Control
* **Hz Toggle:** Instant hardware-level refresh rate switching (60Hz ↔ 144Hz) with optimistic UI updates and a 5-second polling lockout for stability.
* **TDP Control:** Unlocked **Custom Mode (Electric Purple)** allowing precision TDP injection from **9W to 30W** via direct WMI method calls.
* **Brightness Control:** Native WMI-integrated brightness slider for seamless screen adjustments.
* **Thermal Mode Profiles:** Direct WMI injection for **Quiet (9W)**, **Balanced (15W)**, and **Performance (20W)** profiles.
* **Legion L & R Listener:** High-speed raw HID implementation monitoring Byte 18, Bit 6. Pressing the Legion R button instantly toggles UI visibility.
* **Admin Check:** Integrated security token verification; the app auto-elevates on startup to ensure WMI, Registry, and HID access.
* **MS-Gamebar Fix:** Integrated registry patcher to suppress intrusive Windows Game Bar pop-ups during gameplay.

### 🎮 Input & Navigation
* **Threaded Input:** Controller and Mouse polling run in an isolated background thread to ensure zero lag in the System Tray and UI.
* **Exclusive Touchpad Mode:**
    * **Tap-to-Click:** Hardware-level tap detection.
    * **L/R Detection:** Left side tap (< 500) = Left Click | Right side tap (>= 500) = Right Click.
* **Controller-to-Mouse Emulation (Analog):**
    * **RS (Right Stick):** High-polling mouse cursor movement.
    * **RB (Right Bumper):** Left Mouse Click.
    * **RT (Right Trigger):** Right Mouse Click.
* **Dynamic Sensitivity:** Custom-rendered "Red Ball" slider with a real-time aura-glow tracker.
* **Global Hotkey:** Full support for `Ctrl + G` keyboard summoning via low-level hook.

### 📟 Real-time Monitoring
* **CPU Temperature:** Continuous polling of ACPI thermal zones with 1-decimal place accuracy (Celsius and Fahrenheit).
* **Battery Status:** Live tracking of battery percentage and power state (Plugged In / Discharging).
* **WMI SKU Detection:** Dynamically identifies specific Legion Go variants and BIOS SKUs.

### 🖥 Window & UI Management
* **Optimized Layout:** Scaled dimensions (+2%) and matched 25px vertical spacing for perfect component alignment.
* **Visual State Feedback:** The "Mode" button color-codes active input states (Analog: Blue | Touchpad: Grey | Mouse Off: Green).
* **Topmost Focus Logic:** Uses `AttachThreadInput` to break through full-screen games and take immediate focus when summoned.
* **Close Button:** Specialized rounded component (Black BG, Red Border, White X) for rapid UI dismissal.

---

## 🎮 Input Mapping

| Input | Action |
| :--- | :--- |
| **Legion L Button** | Launch Steam / Steam Menu |
| **Legion R Button** | Summon / Hide GO-Helper |
| **Right Stick (RS)** | Mouse Cursor Movement (Analog Mode) |
| **Touchpad Tap** | Left/Right Click (Touchpad Mode) |
| **Right Bumper (RB)** | Left Mouse Click |
| **Right Trigger (RT)** | Right Mouse Click |
| **Ctrl + G** | Toggle GO-Helper Menu (Keyboard) |
| **Close Button (✕)** | Hide UI to Tray |

---

## 🔧 Installation & Usage

1.  **Download:** Download the latest `GO-Helper.exe`.
2.  **Permissions:** Run the application. It will auto-elevate to **Administrator** (Required for HID Listener and WMI).
3.  **Summoning:** Press the hardware **Legion R** button or use the `Ctrl + G` hotkey.
4.  **Auto-Start:** To ensure GO-Helper is always ready, right-click the tray icon and select **"Start with Windows"**.

---

## 🛠 Technical Specifications

* **Language:** C++ / Win32 API / GDI
* **Memory Footprint:** ~2.3 MB - 2.6 MB
* **OS Support:** Windows 10 / 11
* **Hardware Requirement:** Lenovo Legion Go

---

## 📜 Version History
* **v0.140:** Implemented Screen Hz Toggle, Brightness Slider, and forced repaint logic for all child components. Updated UI to Bold headers and "DONATE" caps.
* **v0.133:** Fixed GDI variable declaration conflicts (C2065) in the About window.
* **v0.132:** Added TDP Slider (9W-30W), Custom Thermal Mode, and adjusted layout spacing/button alignment.
* **v0.131:** Offloaded Controller input to a separate thread; implemented Touchpad Tap-to-Click logic.
* **v0.130:** Added "About" window to the system tray menu with functional hyperlinks.
* **v0.129:** Added Close/Hide button and real-time Battery/Charge tracking.

---

## ⚖️ Disclaimers
* THE SOFTWARE IS PROVIDED “AS IS” AND WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED. MISUSE OF THIS SOFTWARE (PARTICULARLY TDP CONTROLS) COULD CAUSE SYSTEM INSTABILITY. USE AT YOUR OWN RISK.
