import * as THREE from "three";

// ---- Math helpers, shared by every page's animation timeline -------------
export function lerp(a, b, t) {
  return a + (b - a) * t;
}

export function clamp01(t) {
  return Math.max(0, Math.min(1, t));
}

export function smoothstep(t) {
  const c = clamp01(t);
  return c * c * (3 - 2 * c);
}

export function hexToRgb(hex) {
  return { r: (hex >> 16) & 255, g: (hex >> 8) & 255, b: hex & 255 };
}

// ---- Toon shading gradient, shared by every cel-shaded character ---------
// Built once and reused everywhere — every MeshToonMaterial in the story
// (cars, ECU nodes, wires) samples this same 4-band gradient.
let sharedToonGradient = null;
export function getToonGradient() {
  if (sharedToonGradient) return sharedToonGradient;
  const canvas = document.createElement("canvas");
  canvas.width = 4;
  canvas.height = 1;
  const ctx = canvas.getContext("2d");
  ["#4d4d4d", "#8a8a8a", "#c2c2c2", "#ffffff"].forEach((c, i) => {
    ctx.fillStyle = c;
    ctx.fillRect(i, 0, 1, 1);
  });
  const texture = new THREE.CanvasTexture(canvas);
  texture.minFilter = THREE.NearestFilter;
  texture.magFilter = THREE.NearestFilter;
  sharedToonGradient = texture;
  return texture;
}

// ---- Cartoon face, shared by every character in the story -----------------
// Two googly eyes + a smile, added directly onto `target`. Every character
// (cars in the road page, ECU nodes in the machinery page) calls this with
// only its own placement (x/y/z/scale) — the face itself never changes.
export function buildCartoonFace(target, { gradientMap, x = 0, y = 0.95, z = 1.6, scale = 1 } = {}) {
  const eyeWhiteMat = new THREE.MeshToonMaterial({ color: 0xffffff, gradientMap });
  const eyePupilMat = new THREE.MeshBasicMaterial({ color: 0x22262b });
  const mouthMat = new THREE.MeshToonMaterial({ color: 0x22262b, gradientMap });

  const eyeGeo = new THREE.SphereGeometry(0.2 * scale, 14, 10);
  const pupilGeo = new THREE.SphereGeometry(0.1 * scale, 10, 8);
  for (const dx of [-0.42 * scale, 0.42 * scale]) {
    const eye = new THREE.Mesh(eyeGeo, eyeWhiteMat);
    eye.position.set(x + dx, y, z);
    target.add(eye);

    const pupil = new THREE.Mesh(pupilGeo, eyePupilMat);
    pupil.position.set(x + dx, y, z + 0.13 * scale);
    target.add(pupil);
  }

  const mouth = new THREE.Mesh(
    new THREE.TorusGeometry(0.26 * scale, 0.045 * scale, 8, 16, Math.PI),
    mouthMat
  );
  mouth.rotation.z = Math.PI; // flips the arc's top half to the bottom, tracing a smile
  mouth.position.set(x, y - 0.37 * scale, z + 0.06 * scale);
  target.add(mouth);
}

// ---- Soft radial glow sprite, shared by every glowing object ---------------
export function buildGlowSprite(color, size = 4) {
  const px = 256;
  const canvas = document.createElement("canvas");
  canvas.width = px;
  canvas.height = px;
  const ctx = canvas.getContext("2d");
  const { r, g, b } = hexToRgb(color);
  const gradient = ctx.createRadialGradient(px / 2, px / 2, 0, px / 2, px / 2, px / 2);
  gradient.addColorStop(0, `rgba(${r},${g},${b},0.85)`);
  gradient.addColorStop(1, `rgba(${r},${g},${b},0)`);
  ctx.fillStyle = gradient;
  ctx.fillRect(0, 0, px, px);

  const texture = new THREE.CanvasTexture(canvas);
  const material = new THREE.SpriteMaterial({ map: texture, transparent: true, depthWrite: false });
  const sprite = new THREE.Sprite(material);
  sprite.scale.set(size, size, 1);
  return sprite;
}

