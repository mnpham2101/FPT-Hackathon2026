package com.hackathon.v2x.ivi.data

import com.hackathon.v2x.ivi.model.R4Message
import com.hackathon.v2x.ivi.model.R4ServiceError
import com.hackathon.v2x.ivi.model.R4StateMessage
import com.hackathon.v2x.ivi.model.R4WarningEvent
import com.hackathon.v2x.ivi.service.R4ListenerService
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
/**
 * Single source of truth for R4 events and scene state.
 *
 * Subtask 4.5.1.4 — Collects raw [R4ListenerService.r4EventFlow], routes by type:
 * - [R4WarningEvent] → re-emitted on [warningEvents]
 * - [R4StateMessage] → last-value-wins on [currentState]
 *
 * Pure Kotlin — zero Android UI or framework imports.
 * Singleton lifetime is provided by [com.hackathon.v2x.ivi.di.AppModule] (16.5.4.1).
 */
class R4Repository {

    private val _warningEvents = MutableSharedFlow<R4WarningEvent>(extraBufferCapacity = 32)
    /** Edge-triggered warning events. Collect in [WarningViewModel]. */
    val warningEvents: SharedFlow<R4WarningEvent> = _warningEvents.asSharedFlow()

    private val _currentState = MutableStateFlow<R4StateMessage?>(null)
    /** Latest state heartbeat from ADA ECU (last-value-wins per R4 spec). */
    val currentState: StateFlow<R4StateMessage?> = _currentState.asStateFlow()

    /**
     * Attach to a live service flow. Call from DI / ViewModel scope after service binds.
     *
     * @param serviceFlow the [R4ListenerService.r4EventFlow] to collect
     * @param scope coroutine scope that owns the collection (should be viewModelScope or app scope)
     */
    fun attachToService(serviceFlow: SharedFlow<R4Message>, scope: CoroutineScope) {
        scope.launch {
            serviceFlow.collect { message ->
                when (message) {
                    is R4WarningEvent -> _warningEvents.emit(message)
                    is R4StateMessage -> _currentState.value = message
                    is R4ServiceError -> Unit // handled by WarningViewModel
                }
            }
        }
    }
}

