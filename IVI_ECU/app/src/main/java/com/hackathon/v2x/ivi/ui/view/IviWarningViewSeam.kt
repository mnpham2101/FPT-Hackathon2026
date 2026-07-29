package com.hackathon.v2x.ivi.ui.view

import androidx.compose.runtime.Composable
import com.hackathon.v2x.ivi.model.SceneGeometry

/**
 * Rendering seam for the R17 Warning View.
 *
 * Decouples the data layer from the rendering engine so implementations are
 * swappable without touching `MainScreen`: the committed M1 renderer is the
 * 2D `CanvasWarningView` (17.5.3.3); an optional 3D `SceneViewWarning3D`
 * stub (17.5.3.6) sits behind a build flag.
 *
 * Contract: this file must keep zero Android UI framework imports — only the
 * Compose runtime annotation and the pure-Kotlin model layer.
 */
interface IviWarningViewSeam {

    /**
     * Draws the warning scene.
     *
     * @param scene ego-relative geometry; `scene.vehicleC` may be `null`
     *   (C not yet tracked) and implementations must render without it
     * @param riskState current risk level from the R4 message
     *   (`"low"` | `"medium"` | `"high"`)
     */
    @Composable
    fun Render(scene: SceneGeometry, riskState: String)
}
