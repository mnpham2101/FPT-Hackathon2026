package com.hackathon.v2x.ivi.ui

import com.hackathon.v2x.ivi.model.R4WarningEvent

/**
 * UI state for the Warning View display area.
 *
 * Subtask 4.5.1.4:
 * - [Idle]   → no active warning; Display Area shows previous mode
 * - [Active] → R4 warning received; Warning View is forced visible
 * - [Error]  → R4ListenerService exhausted retries; show error indicator
 */
sealed class WarningUiState {
    object Idle : WarningUiState()
    data class Active(val event: R4WarningEvent) : WarningUiState()
    object Error : WarningUiState()
}

