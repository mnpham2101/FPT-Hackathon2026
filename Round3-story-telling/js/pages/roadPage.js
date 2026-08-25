import * as THREE from "three";
import {
  lerp,
  clamp01,
  smoothstep,
  getToonGradient,
  addOutline,
  buildGlowSprite,
  buildEnvelopeSprite,
  loadPageHeader,
  applyPageHeader,
  createAnimationStepper,
  showBanner,
  hideBanner,
} from "../common.js";

// This page runs at a single fixed speed — it has no `## ` subsections in
// content/road.md, so it has nothing for nextAnimation/prevAnimation to
// step through (see the bottom of this file).
const SPEED = 1.25;

// The single ink-outline colour every "storybook" shape in this scene uses —
// a bold, uniform outline (rather than per-object tinted outlines) is what
// reads as one consistent illustration style.
const OUTLINE_COLOR = 0x22262b;

// ---- Road geometry constants -------------------------------------------
const LANE_WIDTH = 3.6;
const LANE_RIGHT_X = LANE_WIDTH / 2;
const LANE_LEFT_X = -LANE_WIDTH / 2;
const ROAD_WIDTH = LANE_WIDTH * 2 + 1.0;
const ROAD_LENGTH = 500;

// ---- Story timeline (seconds) ------------------------------------------
const T_CUTIN_START = 3.0;
const T_CUTIN_END = 5.5;
const T_C_APPEAR = 8.0;
const MAIL_TRIGGER_DISTANCE = 70; // B-to-C gap that fires the V2X message — B is still far from C
const MAIL_FLIGHT_DURATION = 1.6;

const CAR_SPEED = 11; // units/sec, world moves toward the camera (+z decreases)

const BANNER_TEXT = "V2X message received — hazard ahead relayed from Vehicle B";

/**
 * Vehicle A drives with Vehicle B ahead of it; B cuts into A's lane, both
 * keep driving until Vehicle C appears in the same lane, then B relays a
 * V2X mail message to A. `onComplete` fires once, the moment that message
 * is delivered, so the caller can unlock navigation to the next page.
 */
