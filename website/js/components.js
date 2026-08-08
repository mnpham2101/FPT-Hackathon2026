/* KIS site — reusable component factories.
   Every visual component is created here and only here; pages and the
   mind map compose these, never hand-build markup. Each factory takes
   one props object sharing the common property set:
     label      — the visible text
     image      — icon id (assets/icons/<id>.svg) or a logo asset path
     href       — the page the component links to (opened in a new tab)
     style      — visual variant, mapped to a CSS modifier class
     animation  — { enter: 'pop'|'rise'|'fade'|'none', delay: ms } */

window.KIS = window.KIS || {};

var SVGNS = 'http://www.w3.org/2000/svg';

/* ---- hand-drawn outlines ----
   A component box's sketch border is a rounded-rect path whose points are
   pushed off the true outline by smooth noise, then stroked twice. The
   wobble is in the geometry, so every stroke stays continuous — displacing
   a rasterized 1.5px CSS border through an SVG turbulence filter (the
   previous approach) thinned it wherever the noise gradient was steep and
   tore it into visibly broken segments. */

/* xorshift32, seeded from a string so one pill's wobble never changes */
function hashSeed(str) {
  var h = 2166136261;
  for (var i = 0; i < str.length; i++) {
    h ^= str.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return h >>> 0;
}

/* Wrap-around smooth noise over t in [0,1): `count` random knots, cosine
   interpolated. Low knot count is what keeps the wobble a gentle wave
   rather than the per-pixel jitter that reads as a ragged line. */
function noiseLoop(seed, count) {
  var s = seed >>> 0 || 1;
  var knots = [];
  for (var i = 0; i < count; i++) {
    s ^= s << 13; s >>>= 0;
    s ^= s >>> 17;
    s ^= s << 5; s >>>= 0;
    knots.push(s / 4294967296 * 2 - 1);
  }
  return function (t) {
    var x = t * count;
    var i0 = Math.floor(x) % count;
    var f = x - Math.floor(x);
    var m = (1 - Math.cos(f * Math.PI)) / 2;
    return knots[i0] * (1 - m) + knots[(i0 + 1) % count] * m;
  };
}

/* Point and outward normal at arc-length d along a w×h rounded rect of
   corner radius r, walking clockwise from the top edge. */
function outlineAt(d, w, h, r) {
  var sx = Math.max(w - 2 * r, 0);
  var sy = Math.max(h - 2 * r, 0);
  var arc = Math.PI * r / 2;
  var u;

  if ((u = d) < sx) return [r + u, 0, 0, -1];
  if ((u = d - sx) < arc) {
    var a1 = -Math.PI / 2 + u / r;
    return [w - r + r * Math.cos(a1), r + r * Math.sin(a1), Math.cos(a1), Math.sin(a1)];
  }
  if ((u = d - sx - arc) < sy) return [w, r + u, 1, 0];
  if ((u = d - sx - arc - sy) < arc) {
    var a2 = u / r;
    return [w - r + r * Math.cos(a2), h - r + r * Math.sin(a2), Math.cos(a2), Math.sin(a2)];
  }
  if ((u = d - sx - 2 * arc - sy) < sx) return [w - r - u, h, 0, 1];
  if ((u = d - 2 * sx - 2 * arc - sy) < arc) {
    var a3 = Math.PI / 2 + u / r;
    return [r + r * Math.cos(a3), h - r + r * Math.sin(a3), Math.cos(a3), Math.sin(a3)];
  }
  if ((u = d - 2 * sx - 3 * arc - sy) < sy) return [0, h - r - u, -1, 0];
  var a4 = Math.PI + (d - 2 * sx - 3 * arc - 2 * sy) / r;
  return [r + r * Math.cos(a4), r + r * Math.sin(a4), Math.cos(a4), Math.sin(a4)];
}

/* Path data for one wobbled outline.

   The displacement is outward-only — [0, amp] rather than [-amp, +amp].
   A path allowed inward exposes a crescent of the box's own clean
   rounded-rect background outside the stroke, and that ruled arc beside
   a hand-drawn line is more conspicuous than the wobble is. Staying on
   or outside the true outline keeps the stroke covering that edge the
   whole way round. */
function sketchPath(w, h, radius, seed, amp) {
  var r = Math.max(Math.min(radius, Math.min(w, h) / 2), 1);
  var len = 2 * Math.max(w - 2 * r, 0) + 2 * Math.max(h - 2 * r, 0) + 2 * Math.PI * r;
  var n = Math.max(40, Math.round(len / 7));
  var noise = noiseLoop(seed, 11);
  var pts = [];

  for (var i = 0; i < n; i++) {
    var t = i / n;
    var p = outlineAt(t * len, w, h, r);
    var o = (noise(t) + 1) / 2 * amp;
    pts.push([p[0] + p[2] * o, p[1] + p[3] * o]);
  }

  /* Quadratic through every sampled point, each segment ending at the
     midpoint of the next pair — the standard closed-loop smoothing, so
     the corners stay curves rather than a visible facet chain. */
  function mid(a, b) {
    return ((a[0] + b[0]) / 2).toFixed(2) + ',' + ((a[1] + b[1]) / 2).toFixed(2);
  }
  var d = 'M' + mid(pts[n - 1], pts[0]);
  for (var j = 0; j < n; j++) {
    var cur = pts[j];
    d += 'Q' + cur[0].toFixed(2) + ',' + cur[1].toFixed(2) + ' ' + mid(cur, pts[(j + 1) % n]);
  }
  return d + 'Z';
}

/* Attach a two-stroke sketch outline to a component box. The paths track
   the element's measured size, so a relabelled or reflowed pill redraws
   itself; the CSS decides whether the outline is visible (sketch theme
   only) and what it is stroked with. */
window.KIS.attachSketchOutline = function (el, seed) {
  var svg = document.createElementNS(SVGNS, 'svg');
  svg.setAttribute('class', 'SketchOutline');
  svg.setAttribute('aria-hidden', 'true');

  var stroke = document.createElementNS(SVGNS, 'path');
  var echo = document.createElementNS(SVGNS, 'path');
  echo.setAttribute('class', 'SketchOutline__echo');
  svg.append(stroke, echo);
  el.prepend(svg);

  function draw() {
    var w = el.offsetWidth, h = el.offsetHeight;
    if (!w || !h) return;
    var cs = getComputedStyle(el);
    var radius = parseFloat(cs.borderTopLeftRadius) || 14;
    var amp = parseFloat(cs.getPropertyValue('--sketch-amp')) || 1.6;
    svg.setAttribute('viewBox', '0 0 ' + w + ' ' + h);
    svg.setAttribute('width', w);
    svg.setAttribute('height', h);
    /* Both strokes share a seed, so the echo tracks the main line and
       drifts off it by the difference in amplitude — a shape drawn over
       twice. Independent seeds made the two wander apart into a smudge. */
    stroke.setAttribute('d', sketchPath(w, h, radius, seed, amp));
    echo.setAttribute('d', sketchPath(w, h, radius, seed, amp * 0.4));
  }

  draw();
  if (window.ResizeObserver) new ResizeObserver(draw).observe(el);
};

/* Apply the shared animation prop to any component element */
window.KIS.applyEnter = function (el, animation) {
  var anim = animation || {};
  if (anim.enter && anim.enter !== 'none') {
    el.classList.add('is-enter-' + anim.enter);
    el.style.setProperty('--enter-delay', (anim.delay || 0) + 'ms');
  }
};

/* PageLink — an icon + text node representing a navigable page.

   Input props (all optional except label/image):
     id          — written to the element's data-id, for callers that
                   need to look this pill up later
     label       — visible text
     image       — icon id, passed to iconMaskUrl()
     href        — the page this pill represents
     style       — 'node' (default) or 'card'; selects the CSS modifier
                   class and this factory's own click behavior below
     onActivate  — function(props), called on every click if given
     animation   — passed through to applyEnter()

   Behavior:
     - style 'card' with an href: this factory opens that href in a new
       tab on click, on its own.
     - style 'node', or no href: this factory does not navigate by
       itself. If onActivate was given, it still runs on click; deciding
       what that click means is entirely the caller's business. */
window.KIS.createPageLink = function (props) {
  var el = document.createElement('div');
  el.className = 'PageLink PageLink--' + (props.style || 'node');
  el.dataset.id = props.id || '';

  var icon = document.createElement('span');
  icon.className = 'PageLink__icon';
  icon.style.setProperty('--icon-url', window.KIS.iconMaskUrl(props.image));

  var label = document.createElement('span');
  label.className = 'PageLink__label';
  label.textContent = props.label;

  el.append(icon, label);
  window.KIS.attachSketchOutline(el, hashSeed(props.id || props.label || 'kis'));

  /* 'card' pills navigate to href on their own; any other style leaves
     that decision to onActivate, below, if the caller passed one. */
  if ((props.style || 'node') === 'card' && props.href) {
    el.addEventListener('click', function () {
      window.open(window.KIS.assetPath(props.href), '_blank', 'noopener');
    });
  }

  if (props.onActivate) {
    el.addEventListener('click', function () { props.onActivate(props); });
  }

  window.KIS.applyEnter(el, props.animation);
  return el;
};

/* LogoFrame — the KIS logo inside the large round-edged rectangle, wrapped
   in a WarningRing (the sweeping IVI-warning-colored outer ring). The
   WarningRing is the positioned/animated element mindmap.js places. */
window.KIS.createLogoFrame = function (props) {
  var ring = document.createElement('div');
  ring.className = 'WarningRing';

  var el = document.createElement('div');
  el.className = 'LogoFrame';

  var img = document.createElement('img');
  img.className = 'LogoFrame__img';
  img.dataset.kisLogo = '';
  img.src = window.KIS.assetPath((props && props.image) || window.KIS.theme.get().assets.logo);
  img.alt = 'KIS — Keep It Simple';
  el.append(img);

  var glob = document.createElement('div');
  glob.className = 'WarningRing__glob';
  glob.setAttribute('aria-hidden', 'true');

  ring.append(el, glob);

  window.KIS.applyEnter(ring, props && props.animation);
  return ring;
};

/* A heading's own text, without the section permalink a generator may have
   appended to it. Reading h.textContent directly drags that glyph into the
   index, so every entry ends in a stray ¶. Cloning and stripping, rather
   than trimming the character off the end, keeps this correct whatever the
   permalink is rendered as. */
function headingText(h) {
  var clone = h.cloneNode(true);
  clone.querySelectorAll('.headerlink, .toclink, [aria-hidden="true"]').forEach(function (el) {
    el.remove();
  });
  return clone.textContent.trim();
}

/* TableOfContents — a nested index of the headings inside some region.

   Input props (all optional except scope):
     scope     — the element whose headings are indexed
     label     — the heading above the list (default 'On this page')
     minLevel  — shallowest heading indexed (default 2, i.e. h2)
     maxLevel  — deepest heading indexed (default 6, i.e. every section)
     minItems  — below this many headings the factory returns null, since a
                 one-line index costs more attention than it saves
     spy       — highlight the entry for the section currently on screen
                 (default true)

   Reads the DOM rather than taking a list of headings, so any page can hand
   it a region and get an index: a generated document body, a hand-written
   page, or a region another component built. Headings with no id are
   skipped — an entry that cannot be linked to is not an entry — so a
   generator feeding this must emit ids on every heading. */
window.KIS.createTableOfContents = function (props) {
  var p = props || {};
  var scope = p.scope;
  if (!scope) return null;

  var min = p.minLevel || 2;
  var max = p.maxLevel || 6;
  var sel = [];
  for (var lv = min; lv <= max; lv++) sel.push('h' + lv);

  var heads = Array.prototype.filter.call(
    scope.querySelectorAll(sel.join(',')),
    function (h) { return h.id; }
  );
  if (heads.length < (p.minItems == null ? 2 : p.minItems)) return null;

  var nav = document.createElement('nav');
  nav.className = 'TableOfContents';
  nav.setAttribute('aria-label', p.label || 'On this page');

  var title = document.createElement('div');
  title.className = 'TableOfContents__title';
  title.textContent = p.label || 'On this page';
  nav.append(title);

  var list = document.createElement('ul');
  list.className = 'TableOfContents__list';
  var links = {};

  heads.forEach(function (h) {
    var depth = Math.min(parseInt(h.tagName.slice(1), 10) - min, 3);
    var li = document.createElement('li');
    li.className = 'TableOfContents__item TableOfContents__item--l' + depth;

    var a = document.createElement('a');
    a.className = 'TableOfContents__link';
    a.href = '#' + h.id;
    a.textContent = headingText(h);
    li.append(a);
    list.append(li);
    links[h.id] = a;
  });

  nav.append(list);

  /* Scroll spy. The observer reports which headings are inside the band, not
     which one the reader is under — a heading scrolled off the top stops
     intersecting while its section is still what fills the screen. So track
     the set of headings above the fold and mark the last of them; that stays
     correct through a section taller than the viewport, which the naive
     "last one that intersected" version does not. */
  if (p.spy !== false && window.IntersectionObserver) {
    var above = {};
    var mark = function () {
      var current = null;
      heads.forEach(function (h) { if (above[h.id]) current = h.id; });
      if (!current) current = heads[0].id;
      Object.keys(links).forEach(function (id) {
        links[id].classList.toggle('is-active', id === current);
      });
    };
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        above[e.target.id] = e.boundingClientRect.top < 0 || e.isIntersecting;
      });
      mark();
    }, { rootMargin: '0px 0px -70% 0px', threshold: 0 });
    heads.forEach(function (h) { io.observe(h); });
  }

  window.KIS.applyEnter(nav, p.animation);
  return nav;
};

/* ThemeSwitch — cycles through KIS.THEMES */
window.KIS.createThemeSwitch = function () {
  var btn = document.createElement('button');
  btn.className = 'ThemeSwitch';
  btn.type = 'button';

  function refresh() {
    btn.textContent = 'Theme: ' + window.KIS.theme.get().label;
  }
  btn.addEventListener('click', function () { window.KIS.theme.next(); });
  document.addEventListener('kis:theme', refresh);
  refresh();
  return btn;
};

/* Mount the fixed top bar carrying the theme switch */
window.KIS.mountTopBar = function () {
  var bar = document.createElement('div');
  bar.className = 'TopBar';
  bar.append(window.KIS.createThemeSwitch());
  document.body.append(bar);
};
