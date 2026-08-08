/* KIS site — shared renderer for every generated document page.

   Counterpart to js/page.js, which renders the hand-written node pages. A
   generated page's HTML is the rendered markdown plus empty mount points;
   <body> names itself through:

     data-doc-node   the KIS.SITE node this document sits under, which is
                     what the breadcrumb is built from
     data-doc-title  the document's own title
     data-doc-source the repo path it was rendered from

   Everything visible here comes from js/components.js, so a document page
   and a node page are built out of the same parts. */

(function () {
  var SITE = window.KIS.SITE;
  var d = document.body.dataset;
  var head = document.getElementById('doc-header');
  var body = document.getElementById('doc-body');
  if (!head || !body) return;

  document.title = d.docTitle + ' — KIS';

  /* header: logo home link + breadcrumb + title + source path */

  var home = document.createElement('a');
  home.href = window.KIS.assetPath('index.html');
  home.title = 'KIS home';
  var logo = document.createElement('img');
  logo.className = 'PageHeader__logo';
  logo.dataset.kisLogo = '';
  logo.src = window.KIS.assetPath(window.KIS.theme.get().assets.logo);
  logo.alt = 'KIS — Keep It Simple, home';
  home.append(logo);

  /* Home → …the node's ancestors… → this document. The node chain is what
     ties a document back into the mind map: a reader who landed on a deep
     page can see which area of the site it belongs to. */
  var crumb = document.createElement('nav');
  crumb.className = 'Breadcrumb';
  var chain = [];
  for (var p = SITE.byId[d.docNode]; p; p = SITE.byId[p.parent]) chain.unshift(p);
  if (!chain.length) chain = [SITE.byId.kis];

  chain.forEach(function (n) {
    var a = document.createElement('a');
    a.href = window.KIS.assetPath(n.id === 'kis' ? 'index.html' : n.href);
    a.textContent = n.id === 'kis' ? 'Home' : n.label;
    crumb.append(a, ' → ');
  });
  crumb.append(d.docTitle);

  var title = document.createElement('div');
  title.className = 'PageHeader__title';
  var node = SITE.byId[d.docNode];
  if (node && node.icon) {
    var icon = document.createElement('span');
    icon.className = 'PageLink__icon';
    icon.style.setProperty('--icon-url', window.KIS.iconMaskUrl(node.icon));
    title.append(icon);
  }
  var h1 = document.createElement('h1');
  h1.textContent = d.docTitle;
  title.append(h1);

  var source = document.createElement('div');
  source.className = 'DocPage__source';
  source.textContent = d.docSource;

  head.append(home, crumb, title, source);

  /* the reusable index, mounted beside the content — every section, since
     the generator gave every heading an id */
  var toc = window.KIS.createTableOfContents({
    scope: body,
    label: 'On this page',
    minLevel: 2,
    maxLevel: 6,
    animation: { enter: 'fade' },
  });
  if (toc) document.getElementById('doc-toc').append(toc);

  /* Every link out of a document opens in a new tab, matching what a
     PageLink does — set here rather than in the generated HTML so the rule
     lives in one place. Same-page jumps (the index, a section permalink, a
     footnote) are excluded: opening a tab to scroll within the page you are
     already on is not navigation. */
  body.querySelectorAll('a[href]').forEach(function (a) {
    if (a.getAttribute('href').charAt(0) === '#') return;
    a.target = '_blank';
    a.rel = 'noopener';
  });

  window.KIS.mountTopBar();
})();
