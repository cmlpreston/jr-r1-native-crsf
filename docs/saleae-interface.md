# Saleae capture interface

`tools/saleae_capture.py` controls the installed Logic 2 application through
Saleae's official local automation API. Logic 2 owns the USB device; this tool
does not replace its USB driver or firmware. It never opens the XIAO serial port,
resets the transmitter, uploads firmware, or drives any probe outputs.

## Setup

```sh
.venv/bin/python -m pip install -r tools/requirements-saleae.txt
.venv/bin/python tools/saleae_capture.py devices
```

In Logic 2 Settings/Preferences enable **Enable automation server**, port 10430.
Existing capture tabs are preserved. Each new capture made by this tool is saved
and its tab closed after export. The device selector excludes simulated devices;
with multiple physical devices pass `--device ID`.

## Capture

```sh
.venv/bin/python tools/saleae_capture.py capture --seconds 2 --channels 0,1,2,3,4 --rate 24000000
```

The command prints its output directory, defaulting to a unique directory in
`/tmp`. `--out /absolute/new/directory` uses a persistent destination. An existing
output directory is rejected to avoid overwriting captures. Outputs include
`capture.sal`, `digital.csv`, `summary.json`, and `manifest.json` recording the
actual selected device and requested settings. Choose `--raw binary` for compact
per-channel binary exports, `both` for both types, or `none` for only the session.
The session is saved before analyzer/export work, preserving it if export fails.
A failed operation returns a nonzero exit code and records failure in the manifest.

Capturing only observes the connected probes. Check their physical assignments;
a filename or preset cannot verify wiring. The earlier LCD wiring was CH0 SCK,
CH1 MOSI, CH2 DC, CH3 RESET, CH4 MISO; this is **different** from the I2C preset.

## Decode I2C

Only with **CH0 on SDA and CH1 on SCL**:

```sh
.venv/bin/python tools/saleae_capture.py capture --channels 0,1 --seconds 2 --analyzers tools/saleae/i2c-ch0-ch1.json
```

Analyzer results are written to `decoded.csv`. Other Logic 2 analyzers can be
specified through a JSON list with `name`, optional `label`, and `settings` using
Saleae's exact analyzer setting names. They must refer to captured channels.

## Reprocess saved captures

```sh
.venv/bin/python tools/saleae_capture.py export '/absolute/path/Session 0.sal' --raw csv
.venv/bin/python tools/saleae_capture.py summary /absolute/path/digital.csv
```

The summary streams the CSV and counts transitions; it does not diagnose protocol
errors or infer their electrical cause. Offline summary needs no running Logic 2.

## Sampling limits

The connected original Logic has a 24 MS/s maximum. This is useful for 100/400 kHz
I2C and slow SPI diagnostics, but cannot decode a 40 MHz LCD clock. Temporarily
lower the target bus rate for those captures or use faster hardware. Supplying
`--signal-hz 4000000` enforces at least four samples per clock, our diagnostic
minimum (not a guarantee of clean signal capture). No firmware speed change is
automatic. Unsupported sample rates are rejected by Logic 2, not silently changed.

Reference: https://saleae.github.io/logic2-automation/
