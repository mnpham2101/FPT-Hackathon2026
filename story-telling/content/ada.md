# How does ADA-ECU do its magic ?

## Data flow

![ADA-ECU fusion flow: ego camera frame becomes B's own-sensor TrackedObject, fuses with the V2X-relayed object for C into one warning event for IVI-ECU](assets/ada-fusion-flow.svg)

## Our evidences

* ADA-ECU writes one EVT JSONL line per event to its event log — event name, two clock stamps, cumulative counters, and a payload.
* r2_ingest — one line per R2 message received from V2X-ECU [3]: payload carries the raw message body plus its derived position/class confidences.
* track_transition — one line per store state change [2]: payload carries the track id, source (own_sensor or v2x_relayed), from/to state, distance, and the transition reason.
* r4_tx — one line per warning sent to IVI-ECU [4]: payload carries the full R4 body, its serialized size, destination, and send outcome.
* Same three events, read in order, show the fuse end to end: a track_transition admitting C from a r2_ingest line, then the r4_tx line carrying that same object back out in the warning event.