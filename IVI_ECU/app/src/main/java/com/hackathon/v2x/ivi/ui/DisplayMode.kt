package com.hackathon.v2x.ivi.ui

/**
 * The view currently occupying the R16 Display Area.
 *
 * HLD §3.1 / Lead `16.5.4.5`: this set is **[C] unchanged** — Warning / Home /
 * Apps / Settings only. Do not add StandbyView or dashcam modes (dashcam deferred).
 *
 * [WarningView] is forced by wake-on-warning; Idle restores
 * [MainViewModel.previousMode] unless the driver overrode.
 */
sealed class DisplayMode {
    /** R17 2D God View (Canvas) while an R4 warning is Active. */
    data object WarningView : DisplayMode()

    data object HomeView : DisplayMode()
    data object AppsView : DisplayMode()
    data object SettingsView : DisplayMode()
}
