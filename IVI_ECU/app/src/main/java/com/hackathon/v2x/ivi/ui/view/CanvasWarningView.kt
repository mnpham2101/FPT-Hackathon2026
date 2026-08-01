package com.hackathon.v2x.ivi.ui.view

import android.util.Log
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.TextMeasurer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.hackathon.v2x.ivi.model.R3Snapshot
import com.hackathon.v2x.ivi.model.R3Timestamps
import com.hackathon.v2x.ivi.model.SceneGeometry
import com.hackathon.v2x.ivi.model.VehiclePosition
import kotlinx.serialization.json.Json
import java.util.Locale
import kotlin.math.hypot

// ---------------------------------------------------------------------------
// R17 God View styling tokens
// ---------------------------------------------------------------------------

private val CanvasBackground = Color(0xFF0D0D1A)
private val CenterlineColor = Color(0xFF333355)
private val EgoColor = Color(0xFF00D4FF)
private val VehicleBColor = Color(0xFFFFB300)
private val GhostCColor = Color(0xFFFF4040)
private val ConnectorColor = Color(0xFF555577)
private val LabelColor = Color(0xFFE8E8F0)

private const val EGO_RADIUS_PX = 24f
private const val VEHICLE_RADIUS_PX = 20f
private const val CENTERLINE_STROKE_PX = 2f
private const val CONNECTOR_STROKE_PX = 1.5f
private const val GHOST_OUTLINE_STROKE_PX = 3f

/** Ghost C dashed outline per R17 spec: 12px dash, 8px gap. */
private val GhostDashEffect = PathEffect.dashPathEffect(floatArrayOf(12f, 8f), 0f)
private val CenterlineDashEffect = PathEffect.dashPathEffect(floatArrayOf(24f, 16f), 0f)
private val ConnectorDashEffect = PathEffect.dashPathEffect(floatArrayOf(8f, 6f), 0f)

/** Forward-heading triangle above ego/B markers. */
private const val HEADING_APEX_OFFSET_PX = 12f
private const val HEADING_HALF_BASE_PX = 7f

private const val LABEL_MARGIN_PX = 6f
private const val CONNECTOR_LABEL_OFFSET_PX = 10f

private val VehicleLabelStyle = TextStyle(
    color = LabelColor,
    fontSize = 12.sp,
    fontFamily = FontFamily.Monospace,
)

// --- Ghost C risk glow (17.5.3.4) ---

private val LowRiskGlowColor = Color(0xFFFFB300)
private val MediumRiskGlowColor = Color(0xFFFF6600)
private val HighRiskGlowColor = Color(0xFFFF1A1A)

private const val GLOW_MIN_ALPHA = 0.3f
private const val GLOW_MAX_ALPHA = 0.8f
private const val GLOW_PERIOD_MS = 1200
private const val GLOW_RING_EXTRA_RADIUS_PX = 10f
private const val GLOW_RING_STROKE_PX = 8f

// --- Ghost C V2X badge (17.5.3.4) ---

private val BadgeBackgroundColor = Color(0xFF1A1A2E)
private const val BADGE_BACKGROUND_ALPHA = 0.85f
private val BadgeCornerRadius = 6.dp
private val BadgePaddingHorizontal = 8.dp
private val BadgePaddingVertical = 4.dp
private const val BADGE_GAP_PX = 8f
private val BadgeTextStyle = TextStyle(
    color = LabelColor,
    fontSize = 11.sp,
    fontFamily = FontFamily.Monospace,
)

// --- Defensive source guard (17.5.3.4) ---

private val UnknownSourceColor = Color(0xFFFFD700)
private const val UNKNOWN_SOURCE_LABEL = "[? UNKNOWN SOURCE]"
private const val LOG_TAG = "IVI_V2X"

/**
 * Shared risk → color mapping (glow ring, warning banner).
 * Unknown risk states render at highest urgency (fail-safe).
 */
internal fun riskColor(riskState: String): Color = when (riskState.lowercase(Locale.US)) {
    "low" -> LowRiskGlowColor
    "medium" -> MediumRiskGlowColor
    "high" -> HighRiskGlowColor
    else -> HighRiskGlowColor
}

