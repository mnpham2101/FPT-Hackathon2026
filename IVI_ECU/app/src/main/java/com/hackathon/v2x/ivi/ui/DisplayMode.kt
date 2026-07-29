package com.hackathon.v2x.ivi.ui

/**
 * The view currently occupying the R16 Display Area.
 *
 * [WarningView] is safety-critical: while active, user navigation away is
 * blocked (see [MainViewModel.setMode]); it clears only via the warning
 * lifecycle (16.5.2.4).
 */
sealed class DisplayMode {
    data object WarningView : DisplayMode()
    data object HomeView : DisplayMode()
    data object AppsView : DisplayMode()
    data object SettingsView : DisplayMode()
}
