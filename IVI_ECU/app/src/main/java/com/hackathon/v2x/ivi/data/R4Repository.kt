package com.hackathon.v2x.ivi.data

import com.hackathon.v2x.ivi.model.R4Message
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
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Single source of truth for R4 events and scene state.
 *
 * Collects [R4ListenerService.r4EventFlow], routes by sealed type:
 * - [R4WarningEvent] → [warningEvents]
 * - [R4StateMessage] → last-value-wins on [currentState]
 *
 * Wire contract is main's [R4Message] only (no out-of-band error subtype).
 */
@Singleton
class R4Repository @Inject constructor() {

    private val _warningEvents = MutableSharedFlow<R4WarningEvent>(replay = 1, extraBufferCapacity = 32)
    val warningEvents: SharedFlow<R4WarningEvent> = _warningEvents.asSharedFlow()

    private val _currentState = MutableStateFlow<R4StateMessage?>(null)
    val currentState: StateFlow<R4StateMessage?> = _currentState.asStateFlow()

    fun attachToService(serviceFlow: SharedFlow<R4Message>, scope: CoroutineScope) {
        scope.launch {
            serviceFlow.collect { message ->
                when (message) {
                    is R4WarningEvent -> _warningEvents.emit(message)
                    is R4StateMessage -> _currentState.value = message
                }
            }
        }
    }
}
