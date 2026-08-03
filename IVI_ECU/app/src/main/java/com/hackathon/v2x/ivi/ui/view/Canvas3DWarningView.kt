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
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.TextMeasurer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.sp
import com.hackathon.v2x.ivi.model.R3Snapshot
import com.hackathon.v2x.ivi.model.SceneGeometry
import com.hackathon.v2x.ivi.model.VehiclePosition
import java.util.Locale
import kotlin.math.hypot

// ---------------------------------------------------------------------------
// Cybertruck HMI Design Tokens
// Steel-cold, angular, military tactical — zero rounded corners
// ---------------------------------------------------------------------------

internal val CyberBackground    = Color(0xFF0A0C10)  // near-black steel
internal val CyberRoad          = Color(0xFF111418)  // slightly lighter lane surface
internal val CyberRoadMark      = Color(0xFF1E2530)  // dim lane markings
internal val CyberGrid          = Color(0xFF141A22)  // subtle background grid

internal val CyberEgo           = Color(0xFFE8ECF0)  // pure white-steel, Ego vehicle
internal val CyberB             = Color(0xFFFFB300)  // amber — occluder B
internal val CyberGhostC        = Color(0xFFE03040)  // desaturated red — Ghost C
internal val CyberConnector     = Color(0xFF2E3D50)  // muted connector lines
internal val CyberText          = Color(0xFFCDD4DC)  // cool-white monospace text
internal val CyberMuted         = Color(0xFF4A5868)  // dim metadata text

internal val CyberRiskLow       = Color(0xFF3A8C4E)  // muted green
internal val CyberRiskMedium    = Color(0xFFCC7A00)  // dark amber
internal val CyberRiskHigh      = Color(0xFFCC2233)  // military red

// Stroke widths
private const val EDGE_STROKE   = 2.5f
private const val DASH_STROKE   = 2.0f
private const val CONNECTOR_SW  = 1.5f
private const val ROAD_SW       = 1.2f

// Dash effects — angular military pattern
private val GhostDash   = PathEffect.dashPathEffect(floatArrayOf(10f, 7f), 0f)
private val ConnectDash = PathEffect.dashPathEffect(floatArrayOf(6f, 5f), 0f)
private val LaneDash    = PathEffect.dashPathEffect(floatArrayOf(20f, 14f), 0f)