/**
 * Committed M1 renderer for the R17 Warning View: a top-down 2D "God View"
 * of Ego (A), the occluder (B), and Ghost C, drawn purely with Compose
 * Canvas calls — no bitmaps or external assets. Pixel geometry comes from
 * [SceneCoordinateMapper.mapScene].
 *
 * Ghost C carries a pulsing risk glow (color from `riskState`), a `[V2X]`
 * info badge, and a defensive source guard: a snapshot whose `source` is not
 * `v2x_relayed` renders as a yellow `[?]` marker and logs at ERROR — relayed
 * data must never be confused with direct detections.
 */
class CanvasWarningView : IviWarningViewSeam {

    @Composable
    override fun Render(scene: SceneGeometry, riskState: String) {
        val textMeasurer = rememberTextMeasurer()

        // Defensive guard: a missing snapshot (dev/mock scene) is trusted;
        // a snapshot with any source other than v2x_relayed is not.
        val snapshot = scene.vehicleCSnapshot
        val cSourceTrusted = snapshot == null || snapshot.source == R3Snapshot.SOURCE_V2X_RELAYED
        if (snapshot != null && !cSourceTrusted) {
            LaunchedEffect(snapshot) {
                Log.e(
                    LOG_TAG,
                    "Ghost C source guard tripped: source=\"${snapshot.source}\" != " +
                        "\"${R3Snapshot.SOURCE_V2X_RELAYED}\"; snapshot=${Json.encodeToString(R3Snapshot.serializer(), snapshot)}",
                )
            }
        }

        val glowTransition = rememberInfiniteTransition(label = "ghostCGlow")
        val glowAlpha by glowTransition.animateFloat(
            initialValue = GLOW_MIN_ALPHA,
            targetValue = GLOW_MAX_ALPHA,
            animationSpec = infiniteRepeatable(tween(GLOW_PERIOD_MS), RepeatMode.Reverse),
            label = "ghostCGlowAlpha",
        )

        Canvas(modifier = Modifier.fillMaxSize()) {
            val render = SceneCoordinateMapper.mapScene(
                scene = scene,
                canvasWidthPx = size.width,
                canvasHeightPx = size.height,
                egoRadiusPx = EGO_RADIUS_PX,
                vehicleBRadiusPx = VEHICLE_RADIUS_PX,
                vehicleCRadiusPx = VEHICLE_RADIUS_PX,
            )

            drawRect(color = CanvasBackground)
            drawRoadCenterline()

            // Connectors go under the vehicle markers.
            drawConnector(
                from = render.ego.offset,
                to = render.vehicleB.offset,
                label = "d_AB = ${formatMeters(scene.vehicleB)} m",
                textMeasurer = textMeasurer,
            )
            val ghostC = render.vehicleC
            val sceneC = scene.vehicleC
            if (ghostC != null && sceneC != null) {
                drawConnector(
                    from = render.vehicleB.offset,
                    to = ghostC.offset,
                    label = "d_AC ≈ ${formatMeters(sceneC)} m",
                    textMeasurer = textMeasurer,
                )
            }

            drawSolidVehicle(render.ego, EgoColor, "EGO", textMeasurer)
            drawSolidVehicle(render.vehicleB, VehicleBColor, "B", textMeasurer)
            if (ghostC != null && sceneC != null) {
                if (cSourceTrusted) {
                    // glowAlpha is read here — inside the draw phase only —
                    // so the pulse invalidates drawing, never composition/layout.
                    drawGhostGlow(ghostC, riskColor(riskState), glowAlpha)
                    drawGhostVehicle(ghostC, textMeasurer)
                    drawGhostBadge(
                        text = "[V2X] C · ${formatMeters(sceneC)} m · RISK: ${riskState.uppercase(Locale.US)}",
                        data = ghostC,
                        textMeasurer = textMeasurer,
                    )
                } else {
                    drawUnknownSourceVehicle(ghostC, textMeasurer)
                }
            }
        }
    }
}

