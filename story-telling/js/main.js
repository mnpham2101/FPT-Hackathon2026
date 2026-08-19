import * as THREE from "three";

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

// ---- Renderer / scene / camera -----------------------------------------
const canvas = document.getElementById("scene");
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.shadowMap.enabled = true;

const SKY_COLOR = 0x8ecbff;

const scene = new THREE.Scene();
scene.background = new THREE.Color(SKY_COLOR);
scene.fog = new THREE.Fog(0xbfe3ff, 110, 420);

const camera = new THREE.PerspectiveCamera(
  55,
  window.innerWidth / window.innerHeight,
  0.1,
  1200
);

window.addEventListener("resize", () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});

// ---- Lighting (bright, clear-sky daytime) --------------------------------
scene.add(new THREE.HemisphereLight(0xbfe3ff, 0x5aa955, 0.75));

const sun = new THREE.DirectionalLight(0xfff4da, 1.25);
sun.position.set(-60, 90, 40);
sun.castShadow = true;
sun.shadow.mapSize.set(2048, 2048);
sun.shadow.camera.left = -60;
sun.shadow.camera.right = 60;
sun.shadow.camera.top = 60;
sun.shadow.camera.bottom = -60;
sun.shadow.camera.far = 220;
scene.add(sun);

scene.add(new THREE.AmbientLight(0xffffff, 0.25));

// ---- Sky dressing: sun disc + drifting clouds ----------------------------
const sunMesh = new THREE.Mesh(
  new THREE.SphereGeometry(9, 20, 20),
  new THREE.MeshBasicMaterial({ color: 0xfff0b8 })
);
sunMesh.position.set(-150, 95, -220);
scene.add(sunMesh);

function buildCloud() {
  const cloud = new THREE.Group();
  const puffMat = new THREE.MeshStandardMaterial({
    color: 0xffffff,
    roughness: 1,
    flatShading: true,
  });
  const puffCount = 4 + Math.floor(Math.random() * 3);
  for (let i = 0; i < puffCount; i++) {
    const puff = new THREE.Mesh(
      new THREE.SphereGeometry(1 + Math.random() * 0.6, 8, 6),
      puffMat
    );
    puff.position.set(
      (Math.random() - 0.5) * 4,
      (Math.random() - 0.5) * 0.7,
      (Math.random() - 0.5) * 2
    );
    puff.scale.y = 0.6;
    cloud.add(puff);
  }
  return cloud;
}

const cloudsGroup = new THREE.Group();
for (let i = 0; i < 16; i++) {
  const cloud = buildCloud();
  cloud.scale.setScalar(2 + Math.random() * 2.5);
  cloud.position.set(
    (Math.random() - 0.5) * 300,
    36 + Math.random() * 24,
    -Math.random() * ROAD_LENGTH + 80
  );
  cloudsGroup.add(cloud);
}
scene.add(cloudsGroup);

// ---- Ground + road ----------------------------------------------------
const grass = new THREE.Mesh(
  new THREE.PlaneGeometry(700, ROAD_LENGTH + 200),
  new THREE.MeshStandardMaterial({ color: 0x5fb257, roughness: 1 })
);
grass.rotation.x = -Math.PI / 2;
grass.position.z = -ROAD_LENGTH / 2 + 100;
grass.position.y = -0.02;
grass.receiveShadow = true;
scene.add(grass);

const road = new THREE.Mesh(
  new THREE.PlaneGeometry(ROAD_WIDTH, ROAD_LENGTH),
  new THREE.MeshStandardMaterial({ color: 0x454b54, roughness: 0.95 })
);
road.rotation.x = -Math.PI / 2;
road.position.z = -ROAD_LENGTH / 2 + 100;
road.receiveShadow = true;
scene.add(road);

// Lane edge lines (solid)
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

// Center dashed lane divider
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

// ---- Landscape: rolling hills + distant mountains ------------------------
function buildHill(color, radius) {
  return new THREE.Mesh(
    new THREE.SphereGeometry(radius, 12, 8, 0, Math.PI * 2, 0, Math.PI / 2),
    new THREE.MeshStandardMaterial({ color, roughness: 1, flatShading: true })
  );
}

const hillColors = [0x5aa955, 0x4f9a4a, 0x6ab35e];
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

const mountainMat = new THREE.MeshStandardMaterial({
  color: 0x8391bd,
  roughness: 1,
  flatShading: true,
});
const mountainsGroup = new THREE.Group();
for (let i = 0; i < 8; i++) {
  const side = i % 2 === 0 ? -1 : 1;
  const height = 60 + Math.random() * 50;
  const mountain = new THREE.Mesh(
    new THREE.ConeGeometry(40 + Math.random() * 30, height, 4),
    mountainMat
  );
  mountain.position.set(
    side * (140 + Math.random() * 100),
    height / 2 - 6,
    -Math.random() * ROAD_LENGTH
  );
  mountain.rotation.y = Math.random() * Math.PI;
  mountainsGroup.add(mountain);
}
scene.add(mountainsGroup);

