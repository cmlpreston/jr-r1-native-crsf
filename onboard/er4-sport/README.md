# Receiver telemetry moved

The receiver firmware, S.Port decoding, host tests, wiring notes and upload
workflow now live in the separate
[ExpressLRS-SPort fork](https://github.com/cmlpreston/ExpressLRS-SPort), branch
`codex/sport-telemetry`, based on ExpressLRS 4.1.0.

See [the receiver project guide](https://github.com/cmlpreston/ExpressLRS-SPort/blob/codex/sport-telemetry/sport/README.md)
and [ER4 first-trial instructions](https://github.com/cmlpreston/ExpressLRS-SPort/blob/codex/sport-telemetry/sport/er4-sport/first-trial.md).

This transmitter repository continues to own production CRSF parsing, display
and alarms. Receiver tests/builds/uploads no longer run from here.
No receiver was flashed during migration.
