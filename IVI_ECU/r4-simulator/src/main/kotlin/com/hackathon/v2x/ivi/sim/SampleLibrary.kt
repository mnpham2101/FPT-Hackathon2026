package com.hackathon.v2x.ivi.sim

/**
 * Loads the frozen contract samples off the `:contract` classpath
 * (`/contracts/samples/…`, HLD D6). A sample carried as a literal anywhere in
 * this module is a defect: a simulator with its own copy of the schema is a
 * second, unversioned contract.
 */
object SampleLibrary {

    fun read(name: String): String {
        require(name in Scenario.KNOWN_SAMPLES) {
            "unknown sample '$name' — known: ${Scenario.KNOWN_SAMPLES.sorted()}"
        }
        val path = "/contracts/samples/$name.json"
        val stream = javaClass.getResourceAsStream(path)
            ?: throw IllegalStateException("frozen sample missing from the :contract classpath: $path")
        return stream.bufferedReader().use { it.readText() }
    }
}
