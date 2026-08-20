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
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.rotate
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
// Legacy Cyber* token aliases — kept for unit-test backward-compatibility
// (CybertruckGodViewTest asserts exact ARGB hex values)
// ---------------------------------------------------------------------------
internal val CyberBackground = Color(0xFF0A0C10)
internal val CyberEgo        = Color(0xFFE8ECF0)
internal val CyberB          = Color(0xFFFFB300)
internal val CyberGhostC     = Color(0xFFE03040)
internal val CyberRiskLow    = Color(0xFF3A8C4E)
internal val CyberRiskMedium = Color(0xFFCC7A00)
internal val CyberRiskHigh   = Color(0xFFCC2233)

// ---------------------------------------------------------------------------
// Design Tokens — Tesla / Cybertruck IVI palette
// ---------------------------------------------------------------------------
private val BgColor         = Color(0xFF0D0D1A)
private val RoadColor       = Color(0xFF1B1E27)
private val RoadEdgeColor   = Color(0xFF3A4050)
private val LaneDivColor    = Color(0xFF333355)
private val EgoColor        = Color(0xFFE8ECF0)
private val BColor          = Color(0xFFFFB300)
private val GhostCColor     = Color(0xFFFF4040)
private val ConnectorColor  = Color(0xFF555577)
private val TextMain        = Color(0xFFE8E8F0)
private val TextMuted       = Color(0xFF8A91A6)
private val TextDim         = Color(0xFF6E7690)
private val PanelDiv        = Color(0xFF262B38)
private val BlindZoneColor  = Color(0xFFFF4040)

private val RiskLow    = Color(0xFFFFB300)
private val RiskMedium = Color(0xFFFF6600)
private val RiskHigh   = Color(0xFFFF1A1A)

private val LaneDash  = PathEffect.dashPathEffect(floatArrayOf(24f, 16f), 0f)
private val GhostDash = PathEffect.dashPathEffect(floatArrayOf(12f, 8f), 0f)
private val RailDash  = PathEffect.dashPathEffect(floatArrayOf(8f, 6f), 0f)

