# Local Wi-Fi setup

On the LCD open Setup → Handset → Wi-Fi / web setup. Setup forces output CUT and disables arming until setup is closed; closing does not arm automatically. Home Wi-Fi auto-connect defaults to ON. The authenticated web server starts automatically on connection; no setup-page visit is needed.

Scan for a 2.4 GHz home network, select it, enter its password with the on-screen keyboard, press Done and Connect. Successful credentials are saved for automatic connection at subsequent boots. Open http://r1evo.local in a browser, or use the IP displayed on the LCD. Browser username is `r1`; password is the saved setup password shown on the LCD (not your home Wi-Fi password).

Alternatively join the R-1-EVO access point shown on the LCD using that same setup password, then open http://192.168.4.1. The password is reused across sessions and restarts; use Change password on the LCD to replace it. This is a local HTTP service, not an Internet-facing or TLS service. Home credentials are stored in the ESP32 preferences storage.

The initial portal edits the active model name and sixteen channels' endpoints, rates, expo, ramps, reversing, trim and sub-trim. Its downloadable parameter snapshot is not a complete model backup.

Map upload accepts full-colour version-2 `.r1map` image packages (up to 512 × 512 RGB565 pixels, 512 KiB pixel data in PSRAM), plus legacy version-1 vector packages and local GeoJSON. Maps cover at most 0.25 degrees per axis. Vector imports retain the 32-path / 512-point / 48 KB limits. GPS views overlay the trace on the saved map near its coordinates. Aerial imagery and logo upload are not implemented yet.

If map storage is uninitialised, explicitly prepare it from the portal. Preparation formats only the asset filesystem; model preferences use a separate partition. Existing mounted storage is never reformatted by this operation. Invalid or interrupted map uploads retain the previous map.

Validation: native tests include map decoding/limits; simulator audits cover setup navigation, keyboard entry, parameter persistence and CUT gating. Physical Wi-Fi connection and browser operation require a hardware trial.

## LCD startup trial

A lit but blank panel occurred after upload even though software boot checks passed. The XIAO Waveshare driver now holds K2 GPB3 LCD_RESET low while Arduino_GFX configures SPI mode 3, then performs the external reset and normal ST7796 initialization. Previously reset was released before SPI initialization, with LCD CS permanently grounded. This sequencing change is a hypothesis under hardware test, not a verified root cause. SPI remains at the requested 24 MHz. Observe the panel before opening serial, because opening the USB serial connection has caused another chip reset in testing.

The reset-order trial at 24 MHz remained blank after a physical reset and two power cycles, despite software initialization and full-frame transfer logs. A subsequent trial changes only the XIAO LCD rate to 4 MHz, retaining Wi-Fi and the reset sequencing change. Hardware result pending.

Grounded-CS bench test: the standalone SPIClass driver initially changed from mode 0 to mode 3 after releasing LCD reset. Moving that mode change under asserted reset restored colour cycling with CS physically grounded at 4 MHz; the user confirmed reset-button and power-cycle startup. Controlled-CS tests had masked this diagnostic sequencing issue. Readback remains unverified and is excluded. This does not establish the cause of the earlier full-application failure.

The full-application follow-up retains the existing mode-before-reset order and now matches the verified bench waits: 500 ms before reset release, 500 ms after release, 500 ms after software reset, and 500 ms after sleep-out and display-on. The XIAO display remains at 4 MHz. Full-application hardware validation is pending. Production CS remains grounded, with no MISO reads.

The full application with longer waits still showed a blank panel, although boot reached LVGL and CUT. Next trial switches only the XIAO Waveshare bus from Arduino_ESP32SPI to Arduino_HWSPI backed by SPIClass(FSPI), as used in the successful bench test. An empty beginWrite/endWrite transaction applies mode 3 while reset remains asserted. Physical MISO remains unused. Hardware result pending.

The user confirmed that the full dashboard works with Arduino_HWSPI/SPIClass at 4 MHz, including reset-button and full power-cycle startup, with CS grounded. This isolates a transport implementation difference, not yet its exact electrical cause. The next user-requested trial changes only the XIAO requested SPI rate to 24 MHz; hardware validation pending.

The user confirmed the full UI works at 24 MHz requested with SPIClass. Retrying 32 MHz requested (the setting that previously failed with the ESP32-specific backend), with no other runtime change. Hardware result pending.

The user confirmed successful operation at 32 MHz requested with SPIClass. Next trial raises only the requested rate to 40 MHz; hardware validation pending.

## Browser map picker and handset assignments

The local portal now offers address/place search (Photon), a Street / Satellite selector and draggable map
with zoom buttons and keyboard panning, and an outlined download area. External
requests run in the browser, so the computer/phone needs internet access (usually
use home Wi-Fi rather than the isolated setup AP). Search only runs on submit;
results are cached for the page session and requests are throttled. No handset
password or session token is sent to map services.