// Typography
private val HudMono = TextStyle(color = CyberText, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
private val HudMonoSmall = TextStyle(color = CyberMuted, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
private val HudHeader = TextStyle(
    color = CyberText, fontSize = 18.sp,
    fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold
)
private val HudSubHeader = TextStyle(
    color = CyberGhostC, fontSize = 12.sp,
    fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold
)
private val BadgeText = TextStyle(
    color = CyberRiskHigh, fontSize = 10.sp,
    fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold
)

// Glow pulse
private const val GLOW_MIN   = 0.20f
private const val GLOW_MAX   = 0.70f
private const val GLOW_MS    = 1400
private const val LOG_TAG    = "IVI_V2X_3D"

// ---------------------------------------------------------------------------
// Cybertruck Risk colour
// ---------------------------------------------------------------------------

internal fun cyberRiskColor(riskState: String): Color = when (riskState.lowercase(Locale.US)) {
    "low"    -> CyberRiskLow
    "medium" -> CyberRiskMedium
    "high"   -> CyberRiskHigh
    else     -> CyberRiskHigh
}

// ---------------------------------------------------------------------------
// Canvas3DWarningView — Cybertruck HMI Renderer (implements IviWarningViewSeam)
// ---------------------------------------------------------------------------

/**
 * Cybertruck-style 3D God View renderer.
 *
 * Design language: angular military HUD, steel-cold palette, no rounded
 * corners, sharp monospace typography, radar-pulse threat rings, isometric
 * road perspective identical to the Tesla Cybertruck centre console UI.
 */
class Canvas3DWarningView : IviWarningViewSeam {

    @Composable
    override fun Render(scene: SceneGeometry, riskState: String) {
        val textMeasurer = rememberTextMeasurer()

        val snapshot = scene.vehicleCSnapshot
        val cSourceTrusted = isGhostCSourceTrusted(snapshot)
        if (snapshot != null && !cSourceTrusted) {
            LaunchedEffect(snapshot) {
                Log.e(LOG_TAG, ghostCSourceGuardErrorMessage(snapshot))
            }
        }

        // Radar pulse animation for Ghost C
        val radarTransition = rememberInfiniteTransition(label = "cyberRadar")
        val radarAlpha by radarTransition.animateFloat(
            initialValue = GLOW_MIN, targetValue = GLOW_MAX,
            animationSpec = infiniteRepeatable(tween(GLOW_MS), RepeatMode.Reverse),
            label = "cyberRadarAlpha",
        )

        Canvas(modifier = Modifier.fillMaxSize()) {
            val w = size.width
            val h = size.height

            // ── Background ──────────────────────────────────────────────────
            drawRect(color = CyberBackground)

            // ── Road & lane geometry ─────────────────────────────────────────
            drawCyberRoad(w, h)

            // ── Build 3D vehicle boxes ──────────────────────────────────────
            val egoBox = Scene3DProjection.buildVehicleBox(
                scene.ego, canvasWidthPx = w, canvasHeightPx = h)
            val bBox = Scene3DProjection.buildVehicleBox(
                scene.vehicleB, canvasWidthPx = w, canvasHeightPx = h)
            val sceneC = scene.vehicleC
            val cBox  = sceneC?.let {
                Scene3DProjection.buildVehicleBox(it, canvasWidthPx = w, canvasHeightPx = h)
            }

            // ── Connectors (behind vehicles) ────────────────────────────────
            drawCyberConnector(
                from  = egoBox.centerBase,
                to    = bBox.centerBase,
                label = "AB: ${formatMeters(scene.vehicleB)} m",
                textMeasurer = textMeasurer,
            )
            if (cBox != null && sceneC != null) {
                drawCyberConnector(
                    from  = bBox.centerBase,
                    to    = cBox.centerBase,
                    label = "AC: ${formatMeters(sceneC)} m",
                    textMeasurer = textMeasurer,
                )
            }

            // ── Vehicles ────────────────────────────────────────────────────
            drawCyberVehicle(egoBox, CyberEgo, "EGO", textMeasurer, filled = true)
            drawCyberVehicle(bBox, CyberB, "B", textMeasurer, filled = false)

            if (cBox != null && sceneC != null) {
                if (cSourceTrusted) {
                    drawCyberRadarPulse(cBox.centerBase, cyberRiskColor(riskState), radarAlpha)
                    drawCyberGhostVehicle(cBox, textMeasurer)
                    drawCyberV2XBadge(
                        text = "[V2X] C · ${formatMeters(sceneC)} m · ${riskState.uppercase(Locale.US)}",
                        box  = cBox,
                        riskState = riskState,
                        textMeasurer = textMeasurer,
                    )
                } else {
                    drawCyberVehicle(cBox, Color(0xFFFFD700), UNKNOWN_SOURCE_LABEL, textMeasurer, filled = false)
                }
            }

            // ── HUD Overlay ──────────────────────────────────────────────────
            drawCyberHeader(w, sceneC != null, textMeasurer)
            drawCyberStatusBar(w, h, riskState, textMeasurer)
        }
    }
}

private fun formatMeters(pos: VehiclePosition): String =
    String.format(Locale.US, "%.1f", hypot(pos.x, pos.y))

// ---------------------------------------------------------------------------
// Road geometry — angled isometric lanes (Cybertruck style)
// ---------------------------------------------------------------------------

private fun DrawScope.drawCyberRoad(w: Float, h: Float) {
    // Road surface — dark trapezoid receding into horizon
    val roadPath = Path().apply {
        moveTo(w * 0.25f, h)             // bottom-left
        lineTo(w * 0.75f, h)             // bottom-right
        lineTo(w * 0.60f, h * 0.15f)    // top-right (horizon)
        lineTo(w * 0.40f, h * 0.15f)    // top-left  (horizon)
        close()
    }
    drawPath(roadPath, color = CyberRoad)

    // Left road edge
    drawLine(
        color = CyberRoadMark.copy(alpha = 0.8f),
        start = Offset(w * 0.25f, h),
        end   = Offset(w * 0.40f, h * 0.15f),
        strokeWidth = ROAD_SW,
    )
    // Right road edge
    drawLine(
        color = CyberRoadMark.copy(alpha = 0.8f),
        start = Offset(w * 0.75f, h),
        end   = Offset(w * 0.60f, h * 0.15f),
        strokeWidth = ROAD_SW,
    )

    // Centre dashed lane marking
    drawLine(
        color = CyberRoadMark,
        start = Offset(w * 0.50f, h),
        end   = Offset(w * 0.50f, h * 0.15f),
        strokeWidth = ROAD_SW,
        pathEffect = LaneDash,
    )

    // Receding horizontal grid lines
    for (frac in listOf(0.85f, 0.70f, 0.55f, 0.40f, 0.25f)) {
        val t   = 1f - frac               // 0 at bottom, 1 at horizon
        val xL  = w * 0.25f + t * (w * 0.40f - w * 0.25f)
        val xR  = w * 0.75f - t * (w * 0.75f - w * 0.60f)
        val y   = h * frac
        drawLine(
            color = CyberRoadMark.copy(alpha = 0.4f * (1f - t)),
            start = Offset(xL, y),
            end   = Offset(xR, y),
            strokeWidth = 0.8f,
        )
    }
}

// ---------------------------------------------------------------------------
// Cyber vehicle — angular box, top face emphasised
// ---------------------------------------------------------------------------

private fun DrawScope.drawCyberVehicle(
    box: Vehicle3DBox,
    color: Color,
    label: String,
    textMeasurer: TextMeasurer,
    filled: Boolean,
) {
    val top = Path().apply {
        moveTo(box.topCorners[0].screenX, box.topCorners[0].screenY)
        for (i in 1..3) lineTo(box.topCorners[i].screenX, box.topCorners[i].screenY)
        close()
    }

    if (filled) {
        // Ego gets a subtle translucent fill so it reads as "solid"
        drawPath(top, color = color.copy(alpha = 0.12f))
    }

    // Top face outline — sharp square joints (Cybertruck: no rounded corners)
    drawPath(top, color = color, style = Stroke(
        width = EDGE_STROKE, join = StrokeJoin.Miter,
    ))

    // Side vertical pillars
    for (i in 0..3) {
        drawLine(
            color = color.copy(alpha = 0.6f),
            start = Offset(box.bottomCorners[i].screenX, box.bottomCorners[i].screenY),
            end   = Offset(box.topCorners[i].screenX,   box.topCorners[i].screenY),
            strokeWidth = EDGE_STROKE * 0.8f,
        )
    }

    // Bottom base outline (visible sides only — front & sides, not hidden rear)
    val base = Path().apply {
        moveTo(box.bottomCorners[0].screenX, box.bottomCorners[0].screenY)
        for (i in 1..3) lineTo(box.bottomCorners[i].screenX, box.bottomCorners[i].screenY)
        close()
    }
    drawPath(base, color = color.copy(alpha = 0.25f), style = Stroke(width = EDGE_STROKE * 0.6f))

    // Corner bracket accents (Cybertruck: corner brackets highlight the bounding box)
    drawCornerBrackets(box, color)

    // Label above top face
    val layout = textMeasurer.measure(AnnotatedString(label), HudMono.copy(color = color))
    drawText(
        textLayoutResult = layout,
        topLeft = Offset(
            box.centerTop.screenX - layout.size.width / 2f,
            box.centerTop.screenY - layout.size.height - 5f,
        ),
    )
}

// ---------------------------------------------------------------------------
// Ghost C — dashed wireframe + radar pulse
// ---------------------------------------------------------------------------

private fun DrawScope.drawCyberGhostVehicle(
    box: Vehicle3DBox,
    textMeasurer: TextMeasurer,
) {
    val color = CyberGhostC
    val top = Path().apply {
        moveTo(box.topCorners[0].screenX, box.topCorners[0].screenY)
        for (i in 1..3) lineTo(box.topCorners[i].screenX, box.topCorners[i].screenY)
        close()
    }

    // Translucent red fill
    drawPath(top, color = color.copy(alpha = 0.10f))

    // Dashed top outline
    drawPath(top, color = color, style = Stroke(
        width = DASH_STROKE, pathEffect = GhostDash, join = StrokeJoin.Miter,
    ))

    // Dashed vertical pillars
    for (i in 0..3) {
        drawLine(
            color = color.copy(alpha = 0.7f),
            start = Offset(box.bottomCorners[i].screenX, box.bottomCorners[i].screenY),
            end   = Offset(box.topCorners[i].screenX,   box.topCorners[i].screenY),
            strokeWidth = DASH_STROKE,
            pathEffect  = GhostDash,
        )
    }

    // Dashed base
    val base = Path().apply {
        moveTo(box.bottomCorners[0].screenX, box.bottomCorners[0].screenY)
        for (i in 1..3) lineTo(box.bottomCorners[i].screenX, box.bottomCorners[i].screenY)
        close()
    }
    drawPath(base, color = color.copy(alpha = 0.35f), style = Stroke(
        width = DASH_STROKE * 0.8f, pathEffect = GhostDash,
    ))

    // Ghost label
    val layout = textMeasurer.measure(AnnotatedString("GHOST C"), HudMono.copy(color = color))
    drawText(
        textLayoutResult = layout,
        topLeft = Offset(
            box.centerTop.screenX - layout.size.width / 2f,
            box.centerTop.screenY - layout.size.height - 5f,
        ),
    )
}

// ---------------------------------------------------------------------------
// Radar pulse rings under Ghost C — Cybertruck sonar sweep aesthetic
// ---------------------------------------------------------------------------

private fun DrawScope.drawCyberRadarPulse(
    center: ProjectedPoint3D,
    color: Color,
    alpha: Float,
) {
    val sc = center.scale.coerceIn(0.3f, 1.0f)
    for (ring in 1..3) {
        val rx = (28f + ring * 18f) * sc
        val ry = (14f + ring * 9f)  * sc
        val ringAlpha = (alpha / ring).coerceIn(0f, 1f)
        drawOval(
            color = color.copy(alpha = ringAlpha * 0.5f),
            topLeft = Offset(center.screenX - rx, center.screenY - ry),
            size = Size(rx * 2f, ry * 2f),
            style = Stroke(width = (1.5f / ring), cap = StrokeCap.Butt),
        )
    }
    // Centre cross-hair dot
    drawCircle(
        color = color.copy(alpha = alpha),
        radius = 4f * sc,
        center = Offset(center.screenX, center.screenY),
    )
}

// ---------------------------------------------------------------------------
// Corner bracket accents
// ---------------------------------------------------------------------------

private fun DrawScope.drawCornerBrackets(box: Vehicle3DBox, color: Color) {
    val c = color.copy(alpha = 0.9f)
    val sw = EDGE_STROKE * 1.2f
    for (pt in box.topCorners) {
        val bLen = 6f * pt.scale.coerceIn(0.4f, 1.0f)
        // Horizontal bracket segment
        drawLine(c, start = Offset(pt.screenX - bLen, pt.screenY),
            end = Offset(pt.screenX + bLen, pt.screenY), strokeWidth = sw)
        // Vertical bracket segment
        drawLine(c, start = Offset(pt.screenX, pt.screenY - bLen),
            end = Offset(pt.screenX, pt.screenY + bLen), strokeWidth = sw)
    }
}

// ---------------------------------------------------------------------------
// Connector — angular L-shape (90° bend) for Cybertruck circuit look
// ---------------------------------------------------------------------------

private fun DrawScope.drawCyberConnector(
    from: ProjectedPoint3D,
    to: ProjectedPoint3D,
    label: String,
    textMeasurer: TextMeasurer,
) {
    // 90-degree elbow: go horizontal first, then vertical
    val midX = (from.screenX + to.screenX) / 2f
    drawLine(
        color = CyberConnector,
        start = Offset(from.screenX, from.screenY),
        end   = Offset(midX, from.screenY),
        strokeWidth = CONNECTOR_SW,
        pathEffect = ConnectDash,
    )
    drawLine(
        color = CyberConnector,
        start = Offset(midX, from.screenY),
        end   = Offset(midX, to.screenY),
        strokeWidth = CONNECTOR_SW,
        pathEffect = ConnectDash,
    )
    drawLine(
        color = CyberConnector,
        start = Offset(midX, to.screenY),
        end   = Offset(to.screenX, to.screenY),
        strokeWidth = CONNECTOR_SW,
        pathEffect = ConnectDash,
    )

    // Distance label — placed at the elbow
    val layout = textMeasurer.measure(AnnotatedString(label), HudMonoSmall)
    drawText(
        textLayoutResult = layout,
        topLeft = Offset(midX + 5f, (from.screenY + to.screenY) / 2f - layout.size.height / 2f),
    )
}

// ---------------------------------------------------------------------------
// V2X info badge — no rounded corners, sharp military rect
// ---------------------------------------------------------------------------

private fun DrawScope.drawCyberV2XBadge(
    text: String,
    box: Vehicle3DBox,
    riskState: String,
    textMeasurer: TextMeasurer,
) {
    val layout = textMeasurer.measure(AnnotatedString(text), BadgeText)
    val padH = 10f; val padV = 5f
    val bW = layout.size.width + padH * 2f
    val bH = layout.size.height + padV * 2f
    val bX = box.centerTop.screenX - bW / 2f
    val bY = box.centerTop.screenY - bH - 22f

    drawRect(color = CyberBackground.copy(alpha = 0.92f),
        topLeft = Offset(bX, bY), size = Size(bW, bH))
    drawRect(color = cyberRiskColor(riskState).copy(alpha = 0.8f),
        topLeft = Offset(bX, bY), size = Size(bW, bH),
        style = Stroke(width = 1.2f))
    drawText(layout, topLeft = Offset(bX + padH, bY + padV))
}

// ---------------------------------------------------------------------------
// HUD overlay: NLOS THREAT DETECTED header (top-left)
// ---------------------------------------------------------------------------

private fun DrawScope.drawCyberHeader(
    w: Float,
    hasGhostC: Boolean,
    textMeasurer: TextMeasurer,
) {
    val titleText  = "NLOS GOD VIEW"
    val titleL = textMeasurer.measure(AnnotatedString(titleText), HudHeader)
    drawText(titleL, topLeft = Offset(18f, 14f))

    if (hasGhostC) {
        val warnText = "▲  NLOS THREAT DETECTED"
        val warnL = textMeasurer.measure(AnnotatedString(warnText), HudSubHeader)
        drawText(warnL, topLeft = Offset(18f, 14f + titleL.size.height + 4f))
    }

    // Top-right corner: "V2X RADAR" label
    val radarText = "V2X RADAR"
    val radarL = textMeasurer.measure(AnnotatedString(radarText), HudMonoSmall)
    drawText(radarL, topLeft = Offset(w - radarL.size.width - 18f, 14f))
}

// ---------------------------------------------------------------------------
// Status bar — bottom strip like Cybertruck media bar
// ---------------------------------------------------------------------------

private fun DrawScope.drawCyberStatusBar(
    w: Float,
    h: Float,
    riskState: String,
    textMeasurer: TextMeasurer,
) {
    val barH = 34f
    // Background strip
    drawRect(
        color = Color(0xFF0D1018),
        topLeft = Offset(0f, h - barH),
        size = Size(w, barH),
    )
    // Top border line
    drawLine(
        color = CyberRoadMark,
        start = Offset(0f, h - barH),
        end   = Offset(w, h - barH),
        strokeWidth = 0.8f,
    )

    // Status text
    val risk = riskState.uppercase(Locale.US)
    val riskColor = cyberRiskColor(riskState)
    val statusText = "V2X LINK: ACTIVE   |   RISK: $risk"
    val statusL = textMeasurer.measure(AnnotatedString(statusText),
        HudMono.copy(color = riskColor))
    drawText(statusL, topLeft = Offset(
        (w - statusL.size.width) / 2f,
        h - barH + (barH - statusL.size.height) / 2f,
    ))

    // Left corner — EGO label
    val egoL = textMeasurer.measure(AnnotatedString("EGO  ●"), HudMonoSmall)
    drawText(egoL, topLeft = Offset(18f, h - barH + (barH - egoL.size.height) / 2f))

    // Right corner — timestamp placeholder
    val tsL = textMeasurer.measure(AnnotatedString("R17 ●"), HudMonoSmall)
    drawText(tsL, topLeft = Offset(w - tsL.size.width - 18f,
        h - barH + (barH - tsL.size.height) / 2f))
}
