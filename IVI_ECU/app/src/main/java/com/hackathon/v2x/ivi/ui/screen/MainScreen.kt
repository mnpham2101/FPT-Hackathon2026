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
import androidx.compose.runtime.collectAsState
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
import androidx.lifecycle.viewmodel.compose.viewModel
import com.hackathon.v2x.ivi.ui.DisplayMode
import com.hackathon.v2x.ivi.ui.MainViewModel

// ---------------------------------------------------------------------------
// R16 design tokens — dark automotive scheme. All dimensions are Dp tokens;
// no hardcoded pixel values anywhere in this layout.
// ---------------------------------------------------------------------------

private val BackgroundColor = Color(0xFF1A1A2E)
private val AccentColor = Color(0xFF00D4FF)
private val TextColor = Color(0xFFE8E8F0)
private val PanelColor = Color(0xFF14142A)
private val PanelBorderColor = Color(0xFF2A2A44)

/** Roboto — the Android/AAOS default UI typeface. */
private val UiFont = FontFamily.Default

/** Roboto Mono — AAOS resolves the system monospace family to Roboto Mono. */
private val TechFont = FontFamily.Monospace

/** Android accessibility floor for any tappable area. */
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

/** Display Area takes ~70% of the screen width; the rest splits across the two side bars. */
private const val DISPLAY_AREA_WIDTH_FRACTION = 0.7f
private const val SIDE_BAR_WIDTH_FRACTION = (1f - DISPLAY_AREA_WIDTH_FRACTION) / 2f

/** Below this width the side bars drop text labels and show icons only. */
private val WideLayoutMinWidth = 1024.dp

/** Fade duration for the Display Area mode transition. */
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
        DisplayMode.WarningView -> "WARNING"
        DisplayMode.HomeView -> "HOME"
        DisplayMode.AppsView -> "APPS"
        DisplayMode.SettingsView -> "SETTINGS"
    }

// ---------------------------------------------------------------------------
// Screen scaffold
// ---------------------------------------------------------------------------

/**
 * R16 main HMI screen — stateful entry point. Collects [MainViewModel]'s
 * `currentMode` and delegates rendering to the stateless [MainScreenContent]
 * (which is what previews and tests exercise directly).
 */
@Composable
fun MainScreen(
    modifier: Modifier = Modifier,
    viewModel: MainViewModel = viewModel(),
) {
    val currentMode by viewModel.currentMode.collectAsState()
    MainScreenContent(
        currentMode = currentMode,
        onModeSelected = viewModel::setMode,
        modifier = modifier,
    )
}

/**
 * Stateless scaffold: central Display Area (~70% width) flanked by side
 * button bars, with a status bottom bar. The Display Area content is driven
 * by [currentMode] through an [AnimatedContent] fade switcher.
 */
@Composable
fun MainScreenContent(
    currentMode: DisplayMode,
    onModeSelected: (DisplayMode) -> Unit,
    modifier: Modifier = Modifier,
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
                    DisplayModeSwitcher(currentMode = currentMode)
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

// ---------------------------------------------------------------------------
// Display Area
// ---------------------------------------------------------------------------

/** Central content slot; the Warning View and other views render inside it. */
@Composable
private fun DisplayArea(
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit,
) {
    Box(
        modifier = modifier
            .padding(DisplayAreaPadding)
            .clip(RoundedCornerShape(DisplayAreaCorner))
            .background(PanelColor)
            .border(DisplayBorderWidth, PanelBorderColor, RoundedCornerShape(DisplayAreaCorner)),
        contentAlignment = Alignment.Center,
    ) {
        content()
    }
}

/**
 * Fades between Display Area contents when [currentMode] changes.
 * Warning View gets a real renderer in Task Group 5.3; the other modes are
 * placeholders until their features land.
 */
@Composable
private fun DisplayModeSwitcher(currentMode: DisplayMode, modifier: Modifier = Modifier) {
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
                DisplayMode.WarningView -> WarningViewPlaceholder()
                DisplayMode.HomeView -> ViewPlaceholder("Home View Placeholder")
                DisplayMode.AppsView -> ViewPlaceholder("Apps View Placeholder")
                DisplayMode.SettingsView -> ViewPlaceholder("Settings View Placeholder")
            }
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

// ---------------------------------------------------------------------------
// Side button bars
// ---------------------------------------------------------------------------

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
            // Accessibility: tap area never shrinks below the 48dp minimum.
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

// ---------------------------------------------------------------------------
// Bottom status bar
// ---------------------------------------------------------------------------

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
        StatusIndicator(label = "V2X LINK: STANDBY", dotColor = PanelBorderColor)
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

// ---------------------------------------------------------------------------
// Preview — AAOS landscape default resolution
// ---------------------------------------------------------------------------

@Preview(name = "AAOS 1280x720", widthDp = 1280, heightDp = 720, showBackground = true)
@Composable
private fun MainScreenPreview() {
    MainScreenContent(
        currentMode = DisplayMode.HomeView,
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
