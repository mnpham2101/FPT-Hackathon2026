package com.hackathon.v2x.ivi.ui

/**
 * The view currently occupying the R16 Display Area.
 *
 * Default idle mode is [StandbyView] (black listening surface). [WarningView]
 * is forced by wake-on-warning and restores [MainViewModel.previousMode] on Idle
 * unless the driver overrode.
 */
sealed class DisplayMode {
    /** Black standby surface — default when no hazard is active. */
    data object StandbyView : DisplayMode()

    /** R17 2D God View (Canvas) while an R4 warning is Active. */
    data object WarningView : DisplayMode()

    data object HomeView : DisplayMode()
    data object AppsView : DisplayMode()
    data object SettingsView : DisplayMode()
}
