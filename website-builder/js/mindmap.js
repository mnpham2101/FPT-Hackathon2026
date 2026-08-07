/* KIS site — the homepage mind map.
   Renders the KIS LogoFrame and the four root pairs, and reproduces the
   concept page's behaviour: activating a pair with children draws the
   connecting arrows and reveals the child pairs (animated per the active
   theme); activating a leaf — or the ↗ affordance on any pair — opens
   that pair's page in a new tab. */

(function () {
  var SVGNS = 'http://www.w3.org/2000/svg';
  var GAP = 6; // px between a connector end and the pair's border

  var canvas = document.getElementById('canvas');
  var svg = document.getElementById('connectors');
  var SITE = window.KIS.SITE;

  var els = {};      // node id -> mounted element
  var lines = {};    // "parent->child" -> <line>
  var expanded = new Set();
  var reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  /* ---- mounting ---- */

  function place(el, pos) {
    el.style.left = pos[0] + '%';
    el.style.top = pos[1] + '%';
  }

  function addPair(node, index, animate) {
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
    place(el, node.pos);
    canvas.append(el);
    els[node.id] = el;
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
    SITE.childrenOf(id).forEach(function (child, i) { addPair(child, i, true); });
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
     even while a pair's enter animation is still scaling it. */

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

  function drawLinks(parentId, animate) {
    var anim = window.KIS.theme.get().animation;
    var pRect = rectOf(els[parentId]);

    SITE.childrenOf(parentId).forEach(function (child) {
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
      line.setAttribute('marker-end', 'url(#kis-arrow)');
      if (anim.connectorDash) line.setAttribute('stroke-dasharray', anim.connectorDash);

      if (animate && !reduced) {
        if (anim.connectorDash) {
          // dashed themes fade the line in
          line.style.opacity = '0';
          line.style.transition = 'opacity ' + anim.lineDraw + 'ms ' + anim.lineEasing;
          requestAnimationFrame(function () {
            requestAnimationFrame(function () { line.style.opacity = '1'; });
          });
        } else {
          // solid themes draw the line like a pen stroke
          var len = Math.hypot(to.x - from.x, to.y - from.y);
          line.setAttribute('stroke-dasharray', len);
          line.setAttribute('stroke-dashoffset', len);
          line.style.transition = 'stroke-dashoffset ' + anim.lineDraw + 'ms ' + anim.lineEasing;
          requestAnimationFrame(function () {
            requestAnimationFrame(function () { line.setAttribute('stroke-dashoffset', 0); });
          });
        }
      }

      svg.append(line);
      lines[key] = line;
    });
  }

  function redrawAll() {
    Object.values(lines).forEach(function (l) { l.remove(); });
    lines = {};
    drawLinks('kis', false);
    expanded.forEach(function (id) { drawLinks(id, false); });
  }

  /* ---- boot ---- */

  var kis = SITE.byId.kis;
  var logo = window.KIS.createLogoFrame({ animation: { enter: 'fade' } });
  place(logo, kis.pos);
  canvas.append(logo);
  els.kis = logo;

  SITE.childrenOf('kis').forEach(function (node, i) { addPair(node, i, true); });
  requestAnimationFrame(function () { drawLinks('kis', true); });

  window.KIS.mountTopBar();

  window.addEventListener('resize', redrawAll);
  document.addEventListener('kis:theme', function () {
    // dash pattern and colors change with the theme; icon masks resolve
    // through the theme's iconDir, so remount nothing — just redraw lines
    redrawAll();
  });
})();
