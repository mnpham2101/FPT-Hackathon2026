import * as THREE from "three";
import {
  clamp01,
  smoothstep,
  buildEnvelopeSprite,
  roundRectPath,
  loadPageHeader,
  applyPageHeader,
  createAnimationStepper,
  hideBanner,
  ICON_LIBRARY,
  buildBlueprintBackdrop,
} from "../common.js";

const TRANSITION_DURATION = 1.4; // seconds for one envelope leg (idle -> V2X, V2X -> ADA, ADA -> IVI)

// A node's glow has 3 tiers matching its relation to the relay's current
// step: never reached yet, already visited (the relay moved past it), or
// the one it's on right now. "Visited" is a colour change too (preset[1]
// instead of preset[0]) — a lasting mark of "this ECU has processed the
// message" that the glow alone doesn't carry once focus moves on.
const UNVISITED_GLOW = 0.35;
const VISITED_GLOW = 0.55;
const ACTIVE_GLOW = 0.95;
const VISITED_PRESET_INDEX = 1;
const DEFAULT_PRESET_INDEX = 0;

// Each ECU cycles through its own small set of appearances on click — the
// icon (its function) never changes, only the accent colour does.
const APPEARANCE_PRESETS = {
  v2x: [
    { icon: "radar", accent: "#7fe8ff" },
    { icon: "radar", accent: "#8b7bff" },
    { icon: "radar", accent: "#5be37a" },
  ],
  ada: [
    { icon: "mcu", accent: "#7fe8ff" },
    { icon: "mcu", accent: "#ff6fae" },
    { icon: "mcu", accent: "#ffd166" },
  ],
  ivi: [
    { icon: "display", accent: "#7fe8ff" },
    { icon: "display", accent: "#ff8a5b" },
    { icon: "display", accent: "#ffd166" },
  ],
};

const CARD_W = 300;
const CARD_H = 360;
const CARD_ASPECT = CARD_H / CARD_W;
const CARD_WORLD_W = 2.6;
const CARD_WORLD_H = CARD_WORLD_W * CARD_ASPECT;

function buildCardTexture(label, { icon, accent }) {
  const canvas = document.createElement("canvas");
  canvas.width = CARD_W;
  canvas.height = CARD_H;
  const ctx = canvas.getContext("2d");

  roundRectPath(ctx, 4, 4, CARD_W - 8, CARD_H - 8, 26);
  const bg = ctx.createLinearGradient(0, 0, 0, CARD_H);
  bg.addColorStop(0, "#1c2338");
  bg.addColorStop(1, "#11142a");
  ctx.fillStyle = bg;
  ctx.fill();
  ctx.lineWidth = 3;
  ctx.strokeStyle = accent;
  ctx.globalAlpha = 0.7;
  ctx.stroke();
  ctx.globalAlpha = 1;

  const iconBox = 200;
  ctx.save();
  ctx.translate((CARD_W - iconBox) / 2, 26);
  ICON_LIBRARY[icon](ctx, iconBox, accent);
  ctx.restore();

  ctx.fillStyle = "#eef2f7";
  ctx.font = "600 30px 'Segoe UI', sans-serif";
  ctx.textAlign = "center";
  ctx.fillText(label, CARD_W / 2, CARD_H - 34);

  return new THREE.CanvasTexture(canvas);
}

/**
 * One reusable ECU card: a flat, unlit 2D tile with a soft glow behind it —
 * every node shares this exact construction and the same glow behaviour
 * (`setGlow`). Only `label`, `position` and the starting appearance preset
 * vary per call; the appearance can be swapped to a specific preset at any
 * time via `setAppearanceIndex`, which is how the relay animation marks a
 * node active/visited.
 */
function buildEcuNode({ label, position, preset }) {
  const node = new THREE.Group();
  node.position.set(position.x, position.y, 0);

  const glowMat = new THREE.SpriteMaterial({ transparent: true, depthWrite: false, opacity: 0.5 });
  const glow = new THREE.Sprite(glowMat);
  glow.scale.set(4.2, 4.2, 1);
  glow.position.z = -0.3;
  node.add(glow);

  const cardMat = new THREE.MeshBasicMaterial({ transparent: true });
  const card = new THREE.Mesh(new THREE.PlaneGeometry(CARD_WORLD_W, CARD_WORLD_H), cardMat);
  node.add(card);

  let currentIndex = 0;
  let glowSpriteFn = () => {};

  function applyPreset(p) {
    card.material.map?.dispose();
    card.material.map = buildCardTexture(label, p);
    card.material.needsUpdate = true;
    glowSpriteFn(p.accent);
  }

  glowSpriteFn = (accentHex) => {
    const size = 256;
    const canvas = document.createElement("canvas");
    canvas.width = size;
    canvas.height = size;
    const ctx = canvas.getContext("2d");
    const gradient = ctx.createRadialGradient(size / 2, size / 2, 0, size / 2, size / 2, size / 2);
    gradient.addColorStop(0, `${accentHex}cc`);
    gradient.addColorStop(1, `${accentHex}00`);
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, size, size);
    glow.material.map?.dispose();
    glow.material.map = new THREE.CanvasTexture(canvas);
    glow.material.needsUpdate = true;
  };

  applyPreset(preset[0]);

  return {
    group: node,
    setGlow(intensity) {
      glow.material.opacity = intensity;
    },
    /** Jumps straight to one preset by index — how the relay animation marks a node "active"/"visited" by colour, not just glow. */
    setAppearanceIndex(index) {
      currentIndex = index % preset.length;
      applyPreset(preset[currentIndex]);
    },
  };
}

