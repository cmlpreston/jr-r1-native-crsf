# Saved run records

Each model slot retains its observations until reset: sensor extrema/averages
(including speed and RPM), trip totals, power/energy, battery extrema, timer
elapsed time and up to 100 laps, plus a route track. These are observations,
not copies of receiver counters or handset control settings.

Run review offers **Save records** and **Delete records...**. Deletion asks for
confirmation and requires CUT with the timer paused. It clears this model's
saved and current statistics, laps and GPS route, keeping settings and the
accumulated handset on-time. Other model slots are unaffected. Individual
statistics/lap/trail reset actions also checkpoint their changes.
New observations collected after deletion start a new record set.

Checkpoints run every 30 seconds **while CUT** and observations exist, on leaving
LIVE, and before model changes. Collection continues in RAM while driving;
checkpoint flash writes are deferred until CUT. Abrupt power loss while driving
can lose the uncheckpointed portion of that drive. CUT before normal power-off;
manual Save also requires CUT. Errors are shown on Run
review; the previous committed checkpoint remains if a replacement fails.

On reboot/model load the timer is paused, auto-start readiness is cleared,
rolling windows are empty and all telemetry freshness/continuity clocks reset.
Historical extrema/totals remain visible; integration does not bridge the
power-off gap or count an increased receiver mAh counter as off-time use.
No arm request, PIN, module state, output or calibration state is restored.

## GPS route

User-selected retention is **2,048 points at at most 1 Hz**, recording position
changes. This is approximately 34 minutes before progressive decimation. When
full, alternate retained points are removed and the sampling interval doubles,
keeping an overview of the route instead of dropping its beginning. It is not
the raw approximately 10 Hz GPS stream. Each point includes coordinates,
sample uptime and a power/model-load segment identifier.

The map still uses its small 48-point rendering buffer. Restoring a route
selects points across the saved track for that buffer; the checkpoint retains
the larger track. A GPS CSV/GPX export is not implemented yet.

## Storage and validation

Handset records use private LittleFS files `/run-model-N.dat`, separate from
NVS settings and map files. Storage is mounted without formatting; a storage
failure never triggers an automatic erase/format. Files are written through a
temporary file and atomic rename, with version, payload length and CRC32
validation. Delete atomically commits an empty record, preventing an incomplete
delete from reviving old data. This is logical deletion, not secure flash erase.

Native simulator records live under `JR_R1_SIM_STATE/run-records`. Audits must
always use disposable synthetic preferences. The internal checkpoint format
is versioned and includes value-only class layouts; incompatible versions or
sizes are rejected. Future layout changes need an explicit format migration
if existing records are to be retained across that firmware update.

Focused checks:

```sh
make -C test -j1 build/run_records_test build/pcb_io_test
test/build/run_records_test
test/build/pcb_io_test
make -C simulator -j1
audit_state=$(mktemp -d /tmp/r1-record-audit.XXXXXX)
SDL_VIDEODRIVER=dummy JR_R1_SIM_STATE="$audit_state" \
  JR_R1_RUN_RECORDS_AUDIT_ONLY=1 simulator/build/jr-r1-sim --ui-audit
```

The user-authorized landscape upload on 16 November installed this feature;
postboot read-only checks showed CUT and safe outputs. Physical power
interruption, flash latency and saved-record behavior with real telemetry still
need handset observation. See the migration guide's upload record.
