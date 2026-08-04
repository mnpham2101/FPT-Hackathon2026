package com.hackathon.v2x.ivi.ui.screen

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Build
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import com.hackathon.v2x.ivi.model.R3Snapshot
import com.hackathon.v2x.ivi.model.R3Timestamps
import com.hackathon.v2x.ivi.model.SceneGeometry
import com.hackathon.v2x.ivi.model.VehiclePosition
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.hackathon.v2x.ivi.BuildConfig
import com.hackathon.v2x.ivi.ui.DisplayMode
import com.hackathon.v2x.ivi.ui.MainViewModel
import com.hackathon.v2x.ivi.ui.WarningUiState
import com.hackathon.v2x.ivi.ui.WarningViewModel
import com.hackathon.v2x.ivi.ui.view.IviWarningViewSeam

// ---------------------------------------------------------------------------
// R16 design tokens — dark automotive scheme. All dimensions are Dp tokens.
// ---------------------------------------------------------------------------

private val BackgroundColor = Color(0xFF1A1A2E)
private val AccentColor = Color(0xFF00D4FF)
private val TextColor = Color(0xFFE8E8F0)
private val PanelColor = Color(0xFF14142A)
private val PanelBorderColor = Color(0xFF2A2A44)

private val UiFont = FontFamily.Default
private val TechFont = FontFamily.Monospace

private val MinTouchTarget = 48.dp
private val SideButtonSize = 72.dp
private val SideBarPadding = 12.dp
private val SideButtonSpacing = 16.dp
private val SideButtonCorner = 12.dp
private val SideButtonIconSize = 28.dp
private val DisplayAreaPadding = 12.dp
private val DisplayAreaCorner = 16.dp
private val DisplayBorderWidth = 1.dp
private val BottomBarHeight = 56.dp
private val BottomBarPadding = 16.dp
private val StatusDotSize = 10.dp
private val StatusSpacing = 24.dp

private const val DISPLAY_AREA_WIDTH_FRACTION = 0.7f
private const val SIDE_BAR_WIDTH_FRACTION = (1f - DISPLAY_AREA_WIDTH_FRACTION) / 2f
private val WideLayoutMinWidth = 1024.dp
private const val MODE_TRANSITION_DURATION_MS = 200

private data class SideBarItem(val icon: ImageVector, val label: String, val mode: DisplayMode)

private val LeftBarItems = listOf(
    SideBarItem(Icons.Filled.Home, "Home", DisplayMode.HomeView),
    SideBarItem(Icons.Filled.Menu, "Apps", DisplayMode.AppsView),
    SideBarItem(Icons.Filled.Build, "3D HUD", DisplayMode.WarningView),
)

private val RightBarItems = listOf(
    SideBarItem(Icons.Filled.Settings, "Settings", DisplayMode.SettingsView),
)

private val DisplayMode.statusLabel: String
    get() = when (this) {
        DisplayMode.WarningView -> "WARNING"
        DisplayMode.HomeView -> "HOME"
        DisplayMode.AppsView -> "APPS"
        DisplayMode.SettingsView -> "SETTINGS"
    }

/**
 * R16 main HMI — default [DisplayMode.HomeView]; wake-on-warning → God View
 * via [IviWarningViewSeam] (no banner overlay; Lead `17.5.5.6`).
 */
@Composable
fun MainScreen(
    mainViewModel: MainViewModel,
    warningViewModel: WarningViewModel,
    warningViewSeam: IviWarningViewSeam,
    modifier: Modifier = Modifier,
) {
    val currentMode by mainViewModel.currentMode.collectAsStateWithLifecycle()
    val uiWarningState by warningViewModel.uiWarningState.collectAsStateWithLifecycle()
    val latestScene by warningViewModel.latestScene.collectAsStateWithLifecycle()
    MainScreenContent(
        currentMode = currentMode,
        onModeSelected = mainViewModel::setMode,
        uiWarningState = uiWarningState,
        latestScene = latestScene,
        warningViewSeam = warningViewSeam,
        modifier = modifier,
    )
}

