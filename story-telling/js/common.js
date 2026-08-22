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

// ---- Pause control, shared by every page's animation timeline -------------
// "Animation" (what up/down step through) is owned by each page — roadPage
// steps its own playback speed, ecuPage steps which ECU is highlighted.
// Pause is the one thing that stays global: it multiplies into every page's
// own dt as a 0/1 factor, so Space freezes whichever page is active without
// either page needing to know about it.
export function createPauseController() {
  let paused = false;
  return {
    togglePause() {
      paused = !paused;
    },
    isPaused() {
      return paused;
    },
    getFactor() {
      return paused ? 0 : 1;
    },
  };
}

// ---- Rounded-rectangle path, shared by every canvas-drawn UI card ---------
export function roundRectPath(ctx, x, y, w, h, r) {
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r);
  ctx.arcTo(x, y, x + w, y, r);
  ctx.closePath();
}

// ---- Ink outline, shared by every "storybook" shape -----------------------
// The classic cheap toon-outline trick: a second copy of the same geometry,
// scaled up a few percent, drawn back-face-only so only its silhouette
// shows around the real mesh. Reuses the source geometry (no clone) and
// rides along as a child, so it inherits the mesh's own transform for free.
export function addOutline(mesh, { color = 0x22262b, thickness = 0.06 } = {}) {
  const outlineMesh = new THREE.Mesh(
    mesh.geometry,
    new THREE.MeshBasicMaterial({ color, side: THREE.BackSide })
  );
  outlineMesh.scale.multiplyScalar(1 + thickness);
  mesh.add(outlineMesh);
  return outlineMesh;
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

// ---- Flat icon glyphs, shared by every card-style node/topic across pages -
// Every icon draws into a local 0..size box (no clearRect — the caller's
// card background already handles that). radar/mcu/display are ecuPage's
// three ECUs; checklist/milestone serve the content pages (our-work-process,
// the-road-ahead).
export const ICON_LIBRARY = {
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
  checklist(ctx, size, accent) {
    const pad = size * 0.18;
    const w = size - pad * 2;
    const h = w * 1.15;
    ctx.strokeStyle = accent;
    ctx.lineWidth = size * 0.035;
    roundRectPath(ctx, pad, pad, w, h, size * 0.05);
    ctx.stroke();
    ctx.fillStyle = accent;
    roundRectPath(ctx, size * 0.38, pad - size * 0.05, size * 0.24, size * 0.09, size * 0.02);
    ctx.fill();
    ctx.lineCap = "round";
    const rowStartY = pad + h * 0.24;
    const rowGap = h * 0.24;
    for (let i = 0; i < 3; i++) {
      const y = rowStartY + i * rowGap;
      ctx.strokeRect(pad + w * 0.1, y - w * 0.05, w * 0.1, w * 0.1);
      if (i < 2) {
        ctx.beginPath();
        ctx.moveTo(pad + w * 0.115, y);
        ctx.lineTo(pad + w * 0.15, y + w * 0.04);
        ctx.lineTo(pad + w * 0.195, y - w * 0.04);
        ctx.stroke();
      }
      ctx.beginPath();
      ctx.moveTo(pad + w * 0.3, y);
      ctx.lineTo(pad + w * 0.85, y);
      ctx.stroke();
    }
  },
  milestone(ctx, size, accent) {
    const cx = size * 0.5;
    ctx.strokeStyle = accent;
    ctx.lineWidth = size * 0.04;
    ctx.lineCap = "round";
    ctx.beginPath();
    ctx.moveTo(cx - size * 0.18, size * 0.82);
    ctx.lineTo(cx - size * 0.18, size * 0.14);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(cx - size * 0.3, size * 0.82);
    ctx.lineTo(cx - size * 0.06, size * 0.82);
    ctx.stroke();
    ctx.fillStyle = accent;
    ctx.globalAlpha = 0.85;
    ctx.beginPath();
    ctx.moveTo(cx - size * 0.18, size * 0.16);
    ctx.lineTo(cx + size * 0.32, size * 0.28);
    ctx.lineTo(cx - size * 0.18, size * 0.4);
    ctx.closePath();
    ctx.fill();
    ctx.globalAlpha = 1;
  },
};

// ---- Blueprint grid backdrop, shared by every page drawn in the blueprint --
// visual language (ecuPage's ECU diagram, every markdown content page).
export function buildBlueprintBackdrop() {
  const size = 512;
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = "#1c2622";
  ctx.fillRect(0, 0, size, size);
  ctx.strokeStyle = "rgba(143, 174, 122, 0.10)";
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

  roundRectPath(ctx, 0, 0, canvas.width, canvas.height, canvas.height / 2);
  ctx.fillStyle = background;
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

// ---- Page header + sections, loaded from markdown --------------------------
// Every page shares this exact loader/applier pair — only the markdown file
// path differs per page. A leading `---` frontmatter block carries style
// (colour, weight); the first `# Heading` is pageTitle; the first non-blank
// line before any `## ` subsection is pageSubtitle (omit it to leave no
// subtitle). Each `## Heading` after that starts one animation step's
// section — its own heading plus a small rendered-markdown body (paragraphs,
// `- `/`* ` bullet lists, or a `![alt](src)` image) — collected into
// `sections`. A page with no `## ` subsections gets an empty `sections` array
// (roadPage: nothing to step through); a page with N of them has N steps
// (ecuPage: 4).
function escapeHtml(text) {
  return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

// A `![alt](src)` line renders as a figure in place of the surrounding text —
// an image swaps in for prose/bullets, it doesn't sit alongside them. No
// caption: the section's own heading (already shown above the textbox body)
// would just repeat itself under the image.
function renderMarkdownBody(lines) {
  const blocks = [];
  let listItems = null;
  const flushList = () => {
    if (listItems) {
      blocks.push(`<ul>${listItems.map((item) => `<li>${item}</li>`).join("")}</ul>`);
      listItems = null;
    }
  };
  for (const raw of lines) {
    const line = raw.trim();
    if (!line) {
      flushList();
      continue;
    }
    const image = line.match(/^!\[([^\]]*)\]\(([^)]+)\)$/);
    if (image) {
      flushList();
      const [, alt, src] = image;
      blocks.push(`<figure class="textbox-figure"><img src="${escapeHtml(src)}" alt="${escapeHtml(alt)}"></figure>`);
      continue;
    }
    const bullet = line.match(/^[-*]\s+(.*)$/);
    if (bullet) {
      listItems = listItems || [];
      listItems.push(escapeHtml(bullet[1]));
    } else {
      flushList();
      blocks.push(`<p>${escapeHtml(line)}</p>`);
    }
  }
  flushList();
  return blocks.join("");
}

function parsePageHeaderMarkdown(raw) {
  const style = {};
  let body = raw;

  const frontmatterMatch = raw.match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n?/);
  if (frontmatterMatch) {
    body = raw.slice(frontmatterMatch[0].length);
    for (const line of frontmatterMatch[1].split(/\r?\n/)) {
      const kv = line.match(/^([\w-]+):\s*(.*)$/);
      if (kv) style[kv[1]] = kv[2].trim().replace(/^["']|["']$/g, "");
    }
  }

  const lines = body.split(/\r?\n/);
  const titleIndex = lines.findIndex((line) => line.trim().startsWith("# "));
  const pageTitle = titleIndex >= 0 ? lines[titleIndex].trim().slice(2).trim() : "";

  const firstSectionIndex = lines.findIndex(
    (line, i) => i > titleIndex && line.trim().startsWith("## ")
  );
  const subtitleLines = lines.slice(titleIndex + 1, firstSectionIndex >= 0 ? firstSectionIndex : lines.length);
  const pageSubtitle = subtitleLines.map((line) => line.trim()).find((line) => line.length > 0) || "";

  const sections = [];
  if (firstSectionIndex >= 0) {
    let current = null;
    for (let i = firstSectionIndex; i < lines.length; i++) {
      const line = lines[i];
      if (line.trim().startsWith("## ")) {
        if (current) sections.push(current);
        current = { heading: line.trim().slice(3).trim(), bodyLines: [] };
      } else if (current) {
        current.bodyLines.push(line);
      }
    }
    if (current) sections.push(current);
  }
  for (const section of sections) {
    section.bodyHtml = renderMarkdownBody(section.bodyLines);
    delete section.bodyLines;
  }

  return { pageTitle, pageSubtitle, style, sections };
}

/** Fetches and parses one page's markdown header + section file. */
export async function loadPageHeader(path) {
  const res = await fetch(path);
  if (!res.ok) throw new Error(`loadPageHeader: ${path} → HTTP ${res.status}`);
  return parsePageHeaderMarkdown(await res.text());
}

const titleEl = document.getElementById("hud-title");
const subtitleEl = document.getElementById("hud-subtitle");

/** Renders a loaded page header — the one place pageTitle/pageSubtitle reach the DOM. */
export function applyPageHeader({ pageTitle = "", pageSubtitle = "", style = {} } = {}) {
  if (titleEl) {
    titleEl.textContent = pageTitle;
    titleEl.style.color = style.titleColor || "";
    titleEl.style.fontWeight = style.titleWeight || "";
  }
  if (subtitleEl) {
    subtitleEl.textContent = pageSubtitle;
    subtitleEl.style.color = style.subtitleColor || "";
    subtitleEl.style.fontWeight = style.subtitleWeight || "";
    // Collapsed rather than left empty-but-visible: an empty div still
    // occupies its line-height + margin, which #animation-textbox's top
    // offset has to reserve room for on every page regardless of whether
    // this page actually has subtitle text (most content pages don't).
    subtitleEl.style.display = pageSubtitle ? "" : "none";
  }
}

// ---- Animation-step textbox, shared by every page's nextAnimation ---------
// One shared "caption card": whichever page's current animation step has a
// markdown section shows it here — pass the section from `sections` (or
// `undefined`/`null` to hide the box, which is what a page with no sections,
// like roadPage, does every time).
const textboxEl = document.getElementById("animation-textbox");
const textboxHeadingEl = document.getElementById("textbox-heading");
const textboxBodyEl = document.getElementById("textbox-body");

export function applyTextBox(section) {
  if (!textboxEl) return;
  if (!section) {
    textboxEl.classList.add("hidden");
    return;
  }
  textboxEl.classList.remove("hidden");
  if (textboxHeadingEl) textboxHeadingEl.textContent = section.heading;
  if (textboxBodyEl) textboxBodyEl.innerHTML = section.bodyHtml || "";
}

// ---- Animation stepper, shared by every page's nextAnimation/prevAnimation -
// The bounds-checked "which step am I on" bookkeeping is identical for any
// page: clamp between 0 and sections.length - 1, no-op at the edges or when the
// target doesn't change, and sync the shared textbox the moment a real move
// happens. A page with no sections (roadPage) gets a stepper whose next()/
// prev() are always no-ops for free — nothing else to special-case.
// `onStep(index)` is optional: it's where a page reacts to the move (e.g.
// ecuPage kicks off its envelope's flight to the new step's waypoint).
export function createAnimationStepper(sections, onStep) {
  let index = 0;

  function goTo(target) {
    if (target < 0 || target >= sections.length || target === index) return;
    index = target;
    applyTextBox(sections[index]);
    onStep?.(index);
  }

  return {
    next: () => goTo(index + 1),
    prev: () => goTo(index - 1),
    reset() {
      index = 0;
      applyTextBox(sections[0]);
    },
    get index() {
      return index;
    },
    get count() {
      return sections.length;
    },
  };
}

// ---- HUD DOM helpers, shared by every page ---------------------------------
const bannerEl = document.getElementById("mail-banner");
const bannerTextEl = bannerEl ? bannerEl.querySelector(".mail-text") : null;

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
