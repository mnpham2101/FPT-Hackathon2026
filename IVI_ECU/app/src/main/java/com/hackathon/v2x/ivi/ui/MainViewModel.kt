package com.hackathon.v2x.ivi.ui

import androidx.lifecycle.ViewModel
import com.hackathon.v2x.ivi.model.R4WarningMessage
import com.hackathon.v2x.ivi.model.SceneGeometry
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * State machine for the Display Area view switcher (16.5.2.3).
 * Single source of truth for which view the Display Area shows.
 */
class MainViewModel : ViewModel() {

    private val _currentMode = MutableStateFlow<DisplayMode>(DisplayMode.HomeView)
    val currentMode: StateFlow<DisplayMode> = _currentMode.asStateFlow()

    private val _warningScene = MutableStateFlow<SceneGeometry?>(null)
    val warningScene: StateFlow<SceneGeometry?> = _warningScene.asStateFlow()

    private val _riskState = MutableStateFlow("clear")
    val riskState: StateFlow<String> = _riskState.asStateFlow()

    /**
     * Requests a Display Area mode change.
     *
     * Safety rule: while [DisplayMode.WarningView] is active, requests to
     * navigate elsewhere are ignored — the warning stays until the warning
     * lifecycle clears it (wake-on-warning restore, 16.5.2.4).
     */
    fun setMode(mode: DisplayMode) {
        if (_currentMode.value == DisplayMode.WarningView && mode != DisplayMode.WarningView) {
            return
        }
        _currentMode.value = mode
    }

    fun onR4Warning(message: R4WarningMessage) {
        _warningScene.value = message.toSceneGeometry()
        _riskState.value = message.riskState
        _currentMode.value = DisplayMode.WarningView
    }
}