// ---- Blueprint wiring: flat, orthogonal traces + pin dots, matching this
// project's own "segmented connectors, never diagonals" diagram convention.
function buildTraceSegment(x1, y1, x2, y2, color) {
  const length = Math.hypot(x2 - x1, y2 - y1);
  const material = new THREE.MeshBasicMaterial({ color });
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(length, 0.07), material);
  mesh.position.set((x1 + x2) / 2, (y1 + y2) / 2, -0.15);
  mesh.rotation.z = Math.atan2(y2 - y1, x2 - x1);
  return mesh;
}

function buildPinDot(x, y, color) {
  const mesh = new THREE.Mesh(new THREE.CircleGeometry(0.09, 16), new THREE.MeshBasicMaterial({ color }));
  mesh.position.set(x, y, -0.1);
  return mesh;
}

/**
 * Camera cuts inside Vehicle A to its three ECUs, laid out like a blueprint
 * diagram — a staggered flow (top-left to bottom-right) with orthogonal
 * wiring, not a single horizontal row. The relayed CPM travels the wire in
 * 4 discrete steps, each gated behind an up-key press and paired 1:1 with a
 * `## ` section in content/ecu.md (shown in the shared textbox):
 *   0 — idle: the envelope waits near V2X-ECU (the page's starting state)
 *   1 — envelope arrives at V2X-ECU; it glows
 *   2 — envelope moves V2X-ECU -> ADA-ECU; ADA-ECU glows
 *   3 — envelope moves ADA-ECU -> IVI-ECU; IVI-ECU glows
 */