export async function createRoadPage({ onComplete } = {}) {
  const header = await loadPageHeader("content/road.md");

  const scene = new THREE.Scene();
  const HORIZON_COLOR = 0xcdeee0;
  scene.background = new THREE.Color(HORIZON_COLOR); // fallback behind the sky dome
  scene.fog = new THREE.Fog(0xbfe6da, 130, 420);

  const camera = new THREE.PerspectiveCamera(55, window.innerWidth / window.innerHeight, 0.1, 1200);

  const toonGradient = getToonGradient();

  // ---- Lighting (soft, warm golden-hour daylight) ------------------------
  scene.add(new THREE.HemisphereLight(0x8fd4cc, 0x7c9c5e, 0.8));

  const sun = new THREE.DirectionalLight(0xfff0c8, 1.2);
  sun.position.set(-60, 90, 40);
  sun.castShadow = true;
  sun.shadow.mapSize.set(2048, 2048);
  sun.shadow.camera.left = -60;
  sun.shadow.camera.right = 60;
  sun.shadow.camera.top = 60;
  sun.shadow.camera.bottom = -60;
  sun.shadow.camera.far = 220;
  scene.add(sun);

  scene.add(new THREE.AmbientLight(0xfff4e0, 0.28));

  // ---- Sky dome: a soft teal-to-cream vertical gradient instead of a flat
  // colour, matching the muted storybook palette used throughout. -----------
  function buildSkyDome() {
    const geometry = new THREE.SphereGeometry(550, 24, 16);
    const material = new THREE.ShaderMaterial({
      uniforms: {
        topColor: { value: new THREE.Color(0x2f9d97) },
        bottomColor: { value: new THREE.Color(0xd8f0e0) },
        offset: { value: 24 },
        exponent: { value: 0.55 },
      },
      vertexShader: `
        varying vec3 vWorldPosition;
        void main() {
          vec4 worldPosition = modelMatrix * vec4(position, 1.0);
          vWorldPosition = worldPosition.xyz;
          gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
        }
      `,
      fragmentShader: `
        uniform vec3 topColor;
        uniform vec3 bottomColor;
        uniform float offset;
        uniform float exponent;
        varying vec3 vWorldPosition;
        void main() {
          float h = normalize(vWorldPosition + vec3(0.0, offset, 0.0)).y;
          gl_FragColor = vec4(mix(bottomColor, topColor, max(pow(max(h, 0.0), exponent), 0.0)), 1.0);
        }
      `,
      side: THREE.BackSide,
    });
    return new THREE.Mesh(geometry, material);
  }
  scene.add(buildSkyDome());

  // ---- Sky dressing: hazy sun + drifting painterly clouds ------------------
  const sunMesh = new THREE.Mesh(
    new THREE.SphereGeometry(9, 20, 20),
    new THREE.MeshBasicMaterial({ color: 0xfff2c4 })
  );
  sunMesh.position.set(-150, 95, -220);
  scene.add(sunMesh);

  const sunGlow = buildGlowSprite(0xfff2c4, 60);
  sunGlow.position.copy(sunMesh.position);
  scene.add(sunGlow);

  function buildCloud() {
    const cloud = new THREE.Group();
    const litMat = new THREE.MeshToonMaterial({ color: 0xffffff, gradientMap: toonGradient });
    const puffCount = 3 + Math.floor(Math.random() * 2);
    for (let i = 0; i < puffCount; i++) {
      const puff = new THREE.Mesh(new THREE.SphereGeometry(1 + Math.random() * 0.7, 10, 8), litMat);
      puff.position.set((Math.random() - 0.5) * 4.2, (Math.random() - 0.5) * 0.6, (Math.random() - 0.5) * 2.2);
      puff.scale.y = 0.62;
      addOutline(puff, { color: OUTLINE_COLOR, thickness: 0.07 });
      cloud.add(puff);
    }
    return cloud;
  }

  const cloudsGroup = new THREE.Group();
  for (let i = 0; i < 16; i++) {
    const cloud = buildCloud();
    cloud.scale.setScalar(2 + Math.random() * 2.5);
    cloud.position.set((Math.random() - 0.5) * 300, 36 + Math.random() * 24, -Math.random() * ROAD_LENGTH + 80);
    cloudsGroup.add(cloud);
  }
  scene.add(cloudsGroup);

  // ---- Ground + road ------------------------------------------------------
  const grass = new THREE.Mesh(
    new THREE.PlaneGeometry(700, ROAD_LENGTH + 200),
    new THREE.MeshToonMaterial({ color: 0x8fae6c, gradientMap: toonGradient })
  );
  grass.rotation.x = -Math.PI / 2;
  grass.position.z = -ROAD_LENGTH / 2 + 100;
  grass.position.y = -0.02;
  grass.receiveShadow = true;
  scene.add(grass);

  // A scatter of wildflowers across the meadow — a cheap point cloud, not
  // individual meshes, for that lush painted-meadow touch.
  function buildMeadowFlowers() {
    const count = 550;
    const positions = new Float32Array(count * 3);
    const colors = new Float32Array(count * 3);
    const palette = [0xf2e6a0, 0xf5f5f0, 0xe8b8cf, 0xb8a8d9].map((c) => new THREE.Color(c));
    for (let i = 0; i < count; i++) {
      const side = Math.random() < 0.5 ? -1 : 1;
      positions[i * 3] = side * (ROAD_WIDTH / 2 + 4 + Math.random() * 45);
      positions[i * 3 + 1] = 0.18;
      positions[i * 3 + 2] = -Math.random() * ROAD_LENGTH + 60;
      const c = palette[Math.floor(Math.random() * palette.length)];
      colors[i * 3] = c.r;
      colors[i * 3 + 1] = c.g;
      colors[i * 3 + 2] = c.b;
    }
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
    geometry.setAttribute("color", new THREE.BufferAttribute(colors, 3));
    const material = new THREE.PointsMaterial({ size: 0.4, vertexColors: true, sizeAttenuation: true });
    return new THREE.Points(geometry, material);
  }
  scene.add(buildMeadowFlowers());

  const road = new THREE.Mesh(
    new THREE.PlaneGeometry(ROAD_WIDTH, ROAD_LENGTH),
    new THREE.MeshToonMaterial({ color: 0x7c766a, gradientMap: toonGradient })
  );
  road.rotation.x = -Math.PI / 2;
  road.position.z = -ROAD_LENGTH / 2 + 100;
  road.receiveShadow = true;
  scene.add(road);

  function addEdgeLine(x) {
    const line = new THREE.Mesh(
      new THREE.PlaneGeometry(0.15, ROAD_LENGTH),
      new THREE.MeshStandardMaterial({ color: 0xf5f7fa })
    );
    line.rotation.x = -Math.PI / 2;
    line.position.set(x, 0.01, road.position.z);
    scene.add(line);
  }
  addEdgeLine(-ROAD_WIDTH / 2 + 0.1);
  addEdgeLine(ROAD_WIDTH / 2 - 0.1);

  const dashGeometry = new THREE.PlaneGeometry(0.15, 3);
  const dashMaterial = new THREE.MeshStandardMaterial({ color: 0xf2d24a });
  const dashGroup = new THREE.Group();
  for (let z = 4; z > -ROAD_LENGTH + 4; z -= 8) {
    const dash = new THREE.Mesh(dashGeometry, dashMaterial);
    dash.rotation.x = -Math.PI / 2;
    dash.position.set(0, 0.01, z);
    dashGroup.add(dash);
  }
  scene.add(dashGroup);

  // ---- Landscape: rolling painterly hills + hazy distant mountains --------
  function buildHill(color, radius) {
    const hill = new THREE.Mesh(
      new THREE.SphereGeometry(radius, 12, 8, 0, Math.PI * 2, 0, Math.PI / 2),
      new THREE.MeshToonMaterial({ color, gradientMap: toonGradient })
    );
    addOutline(hill, { color: OUTLINE_COLOR, thickness: 0.035 });
    return hill;
  }

  const hillColors = [0x8fae6c, 0x7c9c5e, 0x9dbb7a, 0x6f8f56];
  const hillsGroup = new THREE.Group();
  for (let i = 0; i < 26; i++) {
    const side = i % 2 === 0 ? -1 : 1;
    const radius = 14 + Math.random() * 22;
    const hill = buildHill(hillColors[i % hillColors.length], radius);
    hill.position.set(
      side * (ROAD_WIDTH / 2 + 30 + Math.random() * 80),
      -radius * 0.3,
      -Math.random() * ROAD_LENGTH + 60
    );
    hill.receiveShadow = true;
    hillsGroup.add(hill);
  }
  scene.add(hillsGroup);

  const mountainMat = new THREE.MeshToonMaterial({ color: 0x9aa3c2, gradientMap: toonGradient });
  const mountainsGroup = new THREE.Group();
  for (let i = 0; i < 8; i++) {
    const side = i % 2 === 0 ? -1 : 1;
    const height = 60 + Math.random() * 50;
    const mountain = new THREE.Mesh(new THREE.ConeGeometry(40 + Math.random() * 30, height, 4), mountainMat);
    mountain.position.set(side * (140 + Math.random() * 100), height / 2 - 6, -Math.random() * ROAD_LENGTH);
    mountain.rotation.y = Math.random() * Math.PI;
    addOutline(mountain, { color: OUTLINE_COLOR, thickness: 0.03 });
    mountainsGroup.add(mountain);
  }
  scene.add(mountainsGroup);

  // ---- Trees lining the road: bold, chunky, outlined canopies -------------
  const treeBaseColors = [0x5f7a4a, 0x6b8752, 0x557046];
  const treeHighlightColors = [0xa9bd7e, 0xb8c98f];

  function buildTree() {
    const tree = new THREE.Group();
    const trunkMat = new THREE.MeshToonMaterial({ color: 0x8a6a48, gradientMap: toonGradient });
    const baseMat = new THREE.MeshToonMaterial({
      color: treeBaseColors[Math.floor(Math.random() * treeBaseColors.length)],
      gradientMap: toonGradient,
    });
    const highlightMat = new THREE.MeshToonMaterial({
      color: treeHighlightColors[Math.floor(Math.random() * treeHighlightColors.length)],
      gradientMap: toonGradient,
    });

    const trunk = new THREE.Mesh(new THREE.CylinderGeometry(0.18, 0.26, 1.7, 8), trunkMat);
    trunk.position.y = 0.85;
    trunk.castShadow = true;
    addOutline(trunk, { color: OUTLINE_COLOR, thickness: 0.08 });
    tree.add(trunk);

    const leafGeo = new THREE.SphereGeometry(1, 8, 6);
    const mainCanopy = new THREE.Mesh(leafGeo, baseMat);
    mainCanopy.position.set(0, 2.0, 0);
    mainCanopy.scale.setScalar(1.3);
    mainCanopy.castShadow = true;
    addOutline(mainCanopy, { color: OUTLINE_COLOR, thickness: 0.05 });
    tree.add(mainCanopy);

    const accentPuffs = [
      [0.55, 1.65, 0.4, 0.75, baseMat],
      [0.2, 2.35, -0.2, 0.7, highlightMat],
    ];
    for (const [dx, dy, dz, s, mat] of accentPuffs) {
      const leaf = new THREE.Mesh(leafGeo, mat);
      leaf.position.set(dx, dy, dz);
      leaf.scale.setScalar(s);
      leaf.castShadow = true;
      tree.add(leaf);
    }
    return tree;
  }

  const treesGroup = new THREE.Group();
  const TREE_SPACING = 14;
  const TREE_OFFSET_MIN = ROAD_WIDTH / 2 + 3;
  const TREE_OFFSET_RANGE = 10;
  for (let z = 10; z > -ROAD_LENGTH + 10; z -= TREE_SPACING) {
    for (const side of [-1, 1]) {
      if (Math.random() < 0.15) continue;
      const tree = buildTree();
      const offset = TREE_OFFSET_MIN + Math.random() * TREE_OFFSET_RANGE;
      tree.position.set(side * offset, 0, z + (Math.random() - 0.5) * 5);
      tree.scale.setScalar(0.8 + Math.random() * 0.6);
      tree.rotation.y = Math.random() * Math.PI * 2;
      treesGroup.add(tree);
    }
  }
  scene.add(treesGroup);

  // ---- Car body: a two-shell silhouette (painted shell + glass cabin) ----
  // extruded from a hand-drawn side profile, instead of a face-bearing
  // blob — sits on wheels with headlights on the true front (-z, the
  // direction of travel).
  function buildCarBody(paintColor, glassColor) {
    const group = new THREE.Group();
    const L = 2.0; // half-length
    const CLEARANCE = 0.32;

    const lowerShape = new THREE.Shape();
    lowerShape.moveTo(-L, 0);
    lowerShape.lineTo(-L, 0.3);
    lowerShape.lineTo(-L * 0.65, 0.62);
    lowerShape.lineTo(L * 0.55, 0.62);
    lowerShape.lineTo(L * 0.85, 0.46);
    lowerShape.lineTo(L, 0.32);
    lowerShape.lineTo(L, 0);
    lowerShape.closePath();

    const lowerGeo = new THREE.ExtrudeGeometry(lowerShape, {
      depth: 1.9,
      bevelEnabled: true,
      bevelThickness: 0.05,
      bevelSize: 0.05,
      bevelSegments: 2,
      curveSegments: 6,
    });
    lowerGeo.translate(0, CLEARANCE, -0.95);
    const lowerMat = new THREE.MeshToonMaterial({ color: paintColor, gradientMap: toonGradient, side: THREE.DoubleSide });
    const lowerMesh = new THREE.Mesh(lowerGeo, lowerMat);
    lowerMesh.rotation.y = Math.PI / 2;
    lowerMesh.castShadow = true;
    addOutline(lowerMesh, { color: OUTLINE_COLOR, thickness: 0.05 });
    group.add(lowerMesh);

    // Cabin/glass shell overlaps a little into the lower shell at the
    // beltline (0.55 vs the lower shell's 0.62) so there is never a seam gap.
    const upperShape = new THREE.Shape();
    upperShape.moveTo(-L * 0.65, 0.55);
    upperShape.lineTo(-L * 0.55, 0.9);
    upperShape.quadraticCurveTo(-L * 0.25, 1.0, L * 0.05, 0.97);
    upperShape.lineTo(L * 0.42, 0.55);
    upperShape.closePath();

    const upperGeo = new THREE.ExtrudeGeometry(upperShape, {
      depth: 1.6,
      bevelEnabled: true,
      bevelThickness: 0.04,
      bevelSize: 0.04,
      bevelSegments: 2,
      curveSegments: 6,
    });
    upperGeo.translate(0, CLEARANCE, -0.8);
    const upperMat = new THREE.MeshToonMaterial({ color: glassColor, gradientMap: toonGradient, side: THREE.DoubleSide });
    const upperMesh = new THREE.Mesh(upperGeo, upperMat);
    upperMesh.rotation.y = Math.PI / 2;
    upperMesh.castShadow = true;
    addOutline(upperMesh, { color: OUTLINE_COLOR, thickness: 0.05 });
    group.add(upperMesh);

    return group;
  }

  function buildCar(bodyColor, cabinColor) {
    const car = new THREE.Group();
    car.add(buildCarBody(bodyColor, cabinColor));

    const wheelMat = new THREE.MeshToonMaterial({ color: 0x2b2b2b, gradientMap: toonGradient });
    const hubMat = new THREE.MeshToonMaterial({ color: 0xffffff, gradientMap: toonGradient });
    const headlightMat = new THREE.MeshToonMaterial({ color: 0xfff2b0, gradientMap: toonGradient });

    const wheelGeo = new THREE.CylinderGeometry(0.5, 0.5, 0.4, 20);
    const hubGeo = new THREE.CylinderGeometry(0.22, 0.22, 0.06, 16);
    const wheelOffsets = [
      [-1.0, 0.5, 1.35],
      [1.0, 0.5, 1.35],
      [-1.0, 0.5, -1.35],
      [1.0, 0.5, -1.35],
    ];
    for (const [x, y, z] of wheelOffsets) {
      const wheel = new THREE.Mesh(wheelGeo, wheelMat);
      wheel.rotation.z = Math.PI / 2;
      wheel.position.set(x, y, z);
      wheel.castShadow = true;
      car.add(wheel);

      const hub = new THREE.Mesh(hubGeo, hubMat);
      hub.rotation.z = Math.PI / 2;
      hub.position.set(x + Math.sign(x) * 0.22, y, z);
      car.add(hub);
    }

    const headlightGeo = new THREE.SphereGeometry(0.13, 10, 8);
    for (const x of [-0.55, 0.55]) {
      const headlight = new THREE.Mesh(headlightGeo, headlightMat);
      headlight.position.set(x, 0.55, -1.85);
      car.add(headlight);
    }

    return car;
  }

  const vehicleA = buildCar(0x4a8fc0, 0xd3e8ee); // ego vehicle
  const vehicleB = buildCar(0xd4703f, 0xf0dcc0); // relay vehicle — the reference's rust-orange tone
  const vehicleC = buildCar(0x9aa2b1, 0xe9ddc0); // occluded hazard vehicle

  vehicleA.position.set(LANE_RIGHT_X, 0, 0);
  vehicleB.position.set(LANE_LEFT_X, 0, -13);
  vehicleC.position.set(LANE_RIGHT_X, 0, -230);
  vehicleC.visible = false;

  scene.add(vehicleA, vehicleB, vehicleC);

  // ---- Mail sprite (envelope billboard) -----------------------------------
  const mailSprite = buildEnvelopeSprite();
  scene.add(mailSprite);

  // ---- Story state machine --------------------------------------------------
  // A virtual clock, not the wall clock: every timeline check below reads
  // `virtualElapsed`, which only advances by (real delta * the fixed SPEED *
  // the global pause factor). Pausing freezes it in place.
  const clock = new THREE.Clock();
  let virtualElapsed = 0;
  let frozen = false;
  let mailPhase = "idle"; // idle -> flying -> delivered
  let mailElapsed = 0;
  let completed = false;

  function updateCars(elapsed, dt) {
    vehicleA.position.z -= CAR_SPEED * dt;
    vehicleB.position.z -= CAR_SPEED * dt;

    const cutInT = smoothstep((elapsed - T_CUTIN_START) / (T_CUTIN_END - T_CUTIN_START));
    vehicleB.position.x = lerp(LANE_LEFT_X, LANE_RIGHT_X, cutInT);
    vehicleB.rotation.y = -Math.sin(cutInT * Math.PI) * 0.18;

    if (!vehicleC.visible && elapsed >= T_C_APPEAR) {
      vehicleC.visible = true;
    }
  }

  function updateMail(dt) {
    if (mailPhase === "idle") {
      const gap = vehicleC.visible ? vehicleB.position.z - vehicleC.position.z : Infinity;
      if (Math.abs(gap) <= MAIL_TRIGGER_DISTANCE) {
        mailPhase = "flying";
        mailElapsed = 0;
        mailSprite.visible = true;
      }
      return;
    }

    if (mailPhase === "flying") {
      mailElapsed += dt;
      const t = clamp01(mailElapsed / MAIL_FLIGHT_DURATION);
      const eased = smoothstep(t);

      const from = vehicleB.position;
      const to = vehicleA.position;
      mailSprite.position.set(
        lerp(from.x, to.x, eased),
        1.8 + Math.sin(eased * Math.PI) * 1.2,
        lerp(from.z, to.z, eased)
      );
      mailSprite.material.opacity = 1;

      if (t >= 1) {
        mailPhase = "delivered";
        mailSprite.visible = false;
        showBanner(BANNER_TEXT);
        frozen = true;
        if (!completed) {
          completed = true;
          if (onComplete) onComplete();
        }
      }
    }
  }

  function updateCamera() {
    const target = vehicleA.position;
    camera.position.set(target.x, target.y + 5.0, target.z + 11);
    camera.lookAt(target.x, target.y + 1.2, target.z - 8);
  }

  function update(pauseFactor = 1) {
    const rawDt = Math.min(clock.getDelta(), 0.05); // drain the real clock even while frozen/paused
    if (frozen) return;
    const dt = rawDt * SPEED * pauseFactor;
    virtualElapsed += dt;
    updateCars(virtualElapsed, dt);
    updateMail(dt);
    updateCamera();
  }

  // content/road.md has no `## ` subsections, so header.sections is empty —
  // the stepper's next()/prev() are no-ops for free, and reset() hides the
  // shared textbox (nothing to show).
  const stepper = createAnimationStepper(header.sections);

  function onEnter() {
    applyPageHeader(header);
    stepper.reset();
    // Restore this page's own banner state rather than inheriting whatever
    // the previously active page left the shared banner showing.
    if (completed) {
      showBanner(BANNER_TEXT);
    } else {
      hideBanner();
    }
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
    animationLabel: "Playing",
  };
}