// ---- Trees lining the road ------------------------------------------------
function buildTree() {
  const tree = new THREE.Group();
  const trunkMat = new THREE.MeshStandardMaterial({ color: 0x7a5033, roughness: 1 });
  const leafMat = new THREE.MeshStandardMaterial({
    color: 0x3f8f46,
    roughness: 0.9,
    flatShading: true,
  });

  const trunk = new THREE.Mesh(new THREE.CylinderGeometry(0.18, 0.24, 1.6, 8), trunkMat);
  trunk.position.y = 0.8;
  trunk.castShadow = true;
  tree.add(trunk);

  const leafGeo = new THREE.SphereGeometry(1, 8, 6);
  const puffs = [
    [0, 1.9, 0, 1.1],
    [0.5, 1.5, 0.3, 0.75],
    [-0.5, 1.6, -0.3, 0.8],
  ];
  for (const [dx, dy, dz, s] of puffs) {
    const leaf = new THREE.Mesh(leafGeo, leafMat);
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
    if (Math.random() < 0.15) continue; // gaps so the tree line reads as natural, not a fence
    const tree = buildTree();
    const offset = TREE_OFFSET_MIN + Math.random() * TREE_OFFSET_RANGE;
    tree.position.set(side * offset, 0, z + (Math.random() - 0.5) * 5);
    tree.scale.setScalar(0.8 + Math.random() * 0.6);
    tree.rotation.y = Math.random() * Math.PI * 2;
    treesGroup.add(tree);
  }
}
scene.add(treesGroup);

// ---- Toon shading gradient (kid-cartoon cel-shaded look) ------------------
function buildToonGradient() {
  const shadeCanvas = document.createElement("canvas");
  shadeCanvas.width = 4;
  shadeCanvas.height = 1;
  const ctx = shadeCanvas.getContext("2d");
  ["#4d4d4d", "#8a8a8a", "#c2c2c2", "#ffffff"].forEach((c, i) => {
    ctx.fillStyle = c;
    ctx.fillRect(i, 0, 1, 1);
  });
  const texture = new THREE.CanvasTexture(shadeCanvas);
  texture.minFilter = THREE.NearestFilter;
  texture.magFilter = THREE.NearestFilter;
  return texture;
}

const toonGradient = buildToonGradient();

