/* KIS site — theme manager.
   A theme is one name mapping to three things:
     - a CSS variable set  (css/themes.css, matched by [data-theme=name])
     - an asset set        (which logo file, where the icons live)
     - an animation set    (how connectors draw, how nodes enter)
   Components and the mind map read the active theme through KIS.theme.get();
   swapping or adding a theme means editing KIS.THEMES + themes.css only. */

window.KIS = window.KIS || {};

window.KIS.THEMES = {
  sketch: {
    label: 'Sketch',
    assets: {
      logo: 'assets/kis-logo.svg',
      iconDir: 'assets/icons',
    },
    animation: {
      lineDraw: 550,            // connector draw duration, ms
      lineEasing: 'ease-out',
      connectorDash: null,      // solid lines, animated as a pen stroke
      nodeEnter: 'pop',         // PageLink enter animation on the canvas
      nodeStagger: 110,         // ms between sibling entries
      cardEnter: 'fade',        // PageLink enter animation on pages
      cardStagger: 70,
    },
  },
  blueprint: {
    label: 'Blueprint',
    assets: {
      logo: 'assets/kis-logo-blueprint.svg',
      iconDir: 'assets/icons',
    },
    animation: {
      lineDraw: 850,
      lineEasing: 'linear',
      connectorDash: '6 5',     // dashed technical lines, faded in
      nodeEnter: 'rise',
      nodeStagger: 160,
      cardEnter: 'fade',
      cardStagger: 90,
    },
  },
};

window.KIS.theme = {
  current: 'sketch',

  get: function () {
    return window.KIS.THEMES[this.current];
  },

  apply: function (name) {
    if (!window.KIS.THEMES[name]) name = 'sketch';
    this.current = name;
    document.documentElement.dataset.theme = name;
    try { localStorage.setItem('kis-theme', name); } catch (e) { /* file:// may block storage */ }

    // Re-point every mounted logo at the theme's logo asset
    var logo = this.get().assets.logo;
    document.querySelectorAll('img[data-kis-logo]').forEach(function (img) {
      img.src = window.KIS.assetPath(logo);
    });

    document.dispatchEvent(new CustomEvent('kis:theme', { detail: { theme: name } }));
  },

  next: function () {
    var names = Object.keys(window.KIS.THEMES);
    var i = names.indexOf(this.current);
    this.apply(names[(i + 1) % names.length]);
  },
};

/* Resolve a site-relative path against the page's location.
   Pages set data-root=".." on <body>; the homepage sets ".". */
window.KIS.assetPath = function (rel) {
  var root = document.body.dataset.root || '.';
  return root + '/' + rel;
};

/* Icon mask URLs are a special case: they're assigned to --icon-url and
   read by a `mask: var(--icon-url) ...` declaration living in
   css/components.css. A url() reached through var() resolves relative to
   the STYLESHEET that declares the consuming property, not the page or
   wherever the custom property's value was set — so a page-relative path
   here 404s (it resolves against css/, one level too deep). components.css
   is always at <site-root>/css/components.css, so the correct base is the
   fixed "../" back to site root, regardless of which page included it. */
window.KIS.iconMaskUrl = function (name) {
  return 'url("../' + window.KIS.theme.get().assets.iconDir + '/' + name + '.svg")';
};

/* Filter defs referenced by components.css as filter:url(#id) — CSS
   filter() can't be declared inline, and components.css is a static
   file with no per-page templating, so the <filter> elements are
   injected here instead, once, before anything that might reference
   them gets created. Applied only under [data-theme="sketch"]
   (components.css decides that); the elements themselves are harmless
   and unused under other themes. */
(function () {
  var svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('width', '0');
  svg.setAttribute('height', '0');
  svg.style.position = 'absolute';
  svg.innerHTML =
    '<defs>' +
      '<filter id="kis-sketchy-icon" x="-60%" y="-60%" width="220%" height="220%">' +
        '<feTurbulence type="fractalNoise" baseFrequency="0.07" numOctaves="1" seed="4" result="n"/>' +
        '<feDisplacementMap in="SourceGraphic" in2="n" scale="2.5"/>' +
      '</filter>' +
      '<filter id="kis-sketchy-border" x="-60%" y="-60%" width="220%" height="220%">' +
        '<feTurbulence type="fractalNoise" baseFrequency="0.012" numOctaves="1" seed="9" result="n"/>' +
        '<feDisplacementMap in="SourceGraphic" in2="n" scale="2.5"/>' +
      '</filter>' +
    '</defs>';
  document.body.appendChild(svg);
})();

(function () {
  var saved = null;
  try { saved = localStorage.getItem('kis-theme'); } catch (e) { /* ignore */ }
  window.KIS.theme.apply(saved || 'sketch');
})();
