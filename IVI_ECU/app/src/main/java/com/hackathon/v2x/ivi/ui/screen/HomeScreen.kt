package com.hackathon.v2x.ivi.ui.screen

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.hackathon.v2x.ivi.BuildConfig
import kotlinx.coroutines.delay
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

// ---------------------------------------------------------------------------
// Design Tokens — Dark Automotive Theme
// ---------------------------------------------------------------------------

private val BackgroundColor = Color(0xFF1A1A2E)
private val PanelColor = Color(0xFF14142A)
private val BorderColor = Color(0xFF2A2A44)
private val AccentCyan = Color(0xFF00D4FF)
private val TextPrimary = Color(0xFFE8E8F0)
private val TextSecondary = Color(0xFF9090B0)

private val StatusGreen = Color(0xFF00E676)
private val StatusAmber = Color(0xFFFFB300)
private val StatusRed = Color(0xFFFF5252)

private val UiFont = FontFamily.Default
private val TechFont = FontFamily.Monospace

/**
 * Automotive Idle Dashboard (`DisplayMode.HomeView`).
 *
 * Provides a clean driver interface while no V2X safety warnings are active:
 * - Real-time digital clock (updates every second)
 * - V2X Listener link status (Port & protocol info)
 * - Vehicle telemetry mockup (Speed, Gear, Heading)
 * - System status diagnostics
 */
@Composable
fun HomeScreen(
    modifier: Modifier = Modifier,
    v2xStatus: String = "ACTIVE",
) {
    var currentTime by remember { mutableStateOf(getFormattedTime()) }

    LaunchedEffect(Unit) {
        while (true) {
            delay(1000L)
            currentTime = getFormattedTime()
        }
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(BackgroundColor)
            .padding(24.dp),
        verticalArrangement = Arrangement.SpaceBetween,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        // Top Bar: Clock + V2X Link Badge
        TopHeaderBar(currentTime = currentTime, v2xStatus = v2xStatus)

        // Center Panel: Vehicle Instrument & V2X Diagnostics
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f)
                .padding(vertical = 16.dp),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            InstrumentClusterCard()
            Spacer(modifier = Modifier.height(16.dp))
            V2xServiceStatusCard()
        }

        // Bottom Footer: System Branding
        Text(
            text = "ANDROID AUTOMOTIVE OS · IVI HEAD UNIT · R16/R17",
            color = TextSecondary,
            fontFamily = TechFont,
            fontSize = 11.sp,
        )
    }
}

@Composable
private fun TopHeaderBar(currentTime: String, v2xStatus: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(PanelColor)
            .border(1.dp, BorderColor, RoundedCornerShape(12.dp))
            .padding(horizontal = 20.dp, vertical = 12.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                text = "🕐 ",
                fontSize = 16.sp,
            )
            Text(
                text = currentTime,
                color = TextPrimary,
                fontFamily = TechFont,
                fontWeight = FontWeight.Bold,
                fontSize = 18.sp,
            )
        }

        V2xLinkBadge(status = v2xStatus)
    }
}

@Composable
private fun V2xLinkBadge(status: String) {
    val (statusColor, labelText) = when (status.uppercase()) {
        "ACTIVE" -> StatusGreen to "V2X LINK: ACTIVE"
        "ERROR" -> StatusRed to "V2X LINK: ERROR"
        else -> StatusAmber to "V2X LINK: STANDBY"
    }

    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Box(
            modifier = Modifier
                .size(10.dp)
                .clip(CircleShape)
                .background(statusColor)
        )
        Text(
            text = labelText,
            color = statusColor,
            fontFamily = TechFont,
            fontWeight = FontWeight.SemiBold,
            fontSize = 13.sp,
        )
    }
}

@Composable
private fun InstrumentClusterCard() {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(16.dp))
            .background(PanelColor)
            .border(1.dp, BorderColor, RoundedCornerShape(16.dp))
            .padding(24.dp),
        contentAlignment = Alignment.Center,
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceAround,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            MetricGauge(label = "SPEED", value = "0", unit = "km/h", accentColor = AccentCyan)
            Box(
                modifier = Modifier
                    .width(1.dp)
                    .height(60.dp)
                    .background(BorderColor)
            )
            MetricGauge(label = "TRANSMISSION", value = "P", unit = "PARK", accentColor = TextPrimary)
            Box(
                modifier = Modifier
                    .width(1.dp)
                    .height(60.dp)
                    .background(BorderColor)
            )
            MetricGauge(label = "HEADING", value = "N", unit = "NORTH", accentColor = AccentCyan)
        }
    }
}

@Composable
private fun MetricGauge(
    label: String,
    value: String,
    unit: String,
    accentColor: Color,
) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(
            text = label,
            color = TextSecondary,
            fontFamily = UiFont,
            fontSize = 11.sp,
            fontWeight = FontWeight.Medium,
        )
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            text = value,
            color = accentColor,
            fontFamily = TechFont,
            fontSize = 32.sp,
            fontWeight = FontWeight.Bold,
        )
        Text(
            text = unit,
            color = TextSecondary,
            fontFamily = TechFont,
            fontSize = 11.sp,
        )
    }
}

@Composable
private fun V2xServiceStatusCard() {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(16.dp))
            .background(PanelColor)
            .border(1.dp, BorderColor, RoundedCornerShape(16.dp))
            .padding(20.dp),
    ) {
        Text(
            text = "SYSTEM DIAGNOSTICS",
            color = AccentCyan,
            fontFamily = TechFont,
            fontWeight = FontWeight.Bold,
            fontSize = 12.sp,
        )
        Spacer(modifier = Modifier.height(12.dp))

        DiagnosticRow(label = "R4 Listener Port", value = "${BuildConfig.R4_UDP_PORT} / UDP")
        DiagnosticRow(label = "Contract Protocol", value = "r4-ada-ivi.schema.json (v1)")
        DiagnosticRow(label = "Display Area Mode", value = "HOME (Idle — Awaiting Warning)")
        DiagnosticRow(label = "Safety Seam", value = "CanvasWarningView 2D God View")
    }
}

@Composable
private fun DiagnosticRow(label: String, value: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(
            text = label,
            color = TextSecondary,
            fontFamily = UiFont,
            fontSize = 13.sp,
        )
        Text(
            text = value,
            color = TextPrimary,
            fontFamily = TechFont,
            fontSize = 13.sp,
        )
    }
}

private fun getFormattedTime(): String =
    SimpleDateFormat("HH:mm:ss", Locale.US).format(Date())