/** Straight-line distance from ego (frame origin) in meters, one decimal. */
private fun formatMeters(position: VehiclePosition): String =
    String.format(Locale.US, "%.1f", hypot(position.x, position.y))

// ---------------------------------------------------------------------------
// DrawScope helpers
// ---------------------------------------------------------------------------

private fun DrawScope.drawRoadCenterline() {
    drawLine(
        color = CenterlineColor,
        start = Offset(size.width / 2f, 0f),
        end = Offset(size.width / 2f, size.height),
        strokeWidth = CENTERLINE_STROKE_PX,
        pathEffect = CenterlineDashEffect,
    )
}

private fun DrawScope.drawConnector(
    from: PixelOffset,
    to: PixelOffset,
    label: String,
    textMeasurer: TextMeasurer,
) {
    drawLine(
        color = ConnectorColor,
        start = Offset(from.x, from.y),
        end = Offset(to.x, to.y),
        strokeWidth = CONNECTOR_STROKE_PX,
        pathEffect = ConnectorDashEffect,
    )
    val layout = textMeasurer.measure(AnnotatedString(label), VehicleLabelStyle)
    drawText(
        textLayoutResult = layout,
        topLeft = Offset(
            x = (from.x + to.x) / 2f + CONNECTOR_LABEL_OFFSET_PX,
            y = (from.y + to.y) / 2f - layout.size.height / 2f,
        ),
    )
}

private fun DrawScope.drawSolidVehicle(
    data: VehicleRenderData,
    color: Color,
    label: String,
    textMeasurer: TextMeasurer,
) {
    val center = Offset(data.offset.x, data.offset.y)
    drawCircle(color = color, radius = data.radiusPx, center = center)
    drawHeadingTriangle(center, data.radiusPx, color)
    drawVehicleLabel(label, data, textMeasurer)
}

private fun DrawScope.drawGhostVehicle(data: VehicleRenderData, textMeasurer: TextMeasurer) {
    drawCircle(
        color = GhostCColor,
        radius = data.radiusPx,
        center = Offset(data.offset.x, data.offset.y),
        style = Stroke(width = GHOST_OUTLINE_STROKE_PX, pathEffect = GhostDashEffect),
    )
    drawVehicleLabel("C", data, textMeasurer)
}

/** Pulsing outer ring around Ghost C; color encodes the current risk state. */
private fun DrawScope.drawGhostGlow(data: VehicleRenderData, color: Color, alpha: Float) {
    drawCircle(
        color = color.copy(alpha = alpha),
        radius = data.radiusPx + GLOW_RING_EXTRA_RADIUS_PX,
        center = Offset(data.offset.x, data.offset.y),
        style = Stroke(width = GLOW_RING_STROKE_PX),
    )
}

/** Rounded info card above Ghost C: `[V2X] C · 28.3 m · RISK: HIGH`. */
private fun DrawScope.drawGhostBadge(
    text: String,
    data: VehicleRenderData,
    textMeasurer: TextMeasurer,
) {
    val layout = textMeasurer.measure(AnnotatedString(text), BadgeTextStyle)
    val padH = BadgePaddingHorizontal.toPx()
    val padV = BadgePaddingVertical.toPx()
    val badgeWidth = layout.size.width + 2f * padH
    val badgeHeight = layout.size.height + 2f * padV
    val topLeft = Offset(
        x = data.offset.x - badgeWidth / 2f,
        y = data.offset.y - data.radiusPx - GLOW_RING_EXTRA_RADIUS_PX - BADGE_GAP_PX - badgeHeight,
    )
    drawRoundRect(
        color = BadgeBackgroundColor.copy(alpha = BADGE_BACKGROUND_ALPHA),
        topLeft = topLeft,
        size = Size(badgeWidth, badgeHeight),
        cornerRadius = CornerRadius(BadgeCornerRadius.toPx()),
    )
    drawText(
        textLayoutResult = layout,
        topLeft = Offset(topLeft.x + padH, topLeft.y + padV),
    )
}

/**
 * Defensive-guard rendering: yellow question-mark circle instead of Ghost C
 * when the snapshot source is not `v2x_relayed`.
 */
