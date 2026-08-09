package com.hackathon.v2x.ivi.sim

/**
 * Resolved run configuration of the simulator.
 *
 * Two factories, one per run mode:
 * - [fromEnv] — in-Room mode; reads exactly the mini-blueprint ADA-node variable
 *   names, so swapping the real ADA image in later needs no node-config edit.
 * - [fromArgs] — host mode; `--host/--port/--scenario/--rate/--start-delay`.
 *
 * [rateHz] `null` means "use the scenario file's own `defaultRateHz`" — an
 * explicit `R4_RATE_HZ` / `--rate` wins over the file.
 */
data class SimConfig(
    val host: String,
    val port: Int,
    val scenarioPath: String,
    val rateHz: Double?,
    val startDelayS: Double,
) {
    companion object {
        const val ENV_HOST = "IVI_ECU_HOST"
        const val ENV_PORT = "IVI_ECU_PORT"
        const val ENV_SCENARIO = "R4_SCENARIO"
        const val ENV_RATE_HZ = "R4_RATE_HZ"
        const val ENV_START_DELAY_S = "START_DELAY_S"

        const val DEFAULT_PORT = 47300
        const val DEFAULT_SCENARIO = "scenarios/approach.json"
        const val DEFAULT_START_DELAY_S = 0.0

        /** In-Room mode. [ENV_HOST] is required — a simulator with a silently-guessed target sends evidence nowhere. */
        fun fromEnv(env: Map<String, String> = System.getenv()): SimConfig {
            val host = env[ENV_HOST]
                ?: throw SimConfigException("$ENV_HOST is not set — the in-Room mode requires the IVI node address")
            return SimConfig(
                host = host,
                port = env[ENV_PORT]?.toPortOrThrow(ENV_PORT) ?: DEFAULT_PORT,
                scenarioPath = env[ENV_SCENARIO] ?: DEFAULT_SCENARIO,
                rateHz = env[ENV_RATE_HZ]?.toRateOrThrow(ENV_RATE_HZ),
                startDelayS = env[ENV_START_DELAY_S]?.toDelayOrThrow(ENV_START_DELAY_S) ?: DEFAULT_START_DELAY_S,
            )
        }

        /** Host mode. `--host` is required for the same reason [ENV_HOST] is. */
        fun fromArgs(args: Array<String>): SimConfig {
            val values = mutableMapOf<String, String>()
            var i = 0
            while (i < args.size) {
                val flag = args[i]
                require(flag.startsWith("--") && i + 1 < args.size) {
                    "malformed argument '$flag' — expected --flag value pairs"
                }
                values[flag] = args[i + 1]
                i += 2
            }
            val unknown = values.keys - setOf("--host", "--port", "--scenario", "--rate", "--start-delay")
            if (unknown.isNotEmpty()) {
                throw SimConfigException("unknown argument(s): ${unknown.joinToString()}")
            }
            val host = values["--host"]
                ?: throw SimConfigException("--host is not set — the host mode requires a target address")
            return SimConfig(
                host = host,
                port = values["--port"]?.toPortOrThrow("--port") ?: DEFAULT_PORT,
                scenarioPath = values["--scenario"] ?: DEFAULT_SCENARIO,
                rateHz = values["--rate"]?.toRateOrThrow("--rate"),
                startDelayS = values["--start-delay"]?.toDelayOrThrow("--start-delay") ?: DEFAULT_START_DELAY_S,
            )
        }

        private fun String.toPortOrThrow(name: String): Int =
            toIntOrNull()?.takeIf { it in 1..65535 }
                ?: throw SimConfigException("$name='$this' is not a port in 1..65535")

        private fun String.toRateOrThrow(name: String): Double =
            toDoubleOrNull()?.takeIf { it > 0.0 }
                ?: throw SimConfigException("$name='$this' is not a positive rate in Hz")

        private fun String.toDelayOrThrow(name: String): Double =
            toDoubleOrNull()?.takeIf { it >= 0.0 }
                ?: throw SimConfigException("$name='$this' is not a non-negative delay in seconds")
    }
}

/** A configuration value is missing or unusable; the message names the variable or flag. */
class SimConfigException(message: String) : IllegalArgumentException(message)
