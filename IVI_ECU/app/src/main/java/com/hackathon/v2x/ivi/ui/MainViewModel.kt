package com.hackathon.v2x.ivi.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/**
 * Display Area view switcher with wake-on-warning (16.5.2.3 / 16.5.2.4).
 *
 * Default mode is [DisplayMode.StandbyView]. Collects [uiWarningState]:
 * - Idle → Active: force [DisplayMode.WarningView], remember [previousMode]
 * - Active → Idle: restore [previousMode] unless the user overrode during the warning
 *
 * Typical demo path: Standby → Warning (God View) → Standby.
 */
class MainViewModel @JvmOverloads constructor(
    private val uiWarningState: StateFlow<WarningUiState> =
        MutableStateFlow(WarningUiState.Idle),
) : ViewModel() {

    private val _currentMode = MutableStateFlow<DisplayMode>(DisplayMode.StandbyView)
    val currentMode: StateFlow<DisplayMode> = _currentMode.asStateFlow()

    /** Mode shown before a warning forced WarningView. Visible to tests. */
    var previousMode: DisplayMode = DisplayMode.StandbyView
        private set

    /**
     * True when the user navigated away while a warning was [WarningUiState.Active].
     * Visible to tests.
     */
    var userOverrodeDuringWarning: Boolean = false
        private set

    private var lastWarningState: WarningUiState = WarningUiState.Idle

    init {
        viewModelScope.launch {
            uiWarningState.collect { state ->
                onWarningState(state)
            }
        }
    }

    /**
     * Requests a Display Area mode change.
     *
     * While a warning is Active, leaving WarningView is allowed but marks
     * [userOverrodeDuringWarning] so Idle will not force-restore [previousMode].
     */
    fun setMode(mode: DisplayMode) {
        if (lastWarningState is WarningUiState.Active &&
            mode != DisplayMode.WarningView
        ) {
            userOverrodeDuringWarning = true
        }
        _currentMode.value = mode
    }

    private fun onWarningState(state: WarningUiState) {
        val previous = lastWarningState
        lastWarningState = state

        when {
            state is WarningUiState.Active && previous !is WarningUiState.Active -> {
                if (_currentMode.value != DisplayMode.WarningView) {
                    previousMode = _currentMode.value
                }
                userOverrodeDuringWarning = false
                _currentMode.value = DisplayMode.WarningView
            }
            state is WarningUiState.Idle && previous is WarningUiState.Active -> {
                if (!userOverrodeDuringWarning) {
                    _currentMode.value = previousMode
                }
            }
        }
    }
}
