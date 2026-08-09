# ADA_ECU — ADA ECU node (R3, R12–R15)

CarSky **Container Node**: ego's perception and fusion node — requirements R3 and R12–R15 of [m1-cooperative-awareness.md](../documents/Requirements/m1-cooperative-awareness.md) §2.

Two processes, one image: the C++17 `ada_ecu` core and the Python detector subprocess, joined by argv, exit codes and R3 JSONL over stdout — [design decision D2](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d2--process-thread-and-mock-model).

## Design of record

- [doc/ada-ecu-hld.md](../documents/Design/ADA-ECU/ada-ecu-hld.md) — the HLD, this folder's sole design authority.
- [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md) — node guide: image tag, blueprint config, env vars, pins, verification.
- [phase2_tasks.md § Per-node build commands](../plans/phase2_tasks.md#per-node-build-commands-cited-in-acceptance-below) — the acceptance-cited build/test rows.

## Build and test

C++ core:

```
cmake -S ADA_ECU -B ADA_ECU/build && cmake --build ADA_ECU/build -j $(nproc) && ctest --test-dir ADA_ECU/build --output-on-failure
```

Python detector:

```
pip install -r ADA_ECU/detector/requirements-dev.txt && python -m pytest ADA_ECU/detector/tests
```

## Clip attribution

The demo clip `media/ego-b-occluding-c.mp4` ships inside every pushed image; provenance in [media/ego-b-occluding-c.source.md](media/ego-b-occluding-c.source.md).

> Dashcam footage: *"Dash Cam View of a Moving Vehicle in a Highway"* by **Mario Angel**, via Pexels — <https://www.pexels.com/video/dash-cam-view-of-a-moving-vehicle-in-a-highway-5915075/>
