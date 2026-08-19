import * as THREE from "three";
import {
  lerp,
  clamp01,
  smoothstep,
  getToonGradient,
  buildCartoonFace,
  buildGlowSprite,
  buildIconTexture,
  buildTextSprite,
  buildEnvelopeSprite,
  setHudSubtitle,
  showBanner,
  hideBanner,
} from "../common.js";

// ---- Shared ECU look: every node uses the exact same body colour and the
// exact same glow behaviour. Only its icon, label and position differ.
const BODY_COLOR = 0x30395a;
const PANEL_COLOR = 0x3f4a72;
const GLOW_COLOR = 0x7fe8ff;
const ICON_STROKE = "#7fe8ff";

const SUBTITLE = "Inside Vehicle A: three ECUs, one shared awareness.";
const BANNER_TEXT = "V2X-ECU received the relayed message";

const T_REACH_START = 0.3;
const T_REACH_END = 1.1;
const T_FLIGHT_START = 0.6;
const T_FLIGHT_END = 2.2;

function drawV2xIcon(ctx, size) {
  ctx.clearRect(0, 0, size, size);
  ctx.strokeStyle = ICON_STROKE;
  ctx.lineWidth = size * 0.05;
  ctx.lineCap = "round";
  const cx = size / 2;
  const cy = size * 0.78;
  for (let i = 1; i <= 3; i++) {
    ctx.beginPath();
    ctx.arc(cx, cy, size * 0.16 * i, Math.PI * 1.15, Math.PI * 1.85);
    ctx.stroke();
  }
  ctx.fillStyle = "#eaf7ff";
  ctx.beginPath();
  ctx.arc(cx, cy, size * 0.06, 0, Math.PI * 2);
  ctx.fill();
}

function drawAdaIcon(ctx, size) {
  ctx.clearRect(0, 0, size, size);
  const cx = size / 2;
  const cy = size / 2;
  ctx.strokeStyle = ICON_STROKE;
  ctx.lineWidth = size * 0.045;
  ctx.beginPath();
  ctx.ellipse(cx, cy, size * 0.38, size * 0.22, 0, 0, Math.PI * 2);
  ctx.stroke();
  ctx.fillStyle = ICON_STROKE;
  ctx.beginPath();
  ctx.arc(cx, cy, size * 0.12, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = "#0a1420";
  ctx.beginPath();
  ctx.arc(cx, cy, size * 0.05, 0, Math.PI * 2);
  ctx.fill();
}

function drawIviIcon(ctx, size) {
  ctx.clearRect(0, 0, size, size);
  const pad = size * 0.18;
  ctx.strokeStyle = ICON_STROKE;
  ctx.lineWidth = size * 0.045;
  ctx.strokeRect(pad, pad, size - pad * 2, size - pad * 2);
  ctx.beginPath();
  ctx.moveTo(size / 2, pad + size * 0.14);
  ctx.lineTo(size - pad - size * 0.1, size - pad - size * 0.1);
  ctx.lineTo(pad + size * 0.1, size - pad - size * 0.1);
  ctx.closePath();
  ctx.stroke();
  ctx.fillStyle = ICON_STROKE;
  ctx.font = `bold ${size * 0.22}px sans-serif`;
  ctx.textAlign = "center";
  ctx.fillText("!", size / 2, size - pad - size * 0.14);
}

/**
 * One reusable ECU character: rounded torso + head, a chest screen showing
 * its function icon, a cartoon face, and a pair of arms. Every node built
 * from this factory shares the same body colour, panel colour and glow
 * behaviour (`setGlow`) — callers only vary the label, icon and position.
 * `reachOut(t)` animates the arms from folded (t=0) to reaching outward and
 * up (t=1); every node has it, even if only one ever plays it.
 */
function buildEcuNode({ label, iconDraw, x }) {
  const gradientMap = getToonGradient();
  const node = new THREE.Group();
  node.position.x = x;

  const glow = buildGlowSprite(GLOW_COLOR, 5.2);
  glow.position.set(0, 1.5, -0.4);
  node.add(glow);

  const torsoMat = new THREE.MeshToonMaterial({ color: BODY_COLOR, gradientMap });
  const torso = new THREE.Mesh(new THREE.BoxGeometry(1.9, 1.6, 1.2), torsoMat);
  torso.position.y = 1.0;
  torso.castShadow = true;
  node.add(torso);

  const headMat = new THREE.MeshToonMaterial({ color: BODY_COLOR, gradientMap });
  const head = new THREE.Mesh(new THREE.BoxGeometry(1.3, 1.05, 1.1), headMat);
  head.position.y = 2.35;
  head.castShadow = true;
  node.add(head);

  const screenMat = new THREE.MeshToonMaterial({ color: PANEL_COLOR, gradientMap });
  const screen = new THREE.Mesh(new THREE.BoxGeometry(1.3, 1.0, 0.08), screenMat);
  screen.position.set(0, 1.05, 0.65);
  node.add(screen);

  const iconTexture = buildIconTexture(iconDraw);
  const iconMat = new THREE.MeshBasicMaterial({ map: iconTexture, transparent: true });
  const icon = new THREE.Mesh(new THREE.PlaneGeometry(1.05, 1.05), iconMat);
  icon.position.set(0, 1.05, 0.71);
  node.add(icon);

  buildCartoonFace(node, { gradientMap, y: 2.42, z: 0.58, scale: 0.78 });

  const label3d = buildTextSprite(label);
  label3d.position.set(0, 3.15, 0);
  node.add(label3d);

  const armMat = new THREE.MeshToonMaterial({ color: PANEL_COLOR, gradientMap });
  const handMat = new THREE.MeshToonMaterial({ color: 0xffd9a0, gradientMap });
  const arms = [];
  for (const side of [-1, 1]) {
    const arm = new THREE.Group();
    const upper = new THREE.Mesh(new THREE.CylinderGeometry(0.14, 0.14, 0.9, 10), armMat);
    upper.position.y = -0.45;
    upper.castShadow = true;
    arm.add(upper);

    const hand = new THREE.Mesh(new THREE.SphereGeometry(0.22, 12, 10), handMat);
    hand.position.y = -0.95;
    hand.castShadow = true;
    arm.add(hand);

    arm.position.set(side * 1.15, 1.55, 0.35);
    arm.rotation.z = side * 0.15; // resting, folded slightly inward
    node.add(arm);
    arms.push({ group: arm, side });
  }

  return {
    group: node,
    setGlow(intensity) {
      glow.material.opacity = intensity;
    },
    reachOut(t) {
      for (const { group, side } of arms) {
        group.rotation.z = lerp(side * 0.15, side * -0.9, t);
        group.rotation.x = lerp(0, -0.6, t);
      }
    },
  };
}

function buildWire(from, to, color) {
  const start = new THREE.Vector3(...from);
  const end = new THREE.Vector3(...to);
  const distance = start.distanceTo(end);
  const material = new THREE.MeshToonMaterial({
    color,
    emissive: color,
    emissiveIntensity: 0.6,
    gradientMap: getToonGradient(),
  });
  const wire = new THREE.Mesh(new THREE.CylinderGeometry(0.05, 0.05, distance, 8), material);
  wire.position.copy(start).lerp(end, 0.5);
  wire.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), end.clone().sub(start).normalize());
  return { mesh: wire, material };
}