**Download selected area** requests OSM feature data from Overpass and renders a
full-colour offline image in the browser: roads and paths, buildings, green
areas, water and available labels. This is our own cartography from OSM data;
it will differ from the online selection map. Public OSM raster tiles are used
only for online browsing, not saved for offline use.

Review the RGB565 image preview, then **Save map to handset**. Saving replaces
the single installed map; export it first if needed. Exported `.r1map` packages
can be reimported offline.

**Satellite** uses Esri World Imagery for global browsing and a single image
export for the selected area. The browser requests geographic coordinates
(EPSG:4326), crops any aspect-ratio expansion reported by the export service,
and converts the result to the same at-most-512×512 RGB565 package. Provider
credits are baked into the image and retained in metadata. This is a trial
integration with a public service; imagery detail/date and availability vary.
No Google credentials or API key are required. Switching styles discards the
pending preview so an old map cannot accidentally be saved as the new style.
The handset upload and CUT-only write restrictions remain the same.

Image packages contain `R1IM`, a little-endian 32-bit metadata length, bounded
JSON metadata, then raw little-endian RGB565 pixels. Metadata specifies an
equirectangular projection and geographic bounds. Firmware validates metadata,
file length and PSRAM allocation before committing the upload. The LCD samples
only each dirty scanline, leaving the GPS trace and markers as live overlays.


Sources/service policies:
- https://github.com/komoot/photon (public service, moderate usage, no SLA)
- https://operations.osmfoundation.org/policies/tiles/ (online viewing only)
- https://wiki.openstreetmap.org/wiki/Overpass_API
- https://www.openstreetmap.org/copyright (OSM attribution / ODbL)
- https://developers.arcgis.com/rest/services-reference/enterprise/export-map/ (image export and returned extent)
- https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer (imagery service and credits)

The controls panel exposes all 19 existing physical controls, grouped as primary
inputs, trims/switches and the R-1 keypad. Choose a control, assignment, compatible
behaviour and active context, then save. Primary wheel/trigger controls select
channel roles and can be disabled to neutral. Buttons can use existing supported
handset functions or auxiliary channels; trims can adjust channel trim/sub-trim
or drive an auxiliary channel; maintained switches also support drive modes.
Unsupported combinations and conflicting channel/trim writers are rejected by
the firmware. Assignments use the active model's existing binding storage and
are read back after saving. Keypad assignment overrides apply only in their
chosen context; disabling one restores the usual action. Wi-Fi setup continues
to hold outputs CUT and requires normal arming after setup closes.

Validation: `make -C test run`, `make -C simulator audit`, and
`node test/setup_portal_map_test.cjs`. The simulator audit covers actual binding
persistence and conflict checks; the Node test exercises browser clipping,
projection, simplification, package limits and incomplete provider responses.


## Persistent setup password

The shared setup hotspot / web-login password is now generated once and saved in
NVS (`r1-portal/password`). Opening setup or rebooting reuses it. First startup
with this firmware creates the persistent credential; the former temporary
password was not stored and cannot be recovered after a reboot. Session request
tokens still rotate independently; these are not user-facing passwords.

Use **Change password** on the LCD to enter 8–16 printable non-space characters.
**Save password** (portrait) or **Save** (landscape) verifies storage before
restarting the setup network. **Cancel** discards editing. Reconnect to the
hotspot and log into the browser as `r1` with the new password. Forgetting the
home Wi-Fi network does not erase this credential. Setup remains opt-in and
holds outputs CUT throughout the password change and network restart.


## Live LCD view

Open **Live handset screen → Connect live screen** in the setup website.
This shows the actual LCD pixels, not an independent simulator. A tap while the logo saver or display sleep is active only wakes the screen, including when remote controls are read-only. It preserves output state and does not activate a control underneath. A short Drive tap navigates to the dashboard; an incomplete longer hold is cancelled. Browser taps operate controls while CUT. To request enabling, hold Drive
until “Release to enable” appears, then release inside the same control.
The handset times an 800 ms minimum hold and requires fresh keep-alives,
a unique gesture ID and the same page epoch. Short holds, movement out of
the control, visibility loss, page changes and lost connections cancel.
PIN unlock and the existing neutral/link safety checks remain required.
No request is sent automatically when the hold reaches its threshold.
While armed or waiting for a link, screen viewing continues. Ordinary remote
interaction requests CUT and waits for acknowledgement before dispatching the
tap. Direct configuration writes remain blocked until CUT is confirmed. **Browse
handset menus** leaves the Wi-Fi landing page while keeping the setup session
active. **Wi-Fi setup screen** returns to its network controls. Close in the remote view returns to the dashboard and keeps the session active.
End hotspot setup on the physical LCD; home-network web access remains available.

