"""Committed fake ``cpm_encode`` helper - test stand-in until group 1.7 builds the real binary.

Speaks the SP HLD D1 stream protocol: one CpmContent JSON per stdin line -> one base64 UPER
payload per stdout line; an encode failure is a single ``{"error": ...}`` reply line and the
stream continues - it never kills the process. Being a fake, the "UPER payload" is simply the
base64 of the UTF-8 input line itself, so tests can predict the reply byte-for-byte without the
real Vanetza codec. Run via ``sys.executable`` (the client's full-argv ``command`` override) so it
behaves identically on Windows and Linux CI.

Behavior is selected by ``argv[1]`` (taking the place of the real helper's ``--stream``):

* ``echo`` - every input line -> base64 of that exact line.
* ``error-on-marker`` - like echo, but an input whose ``stationId`` equals
  ``ERROR_MARKER_STATION_ID`` gets an ``{"error": "boom"}`` reply; the stream continues.
* ``die-after:N`` - echo N lines, then ``exit(1)`` mid-stream (simulates helper death, HLD D4).
"""

import base64
import json
import sys

#: ``error-on-marker`` inputs with this ``stationId`` provoke the ``{"error": "boom"}`` reply.
ERROR_MARKER_STATION_ID = 999


def _station_id(line: str) -> object:
    try:
        parsed = json.loads(line)
    except ValueError:
        return None
    return parsed.get("stationId") if isinstance(parsed, dict) else None


def main(argv: "list[str]") -> int:
    mode = argv[1] if len(argv) > 1 else "echo"
    remaining_before_death: "int | None" = None
    if mode.startswith("die-after:"):
        remaining_before_death = int(mode.split(":", 1)[1])
        mode = "die-after"
    elif mode not in ("echo", "error-on-marker"):
        print(json.dumps({"error": f"unknown fake mode {mode!r}"}), flush=True)
        return 2

    for raw_line in sys.stdin:
        line = raw_line.strip()
        if not line:
            continue
        if mode == "error-on-marker" and _station_id(line) == ERROR_MARKER_STATION_ID:
            print(json.dumps({"error": "boom"}), flush=True)
            continue
        print(base64.b64encode(line.encode("utf-8")).decode("ascii"), flush=True)
        if remaining_before_death is not None:
            remaining_before_death -= 1
            if remaining_before_death <= 0:
                return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
