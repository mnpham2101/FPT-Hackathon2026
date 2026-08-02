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
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
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
import com.hackathon.v2x.ivi.model.SceneGeometry
import com.hackathon.v2x.ivi.ui.DisplayMode
import com.hackathon.v2x.ivi.ui.MainViewModel
import com.hackathon.v2x.ivi.ui.WarningUiState
import com.hackathon.v2x.ivi.ui.WarningViewModel
import com.hackathon.v2x.ivi.ui.view.IviWarningViewSeam

// ---------------------------------------------------------------------------
// R16 design tokens — dark automotive scheme. All dimensions are Dp tokens.
// ---------------------------------------------------------------------------

private val BackgroundColor = Color(0xFF1A1A2E)
/** Standby slate — Lead default black listening surface. */
private val StandbyBlack = Color(0xFF0D0D1A)
private val AccentColor = Color(0xFF00D4FF)
private val TextColor = Color(0xFFE8E8F0)
private val StandbyHintColor = Color(0xFF8A8A9A)
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
)

private val RightBarItems = listOf(
    SideBarItem(Icons.Filled.Settings, "Settings", DisplayMode.SettingsView),
)

private val DisplayMode.statusLabel: String
    get() = when (this) {
        DisplayMode.StandbyView -> "STANDBY"
        DisplayMode.WarningView -> "WARNING"
        DisplayMode.HomeView -> "HOME"
        DisplayMode.AppsView -> "APPS"
        DisplayMode.SettingsView -> "SETTINGS"
    }

/**
 * R16 main HMI — default [DisplayMode.StandbyView]; wake-on-warning → God View
 * via [IviWarningViewSeam] (no banner overlay).
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
                    fillColor = if (currentMode == DisplayMode.StandbyView) {
                        StandbyBlack
                    } else {
                        PanelColor
                    },
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
            BottomNavBar(currentMode = currentMode, modifier = Modifier.fillMaxWidth())
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
                DisplayMode.StandbyView -> StandbyViewContent()
                DisplayMode.WarningView -> WarningViewContent(
                    uiWarningState = uiWarningState,
                    latestScene = latestScene,
                    warningViewSeam = warningViewSeam,
                )
                DisplayMode.HomeView -> ViewPlaceholder("Home View Placeholder")
                DisplayMode.AppsView -> ViewPlaceholder("Apps View Placeholder")
                DisplayMode.SettingsView -> ViewPlaceholder("Settings View Placeholder")
            }
        }
    }
}

@Composable
private fun StandbyViewContent() {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(StandbyBlack),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = "V2X LINK: STANDBY",
            color = StandbyHintColor,
            fontFamily = TechFont,
            fontWeight = FontWeight.Medium,
            fontSize = 16.sp,
        )
    }
}

@Composable
private fun WarningViewContent(
    uiWarningState: WarningUiState,
    latestScene: SceneGeometry?,
    warningViewSeam: IviWarningViewSeam?,
) {
    val active = uiWarningState as? WarningUiState.Active
    val scene = when {
        latestScene == null -> null
        active != null -> latestScene.copy(vehicleCSnapshot = active.event.objectSnapshot)
        else -> latestScene
    }
    if (warningViewSeam != null && scene != null) {
        val riskState = active?.event?.riskState ?: "low"
        warningViewSeam.Render(scene = scene, riskState = riskState)
    } else {
        WarningViewPlaceholder()
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
private fun BottomNavBar(currentMode: DisplayMode, modifier: Modifier = Modifier) {
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
            label = if (currentMode == DisplayMode.WarningView) {
                "V2X LINK: ACTIVE"
            } else {
                "V2X LINK: STANDBY"
            },
            dotColor = if (currentMode == DisplayMode.WarningView) AccentColor else PanelBorderColor,
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

@Preview(name = "AAOS 1280x720 — Standby", widthDp = 1280, heightDp = 720, showBackground = true)
@Composable
private fun MainScreenPreview() {
    MainScreenContent(
        currentMode = DisplayMode.StandbyView,
        onModeSelected = {},
    )
}

@Preview(name = "AAOS 1280x720 — Warning", widthDp = 1280, heightDp = 720, showBackground = true)
@Composable
private fun MainScreenWarningPreview() {
    MainScreenContent(
        currentMode = DisplayMode.WarningView,
        onModeSelected = {},
    )
}
