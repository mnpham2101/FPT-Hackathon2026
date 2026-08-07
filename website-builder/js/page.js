/* KIS site — shared renderer for every node page.
   A page's HTML is boilerplate only: it names itself via
   <body data-page="…" data-root=".."> and this script builds the header,
   breadcrumb, summary and the child PageLink cards from KIS.SITE. */

(function () {
  var SITE = window.KIS.SITE;
  var node = SITE.byId[document.body.dataset.page];
  var app = document.getElementById('app');
  if (!node || !app) return;

  document.title = node.label + ' — KIS';

  /* header: logo home link + breadcrumb + title */
  var header = document.createElement('header');
  header.className = 'PageHeader';

  var home = document.createElement('a');
  home.href = window.KIS.assetPath('index.html');
  home.title = 'KIS home';
  var logo = document.createElement('img');
  logo.className = 'PageHeader__logo';
  logo.dataset.kisLogo = '';
  logo.src = window.KIS.assetPath(window.KIS.theme.get().assets.logo);
  logo.alt = 'KIS — Keep It Simple, home';
  home.append(logo);

  var crumb = document.createElement('nav');
  crumb.className = 'Breadcrumb';
  var chain = [];
  for (var p = node; p; p = SITE.byId[p.parent]) chain.unshift(p);
  chain.forEach(function (n, i) {
    if (i) crumb.append(' → ');
    if (n === node) { crumb.append(n.label); return; }
    var a = document.createElement('a');
    a.href = window.KIS.assetPath(n.id === 'kis' ? 'index.html' : n.href);
    a.textContent = n.id === 'kis' ? 'Home' : n.label;
    crumb.append(a);
  });

  var title = document.createElement('div');
  title.className = 'PageHeader__title';
  var icon = document.createElement('span');
  icon.className = 'PageLink__icon';
  icon.style.setProperty(
    '--icon-url',
    'url("' + window.KIS.assetPath(window.KIS.theme.get().assets.iconDir + '/' + node.icon + '.svg') + '")'
  );
  var h1 = document.createElement('h1');
  h1.textContent = node.label;
  title.append(icon, h1);

  header.append(home, crumb, title);
  app.append(header);

  /* body: summary + child pages as PageLink cards */
  var summary = document.createElement('p');
  summary.textContent = node.summary || '';
  app.append(summary);

  var kids = SITE.childrenOf(node.id);
  if (kids.length) {
    var h2 = document.createElement('h2');
    h2.textContent = 'Explore';
    h2.style.marginTop = '32px';
    app.append(h2);

    var grid = document.createElement('div');
    grid.className = 'CardGrid';
    var anim = window.KIS.theme.get().animation;
    kids.forEach(function (child, i) {
      grid.append(window.KIS.createPageLink({
        id: child.id,
        label: child.label,
        image: child.icon,
        href: child.href,
        style: 'card',
        animation: { enter: anim.cardEnter, delay: i * anim.cardStagger },
      }));
    });
    app.append(grid);
  }

  window.KIS.mountTopBar();
})();
