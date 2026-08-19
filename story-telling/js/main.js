import * as THREE from "three";

// ---- Road geometry constants -------------------------------------------
const LANE_WIDTH = 3.6;
const LANE_RIGHT_X = LANE_WIDTH / 2;
const LANE_LEFT_X = -LANE_WIDTH / 2;
const ROAD_WIDTH = LANE_WIDTH * 2 + 1.0;
const ROAD_LENGTH = 1400;

// ---- Story timeline (seconds) ------------------------------------------
const T_CUTIN_START = 3.0;
const T_CUTIN_END = 5.5;
const T_C_APPEAR = 9.0;
const MAIL_TRIGGER_DISTANCE = 34; // B-to-C gap that fires the V2X message
const MAIL_FLIGHT_DURATION = 1.6;

const CAR_SPEED = 11; // units/sec, world moves toward the camera (+z decreases)

// ---- Renderer / scene / camera -----------------------------------------
const canvas = document.getElementById("scene");
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.shadowMap.enabled = true;

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x0a0e14);
scene.fog = new THREE.Fog(0x0a0e14, 60, 260);

const camera = new THREE.PerspectiveCamera(
  55,
  window.innerWidth / window.innerHeight,
  0.1,
  1000
);

window.addEventListener("resize", () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});

// ---- Lighting -------------------------------------------------------------
scene.add(new THREE.HemisphereLight(0x8fb0ff, 0x1a1a1a, 0.6));

const sun = new THREE.DirectionalLight(0xffffff, 1.1);
sun.position.set(-40, 60, 30);
sun.castShadow = true;
sun.shadow.mapSize.set(2048, 2048);
sun.shadow.camera.left = -60;
sun.shadow.camera.right = 60;
sun.shadow.camera.top = 60;
sun.shadow.camera.bottom = -60;
sun.shadow.camera.far = 200;
scene.add(sun);

// ---- Ground + road ----------------------------------------------------
const grass = new THREE.Mesh(
  new THREE.PlaneGeometry(400, ROAD_LENGTH + 200),
  new THREE.MeshStandardMaterial({ color: 0x14261b, roughness: 1 })
);
grass.rotation.x = -Math.PI / 2;
grass.position.z = -ROAD_LENGTH / 2 + 100;
grass.position.y = -0.02;
grass.receiveShadow = true;
scene.add(grass);

const road = new THREE.Mesh(
  new THREE.PlaneGeometry(ROAD_WIDTH, ROAD_LENGTH),
  new THREE.MeshStandardMaterial({ color: 0x2b2f36, roughness: 0.95 })
);
road.rotation.x = -Math.PI / 2;
road.position.z = -ROAD_LENGTH / 2 + 100;
road.receiveShadow = true;
scene.add(road);

// Lane edge lines (solid)
function addEdgeLine(x) {
  const line = new THREE.Mesh(
    new THREE.PlaneGeometry(0.15, ROAD_LENGTH),
    new THREE.MeshStandardMaterial({ color: 0xd8dee8 })
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

// ---- Car factory --------------------------------------------------------
function buildCar(bodyColor) {
  const car = new THREE.Group();

  const bodyMat = new THREE.MeshStandardMaterial({
    color: bodyColor,
    roughness: 0.4,
    metalness: 0.3,
  });
  const cabinMat = new THREE.MeshStandardMaterial({
    color: 0x1a1d22,
    roughness: 0.3,
    metalness: 0.1,
  });
  const wheelMat = new THREE.MeshStandardMaterial({
    color: 0x101214,
    roughness: 0.9,
  });

  const body = new THREE.Mesh(new THREE.BoxGeometry(1.8, 0.6, 4.0), bodyMat);
  body.position.y = 0.55;
  body.castShadow = true;
  car.add(body);

  const cabin = new THREE.Mesh(
    new THREE.BoxGeometry(1.5, 0.55, 2.0),
    cabinMat
  );
  cabin.position.set(0, 1.05, -0.2);
  cabin.castShadow = true;
  car.add(cabin);

  const wheelGeo = new THREE.CylinderGeometry(0.38, 0.38, 0.32, 16);
  const wheelOffsets = [
    [-0.95, 0.38, 1.3],
    [0.95, 0.38, 1.3],
    [-0.95, 0.38, -1.3],
    [0.95, 0.38, -1.3],
  ];
  for (const [x, y, z] of wheelOffsets) {
    const wheel = new THREE.Mesh(wheelGeo, wheelMat);
    wheel.rotation.z = Math.PI / 2;
    wheel.position.set(x, y, z);
    wheel.castShadow = true;
    car.add(wheel);
  }

  // Tail lights, used later as the mail-message launch point on B.
  const lightMat = new THREE.MeshStandardMaterial({
    color: 0xff3b3b,
    emissive: 0x7a0000,
  });
  for (const x of [-0.7, 0.7]) {
    const light = new THREE.Mesh(new THREE.BoxGeometry(0.25, 0.15, 0.05), lightMat);
    light.position.set(x, 0.6, 2.02);
    car.add(light);
  }

  return car;
}

const vehicleA = buildCar(0x3d8bff); // ego vehicle
const vehicleB = buildCar(0xff5a3d); // relay vehicle
const vehicleC = buildCar(0x8f98a8); // occluded hazard vehicle

vehicleA.position.set(LANE_RIGHT_X, 0, 0);
vehicleB.position.set(LANE_LEFT_X, 0, -3);
vehicleC.position.set(LANE_RIGHT_X, 0, -170);
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
  camera.position.set(target.x, target.y + 4.2, target.z + 9.5);
  camera.lookAt(target.x, target.y + 1.0, target.z - 6);
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
