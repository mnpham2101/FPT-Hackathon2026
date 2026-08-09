// :r4-simulator builds as its OWN Gradle build, not as a module of the IVI project.
//
// The upstream design had it as `include(":r4-simulator")` in IVI_ECU/settings.gradle.kts,
// depending on an `:contract` module that carries the R4 model and the frozen samples. That
// module does not exist here, and creating it would move files out of :app — a change to the
// IVI application, which is frozen. A standalone build keeps every edit inside this folder.
//
// Consequence: the IVI project is unaware of this build. `./gradlew :app:...` from IVI_ECU/ is
// unchanged, and this build is driven explicitly — see build.gradle.kts for the commands.
pluginManagement {
    repositories {
        gradlePluginPortal()
        mavenCentral()
    }
}

dependencyResolutionManagement {
    repositories {
        mavenCentral()
    }
}

rootProject.name = "r4-simulator"
