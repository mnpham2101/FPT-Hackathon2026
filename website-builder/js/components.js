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

/* Apply the shared animation prop to any component element */
window.KIS.applyEnter = function (el, animation) {
  var anim = animation || {};
  if (anim.enter && anim.enter !== 'none') {
    el.classList.add('is-enter-' + anim.enter);
    el.style.setProperty('--enter-delay', (anim.delay || 0) + 'ms');
  }
};

/* PageLink — an icon + text pair representing a navigable page.
   style 'node': positioned pair on the homepage canvas (mind map).
   style 'card': block in a CardGrid on a node page; clicking it opens
   the page. props.onActivate, when given, receives primary clicks
   (the mind map uses it for expand/collapse). */
window.KIS.createPageLink = function (props) {
  var el = document.createElement('div');
  el.className = 'PageLink PageLink--' + (props.style || 'node');
  el.dataset.id = props.id || '';

  var icon = document.createElement('span');
  icon.className = 'PageLink__icon';
  icon.style.setProperty(
    '--icon-url',
    'url("' + window.KIS.assetPath(window.KIS.theme.get().assets.iconDir + '/' + props.image + '.svg') + '")'
  );

  var label = document.createElement('span');
  label.className = 'PageLink__label';
  label.textContent = props.label;

  el.append(icon, label);

  if (props.href) {
    var open = document.createElement('a');
    open.className = 'PageLink__open';
    open.href = window.KIS.assetPath(props.href);
    open.target = '_blank';
    open.rel = 'noopener';
    open.title = 'Open ' + props.label + ' in a new tab';
    open.textContent = '↗';
    open.addEventListener('click', function (e) { e.stopPropagation(); });
    el.append(open);

    if ((props.style || 'node') === 'card') {
      el.addEventListener('click', function () { open.click(); });
    }
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
