# Plan: Wi-Fi Configuration Portal & Auto Fallback (Option B)

This project plan details the implementation of a custom Wi-Fi Access Point (AP) mode with a configuration web interface and auto-fallback behavior. When no credentials are saved or connection fails, the ESP32 will host its own Wi-Fi network. Users can connect, view the battery analyzer, or submit Wi-Fi credentials that save to the Flash memory (Preferences) of the ESP32.

---

## Project Type
- **BACKEND (Embedded Firmware C++ / Arduino)**

---

## Success Criteria

1. **Auto-AP Mode on Boot**: If no credentials are saved in the ESP32 Flash memory, or if the ESP32 fails to connect to the saved Wi-Fi network within 8 seconds, it will start its own Wi-Fi Access Point named `OBI-ESP32-Makita` (default IP `192.168.4.1`).
2. **Web Portal UI (Two Options)**:
   - **Tab 1: Battery Analyzer**: Direct access to the current battery diagnostic dashboard.
   - **Tab 2: Wi-Fi Settings**: Displays a Wi-Fi scanning list and a form to submit new Wi-Fi credentials.
3. **Persisted Credentials**: Wi-Fi credentials submitted via the form are saved to ESP32 Non-Volatile Storage (NVS) using the `Preferences` library.
4. **Dynamic Transition**: Upon receiving new Wi-Fi credentials, the ESP32 saves them, sends a success response, and restarts to attempt connection to the new network.
5. **Captive Portal Redirect**: Includes a basic DNS server in AP mode to redirect initial device connections to `192.168.4.1`.

---

## Tech Stack
- **Framework**: Arduino Core for ESP32
- **Storage**: `Preferences.h` (NVS) for persistent Wi-Fi credentials
- **Web UI**: HTML5 / CSS3 / Vanilla JS (embedded inside `src/web_interface.h`)
- **DNS Server**: `DNSServer.h` to facilitate Captive Portal redirection in AP mode

---

## File Structure

- [main.cpp](file:///d:/Cac-du-an-ESP/obi-esp32-makita/src/main.cpp) (Setup AP/STA logic, DNS Server, API endpoints, Preferences saving)
- [web_interface.h](file:///d:/Cac-du-an-ESP/obi-esp32-makita/src/web_interface.h) (Update `INDEX_HTML` to support tabs and Wi-Fi configuration form)

---

## Task Breakdown

### Task 1: Initialize Preferences and Wi-Fi Load Logic
- **Agent**: `backend-specialist`
- **Skill**: `clean-code`
- **Priority**: P0
- **Dependencies**: None
- **INPUT**: `src/main.cpp`
- **OUTPUT**: `src/main.cpp` configured to read SSID/Password from `Preferences` namespace `wifi`.
- **VERIFY**: Serial output shows:
  - `"Preferences loaded. Saved SSID: [SSID]"` if exists.
  - `"No credentials saved. Falling back to AP mode."` if not.

### Task 2: Implement Access Point (SoftAP) and DNS Captive Portal
- **Agent**: `backend-specialist`
- **Skill**: `clean-code`
- **Priority**: P1
- **Dependencies**: Task 1
- **INPUT**: `src/main.cpp`
- **OUTPUT**: Integration of `DNSServer` and SoftAP initiation when STA connection fails after 8 seconds.
- **VERIFY**: ESP32 hosts `OBI-ESP32-Makita` SSID. Connecting to it and attempting to browse redirects to `192.168.4.1`.

### Task 3: Build Wi-Fi Scan & Save REST APIs
- **Agent**: `backend-specialist`
- **Skill**: `api-patterns`
- **Priority**: P1
- **Dependencies**: Task 2
- **INPUT**: `src/main.cpp` WebServer endpoints
- **OUTPUT**:
  - `GET /api/wifi/scan`: Returns a JSON array of scanned SSIDs.
  - `POST /api/wifi/save`: Receives JSON `{ "ssid": "...", "pass": "..." }`, saves to NVS, and schedules a restart after 2 seconds.
- **VERIFY**: Querying endpoints via browser/Postman successfully scans networks and saves new credentials.

### Task 4: Web UI Tab & Config Form Redesign
- **Agent**: `frontend-specialist`
- **Skill**: `frontend-design`
- **Priority**: P2
- **Dependencies**: Task 3
- **INPUT**: `src/web_interface.h`
- **OUTPUT**: Redesigned single-page Web UI containing tabs for "Battery Status" and "Wi-Fi Config" with high-contrast styling matching the dark OBI aesthetic.
- **VERIFY**: Connecting to `192.168.4.1` displays the tabs. Banning public/violet colors.

### Task 5: Compilation and Verification Build
- **Agent**: `test-engineer`
- **Skill**: `testing-patterns`
- **Priority**: P3
- **Dependencies**: Task 4
- **INPUT**: Code edits completed
- **OUTPUT**: Successful builds for both configurations
- **VERIFY**:
  - `pio run -e esp32s3_display` builds successfully.
  - `pio run -e esp32c3_web` builds successfully.

---

## Phase X: Verification

- [ ] Verify both `esp32s3_display` and `esp32c3_web` build successfully.
- [ ] Verify that ESP32 runs `setupDisplay()` immediately on startup.
- [ ] Verify that NVS credentials saving does not cause memory leaks or core panics.
