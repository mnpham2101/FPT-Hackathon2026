# How does ADA-ECU work?

## Data flow

![ADA-ECU fusion flow: ego camera frame becomes B's own-sensor TrackedObject, fuses with the V2X-relayed object for C into one warning event for IVI-ECU](assets/ada-fusion-flow.svg)

## Our evidence

* One EVT JSONL line per event: name, two timestamps, counters, payload.
* `r2_ingest` — one line per R2 message from V2X-ECU. Payload: raw body, position/class confidence.
* `track_transition` — one line per state change. Payload: track id, source, from/to state, distance, reason.
* `r4_tx` — one line per warning sent to IVI-ECU. Payload: R4 body, size, destination, outcome.
* Read in order, the three events trace one object end to end: ingested, fused, sent as a warning.