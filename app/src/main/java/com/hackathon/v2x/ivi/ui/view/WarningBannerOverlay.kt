package com.hackathon.v2x.ivi.ui.view

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.hackathon.v2x.ivi.BuildConfig
import com.hackathon.v2x.ivi.model.R3Snapshot
import com.hackathon.v2x.ivi.model.SceneGeometry
import com.hackathon.v2x.ivi.model.VehiclePosition

// ---------------------------------------------------------------------------
// Banner styling tokens
// ---------------------------------------------------------------------------

private const val WARNING_BANNER_TEXT = "⚠  NLOS OBSTRUCTION — Vehicle C ahead (relayed via V2X)"

private val BannerTextColor = Color(0xFFE8E8F0)
private const val BANNER_BACKGROUND_ALPHA = 0.85f
private val BannerPadding = 12.dp
private val BannerTextSize = 16.sp
private const val PROGRESS_TRACK_ALPHA = 0.25f

/**
 * Alert banner overlaid (via `Box`) on top of [CanvasWarningView]: warning
 * type text, a countdown bar depleting over [warningTimeoutMs], and a
 * dismiss button.
 *
 * State contract:
 * - [warningActive] mirrors `WarningViewModel.uiWarningState != Idle` — the
 *   wiring lands with 4.5.1.4 / 16.5.4.1. When the state returns to `Idle`
 *   this becomes `false` and the banner disappears (the ViewModel clears the
 *   canvas through its own state).
 * - The [X] button hides *only this banner*; the Warning View canvas
 *   underneath keeps rendering — dismissal is local UI state that resets
 *   when the next warning activates.
 * - The countdown bar is display-only; the authoritative auto-dismiss timer
 *   lives in `WarningViewModel`.
 */
@Composable
fun WarningBannerOverlay(
    warningActive: Boolean,
    riskState: String,
    modifier: Modifier = Modifier,
    warningTimeoutMs: Long = BuildConfig.WARNING_TIMEOUT_MS,
) {
    var dismissed by remember(warningActive) { mutableStateOf(false) }

    val countdown = remember { Animatable(1f) }
    LaunchedEffect(warningActive) {
        if (warningActive) {
            countdown.snapTo(1f)
            countdown.animateTo(
                targetValue = 0f,
                animationSpec = tween(warningTimeoutMs.toInt(), easing = LinearEasing),
            )
        }
    }

    if (!warningActive || dismissed) {
        return
    }

    Box(modifier = modifier.fillMaxSize()) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .align(Alignment.TopCenter)
                .background(riskColor(riskState).copy(alpha = BANNER_BACKGROUND_ALPHA)),
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = BannerPadding),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = WARNING_BANNER_TEXT,
                    color = BannerTextColor,
                    fontSize = BannerTextSize,
                    fontWeight = FontWeight.SemiBold,
                    modifier = Modifier
                        .weight(1f)
                        .padding(vertical = BannerPadding),
                )
                // IconButton's default touch target already satisfies the 48dp floor.
                IconButton(onClick = { dismissed = true }) {
                    Icon(
                        imageVector = Icons.Filled.Close,
                        contentDescription = "Dismiss warning banner",
                        tint = BannerTextColor,
                    )
                }
            }
            LinearProgressIndicator(
                progress = { countdown.value },
                modifier = Modifier.fillMaxWidth(),
                color = BannerTextColor,
                trackColor = BannerTextColor.copy(alpha = PROGRESS_TRACK_ALPHA),
            )
        }
    }
}

// ---------------------------------------------------------------------------
// Previews
// ---------------------------------------------------------------------------

@Preview(name = "Banner — low risk", widthDp = 800, heightDp = 120, showBackground = true)
@Composable
private fun WarningBannerLowPreview() {
    WarningBannerOverlay(warningActive = true, riskState = "low")
}

@Preview(name = "Banner — medium risk", widthDp = 800, heightDp = 120, showBackground = true)
@Composable
private fun WarningBannerMediumPreview() {
    WarningBannerOverlay(warningActive = true, riskState = "medium")
}

@Preview(name = "Banner — high risk", widthDp = 800, heightDp = 120, showBackground = true)
@Composable
private fun WarningBannerHighPreview() {
    WarningBannerOverlay(warningActive = true, riskState = "high")
}

@Preview(name = "Banner over God View", widthDp = 420, heightDp = 560, showBackground = true)
@Composable
private fun WarningBannerOverCanvasPreview() {
    Box {
        CanvasWarningView().Render(
            scene = SceneGeometry(
                ego = VehiclePosition(0f, 0f),
                vehicleB = VehiclePosition(20f, 0f),
                vehicleC = VehiclePosition(35f, 0f),
                vehicleCSnapshot = R3Snapshot(
                    id = "C",
                    source = R3Snapshot.SOURCE_V2X_RELAYED,
                    position = VehiclePosition(35f, 0f),
                    distance = 35f,
                    speed = 8.3f,
                    confidence = 0.9f,
                    state = "tracked",
                ),
            ),
            riskState = "high",
        )
        WarningBannerOverlay(warningActive = true, riskState = "high")
    }
}