The server allocates a 300 KiB RGB565 screen buffer in PSRAM on first connection,
releases it when the web server stops, and tracks changed rows. The browser polls at
most twice per second, pauses in background tabs, and disconnects visibly on
network failure. Frames use a bounded binary R1SC header and RGB565 row data.
Existing basic authentication and session tokens protect access and taps.
Page epochs reject taps aimed at a previous page; physical touch takes priority.
All capture and HTTP/UI operations run on the UI loop. The independent control
task continues to enforce CUT throughout the setup session.

Tests include dirty-row tracking, unchanged frames, resync and coordinate
bounds, plus actual UI navigation, editor protection, CUT-only remote writes
and rejected arming attempts. Physical network responsiveness needs a handset
trial after flashing.


## Automatic home Wi-Fi

**Auto Wi-Fi: ON/OFF** on the LCD Wi-Fi setup page is saved separately from
network credentials and defaults to ON. With a saved SSID, the handset joins
that network during boot without opening setup or dismissing startup warnings.
Each attempt allows up to 30 seconds for association and DHCP. Failed attempts
retry after 2, 5, 10, 20, then at most 30 seconds. A 300 ms settling period
separates disconnect from the next association. Wi-Fi power saving is disabled
for reliable portal access; mDNS failures retry every five seconds. Starting a
connection or reconnect is deferred while armed. No hotspot is started in
this background mode. Disabling auto-connect prevents background connections;
explicitly opening setup still permits connection and starts the setup hotspot.

Closing setup stops the hotspot. With auto-connect ON, the home connection
and authenticated web server stay available. Viewing never forces CUT or arms
the transmitter. Ordinary mirrored-screen interaction automatically requests CUT, cancels any
pending Link wait/enable request, and waits for the control loop to acknowledge
safe outputs before dispatching the tap through the normal LCD UI. This also
applies while driving: remote interaction stops live outputs. Wake-only taps
preserve output state. Re-enabling requires the explicit Drive hold.
The control loop supplies the CUT/arm-request state before
HTTP processing; write endpoints, upload chunks and UI mutation methods enforce
the gate. Physical arming blocks direct configuration writes; normal remote UI interaction
uses the CUT handoff above.
The header Wi-Fi symbol is green when connected, amber while connecting, blue
when only the setup hotspot is available, and dim grey when disconnected/off.
A Wi-Fi association does not indicate an internet connection.

Remote Close/Home/Back navigation keeps the setup session and CUT lock active.
Remote Connect, Forget network and Change setup password are rejected with a
browser explanation; use the physical LCD for actions that change the transport.


Home-network web access is automatic; visit `http://r1evo.local` and log in
with the existing `r1` / setup-password credentials. The status banner reflects
whether remote changes are currently allowed. All changes still require a valid
session token. Merely opening the site or watching the screen does not prevent
normal local arming. An authenticated Drive hold can request enabling through
the same PIN, neutral and receiver-link checks as the LCD.

Calibration and TX-configuration mode also inhibit live outputs and permit
remote Back/Cancel. Pending arming and armed operation require the CUT handoff
for mirrored UI interaction. Startup warnings and faults continue to block
configuration writes. The write policy is tested across every output-safety enum value.

## Boot PIN trial

Every startup requires the fixed trial PIN `1234` on the LCD or mirrored web keypad after the logo.
Use the numeric touchscreen and Unlock, or keypad Previous/Next/Select.
Incorrect or incomplete entries clear the digits and allow retry; Clear resets
the entry. There is no persistent lockout in this prototype. Unlock is kept
only in RAM, survives screen sleep, and is lost on reset. It never arms outputs
or bypasses startup warnings and neutral checks. Home Wi-Fi can connect while
locked. Authenticated web configuration (channels, assignments, model name,
maps, hostname and restart) remains available while CUT without entering the
boot PIN. The authenticated live web screen accepts taps on the PIN keypad, using the same PIN check as the LCD. Home and Setup shortcuts remain blocked until unlock; an accepted PIN leaves outputs CUT. Web sign-in
and the CUT/arming checks still protect every configuration write.
The hardcoded trial PIN is an operation lock, not secure protection against
someone reflashing the device. PIN configuration/recovery remains future work.

## Runtime memory diagnostics

The web page's **Memory diagnostics** panel reads authenticated `/api/memory`.
It is read-only, available while CUT or armed, and does not require unlocking the
LCD PIN. It shows internal 8-bit heap, PSRAM and internal DMA heap separately;
DMA is a subset and must not be added to internal capacity. Heap totals are
runtime allocator totals, not the PlatformIO static-RAM limit.

The Arduino/UI task samples at most once every two seconds. LVGL monitoring
runs on its owning task, never the CRSF or input task. The web page loads one
snapshot automatically and provides manual refresh and opt-in five-second
refresh (paused in hidden tabs). The serial command `MEMORY` plus Enter forces
an immediate sample; it also works in halted startup, with LVGL marked unavailable.