// ---- Cartoon car factory --------------------------------------------------
// The camera trails the cars from behind, so the "face" (eyes + smile) sits
// on the +z side, the side actually visible to the viewer; small headlight
// bumps mark the -z side, the true direction of travel.
function buildCar(bodyColor, cabinColor) {
  const car = new THREE.Group();

  const bodyMat = new THREE.MeshToonMaterial({ color: bodyColor, gradientMap: toonGradient });
  const cabinMat = new THREE.MeshToonMaterial({ color: cabinColor, gradientMap: toonGradient });
  const wheelMat = new THREE.MeshToonMaterial({ color: 0x2b2b2b, gradientMap: toonGradient });
  const hubMat = new THREE.MeshToonMaterial({ color: 0xffffff, gradientMap: toonGradient });
  const eyeWhiteMat = new THREE.MeshToonMaterial({ color: 0xffffff, gradientMap: toonGradient });
  const eyePupilMat = new THREE.MeshBasicMaterial({ color: 0x22262b });
  const mouthMat = new THREE.MeshToonMaterial({ color: 0x22262b, gradientMap: toonGradient });
  const headlightMat = new THREE.MeshToonMaterial({ color: 0xfff2b0, gradientMap: toonGradient });

  // Rounded jellybean body.
  const body = new THREE.Mesh(new THREE.SphereGeometry(1, 24, 16), bodyMat);
  body.scale.set(1.05, 0.72, 1.9);
  body.position.y = 0.72;
  body.castShadow = true;
  car.add(body);

  // Cabin bump / roof.
  const cabin = new THREE.Mesh(new THREE.SphereGeometry(0.75, 20, 14), cabinMat);
  cabin.scale.set(0.95, 0.6, 1.0);
  cabin.position.set(0, 1.28, 0);
  cabin.castShadow = true;
  car.add(cabin);

  // Big cartoon wheels with white hubcaps.
  const wheelGeo = new THREE.CylinderGeometry(0.5, 0.5, 0.4, 20);
  const hubGeo = new THREE.CylinderGeometry(0.22, 0.22, 0.06, 16);
  const wheelOffsets = [
    [-1.05, 0.5, 1.3],
    [1.05, 0.5, 1.3],
    [-1.05, 0.5, -1.3],
    [1.05, 0.5, -1.3],
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

  // Friendly face on the rear (camera-facing) side.
  const eyeGeo = new THREE.SphereGeometry(0.2, 14, 10);
  const pupilGeo = new THREE.SphereGeometry(0.1, 10, 8);
  for (const ex of [-0.42, 0.42]) {
    const eye = new THREE.Mesh(eyeGeo, eyeWhiteMat);
    eye.position.set(ex, 0.95, 1.62);
    car.add(eye);

    const pupil = new THREE.Mesh(pupilGeo, eyePupilMat);
    pupil.position.set(ex, 0.95, 1.75);
    car.add(pupil);
  }

  const mouth = new THREE.Mesh(
    new THREE.TorusGeometry(0.26, 0.045, 8, 16, Math.PI),
    mouthMat
  );
  mouth.rotation.z = Math.PI; // flips the arc's top half to the bottom, tracing a smile
  mouth.position.set(0, 0.58, 1.68);
  car.add(mouth);

  // Small round headlights on the true front (direction of travel).
  const headlightGeo = new THREE.SphereGeometry(0.14, 10, 8);
  for (const x of [-0.6, 0.6]) {
    const headlight = new THREE.Mesh(headlightGeo, headlightMat);
    headlight.position.set(x, 0.6, -1.7);
    car.add(headlight);
  }

  return car;
}

const vehicleA = buildCar(0x3d8bff, 0xcdeaff); // ego vehicle
const vehicleB = buildCar(0xff7a3d, 0xffe6c2); // relay vehicle
const vehicleC = buildCar(0x9aa2b1, 0xf2e6a8); // occluded hazard vehicle

vehicleA.position.set(LANE_RIGHT_X, 0, 0);
vehicleB.position.set(LANE_LEFT_X, 0, -13);
vehicleC.position.set(LANE_RIGHT_X, 0, -230);
vehicleC.visible = false;

scene.add(vehicleA, vehicleB, vehicleC);

// ---- Mail sprite (envelope billboard) -----------------------------------
function buildEnvelopeSprite() {
  const size = 128;
  const canvasTex = document.createElement("canvas");
  canvasTex.width = size;
  canvasTex.height = size;
  const ctx = canvasTex.getContext("2d");
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

  const texture = new THREE.CanvasTexture(canvasTex);
  const material = new THREE.SpriteMaterial({ map: texture, depthTest: false });
  const sprite = new THREE.Sprite(material);
  sprite.scale.set(1.4, 1.4, 1);
  sprite.visible = false;
  sprite.renderOrder = 999;
  return sprite;
}

const mailSprite = buildEnvelopeSprite();
scene.add(mailSprite);

// ---- HUD elements ---------------------------------------------------------
const mailBanner = document.getElementById("mail-banner");
mailBanner.classList.remove("hidden");

// ---- Story state machine --------------------------------------------------
const clock = new THREE.Clock();

let frozen = false;
let mailPhase = "idle"; // idle -> flying -> delivered
let mailElapsed = 0;

function lerp(a, b, t) {
  return a + (b - a) * t;
}

function clamp01(t) {
  return Math.max(0, Math.min(1, t));
}

function smoothstep(t) {
  const c = clamp01(t);
  return c * c * (3 - 2 * c);
}

function updateCars(elapsed, dt) {
  // Both A and B drive forward at a constant world speed. B starts slightly
  // ahead in z and merges from the left lane into A's lane between
  // T_CUTIN_START and T_CUTIN_END, ending up directly in front of A.
  vehicleA.position.z -= CAR_SPEED * dt;
  vehicleB.position.z -= CAR_SPEED * dt;

  const cutInT = smoothstep((elapsed - T_CUTIN_START) / (T_CUTIN_END - T_CUTIN_START));
  vehicleB.position.x = lerp(LANE_LEFT_X, LANE_RIGHT_X, cutInT);
  vehicleB.rotation.y = -Math.sin(cutInT * Math.PI) * 0.18;

  // Vehicle C appears far down A's own lane once the story reaches its cue —
  // it never enters B's or A's lane change, it is simply revealed ahead.
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
      mailBanner.classList.add("visible");
      frozen = true; // the animation stops on message delivery, per the story beat
    }
  }
}

function updateCamera() {
  const target = vehicleA.position;
  camera.position.set(target.x, target.y + 5.0, target.z + 11);
  camera.lookAt(target.x, target.y + 1.2, target.z - 8);
}

function animate() {
  requestAnimationFrame(animate);
  const dt = Math.min(clock.getDelta(), 0.05);
  const elapsed = clock.getElapsedTime();

  if (!frozen) {
    updateCars(elapsed, dt);
    updateMail(dt);
    updateCamera();
  }

  renderer.render(scene, camera);
}

animate();
