# R-1 Config for iOS

Native SwiftUI companion for the Seeed XIAO ESP32-S3 R-1 EVO transmitter.
Requires iOS/iPadOS 17 or newer. Uses the existing Wi-Fi setup API; no firmware
changes, uploads, BLE service, third-party packages or cloud account are needed.

## Run on an iPhone

1. Open `R1Config.xcodeproj` in Xcode. Select the `R1Config` scheme.
2. In Signing & Capabilities choose your development team and a unique bundle
   identifier. Connect your iPhone and select it as the run destination.
3. Run. Allow Local Network access when prompted.
4. Put the phone and handset on the same Wi-Fi. Connect to `r1evo.local`, the
   handset's configured `.local` hostname, or the IP shown on its LCD.
   Alternatively join the R-1-EVO hotspot in iPhone Settings and connect to
   `192.168.4.1`. Open Setup → Handset → Wi-Fi / web setup on the transmitter.
5. Enter the setup password displayed on the handset. The app uses username
   `r1`. This password is separate from the boot PIN and home Wi-Fi password.

If access was denied, enable Local Network permission in iOS Settings for
R-1 Config. Reconnect after a handset reboot, setup-password change, or network
change. This prototype does not store credentials across connections or launches.

## First version

- Authenticated connection with explicit hostname/IP entry.
- Active model, slot and permission status; refresh every five seconds while
  foregrounded, manual refresh, and pull to refresh.
- Rename the active model (1–12 printable ASCII characters).
- Edit all sixteen channels: endpoints, rates, expo, ramps, reversal, profile
  sub-trim, trim and sub-trim. Trim/sub-trim are shown in firmware permille units.
- Fresh CUT permission/session check before saving; changed-model/channel
  detection; read-back verification; no automatic retries after failed writes.
- Open the existing web portal for assignments, mixing, appearance, maps,
  diagnostics and the live screen. Browser sign-in is separate.

Edits stay as drafts until Save. Leaving a channel editor discards its draft.
The app does not offer arming or firmware upload. Viewing alone does not request
CUT. The firmware enforces its own write gate. A connection or save error makes
editing unavailable until a successful refresh. A lost response can mean the
save reached the handset: inspect the read-back before retrying.

Use only one editor (LCD, app, or portal) at a time. The current firmware has no
revision-based compare-and-swap API: another editor can change the active model
or settings between the app's preflight read and POST. Preflight/read-back checks
reduce accidental overwrites but cannot remove that race.

Transport is local HTTP with Basic authentication and the firmware's session
token. Use a trusted network. Credentials stay in an ephemeral URLSession;
redirects, cookies, disk cache and credential storage are disabled. The local
network transport exception is declared in Info.plist; no blanket arbitrary-load
exception is enabled. See [Apple's local networking transport documentation](https://developer.apple.com/documentation/bundleresources/information-property-list/nsapptransportsecurity/nsallowslocalnetworking).

## Validation

```sh
swift test --package-path ios
xcodebuild -project ios/R1Config.xcodeproj -scheme R1Config \
  -sdk iphonesimulator -configuration Debug \
  -derivedDataPath /tmp/r1-ios-derived CODE_SIGNING_ALLOWED=NO build
```

Run these from the repository root. Protocol tests inject URLProtocol responses;
no test contacts hardware. Physical iPhone Wi-Fi connectivity, permission prompts,
background/resume behavior and actual handset saves still need device validation.
Simulator compilation alone does not validate these behaviors.
