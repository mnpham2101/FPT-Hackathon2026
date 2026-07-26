# ADA ECU

Phase 2/3/4 scaffold for the ADA ECU track:

- Phase 2: R3 `TrackedObject` store, R13 admission gate, config, JSONL evidence logs, mock inputs.
- Phase 3 seam: Python detector subprocess emits R3 JSONL with `source = "own_sensor"`.
- Phase 4 seam: R2 V2X object input becomes `source = "v2x_relayed"` tracks, CRA emits R4 warnings for IVI.

The core is C++17 and uses no middleware. JSON parsing in this scaffold is intentionally narrow and contract-shaped; replace it with `nlohmann/json` once dependency packaging is finalized.

## Build

```sh
cmake -S ada-ecu -B ada-ecu/build
cmake --build ada-ecu/build
ctest --test-dir ada-ecu/build --output-on-failure
```

## Run Mock

```sh
ada-ecu/build/ada_ecu --config ada-ecu/config/ada-ecu.conf --mock
```