@Composable
fun MainScreenContent(
    currentMode: DisplayMode,
    onModeSelected: (DisplayMode) -> Unit,
    modifier: Modifier = Modifier,
    uiWarningState: WarningUiState = WarningUiState.Idle,
    latestScene: SceneGeometry? = null,
    warningViewSeam: IviWarningViewSeam? = null,
    linkStatusLabel: String = "BOUND :${BuildConfig.R4_UDP_PORT}",
) {
    BoxWithConstraints(
        modifier = modifier
            .fillMaxSize()
            .background(BackgroundColor)
    ) {
        val showButtonLabels = maxWidth >= WideLayoutMinWidth

        Column(modifier = Modifier.fillMaxSize()) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
            ) {
                SideButtonBar(
                    items = LeftBarItems,
                    showLabels = showButtonLabels,
                    onModeSelected = onModeSelected,
                    modifier = Modifier
                        .fillMaxHeight()
                        .weight(SIDE_BAR_WIDTH_FRACTION)
                )
                DisplayArea(
                    modifier = Modifier
                        .fillMaxHeight()
                        .weight(DISPLAY_AREA_WIDTH_FRACTION),
                ) {
                    DisplayModeSwitcher(
                        currentMode = currentMode,
                        uiWarningState = uiWarningState,
                        latestScene = latestScene,
                        warningViewSeam = warningViewSeam,
                    )
                }
                SideButtonBar(
                    items = RightBarItems,
                    showLabels = showButtonLabels,
                    onModeSelected = onModeSelected,
                    modifier = Modifier
                        .fillMaxHeight()
                        .weight(SIDE_BAR_WIDTH_FRACTION)
                )
            }
            BottomNavBar(
                currentMode = currentMode,
                linkStatusLabel = linkStatusLabel,
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}

@Composable
private fun DisplayArea(
    modifier: Modifier = Modifier,
    fillColor: Color = PanelColor,
    content: @Composable () -> Unit,
) {
    Box(
        modifier = modifier
            .padding(DisplayAreaPadding)
            .clip(RoundedCornerShape(DisplayAreaCorner))
            .background(fillColor)
            .border(DisplayBorderWidth, PanelBorderColor, RoundedCornerShape(DisplayAreaCorner)),
        contentAlignment = Alignment.Center,
    ) {
        content()
    }
}

@Composable
private fun DisplayModeSwitcher(
    currentMode: DisplayMode,
    uiWarningState: WarningUiState,
    latestScene: SceneGeometry?,
    warningViewSeam: IviWarningViewSeam?,
    modifier: Modifier = Modifier,
) {
    AnimatedContent(
        targetState = currentMode,
        transitionSpec = {
            fadeIn(tween(MODE_TRANSITION_DURATION_MS)) togetherWith
                fadeOut(tween(MODE_TRANSITION_DURATION_MS))
        },
        label = "displayModeTransition",
        modifier = modifier,
    ) { mode ->
        Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            when (mode) {
                DisplayMode.WarningView -> WarningViewContent(
                    uiWarningState = uiWarningState,
                    latestScene = latestScene,
                    warningViewSeam = warningViewSeam,
                )
                DisplayMode.HomeView -> HomeScreen()
                DisplayMode.AppsView -> ViewPlaceholder("Apps View Placeholder")
                DisplayMode.SettingsView -> ViewPlaceholder("Settings View Placeholder")
            }
        }
    }
}

@Composable
private fun WarningViewContent(
    uiWarningState: WarningUiState,
    latestScene: SceneGeometry?,
    warningViewSeam: IviWarningViewSeam?,
) {
    var is3DMode by remember { mutableStateOf(true) }

    val defaultMockScene = SceneGeometry(
        ego = VehiclePosition(0f, 0f),
        vehicleB = VehiclePosition(-3.5f, 15.0f),
        vehicleC = VehiclePosition(2.5f, 22.0f),
        vehicleCSnapshot = R3Snapshot(
            id = "V-C-GHOST",
            objectClass = "vehicle",
            source = "v2x_relayed",
            position = VehiclePosition(2.5f, 22.0f),
            distance = 22.1f,
            speed = 18.5f,
            confidence = 0.88f,
            state = "tracked",
            timestamps = R3Timestamps(1L, 2L, 2L)
        )
    )

    val active = uiWarningState as? WarningUiState.Active
    val scene = when {
        active != null -> latestScene?.copy(vehicleCSnapshot = active.event.objectSnapshot) ?: defaultMockScene
        latestScene != null -> latestScene
        else -> defaultMockScene
    }

    Box(modifier = Modifier.fillMaxSize()) {
        val riskState = active?.event?.riskState ?: "high"
        if (is3DMode) {
            val view3D = androidx.compose.runtime.remember { com.hackathon.v2x.ivi.ui.view.Canvas3DWarningView() }
            view3D.Render(scene = scene, riskState = riskState)
        } else {
            val view2D = androidx.compose.runtime.remember { com.hackathon.v2x.ivi.ui.view.CanvasWarningView() }
            view2D.Render(scene = scene, riskState = riskState)
        }


        // Top Floating Warning Banner Overlay
        com.hackathon.v2x.ivi.ui.view.WarningBannerOverlay(
            warningActive = active != null || scene == defaultMockScene,
            riskState = riskState,
            modifier = Modifier.align(Alignment.TopCenter).padding(top = 8.dp),
        )

        // HUD 2D/3D Mode Switcher Toggle Pill
        Box(
            modifier = Modifier
                .align(Alignment.TopEnd)
                .padding(16.dp)
                .clip(RoundedCornerShape(20.dp))
                .background(Color(0xFF141829).copy(alpha = 0.85f))
                .border(1.dp, Color(0xFF00E5FF).copy(alpha = 0.6f), RoundedCornerShape(20.dp))
                .clickable { is3DMode = !is3DMode }
                .padding(horizontal = 14.dp, vertical = 8.dp),
        ) {
            Text(
                text = if (is3DMode) "MODE: 3D HUD (TAP FOR 2D)" else "MODE: 2D HUD (TAP FOR 3D)",
                color = Color(0xFF00E5FF),
                fontFamily = FontFamily.Monospace,
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
            )
        }
    }
}