private val StyleMain = TextStyle(color = TextMain, fontSize = 13.sp, fontFamily = FontFamily.Monospace)
private val StyleMuted = TextStyle(color = TextMuted, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
private val StyleDim = TextStyle(color = TextDim, fontSize = 11.sp, fontFamily = FontFamily.Monospace, letterSpacing = 1.8.sp)
private val StyleBadge = TextStyle(color = GhostCColor, fontSize = 11.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
private val StyleHeader = TextStyle(color = TextMain, fontSize = 16.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
private val StyleAlert = TextStyle(color = GhostCColor, fontSize = 11.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
private val StyleBlindZone = TextStyle(color = BlindZoneColor.copy(alpha = 0.7f), fontSize = 9.sp, fontFamily = FontFamily.Monospace, letterSpacing = 1.6.sp)

internal fun cyberRiskColor(riskState: String): Color = when (riskState.lowercase(Locale.US)) {
    "low"    -> CyberRiskLow
    "medium" -> CyberRiskMedium
    else     -> CyberRiskHigh
}

private const val LOG_TAG = "IVI_V2X_GODVIEW"
private const val GLOW_MS = 1200

// ---------------------------------------------------------------------------
// Canvas3DWarningView — Tesla top-down God View (faithful to ivi-god-view-warning-screen.svg)
// ---------------------------------------------------------------------------

/**
 * Tesla-style top-down God View renderer.
 *
 * Layout:
 *   LEFT  (60%): Road viewport with ego A at bottom, occluder B ahead, ghost C at top.
 *   RIGHT (40%): Side panel with legend, risk colour key.
 * Bottom: Status bar.
 */
class Canvas3DWarningView : IviWarningViewSeam {

    @Composable
    override fun Render(scene: SceneGeometry, riskState: String) {
        val textMeasurer = rememberTextMeasurer()

        val snapshot = scene.vehicleCSnapshot
        val cSourceTrusted = isGhostCSourceTrusted(snapshot)
        if (snapshot != null && !cSourceTrusted) {
            LaunchedEffect(snapshot) { Log.e(LOG_TAG, ghostCSourceGuardErrorMessage(snapshot)) }
        }

        val glowTrans = rememberInfiniteTransition(label = "glow")
        val glowAlpha by glowTrans.animateFloat(
            initialValue = 0.25f, targetValue = 0.80f,
            animationSpec = infiniteRepeatable(tween(GLOW_MS), RepeatMode.Reverse),
            label = "glowAlpha",
        )

        Canvas(modifier = Modifier.fillMaxSize()) {
            val w = size.width
            val h = size.height

            val panelX  = w * 0.60f   // divider between road view and side panel
            val roadW   = panelX
            val barH    = 32f
            val roadH   = h - barH

            // ── Background ───────────────────────────────────────────────────
            drawRect(BgColor)

            // ── Road viewport (left 60%) ──────────────────────────────────────
            drawRoad(roadW = roadW, roadH = roadH)

            // ── Blind zone (occlusion cone behind B) ─────────────────────────
            val egoX = roadW * 0.50f
            val egoY = roadH * 0.84f   // ego A anchor
            val bX   = roadW * 0.50f
            val bY   = roadH * 0.56f   // vehicle B anchor
            val cX   = roadW * 0.50f
            val cY   = roadH * 0.28f   // ghost C anchor

            drawBlindZone(egoX, egoY, bX, bY, roadW, textMeasurer)

            // ── Distance rails ───────────────────────────────────────────────
            val hasC = scene.vehicleC != null && cSourceTrusted
            val distAB = formatMeters(scene.vehicleB)
            val distAC = if (scene.vehicleC != null) formatMeters(scene.vehicleC!!) else null

            drawDistanceRail(
                x1 = roadW * 0.12f, y1 = bY, y2 = egoY,
                label = "d_AB = $distAB m",
                dashed = false, textMeasurer = textMeasurer
            )
            if (hasC && distAC != null) {
                drawDistanceRail(
                    x1 = roadW * 0.88f, y1 = cY, y2 = egoY,
                    label = "d_AC ≈ $distAC m",
                    dashed = true, textMeasurer = textMeasurer
                )
            }

            // ── Ghost C (draw first, behind occluder B) ───────────────────────
            if (hasC) {
                val risk = cyberRiskColor(riskState)
                drawGhostC(cx = cX, cy = cY, glowAlpha = glowAlpha, riskColor = risk, textMeasurer = textMeasurer)
                drawGhostBadge(cx = cX, cy = cY, dist = distAC ?: "?", riskState = riskState, textMeasurer = textMeasurer)
            }

            // ── Occluder B ────────────────────────────────────────────────────
            drawCarTopDown(cx = bX, cy = bY, color = BColor, label = "B", scale = 0.92f, filled = false, textMeasurer = textMeasurer)

            // ── Ego A ─────────────────────────────────────────────────────────
            drawCarTopDown(cx = egoX, cy = egoY, color = EgoColor, label = "EGO (A)", scale = 1.0f, filled = true, textMeasurer = textMeasurer)

            // ── Header (top-left) ─────────────────────────────────────────────
            drawHeader(hasGhostC = hasC, textMeasurer = textMeasurer)

            // ── Side panel divider ────────────────────────────────────────────
            drawLine(PanelDiv, start = Offset(panelX, 40f), end = Offset(panelX, h - barH - 4f), strokeWidth = 1.5f)

            // ── Side panel: legend + risk key ─────────────────────────────────
            drawSidePanel(x0 = panelX + 20f, w = w, h = h, barH = barH, riskState = riskState, textMeasurer = textMeasurer)

            // ── Status bar ────────────────────────────────────────────────────
            drawStatusBar(w = w, h = h, barH = barH, riskState = riskState, textMeasurer = textMeasurer)
        }
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

private fun formatMeters(pos: VehiclePosition): String =
    String.format(Locale.US, "%.1f", hypot(pos.x, pos.y))

// ---------------------------------------------------------------------------
// Road — tapered perspective lane (matches SVG polygon points: 140-460/600 & 180-420/600)
// ---------------------------------------------------------------------------

private fun DrawScope.drawRoad(roadW: Float, roadH: Float) {
    // Road surface trapezoid — centered at 0.50f
    val roadPath = Path().apply {
        moveTo(roadW * 0.233f, roadH)          // bottom-left (23.3%)
        lineTo(roadW * 0.767f, roadH)          // bottom-right (76.7%)
        lineTo(roadW * 0.700f, roadH * 0.09f) // top-right (70.0%)
        lineTo(roadW * 0.300f, roadH * 0.09f) // top-left  (30.0%)
        close()
    }
    drawPath(roadPath, color = RoadColor)

    // Road edges
    drawLine(RoadEdgeColor, start = Offset(roadW * 0.233f, roadH), end = Offset(roadW * 0.300f, roadH * 0.09f), strokeWidth = 2.5f)
    drawLine(RoadEdgeColor, start = Offset(roadW * 0.767f, roadH), end = Offset(roadW * 0.700f, roadH * 0.09f), strokeWidth = 2.5f)

    // Lane dividers (dashed)
    drawLine(LaneDivColor, start = Offset(roadW * 0.412f, roadH), end = Offset(roadW * 0.433f, roadH * 0.09f), strokeWidth = 2f, pathEffect = LaneDash)
    drawLine(LaneDivColor, start = Offset(roadW * 0.588f, roadH), end = Offset(roadW * 0.567f, roadH * 0.09f), strokeWidth = 2f, pathEffect = LaneDash)

    // Horizon fade rect (gradient-like fade toward top)
    val fadeH = roadH * 0.22f
    val fadePath = Path().apply {
        moveTo(roadW * 0.300f, roadH * 0.09f)
        lineTo(roadW * 0.700f, roadH * 0.09f)
        lineTo(roadW * 0.700f, roadH * 0.09f + fadeH)
        lineTo(roadW * 0.300f, roadH * 0.09f + fadeH)
        close()
    }
    drawPath(fadePath, brush = Brush.verticalGradient(
        colors = listOf(BgColor.copy(alpha = 0.95f), Color.Transparent),
        startY = roadH * 0.09f,
        endY = roadH * 0.09f + fadeH,
    ))
}

// ---------------------------------------------------------------------------
// Blind zone — occlusion cone from ego behind B
// ---------------------------------------------------------------------------

private fun DrawScope.drawBlindZone(
    egoX: Float, egoY: Float,
    bX: Float, bY: Float,
    roadW: Float,
    textMeasurer: TextMeasurer,
) {
    val zoneTopY = roadW * 0.09f * 0.6f // near horizon
    val halfWidth = roadW * 0.065f      // half-width at horizon

    val conePath = Path().apply {
        moveTo(bX, bY)
        lineTo(bX - halfWidth, zoneTopY)
        lineTo(bX + halfWidth, zoneTopY)
        close()
    }

    // Hatched fill (approximate with semi-transparent overlay)
    drawPath(conePath, color = BlindZoneColor.copy(alpha = 0.05f))

    // Dashed left ray
    drawLine(
        color = BlindZoneColor.copy(alpha = 0.35f),
        start = Offset(bX, bY),
        end = Offset(bX - halfWidth, zoneTopY),
        strokeWidth = 1.2f, pathEffect = GhostDash,
    )
    // Dashed right ray
    drawLine(
        color = BlindZoneColor.copy(alpha = 0.35f),
        start = Offset(bX, bY),
        end = Offset(bX + halfWidth, zoneTopY),
        strokeWidth = 1.2f, pathEffect = GhostDash,
    )

    // BLIND ZONE label
    val lbl = textMeasurer.measure(AnnotatedString("BLIND ZONE"), StyleBlindZone)
    drawText(lbl, topLeft = Offset(bX - lbl.size.width / 2f, (bY + zoneTopY) / 2f - lbl.size.height / 2f))
}

// ---------------------------------------------------------------------------
// Distance measurement rail (left or right of road)
// ---------------------------------------------------------------------------

private fun DrawScope.drawDistanceRail(
    x1: Float, y1: Float, y2: Float,
    label: String, dashed: Boolean,
    textMeasurer: TextMeasurer,
) {
    val tickHalf = 8f
    val effect = if (dashed) RailDash else null

    // Vertical line
    drawLine(ConnectorColor, start = Offset(x1, y1), end = Offset(x1, y2), strokeWidth = 1.4f, pathEffect = effect)

    // Tick at top
    drawLine(ConnectorColor, start = Offset(x1 - tickHalf, y1), end = Offset(x1 + tickHalf, y1), strokeWidth = 1.4f)
    // Tick at bottom
    drawLine(ConnectorColor, start = Offset(x1 - tickHalf, y2), end = Offset(x1 + tickHalf, y2), strokeWidth = 1.4f)

    // Label rotated -90° centred on the rail
    val lbl = textMeasurer.measure(AnnotatedString(label), StyleMuted)
    val midY = (y1 + y2) / 2f
    drawText(lbl, topLeft = Offset(x1 - lbl.size.width / 2f, midY - lbl.size.height / 2f))
}

// ---------------------------------------------------------------------------
// Top-down car silhouette — faithful to SVG path in ivi-god-view-warning-screen.svg
// ---------------------------------------------------------------------------

private fun DrawScope.drawCarTopDown(
    cx: Float, cy: Float,
    color: Color,
    label: String,
    scale: Float,
    filled: Boolean,
    textMeasurer: TextMeasurer,
    dashed: Boolean = false,
) {
    val halfW = 24f * scale   // half-width of car body
    val halfL = 50f * scale   // half-length of car body

    // Main body path (rounded-rect SVG path: M0,-50 c14,0 24,8 24,20 v60 …)
    val body = Path().apply {
        // Using cubicTo to approximate the SVG rounded-capsule shape
        moveTo(cx, cy - halfL)
        cubicTo(cx + halfW * 0.58f, cy - halfL, cx + halfW, cy - halfL + halfL * 0.40f, cx + halfW, cy - halfL + halfL * 0.60f)
        lineTo(cx + halfW, cy + halfL * 0.60f)
        cubicTo(cx + halfW, cy + halfL * 0.80f, cx + halfW * 0.67f, cy + halfL, cx, cy + halfL)
        cubicTo(cx - halfW * 0.67f, cy + halfL, cx - halfW, cy + halfL * 0.80f, cx - halfW, cy + halfL * 0.60f)
        lineTo(cx - halfW, cy - halfL + halfL * 0.60f)
        cubicTo(cx - halfW, cy - halfL + halfL * 0.40f, cx - halfW * 0.58f, cy - halfL, cx, cy - halfL)
        close()
    }

    if (filled) {
        // Ego: gradient body fill (light silver)
        drawPath(body, color = color.copy(alpha = 0.45f))
    } else {
        // Other vehicles: subtle fill
        drawPath(body, color = color.copy(alpha = 0.10f))
    }

    // Body outline
    val strokeEffect = if (dashed) GhostDash else null
    drawPath(body, color = color.copy(alpha = if (dashed) 0.8f else 0.7f), style = Stroke(
        width = if (filled) 1.6f else 1.4f,
        join = StrokeJoin.Round,
        pathEffect = strokeEffect,
    ))

    // --- Windshield (front glass — upper quarter of car) ---
    val wsTopY = cy - halfL * 0.55f
    val wsBotY = cy - halfL * 0.10f
    val wsHW   = halfW * 0.65f
    val wsFill = Path().apply {
        moveTo(cx - wsHW * 0.7f, wsTopY)
        lineTo(cx + wsHW * 0.7f, wsTopY)
        lineTo(cx + wsHW, wsBotY)
        lineTo(cx - wsHW, wsBotY)
        close()
    }
    drawPath(wsFill, color = Color(0xFF141C26).copy(alpha = if (filled) 0.8f else 0.6f))
    drawPath(wsFill, color = color.copy(alpha = 0.35f), style = Stroke(width = 1.0f))

    // --- Rear glass ---
    val rwTopY = cy + halfL * 0.10f
    val rwBotY = cy + halfL * 0.55f
    val rwFill = Path().apply {
        moveTo(cx - wsHW, rwTopY)
        lineTo(cx + wsHW, rwTopY)
        lineTo(cx + wsHW * 0.7f, rwBotY)
        lineTo(cx - wsHW * 0.7f, rwBotY)
        close()
    }
    drawPath(rwFill, color = Color(0xFF141C26).copy(alpha = if (filled) 0.75f else 0.55f))
    drawPath(rwFill, color = color.copy(alpha = 0.25f), style = Stroke(width = 0.8f))

    // --- Front wheels (top of car in top-down view) ---
    val wheelR = 5f * scale
    val wheelOffX = halfW + wheelR * 0.2f
    val wheelOffY = cy - halfL * 0.35f
    drawOval(color = Color(0xFF0F131A), topLeft = Offset(cx - wheelOffX - wheelR, wheelOffY - wheelR * 1.8f), size = Size(wheelR * 2f, wheelR * 3.6f))
    drawOval(color = color.copy(alpha = 0.7f), topLeft = Offset(cx - wheelOffX - wheelR, wheelOffY - wheelR * 1.8f), size = Size(wheelR * 2f, wheelR * 3.6f), style = Stroke(width = 1.0f))
    drawOval(color = Color(0xFF0F131A), topLeft = Offset(cx + wheelOffX - wheelR, wheelOffY - wheelR * 1.8f), size = Size(wheelR * 2f, wheelR * 3.6f))
    drawOval(color = color.copy(alpha = 0.7f), topLeft = Offset(cx + wheelOffX - wheelR, wheelOffY - wheelR * 1.8f), size = Size(wheelR * 2f, wheelR * 3.6f), style = Stroke(width = 1.0f))

    // --- Rear wheels (bottom of car) ---
    val rWheelOffY = cy + halfL * 0.35f
    drawOval(color = Color(0xFF0F131A), topLeft = Offset(cx - wheelOffX - wheelR, rWheelOffY - wheelR * 1.8f), size = Size(wheelR * 2f, wheelR * 3.6f))
    drawOval(color = color.copy(alpha = 0.7f), topLeft = Offset(cx - wheelOffX - wheelR, rWheelOffY - wheelR * 1.8f), size = Size(wheelR * 2f, wheelR * 3.6f), style = Stroke(width = 1.0f))
    drawOval(color = Color(0xFF0F131A), topLeft = Offset(cx + wheelOffX - wheelR, rWheelOffY - wheelR * 1.8f), size = Size(wheelR * 2f, wheelR * 3.6f))
    drawOval(color = color.copy(alpha = 0.7f), topLeft = Offset(cx + wheelOffX - wheelR, rWheelOffY - wheelR * 1.8f), size = Size(wheelR * 2f, wheelR * 3.6f), style = Stroke(width = 1.0f))

    // --- Front headlights (top edge) ---
    val hlW = 12f * scale
    val hlH = 5f * scale
    val hlY = cy - halfL
    drawOval(color = if (filled) Color(0xFFFFFFCC).copy(alpha = 0.92f) else color.copy(alpha = 0.55f), topLeft = Offset(cx - wsHW * 0.7f - hlW / 2f, hlY - hlH / 2f), size = Size(hlW, hlH))
    drawOval(color = if (filled) Color(0xFFFFFFCC).copy(alpha = 0.92f) else color.copy(alpha = 0.55f), topLeft = Offset(cx + wsHW * 0.7f - hlW / 2f, hlY - hlH / 2f), size = Size(hlW, hlH))

    // --- Rear tail lights (bottom edge) ---
    val tlY = cy + halfL
    drawOval(color = Color(0xFFFF3344).copy(alpha = if (filled) 0.9f else 0.6f), topLeft = Offset(cx - wsHW * 0.7f - hlW / 2f, tlY - hlH / 2f), size = Size(hlW, hlH))
    drawOval(color = Color(0xFFFF3344).copy(alpha = if (filled) 0.9f else 0.6f), topLeft = Offset(cx + wsHW * 0.7f - hlW / 2f, tlY - hlH / 2f), size = Size(hlW, hlH))

    // --- Label below car ---
    val lbl = textMeasurer.measure(AnnotatedString(label), StyleMuted.copy(color = color))
    drawText(lbl, topLeft = Offset(cx - lbl.size.width / 2f, cy + halfL + 8f))
}

// ---------------------------------------------------------------------------
// Ghost C vehicle — dashed red silhouette with glow ring
// ---------------------------------------------------------------------------

private fun DrawScope.drawGhostC(
    cx: Float, cy: Float,
    glowAlpha: Float,
    riskColor: Color,
    textMeasurer: TextMeasurer,
) {
    val scale = 0.86f
    val rx = 50f * scale
    val ry = 74f * scale

    // Pulsing glow ring
    drawOval(
        color = riskColor.copy(alpha = glowAlpha * 0.5f),
        topLeft = Offset(cx - rx, cy - ry),
        size = Size(rx * 2f, ry * 2f),
        style = Stroke(width = 9f),
    )

    // Draw ghost car with dashed outline
    drawCarTopDown(
        cx = cx, cy = cy,
        color = GhostCColor,
        label = "OCCLUDED VEHICLE C",
        scale = scale,
        filled = false,
        textMeasurer = textMeasurer,
        dashed = true,
    )
}

// ---------------------------------------------------------------------------
// Ghost C V2X info badge
// ---------------------------------------------------------------------------

private fun DrawScope.drawGhostBadge(
    cx: Float, cy: Float,
    dist: String, riskState: String,
    textMeasurer: TextMeasurer,
) {
    val text = "[V2X] C · $dist m · RISK: ${riskState.uppercase(Locale.US)}"
    val lbl = textMeasurer.measure(AnnotatedString(text), StyleBadge)
    val padH = 10f; val padV = 5f
    val bW = lbl.size.width + padH * 2f
    val bH = lbl.size.height + padV * 2f
    val scale = 0.86f
    val bX = cx - bW / 2f
    val bY = cy - 74f * scale - bH - 10f

    drawRect(color = Color(0xFF1A1A2E).copy(alpha = 0.90f), topLeft = Offset(bX, bY), size = Size(bW, bH))
    drawRect(color = GhostCColor.copy(alpha = 0.55f), topLeft = Offset(bX, bY), size = Size(bW, bH), style = Stroke(width = 1.0f))
    drawText(lbl, topLeft = Offset(bX + padH, bY + padV))
}

// ---------------------------------------------------------------------------
// Header — top-left corner
// ---------------------------------------------------------------------------

private fun DrawScope.drawHeader(hasGhostC: Boolean, textMeasurer: TextMeasurer) {
    val titleLbl = textMeasurer.measure(AnnotatedString("NLOS GOD VIEW"), StyleHeader)
    drawText(titleLbl, topLeft = Offset(18f, 14f))

    if (hasGhostC) {
        val alertLbl = textMeasurer.measure(AnnotatedString("▲  NLOS THREAT DETECTED"), StyleAlert)
        drawText(alertLbl, topLeft = Offset(18f, 14f + titleLbl.size.height + 4f))
    }
}

// ---------------------------------------------------------------------------
// Side panel — legend + risk colour key (right 40%)
// ---------------------------------------------------------------------------

private fun DrawScope.drawSidePanel(
    x0: Float, w: Float, h: Float, barH: Float,
    riskState: String,
    textMeasurer: TextMeasurer,
) {
    // Header
    val hdrLbl = textMeasurer.measure(AnnotatedString("GOD VIEW · 3 VEHICLES"), StyleDim)
    drawText(hdrLbl, topLeft = Offset(x0, 14f))

    // --- EGO legend ---
    val legendX = x0
    var legendY = 70f

    // Ego mini car swatch
    drawCarMiniSwatch(x = legendX, y = legendY, color = EgoColor, filled = true)
    val egoMainLbl = textMeasurer.measure(AnnotatedString("EGO (A)"), StyleMain)
    val egoSubLbl = textMeasurer.measure(AnnotatedString("this vehicle · own sensors"), StyleMuted)
    drawText(egoMainLbl, topLeft = Offset(legendX + 36f, legendY - 4f))
    drawText(egoSubLbl, topLeft = Offset(legendX + 36f, legendY - 4f + egoMainLbl.size.height + 2f))

    // --- B legend ---
    legendY += 80f
    drawCarMiniSwatch(x = legendX, y = legendY, color = BColor, filled = false)
    val bMainLbl = textMeasurer.measure(AnnotatedString("B — OCCLUDER"), StyleMain)
    val bSubLbl = textMeasurer.measure(AnnotatedString("detected directly by A"), StyleMuted)
    drawText(bMainLbl, topLeft = Offset(legendX + 36f, legendY - 4f))
    drawText(bSubLbl, topLeft = Offset(legendX + 36f, legendY - 4f + bMainLbl.size.height + 2f))

    // --- C legend ---
    legendY += 80f
    drawCarMiniSwatch(x = legendX, y = legendY, color = GhostCColor, filled = false, dashed = true)
    val cMainLbl = textMeasurer.measure(AnnotatedString("C — OCCLUDED VEHICLE"), StyleMain.copy(color = GhostCColor.copy(alpha = 0.85f)))
    val cSub1Lbl = textMeasurer.measure(AnnotatedString("source: v2x_relayed"), StyleMuted)
    val cSub2Lbl = textMeasurer.measure(AnnotatedString("never seen by A's sensors"), StyleMuted)
    drawText(cMainLbl, topLeft = Offset(legendX + 36f, legendY - 4f))
    drawText(cSub1Lbl, topLeft = Offset(legendX + 36f, legendY - 4f + cMainLbl.size.height + 2f))
    drawText(cSub2Lbl, topLeft = Offset(legendX + 36f, legendY - 4f + cMainLbl.size.height + 2f + cSub1Lbl.size.height + 1f))

    // --- Blind zone legend ---
    legendY += 90f
    drawRect(
        color = BlindZoneColor.copy(alpha = 0.08f),
        topLeft = Offset(legendX, legendY - 14f), size = Size(26f, 44f),
    )
    drawRect(
        color = BlindZoneColor.copy(alpha = 0.3f),
        topLeft = Offset(legendX, legendY - 14f), size = Size(26f, 44f),
        style = Stroke(width = 1.0f, pathEffect = GhostDash),
    )
    val bzMainLbl = textMeasurer.measure(AnnotatedString("BLIND ZONE"), StyleMain)
    val bzSubLbl = textMeasurer.measure(AnnotatedString("A's line of sight, blocked by B"), StyleMuted)
    drawText(bzMainLbl, topLeft = Offset(legendX + 36f, legendY - 4f))
    drawText(bzSubLbl, topLeft = Offset(legendX + 36f, legendY - 4f + bzMainLbl.size.height + 2f))

    // Divider
    legendY += 90f
    drawLine(PanelDiv, start = Offset(x0, legendY), end = Offset(w - 10f, legendY), strokeWidth = 1.5f)

    // --- Risk colour key ---
    legendY += 20f
    val riskHdrLbl = textMeasurer.measure(AnnotatedString("RISK — OCCLUDED C GLOW COLOUR"), StyleDim)
    drawText(riskHdrLbl, topLeft = Offset(x0, legendY))
    legendY += riskHdrLbl.size.height + 12f

    // Low / Medium / High circles
    val risks = listOf(Triple(RiskLow, "LOW", false), Triple(RiskMedium, "MEDIUM", false), Triple(RiskHigh, "HIGH", true))
    for ((i, item) in risks.withIndex()) {
        val (col, name, active) = item
        val cx2 = x0 + 9f + i * 100f
        drawCircle(color = col.copy(alpha = if (active || name.lowercase() == riskState.lowercase()) 1.0f else 0.6f), radius = 7f, center = Offset(cx2, legendY + 7f))
        val rLbl = textMeasurer.measure(AnnotatedString(name), StyleMuted.copy(color = if (name.lowercase() == riskState.lowercase()) TextMain else TextMuted))
        drawText(rLbl, topLeft = Offset(cx2 + 14f, legendY))
    }

    // Footer note
    val note1Lbl = textMeasurer.measure(AnnotatedString("Drawn from R4 warning messages only —"), StyleDim.copy(letterSpacing = 0.sp, fontSize = 10.sp))
    val note2Lbl = textMeasurer.measure(AnnotatedString("no ego detection of C exists at any point."), StyleDim.copy(letterSpacing = 0.sp, fontSize = 10.sp))
    drawText(note1Lbl, topLeft = Offset(x0, h - barH - note1Lbl.size.height - note2Lbl.size.height - 10f))
    drawText(note2Lbl, topLeft = Offset(x0, h - barH - note2Lbl.size.height - 6f))
}

// Mini car swatch for legend (small rounded rect)
private fun DrawScope.drawCarMiniSwatch(x: Float, y: Float, color: Color, filled: Boolean, dashed: Boolean = false) {
    val rect = Path().apply {
        addOval(androidx.compose.ui.geometry.Rect(x, y - 14f, x + 26f, y + 30f))
    }
    if (filled) drawPath(rect, color = color.copy(alpha = 0.75f))
    else drawPath(rect, color = color.copy(alpha = 0.15f))
    drawPath(rect, color = color.copy(alpha = 0.7f), style = Stroke(width = 1.5f, pathEffect = if (dashed) GhostDash else null))
}

// ---------------------------------------------------------------------------
// Status bar — bottom strip
// ---------------------------------------------------------------------------

private fun DrawScope.drawStatusBar(w: Float, h: Float, barH: Float, riskState: String, textMeasurer: TextMeasurer) {
    drawRect(color = Color(0xFF0D1018), topLeft = Offset(0f, h - barH), size = Size(w, barH))
    drawLine(PanelDiv, start = Offset(0f, h - barH), end = Offset(w, h - barH), strokeWidth = 0.8f)

    val risk = riskState.uppercase(Locale.US)
    val riskCol = cyberRiskColor(riskState)
    val statusLbl = textMeasurer.measure(AnnotatedString("● MODE: WARNING   ●  V2X LINK: BOUND · 47300   |   RISK: $risk"), StyleMuted.copy(color = riskCol))
    drawText(statusLbl, topLeft = Offset((w - statusLbl.size.width) / 2f, h - barH + (barH - statusLbl.size.height) / 2f))

    val iviLbl = textMeasurer.measure(AnnotatedString("IVI · R16"), StyleDim.copy(fontSize = 10.sp))
    drawText(iviLbl, topLeft = Offset(w - iviLbl.size.width - 12f, h - barH + (barH - iviLbl.size.height) / 2f))
}

