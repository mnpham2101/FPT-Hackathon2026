import * as THREE from "three";
import {
  loadPageHeader,
  applyPageHeader,
  createAnimationStepper,
  hideBanner,
  buildBlueprintBackdrop,
} from "../common.js";

const DOT_RADIUS = 0.045;
const DOT_SPACING = 0.26;
const DOT_UNVISITED_COLOR = 0x3f4a3d;

// Vertical placement of the progress-dot row, in world units up from scene
// centre — near the bottom of the frustum rather than under the title, out
// of the way of both the HTML title/subtitle above and the large text/image
// textbox (#animation-textbox:has(.textbox-figure) in style.css) that now
// occupies most of the screen. At this camera's fov/distance the visible
// frustum is ~14 units tall (±7 from centre), so this sits just above the
// bottom edge, roughly level with the playback HUD.
const DOTS_Y_OFFSET = -6.3;

// One pip per markdown `## ` section — filled/enlarged for the current and
// already-visited steps, dim otherwise. A page with no sections (content not
// yet authored) gets an empty group, same "nothing to step through" shape as
// roadPage's stepper.
function buildProgressDots(count) {
  const group = new THREE.Group();
  const startX = -((count - 1) * DOT_SPACING) / 2;
  const dots = [];
  for (let i = 0; i < count; i++) {
    const mesh = new THREE.Mesh(
      new THREE.CircleGeometry(DOT_RADIUS, 16),
      new THREE.MeshBasicMaterial({ color: DOT_UNVISITED_COLOR })
    );
    mesh.position.set(startX + i * DOT_SPACING, 0, 0);
    group.add(mesh);
    dots.push(mesh);
  }
  return { group, dots };
}

function applyDotState(dots, index, accent) {
  dots.forEach((dot, i) => {
    dot.material.color.set(i <= index ? accent : DOT_UNVISITED_COLOR);
    dot.scale.setScalar(i === index ? 1.35 : 1);
  });
}

/**
 * The shared scene every markdown-driven content page (v2x, ada, ivi,
 * our-work-process, the-road-ahead) renders: the same blueprint backdrop as
 * ecuPage, and a small row of progress dots near the bottom of the screen
 * tracking the shared textbox stepper. All five pages render identically —
 * only the markdown source and accent colour (dot fill) vary per page; the
 * caller supplies those, everything else (loading the header, wiring the
 * stepper, syncing the shared textbox) is identical across all five.
 */
export async function createContentPage({ contentPath, accent }) {
  const header = await loadPageHeader(contentPath);

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x1c2622);

  const camera = new THREE.PerspectiveCamera(50, window.innerWidth / window.innerHeight, 0.1, 200);
  camera.position.set(0, 0, 15);
  camera.lookAt(0, 0, 0);

  const backdrop = new THREE.Mesh(
    new THREE.PlaneGeometry(60, 60),
    new THREE.MeshBasicMaterial({ map: buildBlueprintBackdrop() })
  );
  backdrop.position.z = -2;
  scene.add(backdrop);

  const { group: dotsGroup, dots } = buildProgressDots(header.sections.length);
  scene.add(dotsGroup);

  const clock = new THREE.Clock();
  let elapsed = 0;

  const stepper = createAnimationStepper(header.sections, (targetIndex) => {
    applyDotState(dots, targetIndex, accent);
  });

  // Gentle idle life for the dot row — a slow bob, not a story beat, so it
  // never needs to gate on step transitions the way ecuPage's envelope does.
  function update(pauseFactor = 1) {
    const dt = Math.min(clock.getDelta(), 0.05) * pauseFactor;
    elapsed += dt;
    dotsGroup.position.y = DOTS_Y_OFFSET + Math.sin(elapsed * 0.8) * 0.04;
  }

  function onEnter() {
    applyPageHeader(header);
    hideBanner();
    stepper.reset();
    applyDotState(dots, 0, accent);
    clock.start();
  }

  function onExit() {
    clock.stop();
  }

  return {
    scene,
    camera,
    update,
    onEnter,
    onExit,
    nextAnimation: stepper.next,
    prevAnimation: stepper.prev,
    get animationLabel() {
      return stepper.count > 0 ? `Step ${stepper.index + 1} of ${stepper.count}` : "No steps";
    },
  };
}