// ---- Canvas-drawn icon texture, shared by anything that needs a small ------
// hand-drawn glyph (ECU function icons today, reusable for any future page).
export function buildIconTexture(draw, size = 128) {
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d");
  draw(ctx, size);
  return new THREE.CanvasTexture(canvas);
}

// ---- Pill-shaped text label sprite -----------------------------------------
export function buildTextSprite(text, { color = "#eef2f7", background = "rgba(10,14,20,0.6)", fontSize = 42 } = {}) {
  const canvas = document.createElement("canvas");
  const ctx = canvas.getContext("2d");
  const font = `600 ${fontSize}px "Segoe UI", sans-serif`;
  ctx.font = font;
  const paddingX = fontSize * 0.5;
  canvas.width = Math.ceil(ctx.measureText(text).width + paddingX * 2);
  canvas.height = Math.ceil(fontSize * 1.6);
  // Resizing the canvas resets its context state, so the font must be re-applied.
  ctx.font = font;
  ctx.textBaseline = "middle";
  ctx.textAlign = "center";

  const radius = canvas.height / 2;
  ctx.fillStyle = background;
  ctx.beginPath();
  ctx.moveTo(radius, 0);
  ctx.arcTo(canvas.width, 0, canvas.width, canvas.height, radius);
  ctx.arcTo(canvas.width, canvas.height, 0, canvas.height, radius);
  ctx.arcTo(0, canvas.height, 0, 0, radius);
  ctx.arcTo(0, 0, canvas.width, 0, radius);
  ctx.closePath();
  ctx.fill();

  ctx.fillStyle = color;
  ctx.fillText(text, canvas.width / 2, canvas.height / 2 + 2);

  const texture = new THREE.CanvasTexture(canvas);
  const material = new THREE.SpriteMaterial({ map: texture, depthTest: false, transparent: true });
  const sprite = new THREE.Sprite(material);
  const worldScale = 0.014;
  sprite.scale.set(canvas.width * worldScale, canvas.height * worldScale, 1);
  return sprite;
}

// ---- Envelope billboard, shared by every page the mail travels through ----
export function buildEnvelopeSprite() {
  const size = 128;
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d");
  ctx.clearRect(0, 0, size, size);
  ctx.fillStyle = "#eef4ff";
  ctx.strokeStyle = "#3d8bff";
  ctx.lineWidth = 6;
  const pad = 16;
  ctx.fillRect(pad, pad + 12, size - pad * 2, size - pad * 2 - 12);
  ctx.strokeRect(pad, pad + 12, size - pad * 2, size - pad * 2 - 12);
  ctx.beginPath();
  ctx.moveTo(pad, pad + 12);
  ctx.lineTo(size / 2, size / 2 + 6);
  ctx.lineTo(size - pad, pad + 12);
  ctx.stroke();

  const texture = new THREE.CanvasTexture(canvas);
  const material = new THREE.SpriteMaterial({ map: texture, depthTest: false });
  const sprite = new THREE.Sprite(material);
  sprite.scale.set(1.4, 1.4, 1);
  sprite.visible = false;
  sprite.renderOrder = 999;
  return sprite;
}

// ---- HUD DOM helpers, shared by every page ---------------------------------
const hudSubtitleEl = document.getElementById("hud-subtitle");
const bannerEl = document.getElementById("mail-banner");
const bannerTextEl = bannerEl ? bannerEl.querySelector(".mail-text") : null;

export function setHudSubtitle(text) {
  if (hudSubtitleEl) hudSubtitleEl.textContent = text;
}

export function showBanner(text) {
  if (!bannerEl) return;
  if (bannerTextEl) bannerTextEl.textContent = text;
  bannerEl.classList.remove("hidden");
  bannerEl.classList.add("visible");
}

export function hideBanner() {
  if (!bannerEl) return;
  bannerEl.classList.remove("visible");
  bannerEl.classList.add("hidden");
}
