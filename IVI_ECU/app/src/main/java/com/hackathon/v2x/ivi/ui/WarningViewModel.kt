package com.hackathon.v2x.ivi.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.hackathon.v2x.ivi.BuildConfig
import com.hackathon.v2x.ivi.data.R4Repository
import com.hackathon.v2x.ivi.model.R4Message
import com.hackathon.v2x.ivi.model.R4WarningEvent
import com.hackathon.v2x.ivi.model.SceneGeometry
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * ViewModel bridging [R4Repository] to the Warning View composables.
 *
 * Subtask 4.5.1.4:
 * - [uiWarningState] transitions: Idle → Active on incoming warning event
 * - [uiWarningState] auto-clears Active → Idle after [BuildConfig.WARNING_TIMEOUT_MS]
 * - [latestScene] carries the current [SceneGeometry] for the God-View canvas
 * - All coroutines live in [viewModelScope] — no leaks on ViewModel clear
 */
@HiltViewModel
class WarningViewModel @Inject constructor(
    private val repository: R4Repository,
) : ViewModel() {

    private val _uiWarningState = MutableStateFlow<WarningUiState>(WarningUiState.Idle)
    val uiWarningState: StateFlow<WarningUiState> = _uiWarningState.asStateFlow()

    private val _latestScene = MutableStateFlow<SceneGeometry?>(null)
    val latestScene: StateFlow<SceneGeometry?> = _latestScene.asStateFlow()

    private var autoClearJob: Job? = null

    init {
        collectWarningEvents()
    }

    /**
     * Attach to the live service flow after the service binds.
     * Safe to call multiple times — collection is scoped to [viewModelScope].
     */
    fun attachService(serviceFlow: SharedFlow<R4Message>) {
        repository.attachToService(serviceFlow, viewModelScope)
    }

    private fun collectWarningEvents() {
        viewModelScope.launch {
            repository.warningEvents.collect { event ->
                onWarningReceived(event)
            }
        }
        viewModelScope.launch {
            repository.currentState.collect {
                // State heartbeat received — scene data updated via warningEvents geometry
            }
        }
    }

    private fun onWarningReceived(event: R4WarningEvent) {
        _uiWarningState.value = WarningUiState.Active(event)
        // Lead 17.5.4.4 / R19: ghost C must render from the warning objectSnapshot.
        _latestScene.value = event.geometry.copy(vehicleCSnapshot = event.objectSnapshot)
        scheduleAutoClear()
    }

    private fun scheduleAutoClear() {
        autoClearJob?.cancel()
        autoClearJob = viewModelScope.launch {
            delay(BuildConfig.WARNING_TIMEOUT_MS)
            // Only clear if still Active (user might have already seen a new warning)
            if (_uiWarningState.value is WarningUiState.Active) {
                _uiWarningState.value = WarningUiState.Idle
                _latestScene.value = null
            }
        }
    }

    fun onServiceError() {
        _uiWarningState.value = WarningUiState.Error
    }

    override fun onCleared() {
        autoClearJob?.cancel()
        super.onCleared()
    }
}

