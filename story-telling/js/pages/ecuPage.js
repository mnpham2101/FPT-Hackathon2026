import * as THREE from "three";
import {
  clamp01,
  smoothstep,
  buildEnvelopeSprite,
  roundRectPath,
  loadPageHeader,
  applyPageHeader,
  showBanner,
  hideBanner,
} from "../common.js";

const BANNER_TEXT = "V2X-ECU received the relayed message";

const T_FLIGHT_START = 0.6;
const T_FLIGHT_END = 2.2;

// ---- Modern flat icon glyphs -----------------------------------------------
// Every icon draws into a local 0..size box (no clearRect — the card
// background already handles that). Each ECU has exactly one icon, matching
// its function: radar (V2X-ECU's cooperative sensing), mcu (ADA-ECU's
// fusion chip), display (IVI-ECU's driver screen).
const ICON_LIBRARY = {
  radar(ctx, size, accent) {
    const cx = size * 0.5;
    const cy = size * 0.5;
    ctx.strokeStyle = accent;
    ctx.lineWidth = size * 0.03;
    for (let i = 1; i <= 3; i++) {
      ctx.beginPath();
      ctx.arc(cx, cy, size * 0.1 * i, 0, Math.PI * 2);
      ctx.stroke();
    }
    ctx.fillStyle = accent;
    ctx.globalAlpha = 0.35;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, size * 0.3, -Math.PI * 0.5, -Math.PI * 0.2);
    ctx.closePath();
    ctx.fill();
    ctx.globalAlpha = 1;
    ctx.beginPath();
    ctx.arc(cx, cy, size * 0.035, 0, Math.PI * 2);
    ctx.fill();
  },
  mcu(ctx, size, accent) {
    const pad = size * 0.28;
    const w = size - pad * 2;
    ctx.strokeStyle = accent;
    ctx.lineWidth = size * 0.04;
    roundRectPath(ctx, pad, pad, w, w, size * 0.06);
    ctx.stroke();
    ctx.strokeRect(pad + w * 0.22, pad + w * 0.22, w * 0.56, w * 0.56);
    ctx.lineCap = "round";
    for (let i = 0; i < 3; i++) {
      const t = pad + w * (0.22 + i * 0.28);
      for (const [x1, y1, x2, y2] of [
        [t, pad, t, pad - size * 0.06],
        [t, pad + w, t, pad + w + size * 0.06],
        [pad, t, pad - size * 0.06, t],
        [pad + w, t, pad + w + size * 0.06, t],
      ]) {
        ctx.beginPath();
        ctx.moveTo(x1, y1);
        ctx.lineTo(x2, y2);
        ctx.stroke();
      }
    }
  },
  display(ctx, size, accent) {
    const pad = size * 0.16;
    const w = size - pad * 2;
    const h = w * 0.68;
    ctx.strokeStyle = accent;
    ctx.lineWidth = size * 0.045;
    roundRectPath(ctx, pad, pad, w, h, size * 0.06);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(size * 0.5, pad + h);
    ctx.lineTo(size * 0.5, pad + h + size * 0.08);
    ctx.moveTo(size * 0.38, pad + h + size * 0.08);
    ctx.lineTo(size * 0.62, pad + h + size * 0.08);
    ctx.stroke();
  },
};

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
 * every node shares this exact construction and the same glow-pulse
 * behaviour (`setGlow`). Only `label`, `position` and the starting
 * appearance preset vary per call, and the appearance itself can be swapped
 * live at any time via `setAppearance`, which is what the on-screen
 * "customize" buttons call.
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
    cycleAppearance() {
      currentIndex = (currentIndex + 1) % preset.length;
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

function buildBlueprintBackdrop() {
  const size = 512;
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = "#0b0f1a";
  ctx.fillRect(0, 0, size, size);
  ctx.strokeStyle = "rgba(127, 232, 255, 0.08)";
  ctx.lineWidth = 1;
  const step = 32;
  for (let x = 0; x <= size; x += step) {
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, size);
    ctx.stroke();
  }
  for (let y = 0; y <= size; y += step) {
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(size, y);
    ctx.stroke();
  }
  const texture = new THREE.CanvasTexture(canvas);
  texture.wrapS = THREE.RepeatWrapping;
  texture.wrapT = THREE.RepeatWrapping;
  texture.repeat.set(4, 4);
  return texture;
}

/**
 * Camera cuts inside Vehicle A to its three ECUs, laid out like a blueprint
 * diagram — a staggered flow (top-left to bottom-right) with orthogonal
 * wiring, not a single horizontal row. The relayed mail arrives first at
 * V2X-ECU and is absorbed there; the scene then freezes.
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

  const envelopeStart = new THREE.Vector3(V2X_POS.x - 2.5, V2X_POS.y + 4, 2);
  const catchPoint = new THREE.Vector3(V2X_POS.x, V2X_POS.y + halfH + 0.3, 0.2);

  // ---- Live customization: click-free HTML controls, one per ECU --------
  const panel = document.getElementById("ecu-customize-panel");
  const customizeButtons = {
    v2x: document.getElementById("customize-v2x"),
    ada: document.getElementById("customize-ada"),
    ivi: document.getElementById("customize-ivi"),
  };
  const nodeByKey = { v2x: v2xNode, ada: adaNode, ivi: iviNode };
  const customizeHandlers = {};
  for (const key of Object.keys(customizeButtons)) {
    const button = customizeButtons[key];
    if (!button) continue;
    customizeHandlers[key] = () => nodeByKey[key].cycleAppearance();
    button.addEventListener("click", customizeHandlers[key]);
  }

  const clock = new THREE.Clock();
  let virtualElapsed = 0;
  let frozen = false;

  function update(speed = 1) {
    const rawDt = Math.min(clock.getDelta(), 0.05);
    if (frozen) return;
    const dt = rawDt * speed;
    virtualElapsed += dt;

    const pulse = 0.5 + Math.sin(virtualElapsed * 2.2) * 0.22;
    for (const node of nodes) node.setGlow(pulse);

    const flightT = clamp01((virtualElapsed - T_FLIGHT_START) / (T_FLIGHT_END - T_FLIGHT_START));
    if (virtualElapsed >= T_FLIGHT_START && flightT < 1) {
      mailSprite.visible = true;
      mailSprite.position.lerpVectors(envelopeStart, catchPoint, smoothstep(flightT));
    }

    if (flightT >= 1 && !frozen) {
      mailSprite.visible = false;
      v2xNode.setGlow(1);
      showBanner(BANNER_TEXT);
      frozen = true;
    }
  }

  function onEnter() {
    applyPageHeader(header);
    hideBanner(); // this page replays from scratch each visit and re-shows its own banner on catch
    panel?.classList.remove("hidden");
    frozen = false;
    virtualElapsed = 0;
    mailSprite.visible = false;
    clock.start();
  }

  function onExit() {
    panel?.classList.add("hidden");
    clock.stop();
  }

  return { scene, camera, update, onEnter, onExit };
}