Free/minimum/largest values are bytes internally and displayed as KiB. Heap
minimums are allocator low-water marks since boot; summed region minimums may
not have occurred simultaneously. LVGL reports pool used, largest sampled usage, largest
free block and fragmentation. ESP-IDF stack high-water marks are **minimum unused
bytes**, unlike the word units used by vanilla FreeRTOS. A missing task reports
-1/not started. Monitoring does not reset any of these historical metrics. The LVGL peak is sampled because this LVGL allocator's native peak accounting can report less than current occupied pool space; brief peaks between samples can be missed.

After flashing, record a baseline, then exercise complex pages, the live LCD
mirror and a maximum-sized map upload. Compare minimum internal heap, largest
block, LVGL peak and UI/control/CRSF stack margins. No universal pass/fail threshold
is assumed. Small transient allocations can be reflected in minimum heap even
when the two-second snapshot misses their largest-block changes.

Map saving refreshes the authenticated boot session before uploading, preserving
the prepared image and form drafts. If the server explicitly rejects that upload
with `403 Invalid setup session`, the browser refreshes and retries once. Network
errors, unknown timeout outcomes, permission failures and validation failures are
not retried automatically. CUT and prepared storage are checked again each time.
Older open pages need one reload to obtain this client fix; export a prepared map
package first, then import it after reloading so the image is not lost.


## Browser sign-in

The home page shows a normal username/password form, avoiding native Basic-auth
dialog problems in embedded browsers. Use username `r1` and the displayed setup
password. Credentials are POSTed, never placed in the URL. An HttpOnly,
SameSite=Strict session cookie is shared by API requests, the live screen and map
uploads. It expires when the browser session ends and is invalidated when the web
server restarts or the setup password changes. Existing Basic-auth clients remain
supported. The local server uses HTTP, so this does not encrypt traffic.
The boot PIN and CUT/write protections are unchanged.

Remote enabling ends an active setup session at hold start only when saved
home Wi-Fi is connected and auto-connect is enabled; station web access stays
available. Hotspot-only setup cannot be used to enable outputs. Once enabling
is requested, direct configuration writes become read-only; screen viewing
continues. Return to CUT with the physical handset control or ordinary remote
UI interaction, which performs the acknowledged CUT handoff.


## Testing without the LCD or touchscreen

On the XIAO production environment, failed LCD-driver/touch/expander startup now
opens the authenticated setup hotspot automatically. LVGL continues rendering
for the web mirror, USB input diagnostics remain available, and normal arming
stays inhibited. No normal control task starts in this recovery loop. The
existing separately authorized USB RF-bench path remains available.

Connect USB serial at 115200 baud and send commands terminated by Enter:

| Command | Effect |
| --- | --- |
| `WIFISETUP` | Request CUT and open setup without using the LCD. |
| `WIFISTATUS` | Show readiness, hotspot name, addresses and the shared setup/web password. Credentials print only on this explicit USB request. |
| `HEADLESS ON` | Request CUT, save headless mode, stop LCD transfers and physical touch handling, inhibit normal arming, and open setup. Future boots skip LCD/touch initialization. |
| `HEADLESS OFF` | Request CUT and clear the saved headless flag. Restart to retry the hardware; the current recovery latch stays set. |
| `WIFICLOSE` | Request CUT and close hotspot setup without the LCD. Home-network web access may stay available; this does not clear recovery or arm outputs. |
| `INPUTMON` / `INPUTS` | Inspect physical inputs and expander availability. |
| `BOOTLOG` / `MEMORY` | Inspect startup checkpoints and runtime memory. |

After `WIFISETUP` or automatic recovery, allow a few seconds and send
`WIFISTATUS`. Join the reported hotspot and open `http://192.168.4.1`, or use the
reported home-network address. Log in as `r1` with the reported setup password.
The native iOS app uses the same address/password for channel and model setup;
use the browser portal for the live screen and advanced configuration.

A working SPI driver cannot detect whether this write-only Waveshare LCD is
physically connected or displaying an image. If boot succeeds but the panel is
blank, use `HEADLESS ON` explicitly. It persists separately in `r1-bench` NVS;
it does not alter model settings or the hardware pin map. Resetting/power cycling
keeps headless mode until `HEADLESS OFF` is saved. Normal boot PIN, web
credentials, remote write gates and RF bench authorization remain in force.
Close hotspot setup with `WIFICLOSE` before using the existing USB RF-bench arm
procedure; closing setup alone never enables outputs.

Validation covers persisted headless selection, offscreen LVGL rendering,
remote navigation and normal arm inhibition in both simulator orientations.
Physical missing-screen/touch boot, hotspot access and reset/power-cycle behavior
still require a XIAO hardware trial.
