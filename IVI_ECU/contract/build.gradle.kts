import org.jetbrains.kotlin.gradle.dsl.JvmTarget

// :contract — the frozen R4/R3 wire binding and the shared samples, pure Kotlin/JVM.
// Zero Android dependencies: this module must compile with no Android SDK present, so
// :r4-simulator (and any host-side tool) can depend on the same models the APK parses with.
plugins {
    id("org.jetbrains.kotlin.jvm")
    id("org.jetbrains.kotlin.plugin.serialization")
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
    // api, not implementation: consumers use R4Json and the @Serializable types directly.
    api("org.jetbrains.kotlinx:kotlinx-serialization-json:1.9.0")
    testImplementation("junit:junit:4.13.2")
}
