import * as THREE from "three";
import { createPauseController } from "./common.js";
import { createRoadPage } from "./pages/roadPage.js";
import { createProjectGoalsPage } from "./pages/projectGoalsPage.js";
import { createEcuPage } from "./pages/ecuPage.js";
import { createV2xPage } from "./pages/v2xPage.js";
import { createAdaPage } from "./pages/adaPage.js";
import { createIviPage } from "./pages/iviPage.js";
import { createOurWorkPage } from "./pages/ourWorkPage.js";
import { createNextMileStonePage } from "./pages/nextMileStonePage.js";
import { createOurTeamPage } from "./pages/ourTeamPage.js";

// ---- Renderer -------------------------------------------------------------
const canvas = document.getElementById("scene");
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.shadowMap.enabled = true;

// ---- Pages ------------------------------------------------------------
// Page 1 unlocks page 2's "next" button the moment its own story beat
// (the mail reaching Vehicle A) completes.
const prevBtn = document.getElementById("nav-prev");
const nextBtn = document.getElementById("nav-next");
const hudNumEl = document.getElementById("hud-num");

// Each page loads its own pageTitle/pageSubtitle from markdown before it's
// ready, so the whole set is awaited up front rather than per-navigation.
const pages = await Promise.all([
  createRoadPage({ onComplete: () => { nextBtn.disabled = false; } }),
  createProjectGoalsPage(),
  createEcuPage(),
  createV2xPage(),
  createAdaPage(),
  createIviPage(),
  createOurWorkPage(),
  createNextMileStonePage(),
  createOurTeamPage(),
]);

let currentIndex = 0;
let activePage = pages[currentIndex];

/**
 * The single reusable entry point for page navigation — both the left and
 * the right nav button call this with the page index they want, and any
 * future control (keyboard, a menu, a deep link) can reuse it the same way.
 */
function goToPage(index) {
  const clamped = Math.max(0, Math.min(pages.length - 1, index));
  if (clamped === currentIndex) return;

  activePage.onExit?.();
  currentIndex = clamped;
  activePage = pages[currentIndex];
  activePage.onEnter?.();
  updateHudNum();

  updateNavButtons();
}

/** #hud-num — the deck-style "01", "02", ... prefix, one per page in nav order. */
function updateHudNum() {
  if (hudNumEl) hudNumEl.textContent = String(currentIndex + 1).padStart(2, "0");
}

function updateNavButtons() {
  prevBtn.disabled = currentIndex <= 0;
  // The very first page gates "next" behind its own onComplete callback;
  // every later page is freely reachable once unlocked.
  if (currentIndex > 0) {
    nextBtn.disabled = currentIndex >= pages.length - 1;
  }
}

prevBtn.addEventListener("click", () => goToPage(currentIndex - 1));
nextBtn.addEventListener("click", () => goToPage(currentIndex + 1));

updateNavButtons();
activePage.onEnter?.();
updateHudNum();

// ---- Resize -----------------------------------------------------------
window.addEventListener("resize", () => {
  renderer.setSize(window.innerWidth, window.innerHeight);
  for (const page of pages) {
    page.camera.aspect = window.innerWidth / window.innerHeight;
    page.camera.updateProjectionMatrix();
  }
});

// ---- Pause control --------------------------------------------------------
// Pause is the only thing global; "animation" (what ↑/↓ step through) is
// each page's own concept — nextAnimation()/prevAnimation()/animationLabel
// are a common interface every page implements, but the meaning is
// page-owned: roadPage steps its playback speed, ecuPage steps which ECU is
// spotlighted. main.js never needs to know which.
const pause = createPauseController();
const playbackHud = document.getElementById("hud-playback");

// Refreshed every frame (see animate() below) rather than only on keydown —
// a page's animationLabel can change mid-transition (e.g. ecuPage's "Step
// X of 4" once the envelope lands), so a keydown-only refresh would leave
// the readout stale for the length of that transition.
function refreshPlaybackHud() {
  const state = pause.isPaused() ? "paused" : activePage.animationLabel ?? "";
  playbackHud.textContent = `${state} — ↑/↓ animation, ←/→ page, space to pause`;
}

// One handler, reused by every page: ↑/↓ call the active page's own
// nextAnimation()/prevAnimation(); ←/→ stop whatever's playing and jump
// pages immediately, regardless of playback or story-completion state.
window.addEventListener("keydown", (event) => {
  switch (event.code) {
    case "ArrowUp":
      activePage.nextAnimation?.();
      break;
    case "ArrowDown":
      activePage.prevAnimation?.();
      break;
    case "ArrowRight":
      goToPage(currentIndex + 1);
      break;
    case "ArrowLeft":
      goToPage(currentIndex - 1);
      break;
    case "Space":
      pause.togglePause();
      break;
    default:
      return;
  }
  event.preventDefault(); // don't let arrow keys/space scroll the page
});

// ---- Render loop --------------------------------------------------------
function animate() {
  requestAnimationFrame(animate);
  activePage.update(pause.getFactor());
  renderer.render(activePage.scene, activePage.camera);
  refreshPlaybackHud();
}

animate();
