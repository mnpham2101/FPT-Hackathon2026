package com.hackathon.v2x.ivi.sim

import com.hackathon.v2x.ivi.model.R4StateMessage
import com.hackathon.v2x.ivi.model.R4WarningEvent
import kotlin.system.exitProcess

/**
 * The simulator entrypoint, two run modes:
 * - args present → host mode ([SimConfig.fromArgs]), run from a laptop;
 * - no args → in-Room mode ([SimConfig.fromEnv]), started by the container
 *   entrypoint so a deploy alone produces evidence.
 *
 * Every non-raw payload is built and validated through `:contract`'s `R4Json`
 * BEFORE the first datagram leaves — a payload the simulator cannot parse is a
 * payload the app cannot parse, and the run must fail loudly at the producer (D9).
 */
fun main(args: Array<String>) {
    val config = try {
        if (args.isNotEmpty()) SimConfig.fromArgs(args) else SimConfig.fromEnv()
    } catch (e: SimConfigException) {
        System.err.println("[FATAL] ${e.message}")
        exitProcess(2)
    }

    val scenario: Scenario
    val built: List<BuiltMessage>
    try {
        scenario = ScenarioLoader.load(config.scenarioPath)
        built = scenario.steps.mapIndexed { i, step -> MessageBuilder.build(step, i) }
    } catch (e: ScenarioException) {
        System.err.println("[FATAL] ${config.scenarioPath}: ${e.message}")
        exitProcess(3)
    }

    val rateHz = config.rateHz ?: scenario.defaultRateHz
    val defaultDelayMs = (1000.0 / rateHz).toLong()
    println(
        "[START] scenario=${config.scenarioPath} name=${scenario.name} " +
            "target=${config.host}:${config.port} rateHz=$rateHz loop=${scenario.loop}",
    )

    // The AAOS guest boots slower than a container — hold before the first send.
    if (config.startDelayS > 0.0) Thread.sleep((config.startDelayS * 1000.0).toLong())

    UdpSender(config.host, config.port).use { sender ->
        var cycle = 0
        do {
            built.forEachIndexed { i, payload ->
                sender.send(payload.bytes)
                val desc = when (val m = payload.message) {
                    is R4WarningEvent -> "type=warning risk=${m.riskState} warningType=${m.warningType}"
                    is R4StateMessage -> "type=state seq=${m.seq}"
                    null -> "type=raw"
                }
                println(
                    "[TX] cycle=$cycle step=$i bytes=${payload.bytes.size} " +
                        "-> ${config.host}:${config.port} $desc",
                )
                val delayMs = scenario.steps[i].delayS?.let { (it * 1000.0).toLong() } ?: defaultDelayMs
                Thread.sleep(delayMs)
            }
            cycle++
        } while (scenario.loop)
    }
}