export async function createEcuPage() {
  const header = await loadPageHeader("content/ecu.md");

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x0b0f1a);

  const camera = new THREE.PerspectiveCamera(50, window.innerWidth / window.innerHeight, 0.1, 200);
  camera.position.set(0, 0, 15);
  camera.lookAt(0, 0, 0);

  const backdrop = new THREE.Mesh(
    new THREE.PlaneGeometry(60, 60),
    new THREE.MeshBasicMaterial({ map: buildBlueprintBackdrop() })
  );
  backdrop.position.z = -2;
  scene.add(backdrop);

  // Blueprint-style staggered layout: V2X (top-left) -> ADA (centre) ->
  // IVI (bottom-right), each a step down and across from the last.
  const V2X_POS = { x: -5, y: 3.5 };
  const ADA_POS = { x: 0, y: 0 };
  const IVI_POS = { x: 5, y: -3.5 };
  const halfW = CARD_WORLD_W / 2;
  const halfH = CARD_WORLD_H / 2;

  const v2xNode = buildEcuNode({ label: "V2X-ECU", position: V2X_POS, preset: APPEARANCE_PRESETS.v2x });
  const adaNode = buildEcuNode({ label: "ADA-ECU", position: ADA_POS, preset: APPEARANCE_PRESETS.ada });
  const iviNode = buildEcuNode({ label: "IVI-ECU", position: IVI_POS, preset: APPEARANCE_PRESETS.ivi });
  const nodes = [v2xNode, adaNode, iviNode];
  scene.add(v2xNode.group, adaNode.group, iviNode.group);

  const wireColor = 0x7fe8ff;
  const bendA = { x: V2X_POS.x, y: ADA_POS.y };
  const bendB = { x: ADA_POS.x, y: IVI_POS.y };
  const traceGroup = new THREE.Group();
  traceGroup.add(
    buildTraceSegment(V2X_POS.x, V2X_POS.y - halfH, bendA.x, bendA.y, wireColor),
    buildTraceSegment(bendA.x, bendA.y, ADA_POS.x - halfW, ADA_POS.y, wireColor),
    buildTraceSegment(ADA_POS.x, ADA_POS.y - halfH, bendB.x, bendB.y, wireColor),
    buildTraceSegment(bendB.x, bendB.y, IVI_POS.x - halfW, IVI_POS.y, wireColor)
  );
  for (const [x, y] of [
    [V2X_POS.x, V2X_POS.y - halfH],
    [bendA.x, bendA.y],
    [ADA_POS.x - halfW, ADA_POS.y],
    [ADA_POS.x, ADA_POS.y - halfH],
    [bendB.x, bendB.y],
    [IVI_POS.x - halfW, IVI_POS.y],
  ]) {
    traceGroup.add(buildPinDot(x, y, wireColor));
  }
  scene.add(traceGroup);

  const mailSprite = buildEnvelopeSprite();
  scene.add(mailSprite);

  // One waypoint per animation step: 0 is where the envelope idles before
  // the first up-key press, 1-3 are where it lands on each ECU in turn.
  const WAYPOINTS = [
    new THREE.Vector3(V2X_POS.x - 2.5, V2X_POS.y + 4, 2), // idle, near V2X-ECU
    new THREE.Vector3(V2X_POS.x, V2X_POS.y + halfH + 0.3, 0.2), // landed on V2X-ECU
    new THREE.Vector3(ADA_POS.x, ADA_POS.y + halfH + 0.3, 0.2), // landed on ADA-ECU
    new THREE.Vector3(IVI_POS.x, IVI_POS.y + halfH + 0.3, 0.2), // landed on IVI-ECU
  ];

  const clock = new THREE.Clock();

  // `settledIndex` is the step actually landed on screen (what the glow and
  // the envelope's parked position reflect); while transitioning is true,
  // the envelope is mid-flight toward it and settledIndex hasn't caught up
  // yet. Step 0 (idle) has its own small continuous wiggle; steps 1-3 are
  // fully static once landed — "animation stops" until the next up/down
  // press starts a new transition. The stepper (shared with roadPage) owns
  // which step is *selected* — its onStep callback below is what kicks off
  // that transition, and it syncs the shared textbox on its own.
  let settledIndex = 0;
  let transitioning = false;
  let transitionElapsed = 0;
  let transitionFrom = WAYPOINTS[0];
  let transitionTo = WAYPOINTS[0];
  let pendingIndex = 0;
  let idleElapsed = 0;

  // `nodes[i]`'s own step number is i+1 (V2X=1, ADA=2, IVI=3; step 0 is the
  // idle state before anything is reached). A node ahead of the settled
  // step has never been reached; the settled step's own node is active;
  // anything behind it was visited on an earlier step and stays marked.
  function applyStepAppearance(index) {
    nodes.forEach((node, i) => {
      const nodeStep = i + 1;
      if (nodeStep === index) {
        node.setAppearanceIndex(VISITED_PRESET_INDEX);
        node.setGlow(ACTIVE_GLOW);
      } else if (nodeStep < index) {
        node.setAppearanceIndex(VISITED_PRESET_INDEX);
        node.setGlow(VISITED_GLOW);
      } else {
        node.setAppearanceIndex(DEFAULT_PRESET_INDEX);
        node.setGlow(UNVISITED_GLOW);
      }
    });
  }

  const stepper = createAnimationStepper(header.sections, (targetIndex) => {
    transitionFrom = WAYPOINTS[settledIndex];
    transitionTo = WAYPOINTS[targetIndex];
    transitionElapsed = 0;
    transitioning = true;
    pendingIndex = targetIndex;
  });

  function update(pauseFactor = 1) {
    const rawDt = Math.min(clock.getDelta(), 0.05);
    const dt = rawDt * pauseFactor;

    if (transitioning) {
      transitionElapsed += dt;
      const t = clamp01(transitionElapsed / TRANSITION_DURATION);
      mailSprite.position.lerpVectors(transitionFrom, transitionTo, smoothstep(t));
      if (t >= 1) {
        transitioning = false;
        settledIndex = pendingIndex;
        applyStepAppearance(settledIndex);
      }
      return;
    }

    if (settledIndex === 0) {
      // Idle wiggle: the envelope hasn't been sent anywhere yet.
      idleElapsed += dt;
      mailSprite.position.set(
        WAYPOINTS[0].x + Math.sin(idleElapsed * 5) * 0.25,
        WAYPOINTS[0].y + Math.cos(idleElapsed * 3.3) * 0.18,
        WAYPOINTS[0].z
      );
    }
    // Steps 1-3, once landed, need no further per-frame work — the scene
    // stays exactly as applyStepAppearance/the envelope's parked position left it.
  }

  function onEnter() {
    applyPageHeader(header);
    hideBanner(); // clears whatever roadPage's own banner was showing
    settledIndex = 0;
    transitioning = false;
    transitionElapsed = 0;
    idleElapsed = 0;
    mailSprite.visible = true;
    mailSprite.position.copy(WAYPOINTS[0]);
    applyStepAppearance(0);
    stepper.reset();
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
      return `Step ${stepper.index + 1} of ${stepper.count}`;
    },
  };
}
