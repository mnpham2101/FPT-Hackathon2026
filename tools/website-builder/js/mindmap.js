/* KIS site — the homepage mind map.
   Renders the KIS LogoFrame and the four root nodes (no connector arrows
   to the logo — the WarningRing already marks that relationship), and
   reproduces the concept page's behaviour from there down: activating a
   node with children draws the connecting arrows and reveals the child
   nodes (animated per the active theme), activating again collapses
   them; activating a leaf node opens its page in a new tab. */

(function () {
  var SVGNS = 'http://www.w3.org/2000/svg';
  var GAP = 6; // px between a connector end and the node's border

  var canvas = document.getElementById('canvas');
  var svg = document.getElementById('connectors');
  var SITE = window.KIS.SITE;

  var els = {};      // node id -> mounted element
  var lines = {};    // "parent->child" -> <line>
  var expanded = new Set();
  var reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  var RING_GAP = 0; // px between WarningRing's edge and a root node's edge

  /* ---- mounting ---- */

  function place(el, pos) {
    el.style.left = pos[0] + '%';
    el.style.top = pos[1] + '%';
  }

  function ringRadius() {
    return parseFloat(getComputedStyle(els.kis).getPropertyValue('--ring-radius')) || 0;
  }

  /* Positions a direct child of 'kis' at one of WarningRing's four
     corners, RING_GAP outside the ring's actual *curved* edge — not its
     sharp bounding-box corner. Each corner's stroke is a quarter-circle
     of radius r, centered r-in from the box corner on each axis; placing
     the card RING_GAP beyond that circle (along the same diagonal the
     card already sits on) makes the gap-to-curve exactly RING_GAP
     regardless of r. The previous version measured from the sharp
     corner, so a card could look ~r*(sqrt(2)-1) px farther from the
     ring than RING_GAP actually said, since that's how far the curve
     recedes from the corner point the math was aiming at.
     Still computed from the ring's actual measured box (and its
     --ring-radius), not a hand-tuned percentage, so it stays correct if
     the ring is ever resized. */
  function placeOnRing(el, node) {
    var ring = rectOf(els.kis);
    var cx0 = ring.x + ring.w / 2, cy0 = ring.y + ring.h / 2;
    var r = ringRadius();
    var diag = (r + RING_GAP) / Math.SQRT2;
    var l = ring.w / 2 - r + diag + el.offsetWidth / 2;
    var h = ring.h / 2 - r + diag + el.offsetHeight / 2;
    var signX = (node.corner === 'tr' || node.corner === 'br') ? 1 : -1;
    var signY = (node.corner === 'bl' || node.corner === 'br') ? 1 : -1;
    el.style.left = (cx0 + signX * l) + 'px';
    el.style.top = (cy0 + signY * h) + 'px';
  }

  function positionRootNodes() {
    SITE.childrenOf('kis').forEach(function (node) {
      if (node.corner && els[node.id]) placeOnRing(els[node.id], node);
    });
  }

  /* Rounded-rect outline path, clockwise from just right of the top-left
     corner — the same construction as CSS's own WarningRing::before mask,
     expressed as SVG path data instead of a border. Six straight runs
     (each edge shortened by r at both ends) joined by four quarter-circle
     arcs. Building this from measured w/h/r, instead of a literal string
     hand-edited in CSS, is what keeps .WarningRing__glob's offset-path in
     sync if the ring is ever resized. */
  function ringOutlinePath(w, h, r) {
    return 'path("' +
      'M ' + r + ',0 ' +
      'H ' + (w - r) + ' ' +
      'A ' + r + ',' + r + ' 0 0 1 ' + w + ',' + r + ' ' +
      'V ' + (h - r) + ' ' +
      'A ' + r + ',' + r + ' 0 0 1 ' + (w - r) + ',' + h + ' ' +
      'H ' + r + ' ' +
      'A ' + r + ',' + r + ' 0 0 1 0,' + (h - r) + ' ' +
      'V ' + r + ' ' +
      'A ' + r + ',' + r + ' 0 0 1 ' + r + ',0 Z' +
      '")';
  }

  function fitGlobPath() {
    var glob = els.kis.querySelector('.WarningRing__glob');
    if (!glob) return;
    var ring = rectOf(els.kis);
    glob.style.offsetPath = ringOutlinePath(ring.w, ring.h, ringRadius());
  }

  /* `animate` runs the theme's enter animation on mount — how the four
     root nodes arrive at boot. A child revealed by an expanding arrow
     passes `pending` instead: it mounts hidden, so it can be measured
     for the connector geometry, and revealNode() below plays its enter
     animation once that arrow has finished growing. */
  function addNode(node, index, animate, pending) {
    var anim = window.KIS.theme.get().animation;
    var el = window.KIS.createPageLink({
      id: node.id,
      label: node.label,
      image: node.icon,
      href: node.href,
      style: 'node',
      animation: animate ? { enter: anim.nodeEnter, delay: index * anim.nodeStagger } : null,
      onActivate: function () { onActivate(node); },
    });
    if (SITE.childrenOf(node.id).length) el.classList.add('PageLink--branch');
    if (pending) el.classList.add('is-pending');
    canvas.append(el);
    if (node.corner) placeOnRing(el, node); else place(el, node.pos);
    els[node.id] = el;
  }

  function revealNode(el, animate) {
    if (!el || !el.classList.contains('is-pending')) return;
    el.classList.remove('is-pending');
    if (animate && !reduced) {
      window.KIS.applyEnter(el, { enter: window.KIS.theme.get().animation.nodeEnter });
    }
  }

  /* ---- interaction ---- */

  function onActivate(node) {
    var kids = SITE.childrenOf(node.id);
    if (!kids.length) {
      window.open(window.KIS.assetPath(node.href), '_blank', 'noopener');
      return;
    }
    if (expanded.has(node.id)) collapse(node.id); else expand(node.id);
  }

  function expand(id) {
    expanded.add(id);
    els[id].classList.add('is-expanded');
    SITE.childrenOf(id).forEach(function (child, i) { addNode(child, i, false, true); });
    requestAnimationFrame(function () { drawLinks(id, true); });
  }

  function collapse(id) {
    SITE.childrenOf(id).forEach(function (child) {
      if (expanded.has(child.id)) collapse(child.id);
      var key = id + '->' + child.id;
      if (lines[key]) { lines[key].remove(); delete lines[key]; }
      if (els[child.id]) { els[child.id].remove(); delete els[child.id]; }
    });
    expanded.delete(id);
    els[id].classList.remove('is-expanded');
  }

  /* ---- connector geometry ----
     Layout geometry (offset*) ignores CSS transforms, so rects are stable
     even while a node's enter animation is still scaling it. */

  function rectOf(el) {
    return {
      x: el.offsetLeft - el.offsetWidth / 2,
      y: el.offsetTop - el.offsetHeight / 2,
      w: el.offsetWidth,
      h: el.offsetHeight,
    };
  }

  function center(r) {
    return { x: r.x + r.w / 2, y: r.y + r.h / 2 };
  }

  /* Point on r's border (plus GAP) along the line from r's center to `to` */
  function edgeTowards(r, to) {
    var c = center(r);
    var dx = to.x - c.x, dy = to.y - c.y;
    var tx = dx !== 0 ? (r.w / 2 + GAP) / Math.abs(dx) : Infinity;
    var ty = dy !== 0 ? (r.h / 2 + GAP) / Math.abs(dy) : Infinity;
    var t = Math.min(tx, ty, 0.48);
    return { x: c.x + dx * t, y: c.y + dy * t };
  }

  /* ---- arrow growth ----
     The expansion is the line's endpoint moving out from the parent's
     edge to the child. Only the shaft grows: the arrowhead and the child
     pill are what the arrival produces, so the motion reads as the
     parent reaching out and putting the child there.

     A stroke-dashoffset reveal cannot express this. It moves no
     endpoint, so a marker-end sits at the destination from the first
     frame — markers are placed at a shape's geometric vertices and
     ignore the dash pattern entirely. That is why the arrowhead used to
     appear before its line arrived.

     A <line>'s x1/y1/x2/y2 are plain attributes, not CSS geometry
     properties like a circle's cx/r, so no transition or Web Animation
     can drive them. The rAF tween below steps the attribute itself, and
     carries its own easing for the same reason — anim.lineEasing names a
     CSS timing function nothing here can hand to the browser. */

  var EASINGS = {
    linear: function (t) { return t; },
    'ease-out': function (t) { return 1 - Math.pow(1 - t, 3); },
  };

  function growLine(line, from, to, opts) {
    var ease = EASINGS[opts.easing] || EASINGS['ease-out'];
    var dx = to.x - from.x, dy = to.y - from.y;
    var start = null;

    function step(now) {
      // collapsed, or redrawn by a resize or theme swap, mid-flight
      if (!line.isConnected) return;
      if (start === null) start = now;
      var elapsed = now - start;
      if (elapsed < opts.delay) { requestAnimationFrame(step); return; }

      line.style.opacity = '';
      var t = Math.min(1, (elapsed - opts.delay) / opts.duration);
      line.setAttribute('x2', from.x + dx * ease(t));
      line.setAttribute('y2', from.y + dy * ease(t));
      if (t < 1) requestAnimationFrame(step);
      else opts.onArrive();
    }

    // Zero-length until its turn comes, and hidden until then: a stub at
    // the parent's edge would otherwise sit there as a visible dot for
    // the length of the stagger delay.
    line.style.opacity = '0';
    line.setAttribute('x2', from.x);
    line.setAttribute('y2', from.y);
    requestAnimationFrame(step);
  }

  function drawLinks(parentId, animate) {
    var anim = window.KIS.theme.get().animation;
    var pRect = rectOf(els[parentId]);

    SITE.childrenOf(parentId).forEach(function (child, i) {
      if (!els[child.id]) return;
      var key = parentId + '->' + child.id;
      if (lines[key]) { lines[key].remove(); delete lines[key]; }

      var cRect = rectOf(els[child.id]);
      var from = edgeTowards(pRect, center(cRect));
      var to = edgeTowards(cRect, center(pRect));

      var line = document.createElementNS(SVGNS, 'line');
      line.setAttribute('class', 'Connector');
      line.setAttribute('x1', from.x); line.setAttribute('y1', from.y);
      line.setAttribute('x2', to.x); line.setAttribute('y2', to.y);
      if (anim.connectorDash) line.setAttribute('stroke-dasharray', anim.connectorDash);

      /* Every theme expands the arrow the same way; only its timing and
         dash pattern still differ. The redraws that are not expansions —
         a resize, a theme swap — land the line complete, since their
         children are already on screen. */
      function arrive() {
        line.setAttribute('marker-end', 'url(#kis-arrow)');
        revealNode(els[child.id], animate);
      }

      if (animate && !reduced) {
        growLine(line, from, to, {
          duration: anim.lineDraw,
          easing: anim.lineEasing,
          delay: i * anim.nodeStagger,
          onArrive: arrive,
        });
      } else {
        arrive();
      }

      svg.append(line);
      lines[key] = line;
    });
  }

  function redrawAll() {
    fitGlobPath();
    positionRootNodes();
    Object.values(lines).forEach(function (l) { l.remove(); });
    lines = {};
    expanded.forEach(function (id) { drawLinks(id, false); });
  }

  /* ---- boot ---- */

  var kis = SITE.byId.kis;
  var logo = window.KIS.createLogoFrame({ animation: { enter: 'fade' } });
  place(logo, kis.pos);
  canvas.append(logo);
  els.kis = logo;
  fitGlobPath();

  SITE.childrenOf('kis').forEach(function (node, i) { addNode(node, i, true); });

  window.KIS.mountTopBar();

  window.addEventListener('resize', redrawAll);
  document.addEventListener('kis:theme', function () {
    // dash pattern and colors change with the theme; icon masks resolve
    // through the theme's iconDir, so remount nothing — just redraw lines
    redrawAll();
  });
})();