/**
 * Camera cuts inside Vehicle A to its three ECUs — V2X-ECU, ADA-ECU and
 * IVI-ECU — wired together. The relayed mail arrives first at V2X-ECU,
 * which reaches out and catches it; the scene then freezes.
 */
export function createEcuPage() {
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x0b0f1a);
  scene.fog = new THREE.Fog(0x0b0f1a, 14, 34);

  const camera = new THREE.PerspectiveCamera(50, window.innerWidth / window.innerHeight, 0.1, 200);
  camera.position.set(0, 4.2, 12);
  camera.lookAt(0, 1.7, 0);

  scene.add(new THREE.HemisphereLight(0x3a4a7a, 0x10131c, 0.6));
  const key = new THREE.DirectionalLight(0xdfe9ff, 1.1);
  key.position.set(-6, 10, 8);
  key.castShadow = true;
  scene.add(key);
  scene.add(new THREE.AmbientLight(0xffffff, 0.2));

  const floor = new THREE.Mesh(
    new THREE.PlaneGeometry(40, 40),
    new THREE.MeshStandardMaterial({ color: 0x141928, roughness: 0.9 })
  );
  floor.rotation.x = -Math.PI / 2;
  floor.receiveShadow = true;
  scene.add(floor);

  const NODE_X = 5.4;
  const v2xNode = buildEcuNode({ label: "V2X-ECU", iconDraw: drawV2xIcon, x: -NODE_X });
  const adaNode = buildEcuNode({ label: "ADA-ECU", iconDraw: drawAdaIcon, x: 0 });
  const iviNode = buildEcuNode({ label: "IVI-ECU", iconDraw: drawIviIcon, x: NODE_X });
  const nodes = [v2xNode, adaNode, iviNode];
  scene.add(v2xNode.group, adaNode.group, iviNode.group);

  const wires = [
    buildWire([-NODE_X + 0.95, 1.1, 0.2], [-0.95, 1.1, 0.2], GLOW_COLOR),
    buildWire([0.95, 1.1, 0.2], [NODE_X - 0.95, 1.1, 0.2], GLOW_COLOR),
    buildWire([-NODE_X, 3.2, 0], [NODE_X, 3.2, 0], GLOW_COLOR),
  ];
  for (const { mesh } of wires) scene.add(mesh);

  const mailSprite = buildEnvelopeSprite();
  scene.add(mailSprite);

  const envelopeStart = new THREE.Vector3(-NODE_X, 7.5, -5);
  const catchPoint = new THREE.Vector3(-NODE_X, 2.05, 1.35);

  const clock = new THREE.Clock();
  let frozen = false;

  function update() {
    if (frozen) return;
    clock.getDelta();
    const elapsed = clock.getElapsedTime();

    const pulse = 0.5 + Math.sin(elapsed * 2.2) * 0.22;
    for (const node of nodes) node.setGlow(pulse);
    for (const { material } of wires) material.emissiveIntensity = 0.4 + pulse * 0.5;

    const reachT = smoothstep((elapsed - T_REACH_START) / (T_REACH_END - T_REACH_START));
    v2xNode.reachOut(reachT);

    const flightT = clamp01((elapsed - T_FLIGHT_START) / (T_FLIGHT_END - T_FLIGHT_START));
    if (elapsed >= T_FLIGHT_START && flightT < 1) {
      mailSprite.visible = true;
      const eased = smoothstep(flightT);
      mailSprite.position.lerpVectors(envelopeStart, catchPoint, eased);
    }

    if (flightT >= 1 && !frozen) {
      mailSprite.visible = false;
      v2xNode.setGlow(1);
      showBanner(BANNER_TEXT);
      frozen = true;
    }
  }

  function onEnter() {
    setHudSubtitle(SUBTITLE);
    hideBanner(); // this page replays from scratch each visit and re-shows its own banner on catch
    frozen = false;
    mailSprite.visible = false;
    v2xNode.reachOut(0);
    clock.start();
  }

  function onExit() {
    clock.stop();
  }

  return { scene, camera, update, onEnter, onExit };
}