@Composable
private fun ViewPlaceholder(label: String) {
    Text(
        text = label,
        color = TextColor,
        fontFamily = UiFont,
        fontWeight = FontWeight.SemiBold,
        fontSize = 24.sp,
    )
}

@Composable
private fun WarningViewPlaceholder() {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(
            text = "WARNING VIEW",
            color = AccentColor,
            fontFamily = UiFont,
            fontWeight = FontWeight.SemiBold,
            fontSize = 24.sp,
        )
        Spacer(modifier = Modifier.height(8.dp))
        Text(
            text = "d_AB = --.- m   d_AC = --.- m",
            color = AccentColor,
            fontFamily = TechFont,
            fontSize = 14.sp,
        )
    }
}

@Composable
private fun SideButtonBar(
    items: List<SideBarItem>,
    showLabels: Boolean,
    onModeSelected: (DisplayMode) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier.padding(SideBarPadding),
        verticalArrangement = Arrangement.spacedBy(SideButtonSpacing, Alignment.CenterVertically),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        items.forEach { item ->
            SideBarButton(
                item = item,
                showLabel = showLabels,
                onClick = { onModeSelected(item.mode) },
            )
        }
    }
}

@Composable
private fun SideBarButton(
    item: SideBarItem,
    showLabel: Boolean,
    onClick: () -> Unit,
) {
    Column(
        modifier = Modifier
            .defaultMinSize(minWidth = MinTouchTarget, minHeight = MinTouchTarget)
            .size(SideButtonSize)
            .clip(RoundedCornerShape(SideButtonCorner))
            .background(PanelColor)
            .border(DisplayBorderWidth, PanelBorderColor, RoundedCornerShape(SideButtonCorner))
            .clickable(onClick = onClick),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Icon(
            imageVector = item.icon,
            contentDescription = item.label,
            tint = AccentColor,
            modifier = Modifier.size(SideButtonIconSize),
        )
        if (showLabel) {
            Spacer(modifier = Modifier.height(4.dp))
            Text(
                text = item.label,
                color = TextColor,
                fontFamily = UiFont,
                fontSize = 12.sp,
            )
        }
    }
}

@Composable
private fun BottomNavBar(
    currentMode: DisplayMode,
    linkStatusLabel: String,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier
            .height(BottomBarHeight)
            .background(PanelColor)
            .padding(horizontal = BottomBarPadding),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(StatusSpacing),
    ) {
        StatusIndicator(label = "MODE: ${currentMode.statusLabel}", dotColor = AccentColor)
        StatusIndicator(
            label = "V2X LINK: $linkStatusLabel",
            dotColor = if (currentMode == DisplayMode.WarningView) AccentColor else AccentColor,
        )
        Spacer(modifier = Modifier.weight(1f))
        Text(
            text = "IVI · R16",
            color = TextColor,
            fontFamily = TechFont,
            fontSize = 12.sp,
        )
    }
}

@Composable
private fun StatusIndicator(label: String, dotColor: Color, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier,
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Box(
            modifier = Modifier
                .size(StatusDotSize)
                .clip(CircleShape)
                .background(dotColor)
        )
        Text(
            text = label,
            color = TextColor,
            fontFamily = TechFont,
            fontSize = 12.sp,
        )
    }
}

@Preview(name = "AAOS 1280x720 — Home", widthDp = 1280, heightDp = 720, showBackground = true)
@Composable
private fun MainScreenPreview() {
    MainScreenContent(
        currentMode = DisplayMode.HomeView,
        onModeSelected = {},
        linkStatusLabel = "BOUND :47300",
    )
}

@Preview(name = "AAOS 1280x720 — Warning", widthDp = 1280, heightDp = 720, showBackground = true)
@Composable
private fun MainScreenWarningPreview() {
    MainScreenContent(
        currentMode = DisplayMode.WarningView,
        onModeSelected = {},
        linkStatusLabel = "BOUND :47300",
    )
}
