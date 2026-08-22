"""Scenario Player entrypoint (SP HLD D4) - the blueprint-fixed ``command: ["python", "main.py"]``.

Controller only, no business logic: every decision lives in the collaborators. ``build_and_run``
wires ``player/config.py`` (env + scenario YAML) -> ``player/scenario.py`` (kinematics) ->
``player/encoder_client.py`` (persistent ``cpm_encode --stream`` subprocess) ->
``player/sender.py`` (UDP to ``V2X_ECU_HOST:V2X_ECU_PORT``) -> ``player/generator.py`` (rate
loop) for a classic single-object scenario, or ``player/phased_generator.py`` (D8's repeating
waiting/two-vehicle/three-vehicle cycle) when the loaded ``ScenarioConfig.phases`` is set. Startup
is announced with one ``[START]`` line and any startup/fatal exception is logged as one ``[FATAL]``
line to stdout, both flushed so they land in the container View Log; the process exit code is 0 on
a clean run, 1 on a fatal error. Stdlib + the ``player`` package only.
"""

import dataclasses
import sys
from collections.abc import Callable, Mapping, Sequence

from player.config import load_env, load_scenario
from player.encoder_client import EncoderClient
from player.generator import Generator
from player.phased_generator import PhasedGenerator
from player.scenario import Scenario
from player.sender import UdpSender


def _print_flushed(message: str) -> None:
    """Default log sink: stdout with an immediate flush (container View Log)."""
    print(message, flush=True)


def build_and_run(
    *,
    env: Mapping[str, str] | None = None,
    encoder_command: Sequence[str] | None = None,
    max_ticks: int | None = None,
    log: Callable[[str], None] | None = None,
) -> int:
    """Wire the collaborators and run the generator; return the process exit code.

    All parameters are test hooks with production defaults: ``env=None`` reads ``os.environ``,
    ``encoder_command=None`` lets ``EncoderClient`` spawn the real ``[encoder_path, "--stream"]``
    helper (HLD D1), ``max_ticks=None`` runs per the scenario's ``duration_s``/``loop`` semantics,
    and ``log=None`` prints flushed to stdout. Any startup/fatal exception logs one ``[FATAL]``
    line and returns 1 - the entrypoint never dies on a bare traceback alone.
    """
    sink: Callable[[str], None] = _print_flushed if log is None else log
    try:
        env_cfg = load_env(env)
        sink(
            f"[START] scenario={env_cfg.scenario_config}"
            f" target={env_cfg.v2x_ecu_host}:{env_cfg.v2x_ecu_port}"
        )
        scenario_cfg = load_scenario(env_cfg.scenario_config)
        with (
            EncoderClient(env_cfg.encoder_path, command=encoder_command, log=sink) as encoder,
            UdpSender(env_cfg.v2x_ecu_host, env_cfg.v2x_ecu_port, log=sink) as sender,
        ):
            if scenario_cfg.phases is not None:
                two_vehicle_scenario = Scenario(
                    dataclasses.replace(scenario_cfg, object=scenario_cfg.two_vehicle_object)
                )
                three_vehicle_scenario = Scenario(
                    dataclasses.replace(scenario_cfg, object=scenario_cfg.three_vehicle_object)
                )
                PhasedGenerator(
                    two_vehicle_scenario,
                    three_vehicle_scenario,
                    scenario_cfg.phases,
                    scenario_cfg,
                    encoder.encode,
                    sender.send,
                    log=sink,
                ).run(max_ticks=max_ticks)
            else:
                Generator(
                    Scenario(scenario_cfg), scenario_cfg, encoder.encode, sender.send, log=sink
                ).run(max_ticks=max_ticks)
        return 0
    except Exception as exc:  # fatal boundary: ValueError from config + anything unexpected
        sink(f"[FATAL] {type(exc).__name__}: {exc}")
        return 1


def main() -> int:
    """Production path: real env, real helper command, unbounded run, stdout logging."""
    return build_and_run()


if __name__ == "__main__":
    sys.exit(main())
