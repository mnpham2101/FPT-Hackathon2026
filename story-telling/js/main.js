import * as THREE from "three";
import { createPlaybackController } from "./common.js";
import { createRoadPage } from "./pages/roadPage.js";
import { createEcuPage } from "./pages/ecuPage.js";

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

const pages = [
  createRoadPage({ onComplete: () => { nextBtn.disabled = false; } }),
  createEcuPage(),
];

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

  updateNavButtons();
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

// ---- Resize -----------------------------------------------------------
window.addEventListener("resize", () => {
  renderer.setSize(window.innerWidth, window.innerHeight);
  for (const page of pages) {
    page.camera.aspect = window.innerWidth / window.innerHeight;
    page.camera.updateProjectionMatrix();
  }
});

// ---- Playback control ---------------------------------------------------
// One controller, one set of key bindings — every page's update(speed)
// reads from it the same way, so a new page gets speed/pause for free.
const playback = createPlaybackController();
const playbackHud = document.getElementById("hud-playback");

function refreshPlaybackHud() {
  const state = playback.isPaused() ? "paused" : `${playback.getDisplaySpeed().toFixed(2)}x`;
  playbackHud.textContent = `${state} — ↑/↓ speed, space to pause`;
}

window.addEventListener("keydown", (event) => {
  switch (event.code) {
    case "ArrowUp":
      playback.increaseSpeed();
      break;
    case "ArrowDown":
      playback.decreaseSpeed();
      break;
    case "Space":
      event.preventDefault(); // don't let space scroll the page
      playback.togglePause();
      break;
    default:
      return;
  }
  refreshPlaybackHud();
});

refreshPlaybackHud();

// ---- Render loop --------------------------------------------------------
function animate() {
  requestAnimationFrame(animate);
  activePage.update(playback.getSpeed());
  renderer.render(activePage.scene, activePage.camera);
}

animate();