private fun DrawScope.drawUnknownSourceVehicle(data: VehicleRenderData, textMeasurer: TextMeasurer) {
    val center = Offset(data.offset.x, data.offset.y)
    drawCircle(
        color = UnknownSourceColor,
        radius = data.radiusPx,
        center = center,
        style = Stroke(width = GHOST_OUTLINE_STROKE_PX),
    )
    val questionMark = textMeasurer.measure(
        AnnotatedString("?"),
        VehicleLabelStyle.copy(color = UnknownSourceColor),
    )
    drawText(
        textLayoutResult = questionMark,
        topLeft = Offset(
            x = center.x - questionMark.size.width / 2f,
            y = center.y - questionMark.size.height / 2f,
        ),
    )
    val label = textMeasurer.measure(
        AnnotatedString(UNKNOWN_SOURCE_LABEL),
        VehicleLabelStyle.copy(color = UnknownSourceColor),
    )
    drawText(
        textLayoutResult = label,
        topLeft = Offset(
            x = center.x - label.size.width / 2f,
            y = center.y + data.radiusPx + LABEL_MARGIN_PX,
        ),
    )
}

/** Small triangle pointing forward (up) just above the marker circle. */
private fun DrawScope.drawHeadingTriangle(center: Offset, radius: Float, color: Color) {
    val path = Path().apply {
        moveTo(center.x, center.y - radius - HEADING_APEX_OFFSET_PX)
        lineTo(center.x - HEADING_HALF_BASE_PX, center.y - radius)
        lineTo(center.x + HEADING_HALF_BASE_PX, center.y - radius)
        close()
    }
    drawPath(path = path, color = color)
}

private fun DrawScope.drawVehicleLabel(
    label: String,
    data: VehicleRenderData,
    textMeasurer: TextMeasurer,
) {
    val layout = textMeasurer.measure(AnnotatedString(label), VehicleLabelStyle)
    drawText(
        textLayoutResult = layout,
        topLeft = Offset(
            x = data.offset.x - layout.size.width / 2f,
            y = data.offset.y + data.radiusPx + LABEL_MARGIN_PX,
        ),
    )
}

// ---------------------------------------------------------------------------
// Previews
// ---------------------------------------------------------------------------

private fun previewSnapshot(source: String) = R3Snapshot(
    id = "C",
    objectClass = "vehicle",
    source = source,
    position = VehiclePosition(35f, 0f),
    distance = 35f,
    speed = 8.3f,
    confidence = 0.9f,
    state = "tracked",
    timestamps = R3Timestamps(measured = 0L, received = 0L, lastUpdated = 0L),
)

@Preview(name = "God View — Ego, B, Ghost C", widthDp = 420, heightDp = 560, showBackground = true)
@Composable
private fun CanvasWarningViewPreview() {
    CanvasWarningView().Render(
        scene = SceneGeometry(
            ego = VehiclePosition(0f, 0f),
            vehicleB = VehiclePosition(20f, 0f),
            vehicleC = VehiclePosition(35f, 0f),
            vehicleCSnapshot = previewSnapshot(R3Snapshot.SOURCE_V2X_RELAYED),
        ),
        riskState = "high",
    )
}

@Preview(name = "God View — guard: unknown source", widthDp = 420, heightDp = 560, showBackground = true)
@Composable
private fun CanvasWarningViewGuardPreview() {
    CanvasWarningView().Render(
        scene = SceneGeometry(
            ego = VehiclePosition(0f, 0f),
            vehicleB = VehiclePosition(20f, 0f),
            vehicleC = VehiclePosition(35f, 0f),
            vehicleCSnapshot = previewSnapshot(R3Snapshot.SOURCE_OWN_SENSOR),
        ),
        riskState = "high",
    )
}

@Preview(name = "God View — C not tracked", widthDp = 420, heightDp = 560, showBackground = true)
@Composable
private fun CanvasWarningViewNoCPreview() {
    CanvasWarningView().Render(
        scene = SceneGeometry(
            ego = VehiclePosition(0f, 0f),
            vehicleB = VehiclePosition(20f, 0f),
            vehicleC = null,
        ),
        riskState = "low",
    )
}
