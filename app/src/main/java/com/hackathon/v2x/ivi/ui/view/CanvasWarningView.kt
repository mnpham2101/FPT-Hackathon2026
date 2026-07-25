package com.hackathon.v2x.ivi.ui.view

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
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
import androidx.compose.ui.unit.sp
import com.hackathon.v2x.ivi.model.SceneGeometry
import com.hackathon.v2x.ivi.model.VehiclePosition
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

/**
 * Committed M1 renderer for the R17 Warning View: a top-down 2D "God View"
 * of Ego (A), the occluder (B), and Ghost C, drawn purely with Compose
 * Canvas calls — no bitmaps or external assets. Pixel geometry comes from
 * [SceneCoordinateMapper.mapScene].
 *
 * `riskState` is accepted per the seam contract; it drives the Ghost C glow
 * added in 17.5.3.4.
 */
class CanvasWarningView : IviWarningViewSeam {

    @Composable
    override fun Render(scene: SceneGeometry, riskState: String) {
        val textMeasurer = rememberTextMeasurer()

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
            if (ghostC != null) {
                drawGhostVehicle(ghostC, textMeasurer)
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

@Preview(name = "God View — Ego, B, Ghost C", widthDp = 420, heightDp = 560, showBackground = true)
@Composable
private fun CanvasWarningViewPreview() {
    CanvasWarningView().Render(
        scene = SceneGeometry(
            ego = VehiclePosition(0f, 0f),
            vehicleB = VehiclePosition(20f, 0f),
            vehicleC = VehiclePosition(35f, 0f),
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
