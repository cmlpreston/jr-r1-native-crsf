# Requested transmitter follow-up — 16 November 2026

- Correct installed-handset Idle Up / Drive Mode input identities. User observed
  Idle Up reported as Drive Mode; swap K2 GPA4 and GPB6 identities on the XIAO
  PCB profile. Prototype mapping remains unchanged.
- Investigate a red lock after that switch was operated. Returning the switch
  did not clear it; the status reason was not observed. Do not weaken safety
  guards based on icon colour. Reproduce/read Output status before claiming a fix.
- Retain per-model run summaries, laps, speed/RPM and other sensor extrema,
  trip/power/battery totals and GPS records over power cycles. Include confirmed
  deletion/reset of saved data. Restored observations must not become fresh
  telemetry or resume a running/ready timer or an armed state.
- Add a sensor widget Graph style alongside Number/Dial. Desired window: the
  last minute, scrolling like the live control chart. Missing/stale data should
  leave gaps; units and scale should be explicit. This is a requested follow-up,
  not yet implemented.

User selected a **1 Hz route track with 2,048 points**. This provides about
34 minutes before progressive decimation reduces temporal detail to preserve
the route overview. This is separate from the approximately 10 Hz incoming GPS
stream; it is not a full-rate raw GPS log.
