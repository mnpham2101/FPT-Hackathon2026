import org.jetbrains.kotlin.gradle.dsl.JvmTarget

// :r4-simulator — sanctioned IVI test equipment (HLD §7, D9): scenario-driven R4 traffic
// built from the frozen contract samples, run from a laptop (host mode) or as the
// m1-r4-sim:latest container on the mini-blueprint's ADA node (in-Room mode).
// Zero third-party dependencies beyond kotlinx-serialization — no YAML, no CLI framework,
// no logging framework.
//
// STANDALONE BUILD. This is its own Gradle build (see settings.gradle.kts), so plugin
// versions are declared here rather than inherited from IVI_ECU/build.gradle.kts. They are
// pinned to the versions the IVI application already uses, so the simulator compiles the
// R4 model exactly as the app does.
//
// Build and test:
//     cd IVI_ECU && ./gradlew -p r4-simulator build
// Run against a host listener:
//     cd IVI_ECU/r4-simulator && ../gradlew -p . installDist && build/install/r4-simulator/bin/r4-simulator
plugins {
    id("org.jetbrains.kotlin.jvm") version "2.2.20"
    id("org.jetbrains.kotlin.plugin.serialization") version "2.2.20"
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

// THE R4 CONTRACT IS READ FROM THE APPLICATION, NEVER COPIED.
//
// Upstream this was `implementation(project(":contract"))`. Here the same sources are compiled
// in directly from the app's tree. Both directories are READ ONLY — nothing in this build writes
// to them, so the IVI application is untouched.
//
// Taking a private copy of the model or the samples would create a second, unversioned contract
// that drifts from the app's the moment either is edited; a simulator whose field list has drifted
// emits messages the real app rejects, and the test passes while the system is broken. Compiling
// the app's own sources is what makes "the app can parse this" a fact rather than a hope.
sourceSets {
    main {
        // The R4 message model: R4Message.kt, R3Snapshot.kt, SceneGeometry.kt. Pure Kotlin with
        // only kotlinx.serialization imports — no Android types — so it compiles on a plain JVM.
        kotlin.srcDir("../app/src/main/java/com/hackathon/v2x/ivi/model")
        // The frozen samples SampleLibrary reads off the classpath as /contracts/samples/<name>.json.
        resources.srcDir("../app/src/test/resources")
    }
}

dependencies {
    // Version pinned to the app's own kotlinx-serialization-json (app/build.gradle.kts).
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.9.0")
    testImplementation("junit:junit:4.13.2")
}

tasks.test {
    testLogging {
        events("passed", "failed", "skipped")
    }
}
