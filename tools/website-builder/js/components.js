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
