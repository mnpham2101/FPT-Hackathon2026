import org.jetbrains.kotlin.gradle.dsl.JvmTarget

// :r4-simulator — sanctioned IVI test equipment (HLD §7, D9): scenario-driven R4 traffic
// built from the frozen :contract samples, run from a laptop (host mode) or as the
// m1-r4-sim:latest container on the mini-blueprint's ADA node (in-Room mode).
// Zero dependencies beyond :contract — no YAML, no CLI framework, no logging framework.
plugins {
    id("org.jetbrains.kotlin.jvm")
    id("org.jetbrains.kotlin.plugin.serialization")
    id("application")
}

application {
    mainClass.set("com.hackathon.v2x.ivi.sim.MainKt")
}

java {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
}

kotlin {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
    }
}

dependencies {
    implementation(project(":contract"))
    testImplementation("junit:junit:4.13.2")
}
